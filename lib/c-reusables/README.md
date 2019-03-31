[![Build status (travis-ci.com)](https://img.shields.io/travis/mity/c-reusables/master.svg?label=linux%20build)](https://travis-ci.org/mity/c-reusables)
[![Build status (appveyor.com)](https://img.shields.io/appveyor/ci/mity/c-reusables/master.svg?label=windows%20build)](https://ci.appveyor.com/project/mity/c-reusables/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/mity-c-reusables.svg?label=coverity%20scan)](https://scan.coverity.com/projects/mity-c-reusables)
[![Codecov](https://img.shields.io/codecov/c/github/mity/c-reusables/master.svg?label=code%20coverage)](https://codecov.io/github/mity/c-reusables)


# C Reusables Readme

Home: https://github.com/mity/c-reusables


## Introduction

This repository is collection of assorted highly reusable C utilities.
The following rules apply:

 * **Licensing**: Only code under MIT or in Public Domain is accepted into the
   repository.

 * **Distributing**: Just taking the sources of your interest is enough.
   Copyright notes and licensing terms are placed within each source file.
   Additional files like `LICENSE.md` are **not** required to be distributed.

 * **Self-contained**: Each module is single `*.c` source file with single
   `*.h` header (or just the header in some cases) with no other dependencies
   but system headers and standard C library. Each such module implements only
   tightly related set of functions. Each header provides reasonable
   documentation of the exposed functions and types.

 * **High portability**: All POSIX compatible systems and Windows are supported.
   Most other systems probably too.


## Table of Contents

### Directory `data`

 * `data/buffer.[hc]`: Simple growing buffer.

 * `data/list.h`: Intrusive doubly-linked list.

 * `data/value.[hc]`: Simple value structure, capable of holding various scalar
   types of data (booleans, numeric types, strings) and collections (arrays,
   dictionaries) of such data. It allows to build structured data in run-time;
   for example it can be used as an in-memory storage for JSON-like data.

### Directory `encode`

 * `encode/base64.[hc]`: Encoding and decoding Base64.

 * `encode/hex.[hc]`: Encoding and decoding of bytes into/from hexadecimal
   notation (two hexadecimal digits per byte).

### Directory `hash`

 * `hash/crc32.[hc]`: 32-bit cyclic redundancy check function.

 * `hash/fnv1a.[hc]`: 32-bit and 64-bit Fowler–Noll–Vo (variant 1a) hash
   functions.

### Directory `misc`

 * `misc/cmdline.[hc]`: Lightweight command line (`argc`, `argv`) parsing.

 * `misc/defs.h`: Defining set of basic and frequently used preprocessor macros
   `MIN`, `MAX`, `CLAMP`, `ABS`, `SIZEOF_ARRAY`, `OFFSETOF`, `CONTAINEROF`
   etc.

 * `misc/malloca.h`: `MALLOCA()` and `FREEA()` macros, which are a portable
   variant of `_malloca()` and `_freea()` from Windows SDKs by Microsoft.
   `MALLOCA()` allocates on stack if requested size below some threshold,
   for larger allocations it uses `malloc()`.

### Directory `win32` (Windows only)

 * `win32/memstream.[hc]`: Trivial memory-backed read-only implementation of
   `IStream` COM interface.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/c-reusables/issues
