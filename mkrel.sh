#!/bin/sh
#
# Helper script to make release packages:
#  - mCtrl-$VERSION-src.zip
#  - mCtrl-$VERSION-bin.zip
#
# All packages are put in current directory (and overwritten if exist already).


# We want to control what's going to stdout/err.
exec 3>&1
exec 1>/dev/null 2>&1

CWD=`pwd`
PRJ="$CWD"

# Check we run this script from the right diretory:
if [ ! -x $PRJ/mkrel.sh ]; then
    echo "You have to run mkrel.sh script from the main mCtrl directory." >&3
    exit 1
fi

# We will do all the real work in $TMP directory:
if [ x$TMP = x ]; then
    TMP=/tmp
fi
if [ ! -d $TMP ]; then
    echo "Directory '$TMP' does not exist." >&3
    exit 1
fi


##################
# Detect version #
##################

echo -n "Detecting mCtrl version... " >&3
VERSION_MAJOR=`grep VERSION_MAJOR src/version.h | head -1 | sed "s/\#def.*MAJOR\ *//"`
VERSION_MINOR=`grep VERSION_MINOR src/version.h | head -1 | sed "s/\#def.*MINOR\ *//"`
VERSION_RELEASE=`grep VERSION_RELEASE src/version.h | head -1 | sed "s/\#def.*RELEASE\ *//"`
VERSION=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_RELEASE
if [ x$VERSION = x ]; then
    echo "Failed." >&3
    exit 1
fi
echo "$VERSION" >&3


#####################
# Detect a zip tool #
#####################

echo -n "Detecting zip archiver... " >&3
if which 7za; then
    MKZIP="7za a -r -mx9"
elif which 7z; then
    MKZIP="7z a -r -mx9"
elif which zip; then
    MKZIP="zip -r"
else
    echo "Not found." >&3
    exit 1
fi
echo "$MKZIP" >&3


##################
# Detect lib.exe #
##################

# 1. Try lib.exe in $PATH
# 2. Try default install place(s) for MSVC 10.0
# 3. Try default install place(s) for any MSVC version

echo -n "Detecting lib.exe... " >&3
if which lib.exe ; then
    LIBEXE="lib.exe"
elif [ -x "/c/Program Files/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe" ]; then
    LIBEXE="/c/Program Files/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe"
elif [ -x "/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe" ]; then
    LIBEXE="/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe"
else
    LIBEXE=`ls /c/Program\ Files*/Microsoft\ Visual\ Studio*/VC/bin/amd64/lib.exe | head -1`
fi
if [ "x$LIBEXE" = x ]; then
    echo "Not found: MSVC import libs won't be generated" >&3
else
    echo "$LIBEXE" >&3
fi


#########################
# Build 64-bit binaries #
#########################

rm -rf $TMP/mCtrl-$VERSION
git clone . $TMP/mCtrl-$VERSION

echo -n "Building 64-bit binaries... " >&3
if which x86_64-w64-mingw32-gcc; then
    x86_64-w64-mingw32-gcc -v > $CWD/build-x86_64.log 2>&1
    echo >> $CWD/build-x86_64.log
    (cd $TMP/mCtrl-$VERSION && make DEBUG=0 PREFIX=x86_64-w64-mingw32- all examples >> $CWD/build-x86_64.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: see build-x86_64.log."
        exit 1
    fi
    echo "Done." >&3
    HAVE_X86_64=yes
else
    echo "Skipped: x86_64-w64-mingw32-gcc not found in PATH." >&3
fi

if [ x$HAVE_X86_64 != x ]; then
    echo -n "Create 64-bit MSVC import lib... " >&3
    if [ "x$LIBEXE" != x ]; then
        (cd $TMP/mCtrl-$VERSION && "$LIBEXE" /machine:x64 /def:obj/mCtrl.def /out:lib/mCtrl.lib >> $CWD/build-x86.log 2>&1)
        echo "Done." >&3
    else
        echo "Skipped: lib.exe not found." >&3
    fi

    mv $TMP/mCtrl-$VERSION/bin $TMP/mCtrl-$VERSION/bin64
    mv $TMP/mCtrl-$VERSION/lib $TMP/mCtrl-$VERSION/lib64
    (cd $TMP/mCtrl-$VERSION && make distclean)
    mkdir $TMP/mCtrl-$VERSION/bin
    mkdir $TMP/mCtrl-$VERSION/lib
fi


#########################
# Build 32-bit binaries #
#########################

echo -n "Building 32-bit binaries... " >&3
if which i686-w64-mingw32-gcc; then
    i686-w64-mingw32-gcc -v > $CWD/build-x86.log 2>&1
    echo >> $CWD/build-x86.log
    (cd $TMP/mCtrl-$VERSION && make DEBUG=0 PREFIX=i686-w64-mingw32- all examples >> $CWD/build-x86.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: see build-x86.log."
        exit 1
    fi
    echo "Done." >&3
    HAVE_X86=yes
else
    echo "Skipped: i686-w64-mingw32-gcc not found in PATH." >&3
fi

if [ x$HAVE_X86 != x ]; then
    echo -n "Create 32-bit MSVC import lib... " >&3
    if [ "x$LIBEXE" != x ]; then
        (cd $TMP/mCtrl-$VERSION && "$LIBEXE" /machine:x86 /def:obj/mCtrl.def /out:lib/mCtrl.lib >> $CWD/build-x86.log 2>&1)
        echo "Done." >&3
    else
        echo "Skipped: lib.exe not found." >&3
    fi
fi


##########################
# Generate documentation #
##########################

echo -n "Generate documentation... " >&3
if `which doxygen`; then
    (cd $TMP/mCtrl-$VERSION && ( cat Doxyfile ; echo "PROJECT_NUMBER=$VERSION" ) | doxygen - > $CWD/build-doc.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: See build-doc.log."
        exit 1
    fi
    HAVE_DOC=yes
    echo "Done." >&3
else
    echo "Skipped: doxygen not found in PATH." >&3
fi


###############################
# Make mCtrl-$VERSION-src.zip #
###############################

echo -n "Packing source package... " >&3
git archive --prefix=mCtrl-$VERSION/ --output=mCtrl-$VERSION-src.zip HEAD
if [ $? -ne 0 ]; then
    echo "Failed." >&3
    exit 1
fi
echo "Done." >&3


###############################
# Make mCtrl-$VERSION-bin.zip #
###############################

echo -n "Packing binary package... " >&3
rm -rf $TMP/mCtrl-$VERSION-src
mv $TMP/mCtrl-$VERSION $TMP/mCtrl-$VERSION-src
mkdir $TMP/mCtrl-$VERSION
if [ x$HAVE_X86 != x ]; then
    cp -r $TMP/mCtrl-$VERSION-src/bin $TMP/mCtrl-$VERSION/
    cp -r $TMP/mCtrl-$VERSION-src/lib $TMP/mCtrl-$VERSION/
fi
if [ x$HAVE_X86_64 != x ]; then
    cp -r $TMP/mCtrl-$VERSION-src/bin64 $TMP/mCtrl-$VERSION/
    cp -r $TMP/mCtrl-$VERSION-src/lib64 $TMP/mCtrl-$VERSION/
fi
if [ x$HAVE_DOC != x ]; then
    cp -r $TMP/mCtrl-$VERSION-src/doc $TMP/mCtrl-$VERSION/
fi
cp -r $TMP/mCtrl-$VERSION-src/include $TMP/mCtrl-$VERSION/
cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
cp $TMP/mCtrl-$VERSION-src/COPYING $TMP/mCtrl-$VERSION/
cp $TMP/mCtrl-$VERSION-src/COPYING.lib $TMP/mCtrl-$VERSION/
cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
find $TMP/mCtrl-$VERSION -name .git -exec rm -rf {} \;
find $TMP/mCtrl-$VERSION -name .gitignore -exec rm {} \;
$MKZIP mCtrl-$VERSION-bin.zip $TMP/mCtrl-$VERSION
if [ $? -ne 0 ]; then
    echo "Failed." >&3
    exit 1
fi
rm -rf $TMP/mCtrl-$VERSION
echo "Done." >&3


############
# Clean-up #
############

echo -n "Cleaning... "
rm -rf $TMP/mCtrl-$VERSION
rm -rf $TMP/mCtrl-$VERSION-tmp
echo "Done."

