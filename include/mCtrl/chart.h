/*
 * Copyright (c) 2013 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MCTRL_CHART_H
#define MCTRL_CHART_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Chart control (@c MC_WC_CHART).
 *
 * The chart control is intended to display large sets of numerical data in a
 * visually illustrative way.
 *
 * @attention The chart control requires @c GDIPLUS.DLL bersion 1.0 or newer to
 * work correctly. This library was introduced in Windows XP and Windows Server
 * 2003. If your application needs to use this control on Windows 2000, you may
 * need to distribute @c GDIPLUS.DLL along with your application. (Microsoft
 * released @c GDIPLUS.DLL 1.0 as a redistributable for this purpose.)
 *
 * In general, the control is able to hold a multiple series of data (data
 * sets). Each series typically denotes some quantity evolvement in time or its
 * dependency on other quantity. In the chart each series is represented with a
 * different color, and accompanied with a legend text.
 *
 * The chart control supports charts of many types, depending on the used
 * control style. The type determines how the control presents the data.
 *
 *
 * @section chart_dataset Data Sets
 *
 * To inset, delete or reset the data set values, use messages
 * @c MC_CHM_INSERTDATASET, @c MC_CHM_DELETEDATASET or @c MC_CHM_DELETEALLDATASETS
 * and @c MC_CHM_SETDATASET respectivally.
 *
 * Data sets can have the data only virtually. That is application can provide
 * to the control only an information the data set exists and how large it is.
 * Whenever the control paints and needs really the data, it asks for them
 * with @c MC_CHN_GETDISPINFO message. This is useful especially if the data
 * for the chart are realy large as it avoids duplication of the data in program
 * memory.
 *
 * To set various other attributes of the data set, you may use
 * @c MC_CHM_SETDATASETLEGEND to set its legend or @c MC_CHM_SETDATASETCOLOR
 * to set the color representing the values form the data set.
 *
 *
 * @section chart_axes Axes
 *
 * The control has a concept of two axes, the primary and secondary one.
 * It depends on particular chart type though, whether and how the control uses
 * them. Currently each axis has only a single attribute, a factor exponent,
 * but in future versions more attributes may be added.
 *
 * Usually (for chart types where it gives any sense), the primary axis
 * corresponds to the index of data set and in most cases is displayed as the
 * horizontal (X) axis, and the secondary one corresponds to values in a data
 * set and in most cases is displayed as the vertical (Y) axis.
 *
 * The factor exponent is an integer value in the range (@c -9 to @c +9),
 * and it it is used when painting values for the given axis. An ineteger value
 * is multiplied with <tt>(10 ^ exp)</tt>, where @c ^ marks power and
 * @c exp the exponent.
 *
 * This is especially usefull for charts with non-integer values as negative
 * factor exponent actually turns the data set values into fixed point numbers.
 *
 * For example with factor exponent @c -2, data set with values <tt>{ 5, 100,
 * 101, 102 }</tt> would be displayed as <tt>{ 0.05, 1.00, 1.01, 1.02 }.</tt>
 *
 *
 * @section chart_types Chart Types
 *
 * @attention Different types of chart have different requirements about the
 * data and if the application does not follow these requirements, the chart
 * can just display gibberish, or change the output in future mCtrl versions.
 *
 * @subsection chart_pie Pie Chart
 *
 * The pie chart expects each data set consisting of a single non-negative
 * value. It does not display any axis, but factor exponent of the primary one
 * is used for displaying data set values.
 *
 * @note The pie chart visually differs quite a lot from majority of the other
 * chart types this control provides. Many control attibutes settable by
 * control messages actually have no impact to the pie chart.
 *
 *
 * @subsection chart_scatter Scatter Chart
 *
 * The data sets are expected to have even count of values. The scatter chart
 * interprets each data set as a set of value pairs. Each value with odd index
 * corresponds with the primary (X) axis, and even index with secondary (Y) axis
 * respectivelly.
 *
 *
 * @subsection chart_scatter Column Chart
 *
 * Column chart has only one expectations for the data: All the data sets should
 * be of the same size.
 *
 *
 * @subsection chart_scatter Stacked Column Chart
 *
 * The stacked column chart is the same as the (unstacked) column chart, but
 * the values of different data sets are painted on top of each other.
 *
 *
 * @subsection chart_scatter Bar Chart
 *
 * Generally the chart is equivalent to the column chart, but the primary
 * and secondary axes are swapped; i.e. unlike fpr majority of the
 * other chart types, the vertical axis is primary for this chart type,
 * and the horizontal is secondary.
 *
 * Bar chart has only one expectations for the data: All the data sets should
 * be of the same size.
 *
 *
 * @subsection chart_scatter Stacked Bar Chart
 *
 * The stacked bar chart is the same as the (unstacked) bar chart, but the
 * values of different data sets are painted on top of each other.
 *
 *
 * @section std_msgs Standard Messages
 *
 * These standard messages are handled by the control:
 * - @c WM_GETTEXT
 * - @c WM_SETTEXT
 * - @c CCM_SETNOTIFYWINDOW
 *
 * These standard notifications are sent by the control:
 * - @c NM_OUTOFMEMORY
 */


/**
 * @name Initialization Functions
 */
/*@{*/

/**
 * Registers window class of the control.
 *
 * Note that the function fails, if @c GDIPLUS.DLL is not available.
 *
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcChart_Initialize(void);

/**
 * Unregisters window class of the control.
 */
void MCTRL_API mcChart_Terminate(void);

/*@}*/


/**
 * @name Window Class
 */
/*@{*/

/** Window class name (unicode variant). */
#define MC_WC_CHARTW           L"mCtrl.chart"
/** Window class name (ANSI variant). */
#define MC_WC_CHARTA            "mCtrl.chart"

/*@}*/


/**
 * @name Control Styles
 */
/*@{*/

/** @brief Pie chart. */
#define MC_CHS_PIE             0x0000
/** @brief Scatter chart. */
#define MC_CHS_SCATTER         0x0001
/** @brief Line chart. */
#define MC_CHS_LINE            0x0002
/** @brief Stacked line chart. */
#define MC_CHS_STACKEDLINE     0x0003
/** @brief Area chart. */
#define MC_CHS_AREA            0x0004
/** @brief Stacked area chart. */
#define MC_CHS_STACKEDAREA     0x0005
/** @brief Column chart. */
#define MC_CHS_COLUMN          0x0006
/** @brief Stacked column chart. */
#define MC_CHS_STACKEDCOLUMN   0x0007
/** @brief Bar chart. */
#define MC_CHS_BAR             0x0008
/** @brief Stacked bar chart. */
#define MC_CHS_STACKEDBAR      0x0009
/** @brief This is not actually a valid style, it's the bitmask of chart type. */
#define MC_CHS_TYPEMASK        0x003f

/**
 * @brief Disables a tooltip window.
 *
 * When having a tooltip window associated, the control uses to show additional
 * information about the pointed value in the chart. By default the control
 * creates its own control when this style is not set.
 *
 * @sa MC_CHM_SETTOOLTIPS
 */
#define MC_CHS_NOTOOLTIPS      0x0040

/*@}*/


/**
 * @anchor MC_CHDIF_xxxx
 * @name MC_NMCHDISPINFO::fMask Bits
 */
/*@{*/

/**
 * @brief The control asks for @ref MC_NMCHDISPINFO::piValues.
 *
 * The application is responsible to fill the buffer @c piValues with values
 * of data set determined by @c iDataSet, corresponding to the value indexes
 * in the interval from @c iValueFirst to @c iValueLast (including).
 *
 * The control guarantees the buffer @c piValues is large enough for
 * <tt>(iValueLast-iValueFirst+ 1)</tt> integers.
 */
#define MC_CHDIM_VALUES               (1 << 0)

/*@}*/


/**
 * @name Structures
 */
/*@{*/

/**
 * @brief Structure for manipulating with a data set.
 *
 * On input, set @c dwCount to a count of values in the data set (when inserting
 * or setting a data set), or to a number of values which can be written to the
 * buffer pointed by @c piValues (when getting a data set).
 *
 * If inserting or setting a data set and if @c piValues is set to @c NULL, then
 * the control will retrieve the data dynamically from its parent through
 * a notification @c MC_CHN_GETDISPINFO.
 *
 * @sa MC_CHM_INSERTDATASET MC_CHM_GETDATASET MC_CHM_SETDATASET
 */
typedef struct MC_CHDATASET_tag {
    /** @brief Count of values in the data set. */
    DWORD dwCount;
    /** @brief Pointer to an array of values in the data set. */
    int* piValues;
} MC_CHDATASET;

/**
 * @brief Structure for notification @c MC_CHN_GETDISPINFO.
 */
typedef struct MC_NMCHDISPINFO_tag {
    /** @brief Common notification structure header. */
    NMHDR hdr;
    /** @brief Mask of members the control asks for. */
    DWORD fMask;
    /** @brief Data set index. */
    int iDataSet;
    /** @brief Index of a first value the controls asks for. */
    int iValueFirst;
    /** @brief Index of last value the controls asks for. */
    int iValueLast;
    /** @brief Pointer to a buffer where application fills the value. */
    int* piValues;
} MC_NMCHDISPINFO;

/*@}*/


/**
 * @name Control Messages
 */
/*@{*/

/**
 * @brief Get count of data sets.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c int) Count of data sets.
 */
#define MC_CHM_GETDATASETCOUNT        (MC_CHM_FIRST + 0)

/**
 * @brief Delete all data sets.
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 *
 * @sa MC_CHM_DELETEDATASET
 */
#define MC_CHM_DELETEALLDATASETS      (MC_CHM_FIRST + 1)

/**
 * @brief Insert a data set.
 *
 * If the @c MC_CHDATASET::piValues is set to @c NULL, the control will retrieve
 * the data dynamically from its parent through a notification @c MC_CHN_GETDISPINFO.
 *
 * @param[in] wParam (@c int) Index of the new data set.
 * @param[in] lParam (@c MC_CHDATASET*) Pointer to a data of the new data set.
 * @return (@c int) Index of the new data set, or @c -1 on failure.
 */
#define MC_CHM_INSERTDATASET          (MC_CHM_FIRST + 2)

/**
 * @brief Delete a data set.
 * @param[in] wParam (@c int) Index of the data set.
 * @param lParam Reserved, set to zero.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_DELETEDATASET          (MC_CHM_FIRST + 3)

/**
 * @brief Get a data set.
 *
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in,out] lParam (@c MC_CHDATASET*) Pointer to a data of the new data
 * set. On input, its member @c dwCount must be set to indicate how many values
 * can be written to the buffer pointed by @c piValues. In output, the @c dwCount
 * is updated to a count of values written into the @c piValues. You may also set
 * @c lParam to @c NULL to just get number of values in the data set.
 * @return (@c int) Number of values in the data set, or @c -1 on failure.
 *
 * @note If the data set holds its value only virtually (i.e. if the
 * @c MC_CHDATASET::piValues was @c NULL when the dataset was set), then this
 * message just sets @c MC_CHDATASET::dwCount to zero.
 */
#define MC_CHM_GETDATASET             (MC_CHM_FIRST + 4)

/**
 * @brief Set a data set.
 *
 * If the @c MC_CHDATASET::piValues is set to @c NULL, the control will retrieve
 * the data dynamically from its parent through a notification @c MC_CHN_GETDISPINFO.
 *
 * @param[in] wParam (@c int) Index of the data set to change.
 * @param[in] lParam (@c MC_CHDATASET*) Pointer to a data of the new data set.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASET             (MC_CHM_FIRST + 5)

/**
 * @brief Get a color representing the data set in the chart.
 * @param[in] wParam (@c int) Index of the data set.
 * @param lParam Reserved, set to zero.
 * @return (@c COLORREF) @c The color, or @c -1 on failure.
 */
#define MC_CHM_GETDATASETCOLOR        (MC_CHM_FIRST + 6)

/**
 * @brief Set a color representing the data set in the chart.
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c COLORREF) The color. It may be @c MC_CLR_DEFAULT.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETCOLOR        (MC_CHM_FIRST + 7)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETDATASETLEGENDW      (MC_CHM_FIRST + 8)

/**
 * Not yet implemented.
 */
#define MC_CHM_GETDATASETLEGENDA      (MC_CHM_FIRST + 9)

/**
 * @brief Set legend text of the data set (unicode variant).
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c WCHAR*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETLEGENDW      (MC_CHM_FIRST + 10)

/**
 * @brief Set legend text of the data set (ANSII variant).
 * @param[in] wParam (@c int) Index of the data set.
 * @param[in] lParam (@c WCHAR*) The legend string.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETDATASETLEGENDA      (MC_CHM_FIRST + 11)

/**
 * @brief Gets factor exponent of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to retrieve the exponent of primary
 * axis, or @c 2 to set secondary axis.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The value of the exponent, or @c -666 on failure.
 */
#define MC_CHM_GETFACTOREXPONENT      (MC_CHM_FIRST + 12)

/**
 * @brief Sets factor exponent of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 0 to set the exponent for both axes,
 * @c 1 to set primary axis, or @c 2 to set secondary axis.
 * @param[in] lParam (@c int) The exponent. Only values from @c -9 to @c +9
 * (inclusive) are allowed.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETFACTOREXPONENT      (MC_CHM_FIRST + 13)

/**
 * @brief Gets offset of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to retrieve the exponent of primary
 * axis, or @c 2 to set secondary axis.
 * @param lParam Reserved, set to zero.
 * @return (@c int) The offset, or @c -666 on failure.
 */
#define MC_CHM_GETAXISOFFSET          (MC_CHM_FIRST + 14)

/**
 * @brief Sets offset of a primary or secondary axis.
 * @param[in] wParam (@c int) Set to @c 1 to set primary axis, or @c 2 to set
 * secondary axis.
 * @param[in] lParam (@c int) The offset.
 * @return (@c BOOL) @c TRUE on success, @c FALSE otherwise.
 */
#define MC_CHM_SETAXISOFFSET          (MC_CHM_FIRST + 15)

/**
 * @brief Associate a tooltip window with the chart control.
 * @param[in] wParam (@c HWND) Handle of the tooltip window.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of previous tooltip window or @c NULL if no tooltip
 * was associated with the control.
 * @sa MC_CHS_NOTOOLTIPS
 */
#define MC_CHM_SETTOOLTIPS            (MC_CHM_FIRST + 16)

/**
 * @brief Get tooltip associated with the control
 * @param wParam Reserved, set to zero.
 * @param lParam Reserved, set to zero.
 * @return (@c HWND) Handle of thetooltip window or @c NULL if no tooltip
 * is associated with the control.
 */
#define MC_CHM_GETTOOLTIPS            (MC_CHM_FIRST + 17)

/*@}*/


/**
 * @name Control Notifications
 */
/*@{*/

/**
 * @brief Fired when the control needs some data from its parent.
 *
 * When the control itself holds any data only virtually and it actually needs
 * them, it asks the application to provide them with this message. Application
 * is expected to inspect @c MC_NMCHDISPINFO::fMask and fill the structure
 * with corresponding data.
 *
 * @param[in] wParam (@c int) Id of the control sending the notification.
 * @param[in,out] lParam (@ref MC_NMCHDISPINFO*) Pointer to structure for
 * exchanging the data.
 * @return Ignored.
 */
#define MC_CHN_GETDISPINFO            (MC_CHN_FIRST + 0)

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** Unicode-resolution alias. @sa MC_WC_CHARTW MC_WC_CHARTA */
#define MC_WC_CHART              MCTRL_NAME_AW(MC_WC_CHART)
/** Unicode-resolution alias. @sa MC_CHM_GETDATASETLEGENDW MC_CHM_GETDATASETLEGENDA */
#define MC_CHM_GETDATASETLEGEND  MCTRL_NAME_AW(MC_CHM_GETDATASETLEGEND)
/** Unicode-resolution alias. @sa MC_CHM_SETDATASETLEGENDW MC_CHM_SETDATASETLEGENDA */
#define MC_CHM_SETDATASETLEGEND  MCTRL_NAME_AW(MC_CHM_SETDATASETLEGEND)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_CHART_H */
