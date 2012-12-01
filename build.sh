#!/bin/sh
#
# Helper script to build with different toolchains.

ARCH=i686
DEBUG=1
TOOLCHAIN=auto
MAKETARGETS=


function usage
{
    echo "Usage: $0 [OPTIONS...] [MAKE_TARGETS]"
    echo
    echo "Options: "
    echo "   --arch=ARCH               Specify architecture (default is i686)"
    echo "   --32                      Alias for --arch=i686"
    echo "   --64                      Alias for --arch=x86_64"
    echo "   -r, --release             Make release build"
    echo "   --debug=LEVEL             Make debug build"
    echo "   -d,--debug                Alias for --debug=1"
    echo "   --toolchain=TOOLCHAIN     Specify toolchain (default is auto)"
    echo "   -h, --help                Output this help."
    echo
    echo "Supported architectures:"
    echo "   i686                      Build 32bit binaries"
    echo "   x86_64                    Build 64bit binaries"
    echo
    echo "Supported toolchains:"
    echo "   auto                      Try to guess"
    echo "   mingw-builds              Toolchain from http://sf.net/projects/mingwbuilds"
    echo "   mingw-w64                 Toolchain from http://sf.net/projects/mingw-w64"
    echo
    echo "Supported make targets:"
    echo "   all"
    echo "   clean"
    echo "   distclean"
    echo "   doc"
    echo "   examples"
    echo "   mctrl"
}


while (( "$#" )); do
    case "$1" in
        --arch=* )
            ARCH=`echo "$1" | sed 's/^--arch=//'`
            ;;

        --32 )
            ARCH=i686
            ;;

        --64 )
            ARCH=x86_64
            ;;

        -r | --release )
            DEBUG=0
            ;;

        --debug=* )
            DEBUG=`echo "$1" | sed 's/^--debug=//'`
            ;;

        -d | --debug )
            DEBUG=1
            ;;

        --toolchain=* )
            TOOLCHAIN=`echo "$1" | sed 's/^--toolchain=//'`
            ;;

        -h | --help )
            usage $0
            exit 0
            ;;

        all | examples | clean | distclean | mctrl )
            MAKETARGETS="$MAKETARGETS $1"
            ;;

        * )
            echo "Bad usage."
            echo
            usage $0
            exit 1
            ;;
    esac

    shift
done


case $TOOLCHAIN in
    mingw-builds )
        GCC=gcc
        WINDRES=windres
        DLLTOOL=dlltool
        case $ARCH in
            i?86 )
                GCCFLAGS=-m32
                WINDRESFLAGS=--target=pe-i386
                DLLTOOLFLAGS="-m i386 -f --32"
                ;;

            x86_64 )
                GCCFLAGS=-m64
                WINDRESFLAGS=--target=pe-x86-64
                DLLTOOLFLAGS="-m i386:x86-64 -f --64"
                ;;

            * )
                echo "Unsupported architecture $ARCH."
                exit 1
                ;;
        esac
        ;;

    mingw-w64 | auto )
        # mingw-w64 packages from various people as well as the automatic builds
        # often very differ in whether gcc and/or binutils are prefixed with
        # the target triple or not.
        case $ARCH in
            i?86 )
                if which i686-w64-mingw32-gcc 2>> /dev/null; then
                    GCC=i686-w64-mingw32-gcc
                else
                    GCC=gcc
                fi
                if which i686-w64-mingw32-windres 2>> /dev/null; then
                    WINDRES=i686-w64-mingw32-windres
                    DLLTOOL=i686-w64-mingw32-dlltool
                else
                    WINDRES=windres
                    DLLTOOL=dlltool
                fi
                GCCFLAGS=-m32
                WINDRESFLAGS=--target=pe-i386
                DLLTOOLFLAGS="-m i386 -f --32"
                ;;

            x86_64 )
                if which x86_64-w64-mingw32-gcc >> /dev/null 2>&1; then
                    GCC=x86_64-w64-mingw32-gcc
                else
                    GCC=gcc
                fi
                if which x86_64-w64-mingw32-windres >> /dev/null 2>&1; then
                    WINDRES=x86_64-w64-mingw32-windres
                    DLLTOOL=x86_64-w64-mingw32-dlltool
                else
                    WINDRES=windres
                    DLLTOOL=dlltool
                fi
                GCCFLAGS=-m64
                WINDRESFLAGS=--target=pe-x86-64
                DLLTOOLFLAGS="-m i386:x86-64 -f --64"
                ;;

            * )
                echo "Unsupported architecture $ARCH."
                exit 1
                ;;
        esac
        ;;

    * )
        echo "Unsupported toolchain $TOOLCHAIN."
        exit 1
        ;;
esac


if [ "$MAKETARGETS" = "" ]; then
    MAKETARGETS=all
fi


make GCC="$GCC" CFLAGS="$GCCFLAGS" LDFLAGS="$GCCFLAGS" \
     WINDRES="$WINDRES $WINDRESFLAGS" DLLTOOL="$DLLTOOL $DLLTOOLFLAGS" \
     DEBUG="$DEBUG" $MAKETARGETS
