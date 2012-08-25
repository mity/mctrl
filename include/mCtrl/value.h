/*
 * Copyright (c) 2010-2011 Martin Mitas
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

#ifndef MCTRL_VALUE_H
#define MCTRL_VALUE_H

#include <mCtrl/defs.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * @brief Polymorphic data (@c MC_HVALUETYPE and @c MC_HVALUE).
 *
 * Some controls are able to cope with data of multiple kinds. For example
 * grid control (@c ref MC_WC_GRID) is able to present a table of cells
 * where each cell contains another kind of data, e.g. string, numbers
 * or some images (either bitmaps or icons) and much more.
 *
 * @c MC_HVALUETYPE and @c MC_HVALUE is exactly the abstraction bringing this
 * power to mCtrl. On one side @c MC_HVALUE can contain various kinds of data
 * (according to some @c MC_HVALUETYPE), on the other side the abstraction
 * allows the grid control to manage the values through single interface.
 *
 * The type @c MC_HVALUE is just an opaque type. It is in effect a handle of
 * the particular piece of data. Therefore the application code cannot
 * manipulate with the data directly but only through some functions.
 *
 * @c MC_HVALUETYPE is also an opaque type which determines how the values of
 * that type behave. If you are familiar with object oriented programming you
 * can imagine the type is a virtual method table, with the exception that
 * here a pointer to the vtable is not part of the @c MC_HVALUE data itself.
 *
 * @c MC_HVALUETYPE determines how the data of that type exactly behave.
 * Almost all functions manipulating with some data take both, the @c MC_HVALUE
 * and @c MC_HVALUETYPE as their parameters.
 *
 * @note This is because @c MC_HVALUE does not keep reference to its
 * @c MC_HVALUETYPE. On one side it would be more comfortable but it would make
 * many useful value types much more memory hungry. Often some collections
 * of data require all of their data to be of single type and in such cases
 * tere can be only one variable of @c MC_HVALUETYPE determining type of all
 * data in the collection.
 *
 * If you are familiar with object-oriented programming, you can roughly
 * understand the @c MC_HVALUETYPE as a class and @c MC_HVALUE as an instance.
 *
 * After the value is no longer needed, @ref mcValue_Destroy() can be used to
 * release any resources taken by the value.
 *
 *
 * @section sec_value_builtin Built-in Value Types
 *
 * mCtrl provides value types for some very common kinds of data (e.g. integer
 * numbers, strings etc.).
 *
 * Values of each of these built-in value types can be created by the
 * corresponding factory function. There is also a corresponding getter
 * function which allows to get the data held by the value.
 *
 * Note that for any given value you can only use the getter function
 * corresponding to the value's type. Otherwise the behavior is undefined: it
 * can even lead to an application crash.
 *
 * <table>
 * <tr><th>ID</th><th>Factory function</th><th>Getter function</th></tr><th>Description</th>
 * <tr><td>@c MC_VALUETYPEID_UNDEFINED</td><td></td><td></td><td>Invalid type</td>
 * <tr><td>@ref MC_VALUETYPEID_INT32</td><td>@ref mcValue_CreateFromInt32()</td><td>@ref mcValue_GetInt32()</td><td>32-bit signed integer</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_UINT32</td><td>@ref mcValue_CreateFromUInt32()</td><td>@ref mcValue_GetUInt32()</td><td>32-bit unsigned integer</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_INT64</td><td>@ref mcValue_CreateFromInt64()</td><td>@ref mcValue_GetInt64()</td><td>64-bit signed integer</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_UINT64</td><td>@ref mcValue_CreateFromUInt64()</td><td>@ref mcValue_GetUInt64()</td><td>64-bit unsigned integer</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_STRINGW</td><td>@ref mcValue_CreateFromStringW()</td><td>@ref mcValue_GetStringW()</td><td>Unicode string</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_STRINGA</td><td>@ref mcValue_CreateFromStringA()</td><td>@ref mcValue_GetStringA()</td><td>ANSI string</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_IMMSTRINGW</td><td>@ref mcValue_CreateFromImmStringW()</td><td>@ref mcValue_GetImmStringW()</td><td>Unicode immutable string</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_IMMSTRINGA</td><td>@ref mcValue_CreateFromImmStringA()</td><td>@ref mcValue_GetImmStringA()</td><td>ANSI immutable string</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_COLORREF</td><td>@ref mcValue_CreateFromColorref()</td><td>@ref mcValue_GetColorref()</td><td>Color RGB triplet</td></tr>
 * <tr><td>@ref MC_VALUETYPEID_HICON</td><td>@ref mcValue_CreateFromHIcon()</td><td>@ref mcValue_GetHIcon()</td><td>Icon handle</td></tr>
 * </table>
 *
 *
 * @section sec_value_strings String Values
 *
 * As the table of built-in value types above shows, there are four value types
 * designed to hold strings, identified by the constants:
 *  -- "Ordinary strings" @ref MC_VALUETYPEID_STRINGW and @ref MC_VALUETYPEID_STRINGA
 *  -- "Immutable strings" @ref MC_VALUETYPEID_IMMSTRINGW and @ref MC_VALUETYPEID_IMMSTRINGA
 *
 * There are also two Unicode/ANSI resolution macros @c MC_VALUETYPEID_STRING
 * and @c MC_VALUETYPEID_IMMSTRING, one for the ordinary strings, the letter
 * for the immutable ones.
 *
 * The ordinary strings keep copies of the string buffers used during value
 * creation, while the immutable strings only store pointers to original string
 * buffers and they expect the buffer does not change during lifetime of the
 * value.
 *
 * As it is obvious from the description, the ordinary strings can be used
 * for any strings, while work with the immutable strings can be much more
 * effective but the application is responsible to guarantee immutability
 * of the underlying string buffers pointed by them.
 *
 *
 * @section sec_value_null Values and @c NULL
 *
 * Each value type guarantees that @c NULL is valid value of that type.
 * Furthermore all the value types provide a guaranty that calling
 * @c mcValue_Destroy() for @c NULL values is noop.
 *
 * However it depends on the particular value type, how @c NULL values are
 * interpreted. For the built-in types, it usually corresponds to @c 0 (zero),
 * an empty string or @c NULL handle, depending on the nature of the value type.
 */


/**
 * An opaque type representing the value type.
 */
typedef const void* MC_HVALUETYPE;

/**
 * An opaque type representing the value itself.
 * The application is responsible to remember what's the type of the value.
 */
typedef void* MC_HVALUE;


/**
 * @name ID of Built-in Value Types
 * Use these constants to get corresponding @ref MC_HVALUETYPE with function
 * @ref mcValueType_GetBuiltin().
 */
/*@{*/
#define MC_VALUETYPEID_UNDEFINED        0
/** @brief ID for 32-bit signed integer value type. */
#define MC_VALUETYPEID_INT32            1
/** @brief ID for 32-bit unsigned integer value type. */
#define MC_VALUETYPEID_UINT32           2
/** @brief ID for 64-bit signed integer value type. */
#define MC_VALUETYPEID_INT64            3
/** @brief ID for 64-bit unsigned integer value type. */
#define MC_VALUETYPEID_UINT64           4
/** @brief ID for unicode string value type. */
#define MC_VALUETYPEID_STRINGW          5
/** @brief ID for ANSI string value type. */
#define MC_VALUETYPEID_STRINGA          6
/** @brief ID for immutable Unicode string value type. */
#define MC_VALUETYPEID_IMMSTRINGW       7
/** @brief ID for immutable ANSI string value type. */
#define MC_VALUETYPEID_IMMSTRINGA       8
/** @brief ID for color RGB triplet. */
#define MC_VALUETYPEID_COLORREF         9
/** @brief ID for icon handle (@c HICON). */
#define MC_VALUETYPEID_HICON           10
/*@}*/


/**
 * @brief Retrieve Handle of a value type implemented in mCtrl.
 * @param[in] id The identifier of the requested value type.
 * @return The type handle corresponding to the @c id, or @c NULL.
 * if the @c id is not supported.
 */
MC_HVALUETYPE MCTRL_API mcValueType_GetBuiltin(int id);


/**
 * @name Factory Functions
 */
/*@{*/

/**
 * @brief Create a value holding 32-bit signed integer.
 * @param[out] phValue Filled with new value handle.
 * @param[in] iValue The integer.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromInt32(MC_HVALUE* phValue, INT iValue);

/**
 * @brief Create a value holding 32-bit unsigned integer.
 * @param[out] phValue Filled with new value handle.
 * @param[in] uValue The integer.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromUInt32(MC_HVALUE* phValue, UINT uValue);

/**
 * @brief Create a value holding 64-bit signed integer.
 * @param[out] phValue Filled with new value handle.
 * @param[in] i64Value The integer.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromInt64(MC_HVALUE* phValue, INT64 i64Value);

/**
 * @brief Create a value holding 64-bit unsigned integer.
 * @param[out] phValue Filled with new value handle.
 * @param[in] u64Value The integer.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromUInt64(MC_HVALUE* phValue, UINT64 u64Value);

/**
 * @brief Create a value holding unicode string.
 * @param[out] phValue Filled with new value handle.
 * @param[in] lpStr The string.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromStringW(MC_HVALUE* phValue, LPCWSTR lpStr);

/**
 * @brief Create a value holding ANSI string.
 * @param[out] phValue Filled with new value handle.
 * @param[in] lpStr The string.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromStringA(MC_HVALUE* phValue, LPCSTR lpStr);

/**
 * @brief Create a value holding immutable Unicode string.
 * @param[out] phValue Filled with new value handle.
 * @param[in] lpStr The string.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromImmStringW(MC_HVALUE* phValue, LPCWSTR lpStr);

/**
 * @brief Create a value holding immutable ANSI string.
 * @param[out] phValue Filled with new value handle.
 * @param[in] lpStr The string.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromImmStringA(MC_HVALUE* phValue, LPCSTR lpStr);

/**
 * @brief Create a value holding immutable RGB triplet (@c COLORREF).
 * @param[out] phValue Filled with new value handle.
 * @param[in] crColor The RGB triplet.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromColorref(MC_HVALUE* phValue, COLORREF crColor);

/**
 * @brief Create a value holding a handle to icon (@c HICON).
 * @param[out] phValue Filled with new value handle.
 * @param[in] hIcon The icon handle.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_CreateFromHIcon(MC_HVALUE* phValue, HICON hIcon);

/*@}*/


/**
 * @name Getter Functions
 */
/*@{*/

/**
 * @brief Getter for 32-bit signed integer values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_INT32.
 * @return The integer.
 */
INT MCTRL_API mcValue_GetInt32(const MC_HVALUE hValue);

/**
 * @brief Getter for 32-bit unsigned integer values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_UINT32.
 * @return The integer.
 */
UINT MCTRL_API mcValue_GetUInt32(const MC_HVALUE hValue);

/**
 * @brief Getter for 64-bit signed integer values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_INT64.
 * @return The integer.
 */
INT64 MCTRL_API mcValue_GetInt64(const MC_HVALUE hValue);

/**
 * @brief Getter for 64-bit unsigned integer values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_UINT64.
 * @return The integer.
 */
UINT64 MCTRL_API mcValue_GetUInt64(const MC_HVALUE hValue);

/**
 * @brief Getter for unicode string values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_STRINGW.
 * @return Pointer to the buffer of the string.
 */
LPCWSTR MCTRL_API mcValue_GetStringW(const MC_HVALUE hValue);

/**
 * @brief Getter for ANSI string values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_STRINGA.
 * @return Pointer to the buffer of the string.
 */
LPCSTR MCTRL_API mcValue_GetStringA(const MC_HVALUE hValue);

/**
 * @brief Getter for immutable unicode string values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_IMMSTRINGW.
 * @return Pointer to the buffer of the string.
 */
LPCWSTR MCTRL_API mcValue_GetImmStringW(const MC_HVALUE hValue);

/**
 * @brief Getter for immutable ANSI string values.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_IMMSTRINGA.
 * @return Pointer to the buffer of the string.
 */
LPCSTR MCTRL_API mcValue_GetImmStringA(const MC_HVALUE hValue);

/**
 * @brief Getter for color RGB triplet.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_COLORREF.
 * @return The color RGB triplet.
 */
COLORREF MCTRL_API mcValue_GetColorref(const MC_HVALUE hValue);

/**
 * @brief Getter for icon handle.
 * @param[in] hValue The value. It must be of type @ref MC_VALUETYPEID_HICON.
 * @return The icon handle.
 */
HICON MCTRL_API mcValue_GetHIcon(const MC_HVALUE hValue);

/*@}*/


/**
 * @name Other Value Functions
 */
/*@{*/

/**
 * @brief Duplicates a value.
 * @param[in] hType Type of both the source and target value handles.
 * @param[out] phDest Filled with new value handle.
 * @param[in] hSrc Source value handle.
 * @return @c TRUE on success, @c FALSE on failure.
 */
BOOL MCTRL_API mcValue_Duplicate(MC_HVALUETYPE hType, MC_HVALUE* phDest, const MC_HVALUE hSrc);

/**
 * @brief Destroys a value handle.
 * @param[in] hType Type of the source value.
 * @param[in] hValue The value.
 */
void MCTRL_API mcValue_Destroy(MC_HVALUETYPE hType, MC_HVALUE hValue);

/*@}*/


/**
 * @name Unicode Resolution
 */
/*@{*/

/** @brief Unicode-resolution alias. @sa MC_VALUETYPEID_STRINGW MC_VALUETYPEID_STRINGA */
#define MC_VALUETYPEID_STRING        MCTRL_NAME_AW(MC_VALUETYPEID_STRING)
/** @brief Unicode-resolution alias. @sa MC_VALUETYPEID_IMMSTRINGW MC_VALUETYPEID_IMMSTRINGA */
#define MC_VALUETYPEID_IMMSTRING     MCTRL_NAME_AW(MC_VALUETYPEID_IMMSTRING)
/** @brief Unicode-resolution alias. @sa mcValue_CreateFromStringW mcValue_CreateFromStringA */
#define mcValue_CreateFromString     MCTRL_NAME_AW(mcValue_CreateFromString)
/** @brief Unicode-resolution alias. @sa mcValue_CreateFromImmStringW mcValue_CreateFromImmStringA */
#define mcValue_CreateFromImmString  MCTRL_NAME_AW(mcValue_CreateFromImmString)
/** @brief Unicode-resolution alias. @sa mcValue_GetStringW mcValue_GetStringA */
#define mcValue_GetString            MCTRL_NAME_AW(mcValue_GetString)
/** @brief Unicode-resolution alias. @sa mcValue_GetImmStringW mcValue_GetImmStringA */
#define mcValue_GetImmString         MCTRL_NAME_AW(mcValue_GetImmString)

/*@}*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MCTRL_VALUE_H */
