/*
 * Copyright (c) 2015 Martin Mitas
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

#ifndef DUMMY_DWRITE_H
#define DUMMY_DWRITE_H

#include <windows.h>
#include <unknwn.h>


static const GUID dummy_IID_IDWriteFactory =
        {0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};


/*****************************
 ***  Helper Enumerations  ***
 *****************************/

typedef enum dummy_DWRITE_FACTORY_TYPE_tag dummy_DWRITE_FACTORY_TYPE;
enum dummy_DWRITE_FACTORY_TYPE_tag {
    dummy_DWRITE_FACTORY_TYPE_SHARED = 0,
    dummy_DWRITE_FACTORY_TYPE_ISOLATED
};

typedef enum dummy_DWRITE_FONT_WEIGHT_tag dummy_DWRITE_FONT_WEIGHT;
enum dummy_DWRITE_FONT_WEIGHT_tag {
    dummy_DWRITE_FONT_WEIGHT_THIN = 100,
    dummy_DWRITE_FONT_WEIGHT_EXTRA_LIGHT = 200,
    dummy_DWRITE_FONT_WEIGHT_ULTRA_LIGHT = 200,
    dummy_DWRITE_FONT_WEIGHT_LIGHT = 300,
    dummy_DWRITE_FONT_WEIGHT_SEMI_LIGHT = 350,
    dummy_DWRITE_FONT_WEIGHT_NORMAL = 400,
    dummy_DWRITE_FONT_WEIGHT_REGULAR = 400,
    dummy_DWRITE_FONT_WEIGHT_MEDIUM = 500,
    dummy_DWRITE_FONT_WEIGHT_DEMI_BOLD = 600,
    dummy_DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    dummy_DWRITE_FONT_WEIGHT_BOLD = 700,
    dummy_DWRITE_FONT_WEIGHT_EXTRA_BOLD = 800,
    dummy_DWRITE_FONT_WEIGHT_ULTRA_BOLD = 800,
    dummy_DWRITE_FONT_WEIGHT_BLACK = 900,
    dummy_DWRITE_FONT_WEIGHT_HEAVY = 900,
    dummy_DWRITE_FONT_WEIGHT_EXTRA_BLACK = 950,
    dummy_DWRITE_FONT_WEIGHT_ULTRA_BLACK = 950
};

typedef enum dummy_DWRITE_FONT_STYLE_tag dummy_DWRITE_FONT_STYLE;
enum dummy_DWRITE_FONT_STYLE_tag {
    dummy_DWRITE_FONT_STYLE_NORMAL = 0,
    dummy_DWRITE_FONT_STYLE_OBLIQUE,
    dummy_DWRITE_FONT_STYLE_ITALIC
};

typedef enum dummy_DWRITE_FONT_STRETCH_tag dummy_DWRITE_FONT_STRETCH;
enum dummy_DWRITE_FONT_STRETCH_tag {
    dummy_DWRITE_FONT_STRETCH_UNDEFINED = 0,
    dummy_DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
    dummy_DWRITE_FONT_STRETCH_EXTRA_CONDENSED,
    dummy_DWRITE_FONT_STRETCH_CONDENSED,
    dummy_DWRITE_FONT_STRETCH_SEMI_CONDENSED,
    dummy_DWRITE_FONT_STRETCH_NORMAL,
    dummy_DWRITE_FONT_STRETCH_MEDIUM = dummy_DWRITE_FONT_STRETCH_NORMAL,
    dummy_DWRITE_FONT_STRETCH_SEMI_EXPANDED,
    dummy_DWRITE_FONT_STRETCH_EXPANDED,
    dummy_DWRITE_FONT_STRETCH_EXTRA_EXPANDED,
    dummy_DWRITE_FONT_STRETCH_ULTRA_EXPANDED
};

typedef enum dummy_DWRITE_WORD_WRAPPING_tag dummy_DWRITE_WORD_WRAPPING;
enum dummy_DWRITE_WORD_WRAPPING_tag {
    dummy_DWRITE_WORD_WRAPPING_WRAP = 0,
    dummy_DWRITE_WORD_WRAPPING_NO_WRAP
};

typedef enum dummy_DWRITE_TEXT_ALIGNMENT_tag dummy_DWRITE_TEXT_ALIGNMENT;
enum dummy_DWRITE_TEXT_ALIGNMENT_tag {
    dummy_DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    dummy_DWRITE_TEXT_ALIGNMENT_TRAILING,
    dummy_DWRITE_TEXT_ALIGNMENT_CENTER
};

typedef enum dummy_DWRITE_TRIMMING_GRANULARITY_tag dummy_DWRITE_TRIMMING_GRANULARITY;
enum dummy_DWRITE_TRIMMING_GRANULARITY_tag {
    dummy_DWRITE_TRIMMING_GRANULARITY_NONE = 0,
    dummy_DWRITE_TRIMMING_GRANULARITY_CHARACTER,
    dummy_DWRITE_TRIMMING_GRANULARITY_WORD
};


/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct dummy_DWRITE_TRIMMING_tag dummy_DWRITE_TRIMMING;
struct dummy_DWRITE_TRIMMING_tag {
    dummy_DWRITE_TRIMMING_GRANULARITY granularity;
    UINT32 delimiter;
    UINT32 delimiterCount;
};

typedef struct dummy_DWRITE_FONT_METRICS_tag dummy_DWRITE_FONT_METRICS;
struct dummy_DWRITE_FONT_METRICS_tag {
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

typedef struct dummy_DWRITE_TEXT_METRICS_tag dummy_DWRITE_TEXT_METRICS;
struct dummy_DWRITE_TEXT_METRICS_tag {
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


/******************************
 ***  Forward declarations  ***
 ******************************/

typedef struct dummy_IDWriteFactory_tag         dummy_IDWriteFactory;
typedef struct dummy_IDWriteFont_tag            dummy_IDWriteFont;
typedef struct dummy_IDWriteFontCollection_tag  dummy_IDWriteFontCollection;
typedef struct dummy_IDWriteFontFamily_tag      dummy_IDWriteFontFamily;
typedef struct dummy_IDWriteTextFormat_tag      dummy_IDWriteTextFormat;
typedef struct dummy_IDWriteTextLayout_tag      dummy_IDWriteTextLayout;


/**********************************
 ***  Interface IDWriteFactory  ***
 **********************************/

typedef struct dummy_IDWriteFactoryVtbl_tag dummy_IDWriteFactoryVtbl;
struct dummy_IDWriteFactoryVtbl_tag {
    /* IUknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteFactory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteFactory*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteFactory*);

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
    STDMETHOD(CreateTextFormat)(dummy_IDWriteFactory*, WCHAR const*, void*, dummy_DWRITE_FONT_WEIGHT,
            dummy_DWRITE_FONT_STYLE, dummy_DWRITE_FONT_STRETCH, FLOAT, WCHAR const*, dummy_IDWriteTextFormat**);
    STDMETHOD(dummy_CreateTypography)(void);
    STDMETHOD(dummy_GetGdiInterop)(void);
    STDMETHOD(CreateTextLayout)(dummy_IDWriteFactory*, WCHAR const*, UINT32, dummy_IDWriteTextFormat*,
            FLOAT, FLOAT, dummy_IDWriteTextLayout**);
    STDMETHOD(dummy_CreateGdiCompatibleTextLayout)(void);
    STDMETHOD(CreateEllipsisTrimmingSign)(dummy_IDWriteFactory*, dummy_IDWriteTextFormat*, void**);
    STDMETHOD(dummy_CreateTextAnalyzer)(void);
    STDMETHOD(dummy_CreateNumberSubstitution)(void);
    STDMETHOD(dummy_CreateGlyphRunAnalysis)(void);
};

struct dummy_IDWriteFactory_tag {
    dummy_IDWriteFactoryVtbl* vtbl;
};

#define dummy_IDWriteFactory_QueryInterface(self,a,b)                 (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteFactory_AddRef(self)                             (self)->vtbl->AddRef(self)
#define dummy_IDWriteFactory_Release(self)                            (self)->vtbl->Release(self)
#define dummy_IDWriteFactory_GetSystemFontCollection(self,a,b)        (self)->vtbl->GetSystemFontCollection(self,a,b)
#define dummy_IDWriteFactory_CreateTextFormat(self,a,b,c,d,e,f,g,h)   (self)->vtbl->CreateTextFormat(self,a,b,c,d,e,f,g,h)
#define dummy_IDWriteFactory_CreateTextLayout(self,a,b,c,d,e,f)       (self)->vtbl->CreateTextLayout(self,a,b,c,d,e,f)
#define dummy_IDWriteFactory_CreateEllipsisTrimmingSign(self,a,b)     (self)->vtbl->CreateEllipsisTrimmingSign(self,a,b)


/*******************************
 ***  Interface IDWriteFont  ***
 *******************************/

typedef struct dummy_IDWriteFontVtbl_tag dummy_IDWriteFontVtbl;
struct dummy_IDWriteFontVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteFont*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteFont*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteFont*);

    /* IDWriteFont methods */
    STDMETHOD(dummy_GetFontFamily)(void);
    STDMETHOD(dummy_GetWeight)(void);
    STDMETHOD(dummy_GetStretch)(void);
    STDMETHOD(dummy_GetStyle)(void);
    STDMETHOD(dummy_IsSymbolFont)(void);
    STDMETHOD(dummy_GetFaceNames)(void);
    STDMETHOD(dummy_GetInformationalStrings)(void);
    STDMETHOD(dummy_GetSimulations)(void);
    STDMETHOD_(void, GetMetrics)(dummy_IDWriteFont*, dummy_DWRITE_FONT_METRICS*);
    STDMETHOD(dummy_HasCharacter)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
};

struct dummy_IDWriteFont_tag {
    dummy_IDWriteFontVtbl* vtbl;
};

#define dummy_IDWriteFont_QueryInterface(self,a,b)  (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteFont_AddRef(self)              (self)->vtbl->AddRef(self)
#define dummy_IDWriteFont_Release(self)             (self)->vtbl->Release(self)
#define dummy_IDWriteFont_GetMetrics(self,a)        (self)->vtbl->GetMetrics(self,a)


/*****************************************
 ***  Interface IDWriteFontCollection  ***
 *****************************************/

typedef struct dummy_IDWriteFontCollectionVtbl_tag dummy_IDWriteFontCollectionVtbl;
struct dummy_IDWriteFontCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteFontCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteFontCollection*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteFontCollection*);

    /* IDWriteFontCollection methods */
    STDMETHOD(dummy_GetFontFamilyCount)(void);
    STDMETHOD(GetFontFamily)(dummy_IDWriteFontCollection*, UINT32, dummy_IDWriteFontFamily**);
    STDMETHOD(FindFamilyName)(dummy_IDWriteFontCollection*, WCHAR*, UINT32*, BOOL*);
    STDMETHOD(dummy_GetFontFromFontFace)(void);
};

struct dummy_IDWriteFontCollection_tag {
    dummy_IDWriteFontCollectionVtbl* vtbl;
};

#define dummy_IDWriteFontCollection_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteFontCollection_AddRef(self)                (self)->vtbl->AddRef(self)
#define dummy_IDWriteFontCollection_Release(self)               (self)->vtbl->Release(self)
#define dummy_IDWriteFontCollection_GetFontFamilyCount(self)    (self)->vtbl->GetFontFamilyCount(self)
#define dummy_IDWriteFontCollection_GetFontFamily(self,a,b)     (self)->vtbl->GetFontFamily(self,a,b)
#define dummy_IDWriteFontCollection_FindFamilyName(self,a,b,c)  (self)->vtbl->FindFamilyName(self,a,b,c)


/*************************************
 ***  Interface IDWriteFontFamily  ***
 *************************************/

typedef struct dummy_IDWriteFontFamilyVtbl_tag dummy_IDWriteFontFamilyVtbl;
struct dummy_IDWriteFontFamilyVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteFontFamily*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteFontFamily*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteFontFamily*);

    /* IDWriteFontList methods */
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontCount)(void);
    STDMETHOD(dummy_GetFont)(void);

    /* IDWriteFontFamily methods */
    STDMETHOD(dummy_GetFamilyNames)(void);
    STDMETHOD(GetFirstMatchingFont)(dummy_IDWriteFontFamily*, dummy_DWRITE_FONT_WEIGHT,
            dummy_DWRITE_FONT_STRETCH, dummy_DWRITE_FONT_STYLE, dummy_IDWriteFont**);
    STDMETHOD(dummy_GetMatchingFonts)(void);
};

struct dummy_IDWriteFontFamily_tag {
    dummy_IDWriteFontFamilyVtbl* vtbl;
};

#define dummy_IDWriteFontFamily_QueryInterface(self,a,b)            (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteFontFamily_AddRef(self)                        (self)->vtbl->AddRef(self)
#define dummy_IDWriteFontFamily_Release(self)                       (self)->vtbl->Release(self)
#define dummy_IDWriteFontFamily_GetFirstMatchingFont(self,a,b,c,d)  (self)->vtbl->GetFirstMatchingFont(self,a,b,c,d)


/*************************************
 ***  Interface IDWriteTextFormat  ***
 *************************************/

typedef struct dummy_IDWriteTextFormatVtbl_tag dummy_IDWriteTextFormatVtbl;
struct dummy_IDWriteTextFormatVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteTextFormat*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteTextFormat*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteTextFormat*);

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
    STDMETHOD(GetFontCollection)(dummy_IDWriteTextFormat*, dummy_IDWriteFontCollection**);
    STDMETHOD_(UINT32, GetFontFamilyNameLength)(dummy_IDWriteTextFormat*);
    STDMETHOD(GetFontFamilyName)(dummy_IDWriteTextFormat*, WCHAR*, UINT32);
    STDMETHOD_(dummy_DWRITE_FONT_WEIGHT, GetFontWeight)(dummy_IDWriteTextFormat*);
    STDMETHOD_(dummy_DWRITE_FONT_STYLE, GetFontStyle)(dummy_IDWriteTextFormat*);
    STDMETHOD_(dummy_DWRITE_FONT_STRETCH, GetFontStretch)(dummy_IDWriteTextFormat*);
    STDMETHOD_(FLOAT, GetFontSize)(dummy_IDWriteTextFormat*);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
};

struct dummy_IDWriteTextFormat_tag {
    dummy_IDWriteTextFormatVtbl* vtbl;
};

#define dummy_IDWriteTextFormat_QueryInterface(self,a,b)        (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteTextFormat_AddRef(self)                    (self)->vtbl->AddRef(self)
#define dummy_IDWriteTextFormat_Release(self)                   (self)->vtbl->Release(self)
#define dummy_IDWriteTextFormat_GetFontCollection(self,a)       (self)->vtbl->GetFontCollection(self,a)
#define dummy_IDWriteTextFormat_GetFontFamilyNameLength(self)   (self)->vtbl->GetFontFamilyNameLength(self)
#define dummy_IDWriteTextFormat_GetFontFamilyName(self,a,b)     (self)->vtbl->GetFontFamilyName(self,a,b)
#define dummy_IDWriteTextFormat_GetFontWeight(self)             (self)->vtbl->GetFontWeight(self)
#define dummy_IDWriteTextFormat_GetFontStretch(self)            (self)->vtbl->GetFontStretch(self)
#define dummy_IDWriteTextFormat_GetFontStyle(self)              (self)->vtbl->GetFontStyle(self)
#define dummy_IDWriteTextFormat_GetFontSize(self)               (self)->vtbl->GetFontSize(self)


/*************************************
 ***  Interface IDWriteTextLayout  ***
 *************************************/

typedef struct dummy_IDWriteTextLayoutVtbl_tag dummy_IDWriteTextLayoutVtbl;
struct dummy_IDWriteTextLayoutVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(dummy_IDWriteTextLayout*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(dummy_IDWriteTextLayout*);
    STDMETHOD_(ULONG, Release)(dummy_IDWriteTextLayout*);

    /* IDWriteTextFormat methods */
    STDMETHOD(SetTextAlignment)(dummy_IDWriteTextLayout*, dummy_DWRITE_TEXT_ALIGNMENT);
    STDMETHOD(dummy_SetParagraphAlignment)(void);
    STDMETHOD(SetWordWrapping)(dummy_IDWriteTextLayout*, dummy_DWRITE_WORD_WRAPPING);
    STDMETHOD(dummy_SetReadingDirection)(void);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(SetTrimming)(dummy_IDWriteTextLayout*, const dummy_DWRITE_TRIMMING*, void*);
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
    STDMETHOD(dummy_SetMaxWidth)(void);
    STDMETHOD(dummy_SetMaxHeight)(void);
    STDMETHOD(dummy_SetFontCollection)(void);
    STDMETHOD(dummy_SetFontFamilyName)(void);
    STDMETHOD(dummy_SetFontWeight)(void);
    STDMETHOD(dummy_SetFontStyle)(void);
    STDMETHOD(dummy_SetFontStretch)(void);
    STDMETHOD(dummy_SetFontSize)(void);
    STDMETHOD(dummy_SetUnderline)(void);
    STDMETHOD(dummy_SetStrikethrough)(void);
    STDMETHOD(dummy_SetDrawingEffect)(void);
    STDMETHOD(dummy_SetInlineObject)(void);
    STDMETHOD(dummy_SetTypography)(void);
    STDMETHOD(dummy_SetLocaleName)(void);
    STDMETHOD(dummy_GetMaxWidth)(void);
    STDMETHOD(dummy_GetMaxHeight)(void);
    STDMETHOD(dummy_GetFontCollection2)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength2)(void);
    STDMETHOD(dummy_GetFontFamilyName2)(void);
    STDMETHOD(dummy_GetFontWeight2)(void);
    STDMETHOD(dummy_GetFontStyle2)(void);
    STDMETHOD(dummy_GetFontStretch2)(void);
    STDMETHOD(dummy_GetFontSize2)(void);
    STDMETHOD(dummy_GetUnderline)(void);
    STDMETHOD(dummy_GetStrikethrough)(void);
    STDMETHOD(dummy_GetDrawingEffect)(void);
    STDMETHOD(dummy_GetInlineObject)(void);
    STDMETHOD(dummy_GetTypography)(void);
    STDMETHOD(dummy_GetLocaleNameLength2)(void);
    STDMETHOD(dummy_GetLocaleName2)(void);
    STDMETHOD(dummy_Draw)(void);
    STDMETHOD(dummy_GetLineMetrics)(void);
    STDMETHOD(GetMetrics)(dummy_IDWriteTextLayout*, dummy_DWRITE_TEXT_METRICS*);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetClusterMetrics)(void);
    STDMETHOD(dummy_DetermineMinWidth)(void);
    STDMETHOD(dummy_HitTestPoint)(void);
    STDMETHOD(dummy_HitTestTextPosition)(void);
    STDMETHOD(dummy_HitTestTextRange)(void);
};

struct dummy_IDWriteTextLayout_tag {
    dummy_IDWriteTextLayoutVtbl* vtbl;
};

#define dummy_IDWriteTextLayout_QueryInterface(self,a,b)    (self)->vtbl->QueryInterface(self,a,b)
#define dummy_IDWriteTextLayout_AddRef(self)                (self)->vtbl->AddRef(self)
#define dummy_IDWriteTextLayout_Release(self)               (self)->vtbl->Release(self)
#define dummy_IDWriteTextLayout_SetTextAlignment(self,a)    (self)->vtbl->SetTextAlignment(self,a)
#define dummy_IDWriteTextLayout_SetWordWrapping(self,a)     (self)->vtbl->SetWordWrapping(self,a)
#define dummy_IDWriteTextLayout_SetTrimming(self,a,b)       (self)->vtbl->SetTrimming(self,a,b)
#define dummy_IDWriteTextLayout_GetMetrics(self,a)          (self)->vtbl->GetMetrics(self,a)


#endif  /* DUMMY_DWRITE_H */
