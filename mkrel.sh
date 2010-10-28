#!/bin/sh
#
# Helper script to make release packages:
#  - mCtrl-$VERSION-src.zip
#  - mCtrl-$VERSION-x86.zip (if i686-w64-mingw32-gcc or gcc is found in $PATH)
#  - mCtrl-$VERSION-x86_64.zip (if x86_64-w64-mingw32-gcc is found in $PATH)
#
# All packages are put in current directory (and overwritten if exist already).
# 7z.exe must be in $PATH in order to work properly.


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

# Setup version variables from src/version.h:
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

# Prepare build environment in the $TMP directory:
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
find $TMP/mCtrl-$VERSION -type f -name \*.o -exec rm {} \;
find $TMP/mCtrl-$VERSION -depth -type d -name .git -o -name .gitignore -exec rm -rf {} \;
echo "Done."

# Make mCtrl-$VERSION-src.zip:
echo -n "Making mCtrl-$VERSION-src.zip... "
7z a mCtrl-$VERSION-src.zip $TMP/mCtrl-$VERSION >> /dev/null
mv $TMP/mCtrl-$VERSION $TMP/mCtrl-$VERSION-src
echo "Done."

# Make mCtrl-$VERSION-x86.zip:
echo -n "Making mCtrl-$VERSION-x86.zip... "
if test `which i686-w64-mingw32-gcc`; then
    (cd $TMP/mCtrl-$VERSION-src && make distclean >> /dev/null)
    (cd $TMP/mCtrl-$VERSION-src && doxygen >> /dev/null 2>&1)
    i686-w64-mingw32-gcc -v > $CWD/build-x86.log 2>&1
    echo >> $CWD/build-x86.log
    (cd $TMP/mCtrl-$VERSION-src && make PREFIX=i686-w64-mingw32- all examples >> $CWD/build-x86.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed. See build-x86.log."
        exit 1
    fi
    mkdir $TMP/mCtrl-$VERSION
    cp -rf $TMP/mCtrl-$VERSION-src/bin $TMP/mCtrl-$VERSION/bin
    cp -rf $TMP/mCtrl-$VERSION-src/doc $TMP/mCtrl-$VERSION/doc
    cp -rf $TMP/mCtrl-$VERSION-src/examples $TMP/mCtrl-$VERSION/examples
    cp -rf $TMP/mCtrl-$VERSION-src/include $TMP/mCtrl-$VERSION/include
    cp -rf $TMP/mCtrl-$VERSION-src/lib $TMP/mCtrl-$VERSION/lib
    cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/COPYING* $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
    7z a mCtrl-$VERSION-x86.zip $TMP/mCtrl-$VERSION >> /dev/null
    echo "Done."
else 
    echo "Skipping (i686-w64-mingw32-gcc not found)."
fi

# Make mCtrl-$VERSION-x86_64.zip:
echo -n "Making mCtrl-$VERSION-x86_64.zip... "
if test `which x86_64-w64-mingw32-gcc`; then
    (cd $TMP/mCtrl-$VERSION-src && make distclean >> /dev/null)
    (cd $TMP/mCtrl-$VERSION-src && doxygen >> /dev/null 2>&1)
    x86_64-w64-mingw32-gcc -v > $CWD/build-x86_64.log 2>&1
    echo >> $CWD/build-x86_64.log
    (cd $TMP/mCtrl-$VERSION-src && make PREFIX=x86_64-w64-mingw32- all examples >> $CWD/build-x86_64.log 2>&1)
    if [ $? -ne 0 ]; then
        echo "Failed. See build-x86_64.log."
        exit 1
    fi
    rm -rf $TMP/mCtrl-$VERSION
    mkdir $TMP/mCtrl-$VERSION
    cp -rf $TMP/mCtrl-$VERSION-src/bin $TMP/mCtrl-$VERSION/bin
    cp -rf $TMP/mCtrl-$VERSION-src/doc $TMP/mCtrl-$VERSION/doc
    cp -rf $TMP/mCtrl-$VERSION-src/examples $TMP/mCtrl-$VERSION/examples
    cp -rf $TMP/mCtrl-$VERSION-src/include $TMP/mCtrl-$VERSION/include
    cp -rf $TMP/mCtrl-$VERSION-src/lib $TMP/mCtrl-$VERSION/lib
    cp $TMP/mCtrl-$VERSION-src/AUTHORS $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/COPYING* $TMP/mCtrl-$VERSION/
    cp $TMP/mCtrl-$VERSION-src/README $TMP/mCtrl-$VERSION/
    7z a mCtrl-$VERSION-x86_64.zip $TMP/mCtrl-$VERSION >> /dev/null
    echo "Done."
else
    echo "Skipping (x86_64-w64-mingw32-gcc not found)."
fi

# Clean-up all our stuff in $TMP:
echo -n "Cleaning build environment in $TMP... "
rm -rf $TMP/mCtrl-$VERSION
rm -rf $TMP/mCtrl-$VERSION-src
rm -rf $TMP/mCtrl-$VERSION-x86
rm -rf $TMP/mCtrl-$VERSION-x86_64
echo "Done."
