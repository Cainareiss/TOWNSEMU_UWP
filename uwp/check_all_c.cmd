@echo off
setlocal enabledelayedexpansion
set ROOT=E:\claude-work\townsuwp\repo\uwp
set TEMP=E:\claude-work\tmp
set TMP=E:\claude-work\tmp
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 uwp 10.0.26100.0 >nul || exit /b 1
if exist E:\claude-work\tmp\objall rmdir /s /q E:\claude-work\tmp\objall
mkdir E:\claude-work\tmp\objall
set FILES=
for /f "usebackq delims=" %%f in ("E:\claude-work\tmp\fe_list.txt") do set FILES=!FILES! %ROOT%\%%f
cl /nologo /c /TC /std:c17 /utf-8 /W1 /MP /DSWANUI_XBOX=1 /DTSUGARU_UWP=1 /D_UWP /DWINAPI_FAMILY=WINAPI_FAMILY_APP /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /DUNICODE /D_UNICODE /FI%ROOT%\swanui\compat\swanui_compat.h /I%ROOT%\swanui\compat /I%ROOT%\swanui\src /I%ROOT%\swanui\third_party\stb /I%ROOT%\swanui\third_party\swanstation\dep\libretro-common\include /I%ROOT%\swanui\third_party\rcheevos\include /Fo:E:\claude-work\tmp\objall\ %FILES% %ROOT%\swanui\src\vendor.c
exit /b %errorlevel%
