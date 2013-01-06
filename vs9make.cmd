@echo off

:: acx1 - Application Console Interface - ver. 1
:: 
:: Build script for Windows using Visual Studio 9
::
:: Changelog:
::	* 2013/01/06 Costin Ionescu: initial release

set VS90=%VS90COMNTOOLS%..\..
call %VS90%\vc\vcvarsall.bat x86
if not exist out\mswin_ia32 mkdir out\mswin_ia32
cl.exe /nologo /Ox /LD /Feout\mswin_ia32\acx1.dll /Foout\mswin_ia32\ /Iinclude /DACX1_DL_BUILD src\mswin.c src\common.c
cl.exe /nologo /Ox /Feout\mswin_ia32\test.exe /Foout\mswin_ia32\ /Iinclude src\test.c out\mswin_ia32\acx1.lib
cl.exe /nologo /Ox /Feout\mswin_ia32\linesel.exe /Foout\mswin_ia32\ /Iinclude src\linesel.c out\mswin_ia32\acx1.lib c41.lib

call %VS90%\vc\vcvarsall.bat x86_amd64
if not exist out\mswin_amd64 mkdir out\mswin_amd64
cl.exe /nologo /Ox /LD /Feout\mswin_amd64\acx1.dll /Foout\mswin_amd64\ /Iinclude /DACX1_DL_BUILD src\mswin.c src\common.c
cl.exe /nologo /Ox /Feout\mswin_amd64\test.exe /Foout\mswin_amd64\ /Iinclude src\test.c out\mswin_amd64\acx1.lib
