/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2019-2020 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mdtext.h"
#include "comua.h"
#include "entity.h"
#include "md4c-utf16.h"
#include "c-reusables/data/buffer.h"

#include <float.h>
#include <math.h>


/* Uncomment this to have more verbose traces from this module. */
/*#define MDTEXT_DEBUG     1*/

#ifdef MDTEXT_DEBUG
    #define MDTEXT_TRACE    MC_TRACE
#else
    #define MDTEXT_TRACE    MC_NOOP
#endif



#define MDTEXT_TEXT_COLOR           RGB(0,0,0)
#define MDTEXT_QUOTE_DECOR_COLOR    RGB(223,223,223)
#define MDTEXT_CODE_BK_COLOR        RGB(247,247,247)
#define MDTEXT_HR_COLOR             RGB(191,191,191)
#define MDTEXT_LINK_COLOR           RGB(0,102,204)


#define MDTEXT_NODE_IS_CONTAINER        0x01
#define MDTEXT_NODE_IS_TIGHT            0x02


#define MDTEXT_NODE_SECTION_LINK_COMUA  1


/* Keep the members in this struct in the (unnatural) order they are. It is
 * to minimize the memory consumption. Depending on the Markdown document,
 * we may have quite a lot of these. */
typedef struct mdtext_node_tag mdtext_node_t;
struct mdtext_node_tag {
    RECT rect;

    /* The interpretation of the data is little bit complicated:
     *
     * (1) For normal (leaf) blocks, it describes the text. The first bytes is
     *     the pointer to the c_IDWriteTextLayout DWrite object which can be
     *     used to paint the text with all the bells and whistles.
     *
     *     The pointer is optionally followed by additional "sections" of data.
     *     For example, a (compressed) sequence of UINTs which maps the offsets
     *     as used by the text layout to the offsets in the original raw input
     *     Markdown document. This is used to e.g. handle selection support and
     *     copying the source text into clipboard when user presses Ctrl+C.
     *
     *     Each such section begins with a compressed section id UINT and
     *     followed with compressed UINT specifying the count of bytes in the
     *     section data payload following it.
     *
     *     Inner format of these sections is only documented by the code which
     *     handles the given section.
     *
     * (2) For container blocks which have nested blocks (i.e. with the flag
     *     MDTEXT_NODE_IS_CONTAINER), the data is just a sequence of UINTs.
     *     Those are list of child nodes (indexes into nodes[]).
     *
     * (3) Quite rarely a block may need to combine both the child blocks and
     *     textual contents (or multiple instances of texts). This is most
     *     common for list items: They may have their own text and also contain
     *     other block(s), most often a nested list.
     *
     *     In this case the node is simply considered a container block (see
     *     the point (2)) and the textual contents is downgraded and become
     *     child paragraph(s).
     */
    void* data;
    UINT data_len;

    MD_BLOCKTYPE type   : 8;
    unsigned flags      : 8;
    unsigned aux        : 16;
};

static inline c_IDWriteTextLayout*
mdtext_node_text_layout(mdtext_node_t* node)
{
    if(node->flags & MDTEXT_NODE_IS_CONTAINER)
        return NULL;
    if(node->data_len < sizeof(c_IDWriteTextLayout*))
        return NULL;
    return *(c_IDWriteTextLayout**) node->data;
}

static inline void*
mdtext_node_section(mdtext_node_t* node, UINT id, size_t* p_size)
{
    size_t off;

    if(node->flags & MDTEXT_NODE_IS_CONTAINER)
        return NULL;

    off = sizeof(c_IDWriteTextLayout*);
    while(off < node->data_len) {
        UINT section_id = comua_read(node->data, node->data_len, off, &off);
        size_t section_size = comua_read(node->data, node->data_len, off, &off);

        if(id == section_id) {
            *p_size = section_size;
            return (void*)(((BYTE*) node->data) + off);
        }

        off += section_size;
    }

    return NULL;
}

typedef struct mdtext_tag mdtext_t;
struct mdtext_tag {
    mdtext_node_t* nodes;
    UINT node_count;
    UINT min_width;
    UINT digit_width    : 16;
    UINT line_height    : 16;

    /* A flat buffer holding things link destinations. */
    TCHAR* attr_buffer;

    /* Provided by the control. Do not modify/free these. */
    const TCHAR* text;
    UINT text_len;
    c_IDWriteTextFormat* text_fmt;
};


static void
mdtext_init(mdtext_t* mdtext, c_IDWriteTextFormat* text_fmt, const TCHAR* text, UINT text_len)
{
    c_IDWriteTextLayout* text_layout;

    memset(mdtext, 0, sizeof(mdtext_t));

    mdtext->text = text;
    mdtext->text_len = text_len;
    mdtext->text_fmt = text_fmt;

    /* We use an average digit width for computing e.g. list item paddings. */
    mdtext->digit_width = 10;
    mdtext->line_height = 16;
    text_layout = xdwrite_create_text_layout(_T("1234567890"), 10, text_fmt,
                FLT_MAX, FLT_MAX, XDWRITE_NOWRAP);
    if(text_layout != NULL) {
        c_DWRITE_TEXT_METRICS text_metrics;
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        mdtext->digit_width = ceilf(text_metrics.width / 10.0f);
        mdtext->line_height = ceilf(text_metrics.height);
        c_IDWriteTextLayout_Release(text_layout);
    }

    mdtext->min_width = 0;
}

static void
mdtext_fini(mdtext_t* mdtext)
{
    UINT i;

    for(i = 0; i < mdtext->node_count; i++) {
        mdtext_node_t* node = &mdtext->nodes[i];
        c_IDWriteTextLayout* text_layout = mdtext_node_text_layout(node);

        if(text_layout != NULL)
            c_IDWriteTextLayout_Release(text_layout);
    }

    free(mdtext->nodes);
    free(mdtext->attr_buffer);
}


/****************
 *** Geometry ***
 ****************/

/*Padding determines dimensions of an (optional) frame of a block. The padding
 * is considered to be _inside_ the block (i.e. given some particular block
 * height, any text is painted with some additional horiz. and vert. offsets.
 * This makes a space for e.g. list item marks, block quite marks, etc.
 *
 * Margin is a space _between_ two adjacent blocks. Given our blocks are always
 * laid out vertically, we only support top and bottom margins. The real margin
 * between blocks A and B is MAX(bottom_margin(A), top_margin(B)), assuming
 * the both blocks have the same parent block. (If they do not, the margin
 * is determined by their respective parent blocks.)
 */

static int
mdtext_unit(mdtext_t* mdtext)
{
    return ceilf(c_IDWriteTextFormat_GetFontSize(mdtext->text_fmt));
}

static int
mdtext_padding_left(mdtext_t* mdtext, mdtext_node_t* node)
{
    int unit = mdtext->digit_width;

    switch(node->type) {
        case MD_BLOCK_DOC:
            return 3 * unit;

        case MD_BLOCK_QUOTE:
        case MD_BLOCK_CODE:
            return (3 * unit) / 2;

        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            return 4 * unit;

        default:
            return 0;
    }
}

static int
mdtext_padding_right(mdtext_t* mdtext, mdtext_node_t* node)
{
    if(node->type == MD_BLOCK_UL || node->type == MD_BLOCK_OL)
        return 0;
    return mdtext_padding_left(mdtext, node);
}

static int
mdtext_padding_top(mdtext_t* mdtext, mdtext_node_t* node)
{
    int unit = mdtext_unit(mdtext);

    switch(node->type) {
        case MD_BLOCK_DOC:
            return 2 * unit;

        case MD_BLOCK_QUOTE:
        case MD_BLOCK_CODE:
            return unit / 2;

        default:
            return 0;
    }
}

static int
mdtext_padding_bottom(mdtext_t* mdtext, mdtext_node_t* node)
{
    return mdtext_padding_top(mdtext, node);
}

static int
mdtext_margin_top(mdtext_t* mdtext, mdtext_node_t* node)
{
    if(node->type == MD_BLOCK_LI  &&  (node->flags & MDTEXT_NODE_IS_TIGHT))
        return 0;

    return mdtext_unit(mdtext);
}

static int
mdtext_margin_bottom(mdtext_t* mdtext, mdtext_node_t* node)
{
    return mdtext_margin_top(mdtext, node);
}


/*********************************
 *** Markdown parser callbacks ***
 *********************************/

typedef struct mdtext_stack_record_tag mdtext_stack_record_t;
struct mdtext_stack_record_tag {
    /* Index into mdtext_t::nodes[]. */
    int node_index;
    int last_child_node_index;

    /* Compressed indexes of child nodes. */
    BUFFER children;

    /* Text contents:
     * Begins with pointer to the IDWriteTextLayout.
     * (We plan additional data after it....)
     */
    c_IDWriteTextLayout* text_layout;

    /* COMUA for links.
     */
    BUFFER link_comua;
};

typedef struct mdtext_span_stack_record_tag mdtext_span_stack_record_t;
struct mdtext_span_stack_record_tag {
    UINT beg;
};

typedef struct mdtext_parse_ctx_tag mdtext_parse_ctx_t;
struct mdtext_parse_ctx_tag {
    mdtext_t* mdtext;

    /* Flat array of all nodes in the tree. The root is [0]. */
    ARRAY nodes;

    /* Current block nesting. Each record is mdtext_stack_record_t. */
    STACK stack;

    /* Textual contents of the _current_ node (the top of stack) in order to
     * create its IDWriteTextLayout. Once we decide the text is complete, we
     * "flush" it into mdtext_stack_record_t::text_contents. */
    BUFFER buffer;

    /* Current span nesting, as corresponds to the text in the buffer.
     * Each record is mdtext_span_stack_record_t. */
    STACK span_stack;

    /* Completed (closed) spans are collected in this buffer.
     * Each span is described by three compressed UINTs in this order:
     * span type (MD_SPAN_TYPE), begin offset, span length. */
    BUFFER spans;

    /* Buffer of attribute strings (this is shared by all nodes). */
    BUFFER attr_buffer;

    DWORD flags;
    int x0;
    int x1;
    int y;
    int width;
};

#define NODE_COUNT(ctx)     array_count(&(ctx)->nodes, sizeof(mdtext_node_t))
#define NODE(ctx, index)    ((mdtext_node_t*) array_get_raw(&(ctx)->nodes, (index), sizeof(mdtext_node_t)))

static int
mdtext_ctx_init(mdtext_parse_ctx_t* ctx, mdtext_t* mdtext, UINT width, DWORD flags)
{
    ctx->mdtext = mdtext;
    array_init(&ctx->nodes);
    stack_init(&ctx->stack);
    buffer_init(&ctx->buffer);
    stack_init(&ctx->span_stack);
    buffer_init(&ctx->spans);
    buffer_init(&ctx->attr_buffer);
    ctx->flags = flags;
    ctx->x0 = 0;
    ctx->x1 = width;
    ctx->y = 0;
    ctx->width = width;

    /* Add an empty string so we can share it by all empty attributes. */
    if(MC_ERR(buffer_append(&ctx->attr_buffer, _T(""), sizeof(TCHAR)) != 0)) {
        MC_TRACE("mdtext_ctx_init: buffer_append() failed.");
        return -1;
    }

    return 0;
}

static void
mdtext_ctx_fini(mdtext_parse_ctx_t* ctx)
{
    /* The stack should generally be empty except when we aborted the parsing
     * due to any error. */
    while(!stack_is_empty(&ctx->stack)) {
        mdtext_stack_record_t* stack_record =
                stack_pop_raw(&ctx->stack, sizeof(mdtext_stack_record_t));

        if(stack_record->text_layout != NULL)
            c_IDWriteTextLayout_Release(stack_record->text_layout);

        buffer_fini(&stack_record->children);
        buffer_fini(&stack_record->link_comua);
    }

    /* ctx->nodes are not released here: That's what we are constructing for
     * the mdtext. */

    stack_fini(&ctx->stack);
    buffer_fini(&ctx->buffer);
    stack_fini(&ctx->span_stack);
    buffer_fini(&ctx->spans);
    buffer_fini(&ctx->attr_buffer);
}

static int
mdtext_add_node(mdtext_parse_ctx_t* ctx, MD_BLOCKTYPE type)
{
    mdtext_node_t* node;

    node = (mdtext_node_t*) array_append_raw(&ctx->nodes, sizeof(mdtext_node_t));
    if(MC_ERR(node == NULL)) {
        MC_TRACE("mdtext_add_node: array_append_raw() failed.");
        return -1;
    }

    node->type = type;
    node->flags = 0;
    node->data = NULL;
    node->data_len = 0;
    node->aux = 0;
    return NODE_COUNT(ctx) - 1;
}

static int
mdtext_add_attr(mdtext_parse_ctx_t* ctx, const MD_ATTRIBUTE* attr, size_t* p_offset)
{
    static const MD_CHAR replacement[1] = { 0xfffd };

    int i;
    size_t off = buffer_size(&ctx->attr_buffer);
    entity_t ent;

    if(attr->size == 0) {
        /* En empty attribute: Reuse one empty string we have prepared during
         * ctx initialization. */
        *p_offset = 0;
        return 0;
    }

    for(i = 0; attr->substr_offsets[i] < attr->size; i++) {
        MD_TEXTTYPE sub_type = attr->substr_types[i];
        MD_OFFSET sub_off = attr->substr_offsets[i];
        MD_SIZE sub_size = attr->substr_offsets[i+1] - sub_off;
        const MD_CHAR* sub_text = attr->text + sub_off;

        switch(sub_type) {
            case MD_TEXT_NULLCHAR:
                sub_text = replacement;
                sub_size = MC_SIZEOF_ARRAY(replacement);
                break;

            case MD_TEXT_ENTITY:
                if(sub_size > 2  &&  sub_text[0] == _T('&')  &&  sub_text[sub_size-1] == _T(';')) {
                    if(entity_decode(sub_text+1, &ent) == 0) {
                        sub_text = ent.buffer;
                        sub_size = ent.len;
                        break;
                    } else {
                        MC_TRACE("mdtext_add_attr: Unknown entity name '%.*S'.", (int)sub_size, sub_text);
                    }
                }
                /* Pass through: Output the entity verbatim as an ordinary text. */

            case MD_TEXT_NORMAL:
            default:
                /* noop */
                break;
        }

        if(buffer_append(&ctx->attr_buffer, sub_text, sub_size * sizeof(MD_CHAR)) != 0)
            goto err;
    }
    if(buffer_append(&ctx->attr_buffer, _T(""), sizeof(TCHAR)) != 0)
        goto err;

    *p_offset = off;
    return 0;

err:
    MC_TRACE("mdtext_add_attr: buffer_append() failed.");
    return -1;
}

/* Setup node->data + node_data_len by promoting the text layout and related
 * data to it. */
static int
mdtext_commit_text_contents(mdtext_parse_ctx_t* ctx, mdtext_node_t* node,
                            mdtext_stack_record_t* stack_record)
{
    BUFFER buf = BUFFER_INITIALIZER;
    int ret = -1;

    if(stack_record->text_layout == NULL) {
        node->data = NULL;
        node->data_len = 0;
        return 0;
    }

    if(MC_ERR(buffer_append(&buf, &stack_record->text_layout, sizeof(c_IDWriteTextLayout*)) != 0)) {
        MC_TRACE("mdtext_commit_text_contents: buffer_append() failed.");
        goto err;
    }

    if(!buffer_is_empty(&stack_record->link_comua)) {
        size_t link_comua_size = buffer_size(&stack_record->link_comua);

        if(MC_ERR(comua_append(&buf, MDTEXT_NODE_SECTION_LINK_COMUA, 0) != 0  ||
                  comua_append(&buf, link_comua_size, 0) != 0  ||
                  buffer_append(&buf, buffer_data(&stack_record->link_comua), link_comua_size) != 0))
        {
            MC_TRACE("mdtext_commit_text_contents: buffer_append() failed.");
            goto err;
        }
    }

    node->data_len = buffer_size(&buf);
    node->data = buffer_acquire(&buf);
    ret = 0;

err:
    buffer_fini(&stack_record->link_comua);
    return ret;
}

static int
mdtext_downgrade_text_contents(mdtext_parse_ctx_t* ctx, mdtext_stack_record_t* stack_record)
{
    int node_index;

    if(stack_record->text_layout == NULL)
        return 0;

    node_index = mdtext_add_node(ctx, MD_BLOCK_P);
    if(MC_ERR(node_index < 0)) {
        MC_TRACE("mdtext_downgrade_text_contents: mdtext_add_node() failed.");
        return -1;
    }

    if(MC_ERR(comua_append(&stack_record->children, node_index, COMUA_FLAG_RECORD_LEADER) != 0)) {
        MC_TRACE("mdtext_downgrade_text_contents: comua_append() failed.");
        return -1;
    }

    if(MC_ERR(mdtext_commit_text_contents(ctx, NODE(ctx, node_index), stack_record) != 0)) {
        MC_TRACE("mdtext_downgrade_text_contents: mdtext_commit_text_contents() failed.");
        return -1;
    }

    return 0;
}

static const xdwrite_effect_t mdtext_link_effect =
                    XDWRITE_EFFECT_INIT_CREF(MDTEXT_LINK_COLOR);
static const xdwrite_effect_t mdtext_codespan_effect =
                    XDWRITE_EFFECT_INIT_BK_CREF(MDTEXT_CODE_BK_COLOR);

static void
mdtext_use_code_font(c_IDWriteTextLayout* text_layout, c_DWRITE_TEXT_RANGE range)
{
    static const WCHAR* family_list[] = { L"Consolas", L"Courier New", NULL };
    int i;
    HRESULT hr;

    for(i = 0; family_list[i] != NULL; i++) {
        hr = c_IDWriteTextLayout_SetFontFamilyName(text_layout, family_list[i], range);
        if(SUCCEEDED(hr))
            return;
    }
    MC_TRACE_HR("mdtext_use_code_font: IDWriteTextLayout::SetFontFamilyName() failed.");
}

static int
mdtext_flush_text(mdtext_parse_ctx_t* ctx, mdtext_stack_record_t* stack_record)
{
    c_IDWriteTextLayout* text_layout;
    mdtext_node_t* node;
    TCHAR* text;
    UINT text_len;
    DWORD text_layout_flags;
    float text_min_width;
    void* span_data;
    size_t span_data_len;
    size_t off;

    if(buffer_is_empty(&ctx->buffer))
        return 0;

    if(MC_ERR(mdtext_downgrade_text_contents(ctx, stack_record) != 0)) {
        MC_TRACE("mdtext_flush_text: mdtext_downgrade_text_contents() failed.");
        return -1;
    }

    node = NODE(ctx, stack_record->node_index);

    text_layout_flags = 0;
    switch(node->type) {
        case MD_BLOCK_CODE:
            text_layout_flags |= XDWRITE_NOWRAP;
            break;

        case MD_BLOCK_H:
            /* noop: We never justify the headers; it may look odd for the
             * larger font sizes. */
            break;

        default:
            if(!(ctx->flags & MDTEXT_FLAG_NOJUSTIFY))
                text_layout_flags |= XDWRITE_ALIGN_JUSTIFY;
            break;
    }

    text_len = buffer_size(&ctx->buffer) / sizeof(MD_CHAR);
    text = (TCHAR*) buffer_data(&ctx->buffer);

    /* Ignore final new lines. */
    while(text_len > 0  &&  text[text_len-1] == _T('\n'))
        text_len--;

    text_layout = xdwrite_create_text_layout(
            buffer_data(&ctx->buffer), text_len, ctx->mdtext->text_fmt,
            MC_MAX(0.0f, (float)(ctx->x1 - ctx->x0)), FLT_MAX, text_layout_flags);
    if(MC_ERR(text_layout == NULL)) {
        MC_TRACE("mdtext_flush_text: xdwrite_create_text_layout() failed.");
        return -1;
    }

    MC_ASSERT(stack_record->text_layout == NULL);
    stack_record->text_layout = text_layout;

    switch(node->type) {
        case MD_BLOCK_H:
        {
            static const float size_factor[6] = { 1.66f, 1.33f, 1.17f, 1.0f, 0.83f, 0.75f };
            c_DWRITE_TEXT_RANGE range = { 0, text_len };
            float font_size;

            font_size = c_IDWriteTextLayout_GetFontSize(text_layout);
            font_size *= size_factor[node->aux - 1];
            c_IDWriteTextLayout_SetFontSize(text_layout, font_size, range);
            c_IDWriteTextLayout_SetFontWeight(text_layout, FW_BOLD, range);
            break;
        }

        case MD_BLOCK_CODE:
        {
            c_DWRITE_TEXT_RANGE range = { 0, text_len };

            mdtext_use_code_font(text_layout, range);
            break;
        }

        default:
            break;
    }

    /* Apply spans. */
    span_data = buffer_data(&ctx->spans);
    span_data_len = buffer_size(&ctx->spans);
    off = 0;
    while(off < span_data_len) {
        MD_SPANTYPE span_type;
        c_DWRITE_TEXT_RANGE range;

        span_type = (MD_SPANTYPE) comua_read(span_data, span_data_len, off, &off);
        range.startPosition = comua_read(span_data, span_data_len, off, &off);
        range.length = comua_read(span_data, span_data_len, off, &off);

        switch(span_type) {
            case MD_SPAN_EM:
                c_IDWriteTextLayout_SetFontStyle(text_layout, c_DWRITE_FONT_STYLE_ITALIC, range);
                break;

            case MD_SPAN_STRONG:
                c_IDWriteTextLayout_SetFontWeight(text_layout, FW_BOLD, range);
                break;

            case MD_SPAN_DEL:
                c_IDWriteTextLayout_SetStrikethrough(text_layout, TRUE, range);
                break;

            case MD_SPAN_A:
                c_IDWriteTextLayout_SetDrawingEffect(text_layout,
                            (IUnknown*) &mdtext_link_effect, range);
                c_IDWriteTextLayout_SetUnderline(text_layout, TRUE, range);
                break;

            case MD_SPAN_CODE:
                c_IDWriteTextLayout_SetDrawingEffect(text_layout,
                            (IUnknown*) &mdtext_codespan_effect, range);
                mdtext_use_code_font(text_layout, range);
                break;

            default:
                break;
        }
    }

    /* Calculate minimal width required to present the block. */
    if(!(text_layout_flags & XDWRITE_NOWRAP)) {
        c_IDWriteTextLayout_DetermineMinWidth(text_layout, &text_min_width);
    } else {
        c_DWRITE_TEXT_METRICS text_metrics;
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        text_min_width = text_metrics.width;
    }
    ctx->mdtext->min_width = MC_MAX(ctx->mdtext->min_width,
                ceilf(text_min_width) + ctx->x0 + ctx->width - ctx->x1);

    buffer_clear(&ctx->buffer);
    buffer_clear(&ctx->spans);
    return 0;
}

static int
mdtext_enter_block_cb(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    mdtext_parse_ctx_t* ctx = (mdtext_parse_ctx_t*) userdata;
    mdtext_t* mdtext = ctx->mdtext;
    int node_index;
    mdtext_node_t* node;
    mdtext_stack_record_t* parent_stack_record;
    mdtext_stack_record_t* node_stack_record;

    if(!stack_is_empty(&ctx->stack))
        parent_stack_record = (mdtext_stack_record_t*)
                stack_peek_raw(&ctx->stack, sizeof(mdtext_stack_record_t));
    else
        parent_stack_record = NULL;

    /* Create a new node to represent the block. */
    node_index = mdtext_add_node(ctx, type);
    if(MC_ERR(node_index < 0)) {
        MC_TRACE("mdtext_enter_block_cb: mdtext_add_node(1) failed.");
        return -1;
    }

    /* Register the node as a child in its parent. We may need to flush
     * the parents text and downgrade it to a child paragraph. */
    if(parent_stack_record != NULL) {
        if(MC_ERR(mdtext_flush_text(ctx, parent_stack_record) != 0)) {
            MC_TRACE("mdtext_enter_block_cb: mdtext_flush_text() failed.");
            return -1;
        }

        if(MC_ERR(mdtext_downgrade_text_contents(ctx, parent_stack_record) != 0)) {
            MC_TRACE("mdtext_enter_block_cb: mdtext_downgrade_text_contents() failed.");
            return -1;
        }

        if(MC_ERR(comua_append(&parent_stack_record->children, node_index, COMUA_FLAG_RECORD_LEADER) != 0)) {
            MC_TRACE("mdtext_enter_block_cb: comua_append() failed.");
            return -1;
        }
    }

    /* Setup the new node. */
    node = NODE(ctx, node_index);
    if(parent_stack_record != NULL  &&  parent_stack_record->last_child_node_index >= 0) {
        ctx->y += MC_MAX(
            mdtext_margin_bottom(mdtext, NODE(ctx, parent_stack_record->last_child_node_index)),
            mdtext_margin_top(mdtext, node)
        );
    }
    node->rect.left = ctx->x0;
    node->rect.top = ctx->y;
    node->rect.right = ctx->x1;
    /* ->bottom is set in mdtext_leave_block_cb() when we know its contents. */

    switch(type) {
        case MD_BLOCK_H:
            node->aux = ((MD_BLOCK_H_DETAIL*) detail)->level;
            break;

        case MD_BLOCK_UL:
            if(((MD_BLOCK_UL_DETAIL*) detail)->is_tight)
                node->flags |= MDTEXT_NODE_IS_TIGHT;
            node->aux = 10000 + ((MD_BLOCK_UL_DETAIL*) detail)->mark;
            break;

        case MD_BLOCK_OL:
            if(((MD_BLOCK_OL_DETAIL*) detail)->is_tight)
                node->flags |= MDTEXT_NODE_IS_TIGHT;
            node->aux = ((MD_BLOCK_OL_DETAIL*) detail)->start % 10000;
            break;

        case MD_BLOCK_LI:
        {
            mdtext_node_t* parent = NODE(ctx, parent_stack_record->node_index);

            MC_ASSERT(parent->type == MD_BLOCK_UL || parent->type == MD_BLOCK_OL);
            if(parent->flags & MDTEXT_NODE_IS_TIGHT)
                node->flags |= MDTEXT_NODE_IS_TIGHT;
            node->aux = parent->aux;
            if(parent->aux < 10000)
                parent->aux = (parent->aux + 1) % 10000;
            break;
        }

        default:
            break;
    }

    /* Push the new node to the stack. */
    node_stack_record = (mdtext_stack_record_t*)
                stack_push_raw(&ctx->stack, sizeof(mdtext_stack_record_t));
    if(MC_ERR(node_stack_record == NULL)) {
        MC_TRACE("mdtext_enter_block_cb: stack_push_raw() failed.");
        return -1;
    }
    node_stack_record->node_index = node_index;
    node_stack_record->last_child_node_index = -1;
    node_stack_record->text_layout = NULL;
    buffer_init(&node_stack_record->children);
    buffer_init(&node_stack_record->link_comua);

    /* Define smaller world for children. */
    ctx->x0 += mdtext_padding_left(mdtext, node);
    ctx->x1 -= mdtext_padding_right(mdtext, node);
    ctx->y += mdtext_padding_top(mdtext, node);
    return 0;
}

static int
mdtext_leave_block_cb(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    mdtext_parse_ctx_t* ctx = (mdtext_parse_ctx_t*) userdata;
    mdtext_t* mdtext = ctx->mdtext;
    mdtext_stack_record_t* node_stack_record;
    mdtext_node_t* node;

    MC_ASSERT(!stack_is_empty(&ctx->stack));

    /* Peek only here, we pop only on success path below. This guarantees
     * the cleaner code in mdtext_ctx_fini() would still see the node if we
     * fail below. */
    node_stack_record = (mdtext_stack_record_t*)
                stack_peek_raw(&ctx->stack, sizeof(mdtext_stack_record_t));

    if(MC_ERR(mdtext_flush_text(ctx, node_stack_record) != 0)) {
        MC_TRACE("mdtext_leave_block_cb: mdtext_flush_text() failed.");
        return -1;
    }

    if(!buffer_is_empty(&node_stack_record->children)) {
        if(MC_ERR(mdtext_downgrade_text_contents(ctx, node_stack_record) != 0)) {
            MC_TRACE("mdtext_leave_block_cb: mdtext_downgrade_text_contents() failed.");
            return -1;
        }
    }

    /* Promote the node data into the final mdtext_node_t. */
    node = NODE(ctx, node_stack_record->node_index);
    if(!buffer_is_empty(&node_stack_record->children)) {
        node->data_len = buffer_size(&node_stack_record->children);
        node->data = (void*) buffer_acquire(&node_stack_record->children);
        node->flags |= MDTEXT_NODE_IS_CONTAINER;
    } else {
        if(MC_ERR(mdtext_commit_text_contents(ctx, node, node_stack_record) != 0)) {
            MC_TRACE("mdtext_leave_block_cb: mdtext_commit_text_contents() failed.");
            return -1;
        }
    }

    /* Determine the bottom edge of the node. */
    if(node->flags & MDTEXT_NODE_IS_CONTAINER) {
        MC_ASSERT(node_stack_record->last_child_node_index >= 0);
        node->rect.bottom = NODE(ctx, node_stack_record->last_child_node_index)->rect.bottom;
    } else {
        c_IDWriteTextLayout* text_layout = mdtext_node_text_layout(node);

        node->rect.bottom = node->rect.top;
        if(text_layout != NULL) {
            c_DWRITE_TEXT_METRICS text_metrics;
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            node->rect.bottom += ceilf(text_metrics.height);
        } else {
            node->rect.bottom += mdtext->line_height;
        }
    }
    node->rect.bottom += mdtext_padding_bottom(mdtext, node);

    /* Return to the bigger world. */
    ctx->x0 -= mdtext_padding_left(mdtext, node);
    ctx->x1 += mdtext_padding_right(mdtext, node);
    ctx->y = node->rect.bottom;

    /* Finally, remove the node really from the stack. */
    stack_pop_raw(&ctx->stack, sizeof(mdtext_stack_record_t));
    if(!stack_is_empty(&ctx->stack)) {
        mdtext_stack_record_t* parent_stack_record = (mdtext_stack_record_t*)
                stack_peek_raw(&ctx->stack, sizeof(mdtext_stack_record_t));
        parent_stack_record->last_child_node_index = node_stack_record->node_index;
    }

    return 0;
}

static int
mdtext_enter_span_cb(MD_SPANTYPE type, void* detail, void* userdata)
{
    mdtext_parse_ctx_t* ctx = (mdtext_parse_ctx_t*) userdata;
    mdtext_span_stack_record_t* rec;

    rec = (mdtext_span_stack_record_t*) stack_push_raw(&ctx->span_stack,
                    sizeof(mdtext_span_stack_record_t));
    if(MC_ERR(rec == NULL)) {
        MC_TRACE("mdtext_enter_span_cb: stack_push_raw() failed.");
        return -1;
    }

    rec->beg = buffer_size(&ctx->buffer) / sizeof(TCHAR);
    return 0;
}

static int
mdtext_leave_span_cb(MD_SPANTYPE type, void* detail, void* userdata)
{
    mdtext_parse_ctx_t* ctx = (mdtext_parse_ctx_t*) userdata;
    mdtext_stack_record_t* block_rec;
    mdtext_span_stack_record_t* rec;
    UINT end;

    MC_ASSERT(!stack_is_empty(&ctx->stack));
    block_rec = (mdtext_stack_record_t*)
            stack_peek_raw(&ctx->stack, sizeof(mdtext_stack_record_t));

    MC_ASSERT(!stack_is_empty(&ctx->span_stack));
    rec = (mdtext_span_stack_record_t*) stack_pop_raw(&ctx->span_stack,
                    sizeof(mdtext_span_stack_record_t));

    end = buffer_size(&ctx->buffer) / sizeof(TCHAR);
    if(MC_ERR(comua_append(&ctx->spans, (UINT) type, COMUA_FLAG_RECORD_LEADER) != 0  ||
              comua_append(&ctx->spans, rec->beg, 0) != 0  ||
              comua_append(&ctx->spans, end - rec->beg, 0) != 0)) {
        MC_TRACE("mdtext_leave_span_cb: comua_append() failed.");
        return -1;
    }

    switch(type) {
        case MD_SPAN_A:
        {
            MD_SPAN_A_DETAIL* det = (MD_SPAN_A_DETAIL*) detail;
            size_t off_href, off_title;

            if(MC_ERR(mdtext_add_attr(ctx, &det->href, &off_href) != 0 ||
                      mdtext_add_attr(ctx, &det->title, &off_title) != 0)) {
                MC_TRACE("mdtext_enter_span_cb: mdtext_add_attr(link) failed.");
                return -1;
            }

            if(MC_ERR(comua_append(&block_rec->link_comua, rec->beg, COMUA_FLAG_RECORD_LEADER) != 0 ||
                      comua_append(&block_rec->link_comua, end - rec->beg, 0) != 0 ||
                      comua_append(&block_rec->link_comua, off_href / sizeof(TCHAR), 0) != 0 ||
                      comua_append(&block_rec->link_comua, off_title / sizeof(TCHAR), 0) != 0)) {
                MC_TRACE("mdtext_enter_span_cb: comua_append(link) failed.");
                return -1;
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

static int
mdtext_text_cb(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    static const MD_CHAR br[1] = _T("\n");
    static const MD_CHAR soft_br[1] = _T(" ");
    static const MD_CHAR replacement[1] = { 0xfffd };

    mdtext_parse_ctx_t* ctx = (mdtext_parse_ctx_t*) userdata;
    int err;

    switch(type) {
        case MD_TEXT_SOFTBR:
            err = buffer_append(&ctx->buffer, soft_br, sizeof(soft_br));
            break;

        case MD_TEXT_BR:
            err = buffer_append(&ctx->buffer, br, sizeof(br));
            break;

        case MD_TEXT_NULLCHAR:
            err = buffer_append(&ctx->buffer, replacement, sizeof(replacement));
            break;

        case MD_TEXT_ENTITY:
            if(size > 2  &&  text[0] == _T('&')  &&  text[size-1] == _T(';')) {
                entity_t ent;

                if(entity_decode(text+1, &ent) == 0) {
                    err = buffer_append(&ctx->buffer, ent.buffer, ent.len * sizeof(TCHAR));
                    break;
                } else {
                    MC_TRACE("mdtext_text_cb: Unknown entity name '%.*S'.", (int)size, text);
                }
            }
            /* Pass through: Output the entity verbatim as an ordinary text. */

        case MD_TEXT_NORMAL:
        case MD_TEXT_CODE:
        default:
            err = buffer_append(&ctx->buffer, text, size * sizeof(MD_CHAR));
            break;
    }

    if(MC_ERR(err != 0)) {
        MC_TRACE("mdtext_text_cb: mdtext_append_text() failed.");
        return -1;
    }

    return 0;
}

#ifdef MDTEXT_DEBUG
static void
mdtext_debug_cb(const char* msg, void* userdata)
{
    MC_TRACE("mdtext_debug_cb: %s", msg);
}
#endif  /* #ifdef MDTEXT_DEBUG */


#define MDTEXT_PARSER_FLAGS                                                  \
        (MD_FLAG_COLLAPSEWHITESPACE | MD_FLAG_PERMISSIVEATXHEADERS |         \
         MD_FLAG_STRIKETHROUGH | MD_FLAG_PERMISSIVEAUTOLINKS |               \
         MD_FLAG_NOHTML)

static const MD_PARSER mdtext_parser = {
    0,                      /* abi_version */
    MDTEXT_PARSER_FLAGS,
    mdtext_enter_block_cb,
    mdtext_leave_block_cb,
    mdtext_enter_span_cb,
    mdtext_leave_span_cb,
    mdtext_text_cb,
#ifdef MDTEXT_DEBUG
    mdtext_debug_cb,
#else
    NULL,
#endif
    NULL                    /* syntax() */
};


/************************
 *** Module interface ***
 ************************/

mdtext_t*
mdtext_create(c_IDWriteTextFormat* text_fmt, const TCHAR* text, UINT text_len, UINT width, DWORD flags)
{
    mdtext_t* mdtext;
    mdtext_parse_ctx_t ctx;
    int err;

    mdtext = (mdtext_t*) malloc(sizeof(mdtext_t));
    if(MC_ERR(mdtext == NULL)) {
        MC_TRACE("mdtext_create: malloc() failed.");
        goto err_malloc;
    }
    mdtext_init(mdtext, text_fmt, text, text_len);

    if(MC_ERR(mdtext_ctx_init(&ctx, mdtext, width, flags) != 0)) {
        MC_TRACE("mdtext_create: mdtext_ctx_init() failed.");
        goto err_mdtext_ctx_init;
    }
    err = md_parse(text, text_len, &mdtext_parser, (void*) &ctx);
    if(err == 0)
        MC_ASSERT(stack_is_empty(&ctx.stack));

    /* Take over the nodes (even if all the parsing failed). */
    mdtext->node_count = NODE_COUNT(&ctx);
    mdtext->nodes = (mdtext_node_t*) buffer_acquire(&ctx.nodes);
    mdtext->attr_buffer = (TCHAR*) buffer_acquire(&ctx.attr_buffer);
    mdtext_ctx_fini(&ctx);

    if(MC_ERR(err != 0)) {
        MC_TRACE("mdtext_create: md_parse() failed [%d]", err);
        goto err_parse;
    }

    return (mdtext_t*) mdtext;

err_parse:
    mdtext_destroy(mdtext);
err_mdtext_ctx_init:
    free(mdtext);
err_malloc:
    return NULL;
}

void
mdtext_destroy(mdtext_t* mdtext)
{
    mdtext_fini(mdtext);
    free(mdtext);
}

static int
mdtext_set_width_recurse(mdtext_t* mdtext, mdtext_node_t* node, int x0, int x1, int y0)
{
    int inner_x0, inner_x1;

    node->rect.left = x0;
    node->rect.top = y0;
    node->rect.right = x1;
    node->rect.bottom = node->rect.top + mdtext_padding_top(mdtext, node);

    inner_x0 = x0 + mdtext_padding_left(mdtext, node);
    inner_x1 = x1 - mdtext_padding_right(mdtext, node);

    if(node->flags & MDTEXT_NODE_IS_CONTAINER) {
        size_t off = 0;
        int child_y0 = node->rect.bottom;
        mdtext_node_t* child_node;
        mdtext_node_t* prev_child_node = NULL;

        while(off < node->data_len) {
            child_node = &mdtext->nodes[comua_read(node->data, node->data_len, off, &off)];
            if(prev_child_node != NULL) {
                child_y0 += MC_MAX(mdtext_margin_bottom(mdtext, prev_child_node),
                                   mdtext_margin_top(mdtext, child_node));
            }
            child_y0 = mdtext_set_width_recurse(mdtext, child_node,
                                        inner_x0, inner_x1, child_y0);
            prev_child_node = child_node;
        }

        node->rect.bottom = child_y0;
    } else {
        c_IDWriteTextLayout* text_layout = mdtext_node_text_layout(node);
        if(text_layout != NULL) {
            c_DWRITE_TEXT_METRICS text_metrics;
            c_IDWriteTextLayout_SetMaxWidth(text_layout, (float) (inner_x1 - inner_x0));
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            node->rect.bottom += ceilf(text_metrics.height);
        } else {
            node->rect.bottom += mdtext->line_height;
        }
    }

    node->rect.bottom += mdtext_padding_bottom(mdtext, node);
    return node->rect.bottom;
}

void
mdtext_set_width(mdtext_t* mdtext, UINT width)
{
    if(width == mc_width(&mdtext->nodes[0].rect))
        return;

    mdtext_set_width_recurse(mdtext, &mdtext->nodes[0], 0, width, 0);
}

UINT
mdtext_min_width(mdtext_t* mdtext)
{
    return mdtext->min_width;
}

void
mdtext_size(mdtext_t* mdtext, SIZE* size)
{
    size->cx = mc_width(&mdtext->nodes[0].rect);
    size->cy = mc_height(&mdtext->nodes[0].rect);
}

void
mdtext_paint(mdtext_t* mdtext, c_ID2D1RenderTarget* rt, int x_offset, int y_offset,
             int portview_y0, int portview_y1)
{
    UINT i;
    c_D2D1_COLOR_F c;
    c_ID2D1SolidColorBrush* brush;
    xdwrite_ctx_t xdwrite_ctx;
    HRESULT hr;

    xd2d_color_set_cref(&c, MDTEXT_TEXT_COLOR);
    hr = c_ID2D1RenderTarget_CreateSolidColorBrush(rt, &c, NULL, &brush);
    if(MC_ERR(FAILED(hr))) {
        MC_TRACE_HR("mdtext_paint: ID2D1RenderTarget::CreateSolidColorBrush() failed.");
        return;
    }

    xdwrite_ctx.rt = rt;
    xdwrite_ctx.solid_brush = brush;
    xd2d_color_set_cref(&xdwrite_ctx.default_color, MDTEXT_TEXT_COLOR);

    for(i = 0; i < mdtext->node_count; i++) {
        mdtext_node_t* node = &mdtext->nodes[i];
        int padding_left = mdtext_padding_left(mdtext, node);
        int padding_top = mdtext_padding_top(mdtext, node);
        c_IDWriteTextLayout* text_layout;

        if(node->rect.bottom + y_offset < portview_y0)
            continue;
        if(node->rect.top + y_offset > portview_y1)
            break;

        switch(node->type) {
            case MD_BLOCK_CODE:
            {
                c_D2D1_RECT_F rect = {
                        (float) (node->rect.left + x_offset),
                        (float) (node->rect.top + y_offset),
                        (float) (node->rect.right + x_offset),
                        (float) (node->rect.bottom + y_offset)
                };

                xd2d_color_set_cref(&c, MDTEXT_CODE_BK_COLOR);
                c_ID2D1SolidColorBrush_SetColor(brush, &c);
                c_ID2D1RenderTarget_FillRectangle(rt, &rect, (c_ID2D1Brush*) brush);
                break;
            }

            case MD_BLOCK_QUOTE:
            {
                c_D2D1_RECT_F rect = {
                        (float) (node->rect.left + x_offset),
                        (float) (node->rect.top + y_offset),
                        (float) (node->rect.left + x_offset + padding_left / 2),
                        (float) (node->rect.bottom + y_offset)
                };

                xd2d_color_set_cref(&c, MDTEXT_QUOTE_DECOR_COLOR);
                c_ID2D1SolidColorBrush_SetColor(brush, &c);
                c_ID2D1RenderTarget_FillRectangle(rt, &rect, (c_ID2D1Brush*) brush);
                break;
            }

            case MD_BLOCK_HR:
            {
                c_D2D1_POINT_2F pt0 = {
                        (float) (node->rect.left + x_offset),
                        (float) ((node->rect.top + node->rect.bottom) / 2.0f + y_offset)
                };
                c_D2D1_POINT_2F pt1 = {
                        (float) (node->rect.right + x_offset),
                        (float) ((node->rect.top + node->rect.bottom) / 2.0f + y_offset)
                };

                xd2d_color_set_cref(&c, MDTEXT_HR_COLOR);
                c_ID2D1SolidColorBrush_SetColor(brush, &c);
                c_ID2D1RenderTarget_DrawLine(rt, pt0, pt1, (c_ID2D1Brush*) brush,
                        MC_MAX(1.0f, (float)mc_height(&node->rect) / 8), NULL);
                break;
            }

            case MD_BLOCK_LI:
            {
                xd2d_color_set_cref(&c, MDTEXT_TEXT_COLOR);

                if(node->aux < 10000) {
                    TCHAR buf[8];
                    c_D2D1_RECT_F rect = {
                            (float) (0 + x_offset), (float) node->rect.top + y_offset,
                            (float) (node->rect.left + x_offset + padding_left - mdtext->digit_width), (float) node->rect.bottom + y_offset
                    };
                    c_IDWriteTextLayout* text_layout;

                    _sntprintf(buf, MC_SIZEOF_ARRAY(buf), _T("%u."), (UINT) node->aux);
                    text_layout = xdwrite_create_text_layout(buf, _tcslen(buf),
                            mdtext->text_fmt, rect.right - rect.left, rect.bottom - rect.top, XDWRITE_ALIGN_RIGHT | XDWRITE_NOWRAP);
                    if(text_layout != NULL) {
                        xdwrite_draw(text_layout, &xdwrite_ctx, rect.left, rect.top);
                        c_IDWriteTextLayout_Release(text_layout);
                    }
                } else {
                    float bullet_size = 0.33f * (float)mdtext->line_height;
                    c_D2D1_POINT_2F pt0 = {
                            (float) (node->rect.left + x_offset + padding_left - mdtext->digit_width) - 0.5f * bullet_size,
                            (float) (node->rect.top + y_offset + padding_top) + 0.5f * (float)mdtext->line_height
                    };

                    switch(node->aux - 10000) {
                        case _T('+'):
                        {
                            c_D2D1_RECT_F rect = {
                                    pt0.x - 0.5f * bullet_size, pt0.y - 0.5f * bullet_size,
                                    pt0.x + 0.5f * bullet_size, pt0.y + 0.5f * bullet_size
                            };
                            c_ID2D1RenderTarget_FillRectangle(rt, &rect, (c_ID2D1Brush*) brush);
                        }

                        case _T('-'):
                        {
                            c_D2D1_ELLIPSE ellipse = {
                                    { pt0.x, pt0.y },
                                    0.5f * bullet_size,
                                    0.5f * bullet_size
                            };
                            c_ID2D1RenderTarget_DrawEllipse(rt, &ellipse, (c_ID2D1Brush*) brush, 1.0f, NULL);
                            break;
                        }

                        case _T('*'):
                        default:
                        {
                            c_D2D1_ELLIPSE ellipse = {
                                    { pt0.x, pt0.y },
                                    0.5f * bullet_size,
                                    0.5f * bullet_size
                            };
                            c_ID2D1RenderTarget_FillEllipse(rt, &ellipse, (c_ID2D1Brush*) brush);
                            break;
                        }
                    }
                }
            }

            default:
                break;
        }

        text_layout = mdtext_node_text_layout(node);
        if(text_layout != NULL) {
            xdwrite_draw(text_layout, &xdwrite_ctx,
                    (float)(node->rect.left + padding_left + x_offset),
                    (float)(node->rect.top + padding_top + y_offset));
        }
    }

    c_ID2D1SolidColorBrush_Release(brush);
}


typedef struct mdtext_hit_test_y_cmp_tag mdtext_hit_test_y_cmp_t;
struct mdtext_hit_test_y_cmp_tag {
    mdtext_t* mdtext;
    int y;
};

static int
mdtext_hit_test_y_cmp(const void* key, const void* data, size_t data_len, size_t offset)
{
    mdtext_hit_test_y_cmp_t* cmp_key = (mdtext_hit_test_y_cmp_t*) key;
    mdtext_node_t* node;

    node = &cmp_key->mdtext->nodes[comua_read(data, data_len, offset, NULL)];

    if(cmp_key->y < node->rect.top)
        return -1;
    if(cmp_key->y >= node->rect.bottom)
        return +1;
    return 0;
}

typedef struct mdtext_link_comua_cmp_tag mdtext_link_comua_cmp_t;
struct mdtext_link_comua_cmp_tag {
    mdtext_t* mdtext;
    UINT32 text_pos;
};

static int
mdtext_link_comua_cmp(const void* key, const void* data, size_t data_len, size_t offset)
{
    mdtext_link_comua_cmp_t* cmp_key = (mdtext_link_comua_cmp_t*) key;
    size_t link_beg, link_end;

    link_beg = comua_read(data, data_len, offset, &offset);
    link_end = link_beg + comua_read(data, data_len, offset, &offset);

    if(cmp_key->text_pos < link_beg)
        return -1;
    if(cmp_key->text_pos > link_end)
        return +1;
    return 0;
}

void
mdtext_hit_test(mdtext_t* mdtext, int x, int y, mdtext_hittest_info_t* info)
{
    mdtext_node_t* node;
    c_IDWriteTextLayout* text_layout;
    c_DWRITE_HIT_TEST_METRICS ht_metrics;

    /* Locate the right node. */
    node = &mdtext->nodes[0];
    while(node->flags & MDTEXT_NODE_IS_CONTAINER) {
        mdtext_hit_test_y_cmp_t cmp_key = { mdtext, y };
        size_t offset;

        offset = comua_bsearch(node->data, node->data_len, &cmp_key, mdtext_hit_test_y_cmp);
        if(offset == (size_t) -1)
            break;
        node = &mdtext->nodes[comua_read(node->data, node->data_len, offset, NULL)];
    }

    /* Hit test the text loayout. */
    text_layout = mdtext_node_text_layout(node);
    if(text_layout != NULL) {
        BOOL is_trailing;
        BOOL is_inside;

        c_IDWriteTextLayout_HitTestPoint(text_layout,
                (float) (x - node->rect.left - mdtext_padding_left(mdtext, node)),
                (float) (y - node->rect.top - mdtext_padding_top(mdtext, node)),
                &is_trailing, &is_inside, &ht_metrics);
        info->in_text = (is_inside && ht_metrics.isText);
    } else {
        info->in_text = FALSE;
    }

    /* Check whether the text position corresponds to a link. */
    info->in_link = FALSE;
    if(info->in_text) {
        void* link_comua_data;
        size_t link_comua_size;

        link_comua_data = mdtext_node_section(node, MDTEXT_NODE_SECTION_LINK_COMUA, &link_comua_size);
        if(link_comua_data != NULL) {
            mdtext_link_comua_cmp_t cmp_key = { mdtext, ht_metrics.textPosition };
            size_t offset;

            offset = comua_bsearch(link_comua_data, link_comua_size, &cmp_key, mdtext_link_comua_cmp);
            info->in_link = (offset != (size_t) -1);
            if(info->in_link) {
                comua_read(link_comua_data, link_comua_size, offset, &offset);  /* skip link offset. */
                comua_read(link_comua_data, link_comua_size, offset, &offset);  /* skip link length. */
                info->link_href = mdtext->attr_buffer + comua_read(link_comua_data, link_comua_size, offset, &offset);
                info->link_title = mdtext->attr_buffer + comua_read(link_comua_data, link_comua_size, offset, &offset);
            }
        }
    }
}
