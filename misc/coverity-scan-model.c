/*
 * Copyright (c) 2014 Martin Mitas
 *
 * Model file to eliminate various false positives, when performing static
 * code analyzes of mCtrl source code with Coverity scan.
 * (See http://scan.coverity.com)
 */


/*******************************
 *** _malloca() and _freea() ***
 *******************************/

/* Convince Coverity _malloca() is same as malloc()
 * (However _malloca() is a macro so we have to redefine inline functions
 * it is based on) */
void* _MarkAllocaS(void* ptr, unsigned int marker)   { return ptr; }
void* alloca(size_t size)                            { return __coverity_alloc__(size); }

/* Convince Coverity _freea() is same as free() */
void _freea(void* ptr)                               { __coverity_free__(ptr); }


/************************
 *** Unreachable code ***
 ************************/

/* These are done when an assertion fails (MC_ASSERT). */
void DebugBreak(void)          { __coverity_panic__(); }
void exit(int exitcode)        { __coverity_panic__(); }

/* This is gcc way to mark unreachable code path (MC_UNREACHABLE). */
void __builtin_unreachable()   { __coverity_panic__(); }
