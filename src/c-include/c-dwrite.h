/*
 * Copyright (c) 2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MC_C_DWRITE_H
#define MC_C_DWRITE_H

#include <windows.h>
#include <unknwn.h>


static const GUID c_IID_IDWriteFactory =
        {0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};


/*****************************
 ***  Helper Enumerations  ***
 *****************************/

typedef enum c_DWRITE_FACTORY_TYPE_tag c_DWRITE_FACTORY_TYPE;
enum c_DWRITE_FACTORY_TYPE_tag {
    c_DWRITE_FACTORY_TYPE_SHARED = 0,
    c_DWRITE_FACTORY_TYPE_ISOLATED
};

typedef enum c_DWRITE_FONT_WEIGHT_tag c_DWRITE_FONT_WEIGHT;
enum c_DWRITE_FONT_WEIGHT_tag {
    c_DWRITE_FONT_WEIGHT_THIN = 100,
    c_DWRITE_FONT_WEIGHT_EXTRA_LIGHT = 200,
    c_DWRITE_FONT_WEIGHT_ULTRA_LIGHT = 200,
    c_DWRITE_FONT_WEIGHT_LIGHT = 300,
    c_DWRITE_FONT_WEIGHT_SEMI_LIGHT = 350,
    c_DWRITE_FONT_WEIGHT_NORMAL = 400,
    c_DWRITE_FONT_WEIGHT_REGULAR = 400,
    c_DWRITE_FONT_WEIGHT_MEDIUM = 500,
    c_DWRITE_FONT_WEIGHT_DEMI_BOLD = 600,
    c_DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    c_DWRITE_FONT_WEIGHT_BOLD = 700,
    c_DWRITE_FONT_WEIGHT_EXTRA_BOLD = 800,
    c_DWRITE_FONT_WEIGHT_ULTRA_BOLD = 800,
    c_DWRITE_FONT_WEIGHT_BLACK = 900,
    c_DWRITE_FONT_WEIGHT_HEAVY = 900,
    c_DWRITE_FONT_WEIGHT_EXTRA_BLACK = 950,
    c_DWRITE_FONT_WEIGHT_ULTRA_BLACK = 950
};

typedef enum c_DWRITE_FONT_STYLE_tag c_DWRITE_FONT_STYLE;
enum c_DWRITE_FONT_STYLE_tag {
    c_DWRITE_FONT_STYLE_NORMAL = 0,
    c_DWRITE_FONT_STYLE_OBLIQUE,
    c_DWRITE_FONT_STYLE_ITALIC
};

typedef enum c_DWRITE_FONT_STRETCH_tag c_DWRITE_FONT_STRETCH;
enum c_DWRITE_FONT_STRETCH_tag {
    c_DWRITE_FONT_STRETCH_UNDEFINED = 0,
    c_DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
    c_DWRITE_FONT_STRETCH_EXTRA_CONDENSED,
    c_DWRITE_FONT_STRETCH_CONDENSED,
    c_DWRITE_FONT_STRETCH_SEMI_CONDENSED,
    c_DWRITE_FONT_STRETCH_NORMAL,
    c_DWRITE_FONT_STRETCH_MEDIUM = c_DWRITE_FONT_STRETCH_NORMAL,
    c_DWRITE_FONT_STRETCH_SEMI_EXPANDED,
    c_DWRITE_FONT_STRETCH_EXPANDED,
    c_DWRITE_FONT_STRETCH_EXTRA_EXPANDED,
    c_DWRITE_FONT_STRETCH_ULTRA_EXPANDED
};

typedef enum c_DWRITE_READING_DIRECTION_tag c_DWRITE_READING_DIRECTION;
enum c_DWRITE_READING_DIRECTION_tag {
    c_DWRITE_READING_DIRECTION_LEFT_TO_RIGHT = 0,
    c_DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
};

typedef enum c_DWRITE_WORD_WRAPPING_tag c_DWRITE_WORD_WRAPPING;
enum c_DWRITE_WORD_WRAPPING_tag {
    c_DWRITE_WORD_WRAPPING_WRAP = 0,
    c_DWRITE_WORD_WRAPPING_NO_WRAP
};

typedef enum c_DWRITE_TEXT_ALIGNMENT_tag c_DWRITE_TEXT_ALIGNMENT;
enum c_DWRITE_TEXT_ALIGNMENT_tag {
    c_DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    c_DWRITE_TEXT_ALIGNMENT_TRAILING,
    c_DWRITE_TEXT_ALIGNMENT_CENTER,
    c_DWRITE_TEXT_ALIGNMENT_JUSTIFY
};

typedef enum c_DWRITE_PARAGRAPH_ALIGNMENT_tag c_DWRITE_PARAGRAPH_ALIGNMENT;
enum c_DWRITE_PARAGRAPH_ALIGNMENT_tag {
    c_DWRITE_PARAGRAPH_ALIGNMENT_NEAR = 0,
    c_DWRITE_PARAGRAPH_ALIGNMENT_FAR,
    c_DWRITE_PARAGRAPH_ALIGNMENT_CENTER
};

typedef enum c_DWRITE_TRIMMING_GRANULARITY_tag c_DWRITE_TRIMMING_GRANULARITY;
enum c_DWRITE_TRIMMING_GRANULARITY_tag {
    c_DWRITE_TRIMMING_GRANULARITY_NONE = 0,
    c_DWRITE_TRIMMING_GRANULARITY_CHARACTER,
    c_DWRITE_TRIMMING_GRANULARITY_WORD
};


/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_DWRITE_TRIMMING_tag c_DWRITE_TRIMMING;
struct c_DWRITE_TRIMMING_tag {
    c_DWRITE_TRIMMING_GRANULARITY granularity;
    UINT32 delimiter;
    UINT32 delimiterCount;
};

typedef struct c_DWRITE_FONT_METRICS_tag c_DWRITE_FONT_METRICS;
struct c_DWRITE_FONT_METRICS_tag {
    UINT16 designUnitsPerEm;
    UINT16 ascent;
    UINT16 descent;
    INT16 lineGap;
    UINT16 capHeight;
    UINT16 xHeight;
    INT16 underlinePosition;
    UINT16 underlineThickness;
    INT16 strikethroughPosition;
    UINT16 strikethroughThickness;
};

typedef struct c_DWRITE_TEXT_METRICS_tag c_DWRITE_TEXT_METRICS;
struct c_DWRITE_TEXT_METRICS_tag {
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT widthIncludingTrailingWhitespace;
    FLOAT height;
    FLOAT layoutWidth;
    FLOAT layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
};

typedef struct c_DWRITE_HIT_TEST_METRICS_tag c_DWRITE_HIT_TEST_METRICS;
struct c_DWRITE_HIT_TEST_METRICS_tag {
    UINT32 textPosition;
    UINT32 length;
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT height;
    UINT32 bidiLevel;
    BOOL isText;
    BOOL isTrimmed;
};

typedef struct c_DWRITE_LINE_METRICS_tag c_DWRITE_LINE_METRICS;
struct c_DWRITE_LINE_METRICS_tag {
    UINT32 length;
    UINT32 trailingWhitespaceLength;
    UINT32 newlineLength;
    FLOAT height;
    FLOAT baseline;
    BOOL isTrimmed;
};

typedef struct c_DWRITE_TEXT_RANGE_tag c_DWRITE_TEXT_RANGE;
struct c_DWRITE_TEXT_RANGE_tag {
    UINT32 startPosition;
    UINT32 length;
};


/******************************
 ***  Forward declarations  ***
 ******************************/

typedef struct c_IDWriteFactory_tag             c_IDWriteFactory;
typedef struct c_IDWriteFont_tag                c_IDWriteFont;
typedef struct c_IDWriteFontCollection_tag      c_IDWriteFontCollection;
typedef struct c_IDWriteFontFamily_tag          c_IDWriteFontFamily;
typedef struct c_IDWriteGdiInterop_tag          c_IDWriteGdiInterop;
typedef struct c_IDWriteInlineObject_tag        c_IDWriteInlineObject;
typedef struct c_IDWriteLocalizedStrings_tag    c_IDWriteLocalizedStrings;
typedef struct c_IDWriteTextFormat_tag          c_IDWriteTextFormat;
typedef struct c_IDWriteTextLayout_tag          c_IDWriteTextLayout;


/**********************************
 ***  Interface IDWriteFactory  ***
 **********************************/

typedef struct c_IDWriteFactoryVtbl_tag c_IDWriteFactoryVtbl;
struct c_IDWriteFactoryVtbl_tag {
    /* IUknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFactory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFactory*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFactory*);

    /* IDWriteFactory methods */
    STDMETHOD(dummy_GetSystemFontCollection)(void);
    STDMETHOD(dummy_CreateCustomFontCollection)(void);
    STDMETHOD(dummy_RegisterFontCollectionLoader)(void);
    STDMETHOD(dummy_UnregisterFontCollectionLoader)(void);
    STDMETHOD(dummy_CreateFontFileReference)(void);
    STDMETHOD(dummy_CreateCustomFontFileReference)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
    STDMETHOD(dummy_CreateRenderingParams)(void);
    STDMETHOD(dummy_CreateMonitorRenderingParams)(void);
    STDMETHOD(dummy_CreateCustomRenderingParams)(void);
    STDMETHOD(dummy_RegisterFontFileLoader)(void);
    STDMETHOD(dummy_UnregisterFontFileLoader)(void);
    STDMETHOD(CreateTextFormat)(c_IDWriteFactory*, WCHAR const*, void*, c_DWRITE_FONT_WEIGHT,
            c_DWRITE_FONT_STYLE, c_DWRITE_FONT_STRETCH, FLOAT, WCHAR const*, c_IDWriteTextFormat**);
    STDMETHOD(dummy_CreateTypography)(void);
    STDMETHOD(GetGdiInterop)(c_IDWriteFactory*, c_IDWriteGdiInterop**);
    STDMETHOD(CreateTextLayout)(c_IDWriteFactory*, WCHAR const*, UINT32, c_IDWriteTextFormat*,
            FLOAT, FLOAT, c_IDWriteTextLayout**);
    STDMETHOD(dummy_CreateGdiCompatibleTextLayout)(void);
    STDMETHOD(CreateEllipsisTrimmingSign)(c_IDWriteFactory*, c_IDWriteTextFormat*, c_IDWriteInlineObject**);
    STDMETHOD(dummy_CreateTextAnalyzer)(void);
    STDMETHOD(dummy_CreateNumberSubstitution)(void);
    STDMETHOD(dummy_CreateGlyphRunAnalysis)(void);
};

struct c_IDWriteFactory_tag {
    c_IDWriteFactoryVtbl* vtbl;
};

#define c_IDWriteFactory_QueryInterface(self,a,b)                   (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteFactory_AddRef(self)                               (self)->vtbl->AddRef(self)
#define c_IDWriteFactory_Release(self)                              (self)->vtbl->Release(self)
#define c_IDWriteFactory_GetSystemFontCollection(self,a,b)          (self)->vtbl->GetSystemFontCollection(self,a,b)
#define c_IDWriteFactory_GetGdiInterop(self,a)                      (self)->vtbl->GetGdiInterop(self,a)
#define c_IDWriteFactory_CreateTextFormat(self,a,b,c,d,e,f,g,h)     (self)->vtbl->CreateTextFormat(self,a,b,c,d,e,f,g,h)
#define c_IDWriteFactory_CreateTextLayout(self,a,b,c,d,e,f)         (self)->vtbl->CreateTextLayout(self,a,b,c,d,e,f)
#define c_IDWriteFactory_CreateEllipsisTrimmingSign(self,a,b)       (self)->vtbl->CreateEllipsisTrimmingSign(self,a,b)


/*******************************
 ***  Interface IDWriteFont  ***
 *******************************/

typedef struct c_IDWriteFontVtbl_tag c_IDWriteFontVtbl;
struct c_IDWriteFontVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFont*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFont*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFont*);

    /* IDWriteFont methods */
    STDMETHOD(GetFontFamily)(c_IDWriteFont*, c_IDWriteFontFamily**);
    STDMETHOD_(c_DWRITE_FONT_WEIGHT, GetWeight)(c_IDWriteFont*);
    STDMETHOD_(c_DWRITE_FONT_STRETCH, GetStretch)(c_IDWriteFont*);
    STDMETHOD_(c_DWRITE_FONT_STYLE, GetStyle)(c_IDWriteFont*);
    STDMETHOD(dummy_IsSymbolFont)(void);
    STDMETHOD(dummy_GetFaceNames)(void);
    STDMETHOD(dummy_GetInformationalStrings)(void);
    STDMETHOD(dummy_GetSimulations)(void);
    STDMETHOD_(void, GetMetrics)(c_IDWriteFont*, c_DWRITE_FONT_METRICS*);
    STDMETHOD(dummy_HasCharacter)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
};

struct c_IDWriteFont_tag {
    c_IDWriteFontVtbl* vtbl;
};

#define c_IDWriteFont_QueryInterface(self,a,b)  (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteFont_AddRef(self)              (self)->vtbl->AddRef(self)
#define c_IDWriteFont_Release(self)             (self)->vtbl->Release(self)
#define c_IDWriteFont_GetWeight(self)           (self)->vtbl->GetWeight(self)
#define c_IDWriteFont_GetStretch(self)          (self)->vtbl->GetStretch(self)
#define c_IDWriteFont_GetStyle(self)            (self)->vtbl->GetStyle(self)
#define c_IDWriteFont_GetMetrics(self,a)        (self)->vtbl->GetMetrics(self,a)
#define c_IDWriteFont_GetFontFamily(self,a)     (self)->vtbl->GetFontFamily(self,a)


/*****************************************
 ***  Interface IDWriteFontCollection  ***
 *****************************************/

typedef struct c_IDWriteFontCollectionVtbl_tag c_IDWriteFontCollectionVtbl;
struct c_IDWriteFontCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFontCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFontCollection*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFontCollection*);

    /* IDWriteFontCollection methods */
    STDMETHOD(dummy_GetFontFamilyCount)(void);
    STDMETHOD(GetFontFamily)(c_IDWriteFontCollection*, UINT32, c_IDWriteFontFamily**);
    STDMETHOD(FindFamilyName)(c_IDWriteFontCollection*, WCHAR*, UINT32*, BOOL*);
    STDMETHOD(dummy_GetFontFromFontFace)(void);
};

struct c_IDWriteFontCollection_tag {
    c_IDWriteFontCollectionVtbl* vtbl;
};

#define c_IDWriteFontCollection_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteFontCollection_AddRef(self)                (self)->vtbl->AddRef(self)
#define c_IDWriteFontCollection_Release(self)               (self)->vtbl->Release(self)
#define c_IDWriteFontCollection_GetFontFamilyCount(self)    (self)->vtbl->GetFontFamilyCount(self)
#define c_IDWriteFontCollection_GetFontFamily(self,a,b)     (self)->vtbl->GetFontFamily(self,a,b)
#define c_IDWriteFontCollection_FindFamilyName(self,a,b,c)  (self)->vtbl->FindFamilyName(self,a,b,c)


/*************************************
 ***  Interface IDWriteFontFamily  ***
 *************************************/

typedef struct c_IDWriteFontFamilyVtbl_tag c_IDWriteFontFamilyVtbl;
struct c_IDWriteFontFamilyVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFontFamily*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFontFamily*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFontFamily*);

    /* IDWriteFontList methods */
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontCount)(void);
    STDMETHOD(dummy_GetFont)(void);

    /* IDWriteFontFamily methods */
    STDMETHOD(GetFamilyNames)(c_IDWriteFontFamily*, c_IDWriteLocalizedStrings**);
    STDMETHOD(GetFirstMatchingFont)(c_IDWriteFontFamily*, c_DWRITE_FONT_WEIGHT,
            c_DWRITE_FONT_STRETCH, c_DWRITE_FONT_STYLE, c_IDWriteFont**);
    STDMETHOD(dummy_GetMatchingFonts)(void);
};

struct c_IDWriteFontFamily_tag {
    c_IDWriteFontFamilyVtbl* vtbl;
};

#define c_IDWriteFontFamily_QueryInterface(self,a,b)            (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteFontFamily_AddRef(self)                        (self)->vtbl->AddRef(self)
#define c_IDWriteFontFamily_Release(self)                       (self)->vtbl->Release(self)
#define c_IDWriteFontFamily_GetFirstMatchingFont(self,a,b,c,d)  (self)->vtbl->GetFirstMatchingFont(self,a,b,c,d)
#define c_IDWriteFontFamily_GetFamilyNames(self,a)              (self)->vtbl->GetFamilyNames(self,a)


/*************************************
 ***  Interface IDWriteGdiInterop  ***
 *************************************/

typedef struct c_IDWriteGdiInteropVtbl_tag c_IDWriteGdiInteropVtbl;
struct c_IDWriteGdiInteropVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteGdiInterop*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteGdiInterop*);
    STDMETHOD_(ULONG, Release)(c_IDWriteGdiInterop*);

    /* IDWriteGdiInterop methods */
    STDMETHOD(CreateFontFromLOGFONT)(c_IDWriteGdiInterop*, LOGFONTW const*, c_IDWriteFont**);
    STDMETHOD(dummy_ConvertFontToLOGFONT)(void);
    STDMETHOD(dummy_ConvertFontFaceToLOGFONT)(void);
    STDMETHOD(dummy_CreateFontFaceFromHdc)(void);
    STDMETHOD(dummy_CreateBitmapRenderTarget)(void);
};

struct c_IDWriteGdiInterop_tag {
    c_IDWriteGdiInteropVtbl* vtbl;
};

#define c_IDWriteGdiInterop_QueryInterface(self,a,b)            (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteGdiInterop_AddRef(self)                        (self)->vtbl->AddRef(self)
#define c_IDWriteGdiInterop_Release(self)                       (self)->vtbl->Release(self)
#define c_IDWriteGdiInterop_CreateFontFromLOGFONT(self,a,b)     (self)->vtbl->CreateFontFromLOGFONT(self,a,b)


/*******************************************
 ***  Interface IDWriteLocalizedStrings  ***
 *******************************************/

typedef struct c_IDWriteLocalizedStringsVtbl_tag c_IDWriteLocalizedStringsVtbl;
struct c_IDWriteLocalizedStringsVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteLocalizedStrings*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteLocalizedStrings*);
    STDMETHOD_(ULONG, Release)(c_IDWriteLocalizedStrings*);

    /* IDWriteLocalizedStrings methods */
    STDMETHOD(dummy_GetCount)(void);
    STDMETHOD(dummy_FindLocaleName)(void);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
    STDMETHOD(GetStringLength)(c_IDWriteLocalizedStrings*, UINT32, UINT32*);
    STDMETHOD(GetString)(c_IDWriteLocalizedStrings*, UINT32, WCHAR*, UINT32);
};

struct c_IDWriteLocalizedStrings_tag {
    c_IDWriteLocalizedStringsVtbl* vtbl;
};

#define c_IDWriteLocalizedStrings_QueryInterface(self,a,b)      (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteLocalizedStrings_AddRef(self)                  (self)->vtbl->AddRef(self)
#define c_IDWriteLocalizedStrings_Release(self)                 (self)->vtbl->Release(self)
#define c_IDWriteLocalizedStrings_GetStringLength(self,a,b)     (self)->vtbl->GetStringLength(self,a,b)
#define c_IDWriteLocalizedStrings_GetString(self,a,b,c)         (self)->vtbl->GetString(self,a,b,c)


/***************************************
 ***  Interface IDWriteInlineObject  ***
 ***************************************/

typedef struct c_IDWriteInlineObjectVtbl_tag c_IDWriteInlineObjectVtbl;
struct c_IDWriteInlineObjectVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteInlineObject*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteInlineObject*);
    STDMETHOD_(ULONG, Release)(c_IDWriteInlineObject*);

    /* IDWriteInlineObject methods */
    STDMETHOD(dummy_Draw)(void);
    STDMETHOD(dummy_GetMetrics)(void);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetBreakConditions)(void);
};

struct c_IDWriteInlineObject_tag {
    c_IDWriteInlineObjectVtbl* vtbl;
};

#define c_IDWriteInlineObject_QueryInterface(self,a,b)  (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteInlineObject_AddRef(self)              (self)->vtbl->AddRef(self)
#define c_IDWriteInlineObject_Release(self)             (self)->vtbl->Release(self)


/*************************************
 ***  Interface IDWriteTextFormat  ***
 *************************************/

typedef struct c_IDWriteTextFormatVtbl_tag c_IDWriteTextFormatVtbl;
struct c_IDWriteTextFormatVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteTextFormat*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteTextFormat*);
    STDMETHOD_(ULONG, Release)(c_IDWriteTextFormat*);

    /* IDWriteTextFormat methods */
    STDMETHOD(dummy_SetTextAlignment)(void);
    STDMETHOD(dummy_SetParagraphAlignment)(void);
    STDMETHOD(dummy_SetWordWrapping)(void);
    STDMETHOD(dummy_SetReadingDirection)(void);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(dummy_SetTrimming)(void);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(GetFontCollection)(c_IDWriteTextFormat*, c_IDWriteFontCollection**);
    STDMETHOD_(UINT32, GetFontFamilyNameLength)(c_IDWriteTextFormat*);
    STDMETHOD(GetFontFamilyName)(c_IDWriteTextFormat*, WCHAR*, UINT32);
    STDMETHOD_(c_DWRITE_FONT_WEIGHT, GetFontWeight)(c_IDWriteTextFormat*);
    STDMETHOD_(c_DWRITE_FONT_STYLE, GetFontStyle)(c_IDWriteTextFormat*);
    STDMETHOD_(c_DWRITE_FONT_STRETCH, GetFontStretch)(c_IDWriteTextFormat*);
    STDMETHOD_(FLOAT, GetFontSize)(c_IDWriteTextFormat*);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
};

struct c_IDWriteTextFormat_tag {
    c_IDWriteTextFormatVtbl* vtbl;
};

#define c_IDWriteTextFormat_QueryInterface(self,a,b)        (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteTextFormat_AddRef(self)                    (self)->vtbl->AddRef(self)
#define c_IDWriteTextFormat_Release(self)                   (self)->vtbl->Release(self)
#define c_IDWriteTextFormat_GetFontCollection(self,a)       (self)->vtbl->GetFontCollection(self,a)
#define c_IDWriteTextFormat_GetFontFamilyNameLength(self)   (self)->vtbl->GetFontFamilyNameLength(self)
#define c_IDWriteTextFormat_GetFontFamilyName(self,a,b)     (self)->vtbl->GetFontFamilyName(self,a,b)
#define c_IDWriteTextFormat_GetFontWeight(self)             (self)->vtbl->GetFontWeight(self)
#define c_IDWriteTextFormat_GetFontStretch(self)            (self)->vtbl->GetFontStretch(self)
#define c_IDWriteTextFormat_GetFontStyle(self)              (self)->vtbl->GetFontStyle(self)
#define c_IDWriteTextFormat_GetFontSize(self)               (self)->vtbl->GetFontSize(self)


/*************************************
 ***  Interface IDWriteTextLayout  ***
 *************************************/

typedef struct c_IDWriteTextLayoutVtbl_tag c_IDWriteTextLayoutVtbl;
struct c_IDWriteTextLayoutVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteTextLayout*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteTextLayout*);
    STDMETHOD_(ULONG, Release)(c_IDWriteTextLayout*);

    /* IDWriteTextFormat methods */
    STDMETHOD(SetTextAlignment)(c_IDWriteTextLayout*, c_DWRITE_TEXT_ALIGNMENT);
    STDMETHOD(SetParagraphAlignment)(c_IDWriteTextLayout*, c_DWRITE_PARAGRAPH_ALIGNMENT);
    STDMETHOD(SetWordWrapping)(c_IDWriteTextLayout*, c_DWRITE_WORD_WRAPPING);
    STDMETHOD(SetReadingDirection)(c_IDWriteTextLayout*, c_DWRITE_READING_DIRECTION);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(SetTrimming)(c_IDWriteTextLayout*, const c_DWRITE_TRIMMING*, c_IDWriteInlineObject*);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength)(void);
    STDMETHOD(dummy_GetFontFamilyName)(void);
    STDMETHOD(dummy_GetFontWeight)(void);
    STDMETHOD(dummy_GetFontStyle)(void);
    STDMETHOD(dummy_GetFontStretch)(void);
    STDMETHOD(dummy_GetFontSize)(void);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);

    /* IDWriteTextLayout methods */
    STDMETHOD(SetMaxWidth)(c_IDWriteTextLayout*, FLOAT);
    STDMETHOD(SetMaxHeight)(c_IDWriteTextLayout*, FLOAT);
    STDMETHOD(dummy_SetFontCollection)(void);
    STDMETHOD(SetFontFamilyName)(c_IDWriteTextLayout*, const WCHAR*, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetFontWeight)(c_IDWriteTextLayout*, c_DWRITE_FONT_WEIGHT, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetFontStyle)(c_IDWriteTextLayout*, c_DWRITE_FONT_STYLE, c_DWRITE_TEXT_RANGE);
    STDMETHOD(dummy_SetFontStretch)(void);
    STDMETHOD(SetFontSize)(c_IDWriteTextLayout*, FLOAT, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetUnderline)(c_IDWriteTextLayout*, BOOL, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetStrikethrough)(c_IDWriteTextLayout*, BOOL, c_DWRITE_TEXT_RANGE);
    STDMETHOD(dummy_SetDrawingEffect)(void);
    STDMETHOD(dummy_SetInlineObject)(void);
    STDMETHOD(dummy_SetTypography)(void);
    STDMETHOD(dummy_SetLocaleName)(void);
    STDMETHOD_(FLOAT, GetMaxWidth)(c_IDWriteTextLayout*);
    STDMETHOD(dummy_GetMaxHeight)(void);
    STDMETHOD(dummy_GetFontCollection2)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength2)(void);
    STDMETHOD(dummy_GetFontFamilyName2)(void);
    STDMETHOD(dummy_GetFontWeight2)(void);
    STDMETHOD(dummy_GetFontStyle2)(void);
    STDMETHOD(dummy_GetFontStretch2)(void);
    STDMETHOD(GetFontSize2)(c_IDWriteTextLayout*, UINT32, FLOAT*, c_DWRITE_TEXT_RANGE*);
    STDMETHOD(dummy_GetUnderline)(void);
    STDMETHOD(dummy_GetStrikethrough)(void);
    STDMETHOD(dummy_GetDrawingEffect)(void);
    STDMETHOD(dummy_GetInlineObject)(void);
    STDMETHOD(dummy_GetTypography)(void);
    STDMETHOD(dummy_GetLocaleNameLength2)(void);
    STDMETHOD(dummy_GetLocaleName2)(void);
    STDMETHOD(dummy_Draw)(void);
    STDMETHOD(GetLineMetrics)(c_IDWriteTextLayout*, c_DWRITE_LINE_METRICS*, UINT32, UINT32*);
    STDMETHOD(GetMetrics)(c_IDWriteTextLayout*, c_DWRITE_TEXT_METRICS*);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetClusterMetrics)(void);
    STDMETHOD(DetermineMinWidth)(c_IDWriteTextLayout*, FLOAT*);
    STDMETHOD(HitTestPoint)(c_IDWriteTextLayout*, FLOAT, FLOAT, BOOL*, BOOL*, c_DWRITE_HIT_TEST_METRICS*);
    STDMETHOD(dummy_HitTestTextPosition)(void);
    STDMETHOD(dummy_HitTestTextRange)(void);
};

struct c_IDWriteTextLayout_tag {
    c_IDWriteTextLayoutVtbl* vtbl;
};

#define c_IDWriteTextLayout_QueryInterface(self,a,b)        (self)->vtbl->QueryInterface(self,a,b)
#define c_IDWriteTextLayout_AddRef(self)                    (self)->vtbl->AddRef(self)
#define c_IDWriteTextLayout_Release(self)                   (self)->vtbl->Release(self)
#define c_IDWriteTextLayout_SetTextAlignment(self,a)        (self)->vtbl->SetTextAlignment(self,a)
#define c_IDWriteTextLayout_SetParagraphAlignment(self,a)   (self)->vtbl->SetParagraphAlignment(self,a)
#define c_IDWriteTextLayout_SetWordWrapping(self,a)         (self)->vtbl->SetWordWrapping(self,a)
#define c_IDWriteTextLayout_SetReadingDirection(self,a)     (self)->vtbl->SetReadingDirection(self,a)
#define c_IDWriteTextLayout_SetTrimming(self,a,b)           (self)->vtbl->SetTrimming(self,a,b)
#define c_IDWriteTextLayout_SetMaxWidth(self,a)             (self)->vtbl->SetMaxWidth(self,a)
#define c_IDWriteTextLayout_SetMaxHeight(self,a)            (self)->vtbl->SetMaxHeight(self,a)
#define c_IDWriteTextLayout_SetFontFamilyName(self,a,b)     (self)->vtbl->SetFontFamilyName(self,a,b)
#define c_IDWriteTextLayout_SetFontWeight(self,a,b)         (self)->vtbl->SetFontWeight(self,a,b)
#define c_IDWriteTextLayout_SetFontStyle(self,a,b)          (self)->vtbl->SetFontStyle(self,a,b)
#define c_IDWriteTextLayout_SetFontSize(self,a,b)           (self)->vtbl->SetFontSize(self,a,b)
#define c_IDWriteTextLayout_SetStrikethrough(self,a,b)      (self)->vtbl->SetStrikethrough(self,a,b)
#define c_IDWriteTextLayout_SetUnderline(self,a,b)          (self)->vtbl->SetUnderline(self,a,b)
#define c_IDWriteTextLayout_GetMaxWidth(self)               (self)->vtbl->GetMaxWidth(self)
#define c_IDWriteTextLayout_GetFontSize2(self,a,b,c)        (self)->vtbl->GetFontSize2(self,a,b,c)
#define c_IDWriteTextLayout_GetLineMetrics(self,a,b,c)      (self)->vtbl->GetLineMetrics(self,a,b,c)
#define c_IDWriteTextLayout_GetMetrics(self,a)              (self)->vtbl->GetMetrics(self,a)
#define c_IDWriteTextLayout_DetermineMinWidth(self,a)       (self)->vtbl->DetermineMinWidth(self,a)
#define c_IDWriteTextLayout_HitTestPoint(self,a,b,c,d,e)    (self)->vtbl->HitTestPoint(self,a,b,c,d,e)


#endif  /* MC_C_DWRITE_H */
