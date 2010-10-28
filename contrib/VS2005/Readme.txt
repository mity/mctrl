
This directory contains solution files to build mCtrl with Microsoft Visual
Studio 2005:
    mCtrl.sln - solution file
    mCtrl.vcproj - project for mCtrl.dll 
    mCtrl.def - file listing symbols to be exported from mCtrl.dll
    

Disclaimer
==========

Please note that MS Visual Studio solution and project files are not maintained
properly. These are just added as a favor to those who might find it useful as 
a startpoint iof they really want to play with mCtrl in Visual Studio.


Usage
=====

[Step 1] Copy all three files to root of the mCtrl project (i.e. where Makefile
         lives.

[Step 2] Before building mCtrl.dll with it, you may need to check it is up to 
         date and reflects changes in mCtrl codebase, especialy you may need 
         to add some sources which might be added to the mCtrl codebase later
         since creation of the project file, and add new symbols into 
         mCtrl.def.

[Step 3] Open the solution and try to build. It is possible some 
         incompatibilities might arise but they should not be serious.


Further Notes
=============

* Note that there is only project file for the mCtrl.dll itself, it does not
  cover examples.

* As of 2010, January 20, there are about 40 warnings when built with 
  VisualStudio 2008, qwith both Release and Debug configurations. They should
  be ok. It can be expected that future versions of mCtrl will come with more
  warnings. This is expected as MSVC is not promary development platform.
  Feel free to provide patches fixing them.

* After the successful build I recommand you to check that the resulted 
  mCtrl.dll exports all functions as undecorated (e.g. that there 
  is symbol DllGetVersion rather then DllGetVersion@4). Otherwise you might
  expect problems with any code trying to use the DLL, especially via 
  LoadLibrary() and GetProcAddress(). If the resulted library has some symbols
  decorated, it's probably because the symbol is omited in mCtrl.def.

  

