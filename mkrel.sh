#!/bin/sh
#
# Helper script to make release packages:
#  - mCtrl-$VERSION-src.zip
#  - mCtrl-$VERSION-doc.zip
#  - mCtrl-$VERSION-x86.zip (if i686-w64-mingw32-gcc or gcc is found in $PATH)
#  - mCtrl-$VERSION-x86_64.zip (if x86_64-w64-mingw32-gcc is found in $PATH)
#
# All packages are put in current directory (and overwritten if exist already).


CWD=`pwd`

# Check we run this script from the right diretory:
if [ ! -x $CWD/mkrel.sh ]; then
    echo You have to run mkrel.sh script from the main mCtrl directory.
    exit 1
fi

# We will do all the real work in $TMP directory:
if [ x$TMP = x ]; then
    TMP=/tmp
fi
if [ ! -d $TMP ]; then
    echo Directory '$TMP' does not exist.
    exit 1
fi


##################
# Detect version #
##################

echo -n "Detecting mCtrl version... "
VERSION_MAJOR=`grep VERSION_MAJOR src/version.h | head -1 | sed "s/\#def.*MAJOR\ *//"`
VERSION_MINOR=`grep VERSION_MINOR src/version.h | head -1 | sed "s/\#def.*MINOR\ *//"`
VERSION_RELEASE=`grep VERSION_RELEASE src/version.h | head -1 | sed "s/\#def.*RELEASE\ *//"`
VERSION=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_RELEASE
if [ x$VERSION = x ]; then
    echo "Failed."
    exit 1
fi
echo "$VERSION"


#####################
# Detect a zip tool #
#####################

# -mx9

echo -n "Detecting zip archiver... "
if test `which 7za 2>> /dev/null`; then
    MKZIP="7za a -r -mx9"
elif test `which 7z 2>> /dev/null`; then
    MKZIP="7z a -r -mx9"
elif test `which zip 2>> /dev/null`; then
    MKZIP="zip -r"
else
    echo "Failed: no one found in PATH."
    exit 1
fi
echo $MKZIP


##################
# Detect lib.exe #
##################

echo -n "Detecting lib.exe... "
if test `which lib.exe 2>> /dev/null`; then
    LIBEXE="lib.exe"
    echo "$LIBEXE"
elif [ -x "/c/Program Files/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe" ]; then 
    LIBEXE="/c/Program Files/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe"
    echo "$LIBEXE"
elif [ -x "/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe" ]; then
    LIBEXE="/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/amd64/lib.exe"
    echo "$LIBEXE"
else
    echo "not found."
fi


#############################
# Prepare build environment #
#############################

echo -n "Preparing build environment in $TMP... "
rm -f mCtrl-$VERSION-*.zip
rm -rf $TMP/mCtrl-$VERSION
rm -rf $TMP/mCtrl-$VERSION-src
rm -rf $TMP/mCtrl-$VERSION-x86
rm -rf $TMP/mCtrl-$VERSION-x86_64
cp -rf . $TMP/mCtrl-$VERSION
(cd $TMP/mCtrl-$VERSION  &&  rm -rf doc/*)
(cd $TMP/mCtrl-$VERSION  &&  rm -f mCtrl*.zip)
(cd $TMP/mCtrl-$VERSION  &&  rm -f build*.log)
(cd $TMP/mCtrl-$VERSION  &&  make distclean >> /dev/null)
find $TMP/mCtrl-$VERSION/obj -type f -name \*.o -exec rm {} \;
find $TMP/mCtrl-$VERSION/obj -type f -name \*.def -exec rm {} \;
(cd $TMP/mCtrl-$VERSION  &&  rm -rf .git)
find $TMP/mCtrl-$VERSION -type f -name .gitignore -exec rm -rf {} \;
echo "Done."


###############################
# Make mCtrl-$VERSION-src.zip #
###############################

echo -n "Pack source package... "
$MKZIP mCtrl-$VERSION-src.zip $TMP/mCtrl-$VERSION >> /dev/null
if [ $? -ne 0 ]; then
    echo "Failed."
    exit 1
fi
echo "Done."

mv $TMP/mCtrl-$VERSION $TMP/mCtrl-$VERSION-src


###############################
# Make mCtrl-$VERSION-doc.zip #
###############################

echo -n "Generating doc package... "
if test `which doxygen 2>> /dev/null`; then
    (cd $TMP/mCtrl-$VERSION-src && doxygen > $CWD/build-doc.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: see build-doc.log."
        exit 1
    fi
    HAVE_DOC=1
    echo "Done."
else
    echo "Skipped: doxygen not found in PATH."
fi
if [ x$HAVE_DOC != x ]; then
    echo -n "Pack doc package... "
    mkdir $TMP/mCtrl-$VERSION
    cp -rf $TMP/mCtrl-$VERSION-src/doc $TMP/mCtrl-$VERSION/doc
    cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/COPYING* $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
    $MKZIP mCtrl-$VERSION-doc.zip $TMP/mCtrl-$VERSION >> /dev/null
    if [ $? -ne 0 ]; then
        echo "Failed."
        exit 1
    fi
    echo "Done."
fi


###############################
# Make mCtrl-$VERSION-x86.zip #
###############################

echo -n "Building 32-bit package... "
if test `which i686-w64-mingw32-gcc 2>> /dev/null`; then
    (cd $TMP/mCtrl-$VERSION-src && make distclean >> /dev/null)
    i686-w64-mingw32-gcc -v > $CWD/build-x86.log 2>&1
    echo >> $CWD/build-x86.log
    (cd $TMP/mCtrl-$VERSION-src && make DEBUG=0 PREFIX=i686-w64-mingw32- all examples >> $CWD/build-x86.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: see build-x86.log."
        exit 1
    fi
    HAVE_X86=1
    echo "Done."
else 
    echo "Skipped: i686-w64-mingw32-gcc not not found in PATH."
fi
if [ x$HAVE_X86 != x ]; then
    echo -n "Create 32-bit MSVC import lib... "
    if [ -n "$LIBEXE" ]; then
        (cd $TMP/mCtrl-$VERSION-src && "$LIBEXE" /machine:x86 /def:obj/mCtrl.def /out:lib/mCtrl.lib) >> /dev/null
        echo "Done."
    else
        echo "Skipped: missing lib.exe"
    fi
    
    echo -n "Pack 32-bit package... "
    rm -rf $TMP/mCtrl-$VERSION
    mkdir $TMP/mCtrl-$VERSION
    cp -rf $TMP/mCtrl-$VERSION-src/bin $TMP/mCtrl-$VERSION/bin
    cp -rf $TMP/mCtrl-$VERSION-src/include $TMP/mCtrl-$VERSION/include
    cp -rf $TMP/mCtrl-$VERSION-src/lib $TMP/mCtrl-$VERSION/lib
    cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/COPYING* $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
    $MKZIP mCtrl-$VERSION-x86.zip $TMP/mCtrl-$VERSION >> /dev/null
    if [ $? -ne 0 ]; then
        echo "Failed."
        exit 1
    fi
    echo "Done."
fi


##################################
# Make mCtrl-$VERSION-x86_64.zip #
##################################

echo -n "Building 64-bit package... "
if test `which x86_64-w64-mingw32-gcc 2>> /dev/null`; then
    (cd $TMP/mCtrl-$VERSION-src && make distclean >> /dev/null)
    x86_64-w64-mingw32-gcc -v > $CWD/build-x86_64.log 2>&1
    echo >> $CWD/build-x86_64.log
    (cd $TMP/mCtrl-$VERSION-src && make DEBUG=0 PREFIX=x86_64-w64-mingw32- all examples >> $CWD/build-x86_64.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed: see build-x86_64.log."
        exit 1
    fi
    HAVE_X64=1
    echo "Done."
else
    echo "Skipped: x86_64-w64-mingw32-gcc not found in PATH."
fi
if [ x$HAVE_X64 != x ]; then
    echo -n "Create 64-bit MSVC import lib... "
    if [ -n "$LIBEXE" ]; then
        (cd $TMP/mCtrl-$VERSION-src && "$LIBEXE" /machine:x64 /def:obj/mCtrl.def /out:lib/mCtrl.lib) >> /dev/null
        echo "Done."
    else
        echo "Skipped: missing lib.exe"
    fi
    
    rm -rf $TMP/mCtrl-$VERSION
    mkdir $TMP/mCtrl-$VERSION
    cp -rf $TMP/mCtrl-$VERSION-src/bin $TMP/mCtrl-$VERSION/bin
    cp -rf $TMP/mCtrl-$VERSION-src/include $TMP/mCtrl-$VERSION/include
    cp -rf $TMP/mCtrl-$VERSION-src/lib $TMP/mCtrl-$VERSION/lib
    cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/COPYING* $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
    7z a mCtrl-$VERSION-x86_64.zip $TMP/mCtrl-$VERSION >> /dev/null
    if [ $? -ne 0 ]; then
        echo "Failed."
        exit 1
    fi
    echo "Done."
fi


############
# Clean-up #
############

echo -n "Cleaning... "
rm -rf $TMP/mCtrl-$VERSION
rm -rf $TMP/mCtrl-$VERSION-src
rm -rf $TMP/mCtrl-$VERSION-doc
rm -rf $TMP/mCtrl-$VERSION-x86
rm -rf $TMP/mCtrl-$VERSION-x86_64
echo "Done."
