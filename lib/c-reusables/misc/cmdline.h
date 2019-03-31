/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2017 Martin Mitas
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

#ifndef CRE_CMDLINE_H
#define CRE_CMDLINE_H

#ifdef __cplusplus
extern "C" {
#endif


/* The option may have an argument. (Affects only long option.) */
#define CMDLINE_OPTFLAG_OPTIONALARG     0x0001

/* The option must have an argument.
 * Such short option cannot be grouped within single '-abc'. */
#define CMDLINE_OPTFLAG_REQUIREDARG     0x0002

/* Enable special compiler-like mode for the long option.
 *
 * Note ::shortname is not supported with this flag. CMDLINE_OPTION::shortname
 * is silently ignored if the flag is used.
 *
 * With this flag, CMDLINE_OPTION::longname is treated differently as follows:
 *
 * 1. The option matches if the CMDLINE_OPTION::longname is the exact prefix
 *   of the argv[i] from commandline.
 *
 * 2. Double dash ("--") is not automatically prepended to
 *    CMDLINE_OPTION::longname. (If you desire any leadin dash, include it
 *    explicitly in CMDLINE_OPTION initialization.)
 *
 * 3. An argument (optionally after a whitespace) is required (the flag
 *    CMDLINE_OPTFLAG_COMPILERLIKE implicitly implies also the flag
 *    CMDLINE_OPTFLAG_REQUIREDARG).
 *
 *    But there is no delimiter expected (no "=" between the option and its
 *    argument). Whitespace is optional between the option and its argument.
 *
 *    Intended use is for options similar to what many compilers accept.
 *    For example:
 *      -DDEBUG=0               (-D is the option, DEBUG=0 is the argument).
 *      -Isrc/include           (-I is the option, src/include is the argument).
 *      -isystem /usr/include   (-isystem is the option, /usr/include is the argument).
 *      -lmath                  (-l is the option, math is the argument).
 */
#define CMDLINE_OPTFLAG_COMPILERLIKE    0x0004


/* Special (reserved) option IDs. Do not use these for any CMDLINE_OPTION::id.
 * See documention of cmdline_read() to get info about their meaning.
 */
#define CMDLINE_OPTID_NONE              0
#define CMDLINE_OPTID_UNKNOWN           (-0x7fffffff + 0)
#define CMDLINE_OPTID_MISSINGARG        (-0x7fffffff + 1)
#define CMDLINE_OPTID_BOGUSARG          (-0x7fffffff + 2)


typedef struct CMDLINE_OPTION {
    char shortname;         /* Short (single char) option or 0. */
    const char* longname;   /* Long name (after "--") or NULL. */
    int id;                 /* Non-zero ID to identify the option in the callback; or zero to denote end of options list. */
    unsigned flags;         /* Bitmask of CMDLINE_OPTFLAG_xxxx flags. */
} CMDLINE_OPTION;


/* Parse all options and their arguments as specified by argc, argv accordingly
 * with the given options. The array of supported options has to be ended
 * with member whose CMDLINE_OPTION::id is zero.
 *
 * Note argv[0] is ignored.
 *
 * The callback is called for each (validly matching) option.
 * It is also called for any positional argument (with id set to zero).
 *
 * Special cases (errorneous command line) are reported to the callback by
 * negative id:
 *
 *   -- CMDLINE_OPTID_UNKNOWN: The given option name does not exist.
 *
 *   -- CMDLINE_OPTID_MISSINGARG: The option requires an argument but none
 *      is present on the command line.
 *
 *   -- CMDLINE_OPTID_BOGUSARG: The option expects no argument but some
 *      is provided.
 *
 * In all those cases, name of the affected command line option is provided
 * in arg.
 *
 * On success, zero is returned.
 *
 * If the callback returns non-zero cmdline_read() aborts any subsequent
 * parsing and it returns the same value to the caller.
 */

int cmdline_read(const CMDLINE_OPTION* options, int argc, char** argv,
        int (*callback)(int /*id*/, const char* /*arg*/, void* /*userdata*/),
        void* userdata);


#ifdef __cplusplus
}  /* extern "C" { */
#endif

#endif  /* CRE_CMDLINE_H */
