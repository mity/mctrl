/*
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
     *     The pointer is followed by (compressed) sequence of UINTs which
     *     describe mapping between the offsets as used by the text layout and
     *     the original input Markdown document. This is used to e.g. handle
     *     selection support and copying the source text into clipboard when
     *     user presses Ctrl+C.
     *
     * (2) For container blocks which have nested blocks (i.e. with the flag
     *     MDTEXT_NODE_IS_CONTAINER), the data is just a compressed sequence
     *     if UINTs. Those are list of child nodes (indexes into nodes[]).
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


typedef struct mdtext_tag mdtext_t;
struct mdtext_tag {
    mdtext_node_t* nodes;
    UINT node_count;
    UINT min_width;
    UINT digit_width        : 16;
    UINT empty_line_height  : 16;

    /* Provided by the control. Do not modify/free these. */
    const TCHAR* text;
    UINT text_len;
    c_IDWriteTextFormat* text_fmt;
};


static void
mdtext_init(mdtext_t* mdtext, c_IDWriteTextFormat* text_fmt, const TCHAR* text, UINT text_len)
{
    c_IDWriteTextLayout* text_layout;

    mdtext->nodes = NULL;
    mdtext->node_count = 0;

    mdtext->text = text;
    mdtext->text_len = text_len;
    mdtext->text_fmt = text_fmt;

    /* We use an average digit width for computing e.g. list item paddings. */
    mdtext->digit_width = 10;
    mdtext->empty_line_height = 16;
    text_layout = xdwrite_create_text_layout(_T("1234567890"), 10, text_fmt,
                FLT_MAX, FLT_MAX, XDWRITE_NOWRAP);
    if(text_layout != NULL) {
        c_DWRITE_TEXT_METRICS text_metrics;
        c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
        mdtext->digit_width = ceilf(text_metrics.width / 10.0f);
        mdtext->empty_line_height = ceilf(text_metrics.height);
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

        if((!node->flags & MDTEXT_NODE_IS_CONTAINER)  &&
           node->data_len >= sizeof(c_IDWriteTextLayout*))
        {
            c_IDWriteTextLayout* text_layout = *(c_IDWriteTextLayout**) node->data;
            c_IDWriteTextLayout_Release(text_layout);
        }
    }

    free(mdtext->nodes);
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
    return mdtext_unit(mdtext);
}

static int
mdtext_margin_bottom(mdtext_t* mdtext, mdtext_node_t* node)
{
    return mdtext_margin_top(mdtext, node);
}


/************************
 *** Compressed UINTs ***
 ************************/

static int
mdtext_append_compressed_uint(BUFFER* buffer, UINT num)
{
    BYTE tmp[8];
    size_t n = 0;

    /* Write in the little-endian order 7-bit blocks, with the MSB set to
     * indicate another byte follows. */
    while(num >= 0x80) {
        tmp[n++] = (num & 0x7f) | 0x80;
        num = num >> 7;
    }
    /* The trailing byte with the leftover, with the MSB unset. */
    tmp[n++] = num;

    return buffer_append(buffer, tmp, n);
}

static UINT
mdtext_read_compressed_uint(const void* data, size_t offset, size_t* p_end_offset)
{
    const BYTE* bytes = (const BYTE*)data + offset;
    size_t n = 0;
    UINT num = 0;

    do {
        num |= ((UINT)bytes[n] & 0x7f) << (n * 7);
    } while(bytes[n++] & 0x80);

    *p_end_offset = offset + n;
    return num;
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
    BUFFER text_contents;
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

    DWORD flags;
    int x0;
    int x1;
    int y;
    int width;
};

#define NODE_COUNT(ctx)     array_count(&(ctx)->nodes, sizeof(mdtext_node_t))
#define NODE(ctx, index)    ((mdtext_node_t*) array_get_raw(&(ctx)->nodes, (index), sizeof(mdtext_node_t)))


static void
mdtext_ctx_init(mdtext_parse_ctx_t* ctx, mdtext_t* mdtext, UINT width, DWORD flags)
{
    ctx->mdtext = mdtext;
    array_init(&ctx->nodes);
    stack_init(&ctx->stack);
    buffer_init(&ctx->buffer);
    stack_init(&ctx->span_stack);
    buffer_init(&ctx->spans);
    ctx->flags = flags;
    ctx->x0 = 0;
    ctx->x1 = width;
    ctx->y = 0;
    ctx->width = width;
}

static void
mdtext_ctx_fini(mdtext_parse_ctx_t* ctx)
{
    /* The stack should generally be empty except when we aborted the parsing
     * due to any error. */
    while(!stack_is_empty(&ctx->stack)) {
        mdtext_stack_record_t* stack_record =
                stack_pop_raw(&ctx->stack, sizeof(mdtext_stack_record_t));

        if(!buffer_is_empty(&stack_record->text_contents)) {
            c_IDWriteTextLayout* text_layout = (c_IDWriteTextLayout*)
                    buffer_data_at(&stack_record->text_contents, 0);
            c_IDWriteTextLayout_Release(text_layout);
        }

        array_fini(&stack_record->children);
        buffer_fini(&stack_record->text_contents);
    }

    /* ctx->nodes are not released here: That's what we are constructing for
     * the mdtext. */

    stack_fini(&ctx->stack);
    buffer_fini(&ctx->buffer);
    stack_fini(&ctx->span_stack);
    buffer_fini(&ctx->spans);
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
mdtext_downgrade_text_contents(mdtext_parse_ctx_t* ctx, mdtext_stack_record_t* stack_record)
{
    int node_index;
    mdtext_node_t* node;

    if(buffer_is_empty(&stack_record->text_contents))
        return 0;

    node_index = mdtext_add_node(ctx, MD_BLOCK_P);
    if(MC_ERR(node_index < 0)) {
        MC_TRACE("mdtext_downgrade_text_contents: mdtext_add_node() failed.");
        return -1;
    }

    if(MC_ERR(mdtext_append_compressed_uint(&stack_record->children, node_index) != 0)) {
        MC_TRACE("mdtext_downgrade_text_contents: mdtext_append_compressed_uint() failed.");
        return -1;
    }

    node = NODE(ctx, node_index);
    node->data_len = buffer_size(&stack_record->text_contents);
    node->data = (void*) buffer_acquire(&stack_record->text_contents);
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

    if(!buffer_is_empty(&stack_record->text_contents)) {
        if(MC_ERR(mdtext_downgrade_text_contents(ctx, stack_record) != 0)) {
            MC_TRACE("mdtext_flush_text: mdtext_downgrade_text_contents() failed.");
            return -1;
        }
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

    MC_ASSERT(buffer_is_empty(&stack_record->text_contents));
    if(MC_ERR(buffer_append(&stack_record->text_contents, &text_layout, sizeof(c_IDWriteTextLayout*)) != 0)) {
        MC_TRACE("mdtext_flush_text: buffer_append() failed.");
        c_IDWriteTextLayout_Release(text_layout);
        return -1;
    }

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

        span_type = (MD_SPANTYPE) mdtext_read_compressed_uint(span_data, off, &off);
        range.startPosition = mdtext_read_compressed_uint(span_data, off, &off);
        range.length = mdtext_read_compressed_uint(span_data, off, &off);

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

        if(MC_ERR(mdtext_append_compressed_uint(&parent_stack_record->children, node_index) != 0)) {
            MC_TRACE("mdtext_enter_block_cb: mdtext_append_compressed_uint() failed.");
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
    array_init(&node_stack_record->children);
    buffer_init(&node_stack_record->text_contents);

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
    } else if(!buffer_is_empty(&node_stack_record->text_contents)) {
        node->data_len = buffer_size(&node_stack_record->text_contents);
        node->data = (void*) buffer_acquire(&node_stack_record->text_contents);
    } else {
        node->data_len = 0;
        node->data = NULL;
    }

    /* Determine the bottom edge of the node. */
    if(node->flags & MDTEXT_NODE_IS_CONTAINER) {
        MC_ASSERT(node_stack_record->last_child_node_index >= 0);
        node->rect.bottom = NODE(ctx, node_stack_record->last_child_node_index)->rect.bottom;
    } else {
        c_IDWriteTextLayout* text_layout;

        if(node->data_len >= sizeof(c_IDWriteTextLayout*))
            text_layout = *(c_IDWriteTextLayout**) node->data;
        else
            text_layout = NULL;

        node->rect.bottom = node->rect.top;
        if(text_layout != NULL) {
            c_DWRITE_TEXT_METRICS text_metrics;
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            node->rect.bottom += ceilf(text_metrics.height);
        } else {
            node->rect.bottom += mdtext->empty_line_height;
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
    mdtext_span_stack_record_t* rec;
    UINT end;

    MC_ASSERT(!stack_is_empty(&ctx->span_stack));
    rec = (mdtext_span_stack_record_t*) stack_pop_raw(&ctx->span_stack,
                    sizeof(mdtext_span_stack_record_t));

    end = buffer_size(&ctx->buffer) / sizeof(TCHAR);
    if(MC_ERR(mdtext_append_compressed_uint(&ctx->spans, (UINT) type) != 0  ||
              mdtext_append_compressed_uint(&ctx->spans, rec->beg) != 0  ||
              mdtext_append_compressed_uint(&ctx->spans, end - rec->beg) != 0)) {
        MC_TRACE("mdtext_leave_span_cb: mdtext_append_compressed_uint() failed.");
        return -1;
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
        return NULL;
    }
    mdtext_init(mdtext, text_fmt, text, text_len);

    mdtext_ctx_init(&ctx, mdtext, width, flags);
    err = md_parse(text, text_len, &mdtext_parser, (void*) &ctx);
    if(err == 0)
        MC_ASSERT(stack_is_empty(&ctx.stack));

    /* Take over the nodes (even if all the parsing failed). */
    mdtext->node_count = NODE_COUNT(&ctx);
    mdtext->nodes = (mdtext_node_t*) buffer_acquire(&ctx.nodes);
    mdtext_ctx_fini(&ctx);

    if(MC_ERR(err != 0)) {
        MC_TRACE("mdtext_create: md_parse() failed [%d]", err);
        mdtext_destroy(mdtext);
        return NULL;
    }

    return (mdtext_t*) mdtext;
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
        UINT child_index;

        while(off < node->data_len) {
            child_index = mdtext_read_compressed_uint(node->data, off, &off);
            child_node = &mdtext->nodes[child_index];
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
        if(node->data_len >= sizeof(c_IDWriteTextLayout*)) {
            c_IDWriteTextLayout* text_layout = *(c_IDWriteTextLayout**) node->data;
            c_DWRITE_TEXT_METRICS text_metrics;

            c_IDWriteTextLayout_SetMaxWidth(text_layout, (float) (inner_x1 - inner_x0));
            c_IDWriteTextLayout_GetMetrics(text_layout, &text_metrics);
            node->rect.bottom += ceilf(text_metrics.height);
        } else {
            node->rect.bottom += mdtext->empty_line_height;
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

            default:
                break;
        }

        if(!(node->flags & MDTEXT_NODE_IS_CONTAINER)  &&
           node->data_len >= sizeof(c_IDWriteTextLayout*))
        {
            c_IDWriteTextLayout* text_layout = *(c_IDWriteTextLayout**) node->data;
            xdwrite_draw(text_layout, &xdwrite_ctx,
                    (float)(node->rect.left + padding_left + x_offset),
                    (float)(node->rect.top + padding_top + y_offset));
        }
    }

    c_ID2D1SolidColorBrush_Release(brush);
}
