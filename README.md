[![Build status](https://ci.appveyor.com/api/projects/status/d3ypc0ot57u47itw/branch/master?svg=true)](https://ci.appveyor.com/project/mity/mctrl/branch/master)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/mctrl/badge.svg?flat=1)](https://scan.coverity.com/projects/mctrl)

# mCtrl Readme


## What is mCtrl

mCtrl is C library providing set of additional user interface controls for
MS Windows, intended to be complementary to standard Win32API controls from
`USER32.DLL` and `COMCTL32.DLL`.

API of the library is designed to be similar to the Win32API. I.e. after window
class of particular control is registered with corresponding initialization
function, the control can be normally created with the Win32API's `CreateWindow()`
or `CreateWindowEx()` functions and controlled with `SendMessage()`.


## Getting mCtrl

You can always get the latest version and most actual information on project
webpages:

* http://mctrl.sourceforge.net
* http://sourceforge.net/projects/mctrl

There are usually two packages for each release version available:

* `mCtrl-x.y.z-bin.zip` ... pre-built binary package
* `mCtrl-x.y.z-src.zip` ... source package

The pre-built package contains 32-bit as well as 64-bit binaries of MCTRL.DLL
and examples, and also documentation for application developers. The source
package is direct export of source tree from version control system repository.

The current code (possibly untested and unstable) can also be cloned from git
repository hosted on github:

* http://github.com/mity/mctrl


## Using mCtrl

If you have the pre-built package, using mCtrl is as easy as using any other
DLL.

Header files are located in `include\mCtrl` directory. You should instruct
your C/C++ compiler to search for header files in the 'include' directory and
use the 'mCtrl' in your preprocessor #include directives, e.g.:

```
#include <mCtrl/version.h>
```

Import libraries are located under the `lib` (32-bit libs) and `lib64` (64-bit)
subdirectory:

* `libmCtrl.dll.a` for gcc-based tool chains (e.g. mingw, mingw-w64)
* `mCtrl.lib` for MSVC

And finally deploy your application with the MCTRL.DLL which is located
in the 'bin' (32-bit binaries) and 'bin64' (64-bit) subdirectories in
the prebuilt package.

Documentation for application developers is bundled within the pre-built
package, or you can also find the documentation online:

* http://mctrl.sourceforge.net/doc.php


## Building mCtrl from Sources

Disclaimer: If you want to just use MCTRL.DLL you should probably stick with
the pre-built package.

To build mCtrl yourself from the source package or cloned git repository, first
of all you need to use CMake 3.1 (or newer) to generate project files, Makefile
or whatever for the development tool-chain of your choice. mCtrl is known to
successfully build within following environments.

To build with MSYS + mingw-w64 + Make:
```
$ cmake -G "MSYS Makefiles" path/to/mctrl
```

To build with MSYS + mingw-w64 + Ninja:
```C
$ cmake -G "Ninja" path/to/mctrl
```

To build with Microsoft Visual Studio 2013:
```
$ cmake -G "Visual Studio 12 2013" path/to/mctrl
$ cmake -G "Visual Studio 12 2013 Win64" path/to/mctrl  # 64-bit build
```

Other CMake generators may or may not work. If they do not, then one or more
`CMakeLists.txt` files within mCtrl directory tree may need some tuning.

Use
```
$ cmake --help
```
and refer to CMake documentation to learn more about CMake, its options and
capabilities.

Then proceed to actually build mCtrl by opening the project files in your IDE
or by running the make or other tool corresponding to the used CMake generator.

Finally, consider running a mCtrl test-suite to verify correctness of your
build. The test suite, as well as some examples demonstrating mCtrl, are built
as part of the mCtrl build process.


## License

mCtrl itself is covered with the GNU Lesser General Public License 2.1 or
(at your option) any later version. See file `COPYING.lib` for more info.

In brief, this generally means that:

* Any program or library, even commercial, covered with any proprietary
  license, is allowed to link against the mCtrl's import libraries and
  distribute `MCTRL.DLL` along with the program.

* You can modify `MCTRL.DLL` (or its source code) and distribute such modified
  `MCTRL.DLL` only if the modifications are also licensed under the terms of
  the LGPL 2.1 (or any later version); or under the terms of GPL 2 (or any
  later version).

Source code of all examples, i.e. contents of the directory 'examples' within
the source package (see below), are in public domain.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/mctrl/issues

Please provide the following information with the bug report:

* mCtrl version you are using.
* Whether you use 32-bit or 64-bit build of mCtrl.
* OS version where you reproduce the issue.
* As explicit description of the issue as possible, i.e. what behavior
  you expect and what behavior you see.
  (Reports of the kind "it does not work." do not help).
* If relevant, consider attaching a screenshot or some code reproducing
  the issue.
