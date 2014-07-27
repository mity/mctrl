/*
 * Unit tests verifying correct build options
 * Copyright (c) 2014 Martin Mitas
 */

#include "cutest.h"
#include <windows.h>
#include <tchar.h>


/************************
 *** Exported Symbols ***
 ************************/

static void
expssym_test(void)
{
    HMODULE dll;

    /* Verify via GetProcAddress() that we are able to retrieve some MCTRL.DLL
     * exported symbols. This actually verifies the symbols do not have
     * any undesired decorations.
     */

    dll = LoadLibrary(_T("mCtrl.dll"));
    if(!TEST_CHECK(dll != NULL))
       return;

   TEST_CHECK(GetProcAddress(dll, "DllGetVersion") != NULL);
   TEST_CHECK(GetProcAddress(dll, "mcBeginBufferedAnimation") != NULL);
   TEST_CHECK(GetProcAddress(dll, "mcGrid_Initialize") != NULL);
   TEST_CHECK(GetProcAddress(dll, "mcVersion") != NULL);

   FreeLibrary(dll);
}


/************************
 *** String Resources ***
 ************************/

static BOOL
strres_is_zero_terminated(const WCHAR* str, UINT len)
{
    if(len > 0  &&  str[len-1] == L'\0')
        return TRUE;

#if 1
    /* For now, we use "foo bar\0x" to have the string resources
     * zero-terminated due to CMake limitations:
     *      https://github.com/Kitware/CMake/pull/113
     * (We may get rid of it after the CMake with the fix is released.)
     */
    if(len > 1  &&  str[len-2] == L'\0')
        return TRUE;
#endif

    return FALSE;
}

static BOOL CALLBACK
strres_enum_lang(HMODULE dll, const TCHAR* type, TCHAR* name, WORD lang_id, LPARAM lp)
{
    HRSRC res;
    HGLOBAL glob;
    const WCHAR* str;
    UINT len;
    int i;

    if(!TEST_CHECK(IS_INTRESOURCE(name)))
        goto out;

    res = FindResourceEx(dll, RT_STRING, (ULONG_PTR)name, lang_id);
    if(!TEST_CHECK(res != NULL))
        goto out;

    glob = LoadResource(dll, res);
    if(!TEST_CHECK(glob != NULL))
        goto out;

    /* String resources are stored in blocks of 16 strings. */
    str = (const WCHAR*) LockResource(glob);
    for(i = 0; i < 16; i++) {
        len = str[0];
        str++;

        if(len > 0) {
            TEST_CHECK_(strres_is_zero_terminated(str, len), "[%u:%u] zero-terminated",
                        lang_id, (((ULONG_PTR)name - 1) * 16 + i));
            str += len;
        }
    }

out:
    return TRUE;
}

static BOOL CALLBACK
strres_enum(HMODULE dll, const TCHAR* type, TCHAR* name, LPARAM lp)
{
    EnumResourceLanguages(dll, type, name, strres_enum_lang, 0);
    return TRUE;
}

static void
strres_test(void)
{
    /* Resource compilers differ in their behavior about zero terminators
     * of string resources.
     *
     * RC.EXE (as of MSVC 12) by default produces unterminated string resources,
     * even if resource scripts explicitly uses them (e.g. "foo bar\0"). The
     * tool supports the option /n which forces each string resource to be
     * zero-terminated.
     *
     * windres (as of GNU binutils 2.24) follows blindly the resource script,
     * i.e. the string resources are zero-terminated if (and only if) zero
     * is explicitly specified (e.g. "foo bar\0")
     *
     * mCtrl.dll needs zero-terminated sring resources to work properly.
     */

    HMODULE dll;

    dll = LoadLibrary(_T("mCtrl.dll"));
    if(!TEST_CHECK(dll != NULL))
       return;

    EnumResourceNames(dll, MAKEINTRESOURCE(RT_STRING), strres_enum, 0);

    FreeLibrary(dll);
}


/*****************
 *** Test List ***
 *****************/

TEST_LIST = {
    { "exported-symbols", expssym_test },
    { "string-resources", strres_test },
    { 0 }
};

