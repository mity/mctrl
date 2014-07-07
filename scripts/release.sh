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

# Change dir to project root.
cd `dirname "$0"`/..

CWD=`pwd`
PRJ="$CWD"

# Sanity check we run this script from the right directory:
if [ ! -x $PRJ/scripts/release.sh ]; then
    echo "There is some path mismatch." >&3
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
VERSION_MAJOR=`grep MCTRL_MAJOR_VERSION CMakeLists.txt | head -1 | sed 's/[^0-9]//g'`
VERSION_MINOR=`grep MCTRL_MINOR_VERSION CMakeLists.txt | head -1 | sed 's/[^0-9]//g'`
VERSION_PATCH=`grep MCTRL_PATCH_VERSION CMakeLists.txt | head -1 | sed 's/[^0-9]//g'`
VERSION=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH
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


###########################
# Detect a build back-end #
###########################

echo -n "Detected build back-end... " >&3
if which ninja; then
    CMAKE_GENERATOR="Ninja"
    BUILD="ninja -v"
elif which make; then
    CMAKE_GENERATOR="MSYS Makefiles"
    BUILD="make VERBOSE=1"
else
    echo "Not found." >&3
    exit 1
fi
echo "$BUILD" >&3


#########################
# Build 64-bit binaries #
#########################

rm -rf $TMP/mCtrl-$VERSION
git clone . $TMP/mCtrl-$VERSION

echo -n "Building 64-bit binaries... " >&3
mkdir -p "$TMP/mCtrl-$VERSION/build64"

(cd "$TMP/mCtrl-$VERSION/build64" && \
 cmake -D CMAKE_BUILD_TYPE=Release \
       -D CMAKE_C_FLAGS="-m64" \
       -D CMAKE_EXE_LINKER_FLAGS="-m64" \
       -D CMAKE_SHARED_LINKER_FLAGS="-m64" \
       -D CMAKE_RC_FLAGS="--target=pe-x86-64" \
       -D DLLTOOL_FLAGS="-m;i386:x86-64;-f;--64" \
       -G "$CMAKE_GENERATOR" .. && \
 $BUILD > $PRJ/build-x86_64.log 2>&1)
if [ $? -eq 0 ]; then
    HAVE_X86_64=yes
    echo "Done." >&3
else
    echo "Failed. See build-x86_64.log." >&3
fi


#########################
# Build 32-bit binaries #
#########################

echo -n "Building 32-bit binaries... " >&3
mkdir -p "$TMP/mCtrl-$VERSION/build32"

(cd "$TMP/mCtrl-$VERSION/build32" && \
 cmake -D CMAKE_BUILD_TYPE=Release \
       -D CMAKE_C_FLAGS="-m32 -march=i586 -mtune=core2" \
       -D CMAKE_EXE_LINKER_FLAGS="-m32 -march=i586 -mtune=core2" \
       -D CMAKE_SHARED_LINKER_FLAGS="-m32 -march=i586 -mtune=core2" \
       -D CMAKE_RC_FLAGS="--target=pe-i386" \
       -D DLLTOOL_FLAGS="-m;i386;-f;--32" \
       -G "$CMAKE_GENERATOR" .. && \
 $BUILD > $PRJ/build-x86.log 2>&1)
if [ $? -eq 0 ]; then
    HAVE_X86=yes
    echo "Done." >&3
else
    echo "Failed. See build-x86.log." >&3
fi


##########################
# Generate documentation #
##########################

echo -n "Generate documentation... " >&3
if `which doxygen`; then
    (cd $TMP/mCtrl-$VERSION && ( cat Doxyfile ; echo "PROJECT_NUMBER=$VERSION" ) | doxygen - > $PRJ/build-doc.log 2>&1)
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
# Make mCtrl-$VERSION-bin.zip #
###############################

echo -n "Packing binary package... " >&3
rm -rf $TMP/mCtrl-$VERSION-src
mv $TMP/mCtrl-$VERSION $TMP/mCtrl-$VERSION-src
mkdir $TMP/mCtrl-$VERSION
if [ x$HAVE_X86 != x ]; then
    mkdir -p $TMP/mCtrl-$VERSION/bin
    cp $TMP/mCtrl-$VERSION-src/build32/mCtrl.dll $TMP/mCtrl-$VERSION/bin/
    cp $TMP/mCtrl-$VERSION-src/build32/ex_*.exe $TMP/mCtrl-$VERSION/bin/
    mkdir -p $TMP/mCtrl-$VERSION/lib
    cp $TMP/mCtrl-$VERSION-src/build32/libmCtrl.dll.a $TMP/mCtrl-$VERSION/lib/libmCtrl.dll.a
    cp $TMP/mCtrl-$VERSION-src/build32/libmCtrl.dll.a $TMP/mCtrl-$VERSION/lib/mCtrl.lib
fi
if [ x$HAVE_X86_64 != x ]; then
    mkdir -p $TMP/mCtrl-$VERSION/bin64
    cp $TMP/mCtrl-$VERSION-src/build64/mCtrl.dll $TMP/mCtrl-$VERSION/bin64/
    cp $TMP/mCtrl-$VERSION-src/build64/ex_*.exe $TMP/mCtrl-$VERSION/bin64/
    mkdir -p $TMP/mCtrl-$VERSION/lib64
    cp $TMP/mCtrl-$VERSION-src/build64/libmCtrl.dll.a $TMP/mCtrl-$VERSION/lib64/libmCtrl.dll.a
    cp $TMP/mCtrl-$VERSION-src/build64/libmCtrl.dll.a $TMP/mCtrl-$VERSION/lib64/mCtrl.lib
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

