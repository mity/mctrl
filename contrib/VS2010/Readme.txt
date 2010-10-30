
This directory contains solution files to build mCtrl with MS Visual Studio.
The file were created in MS VisualStudio 2010 Express.


Disclaimer
==========

Please note that MS Visual Studio solution and project files are not maintained
properly as it is not te main development tool for the project. They are just
added as a favor to those who might find it useful as a startpoint iof they 
really want to play with mCtrl in Visual Studio.

If you are lucky and the files were updated recently, you should be able to
just build it anduse it. Otherwise you may need to update the project files,
e.g. to include some more recent source file into the solution etc.


Usage
=====

Just open the solution file (mCtrl.sln) where it resides in the project tree
and build it.


Further Notes
=============

* Note that there is only project file for the mCtrl.dll itself, it does not
  cover examples.

* There are usually some warnings when built with VisualStudio, because every
  compiler does not like different things. Hopefully they are not harmfull.

* After the successful build I recommand you to check that the resulted 
  mCtrl.dll exports all functions it should (you may need to update mCtrl.def
  if not) and that the symbols are not decorated (e.g. that there is symbol 
  DllGetVersion rather then DllGetVersion@4). Otherwise you might
  expect problems with any code trying to use the DLL, especially via 
  LoadLibrary() and GetProcAddress(). If the resulted library has some symbols
  decorated, it's probably because the symbol is omited in mCtrl.def.


