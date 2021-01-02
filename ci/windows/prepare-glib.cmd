@echo off
SETLOCAL EnableDelayedExpansion

set PATH=C:\Program Files\Meson;%PATH%

mkdir %PangoPath%
cd /D %PangoPath%

echo downloading glib...
curl -L -o glib.tar.xz https://download.gnome.org/sources/glib/2.67/glib-2.67.1.tar.xz
7z x glib.tar.xz -so | 7z x -si -ttar -aoa > nul
rename glib-2.67.1 glib

echo downloading libffi...
curl -o libffi.tar.gz ftp://sourceware.org/pub/libffi/libffi-3.3.tar.gz
7z x libffi.tar.gz -so | 7z x -si -ttar -aoa > nul
rename libffi-3.3 libffi

echo checking and setup vs
vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > VSinstallationPath.txt
set /p VSinstallationPath= < VSinstallationPath.txt
call "%VSinstallationPath%\VC\Auxiliary\Build\vcvarsall.bat" amd64

echo checking path...
path

echo configuring glib...
cd /D %PangoPath%\glib
patch -p0 < %GitSource%\ci\windows\glib-meson.patch
patch -p1 < %GitSource%\ci\windows\glib-nolibintl.patch
set INCLUDE=%INCLUDE%;%DepsBasePath%\win64\include
set LIB=%LIB%;%DepsBasePath%\win64\bin\
echo INCLUDE=%INCLUDE%
echo LIB=%LIB%
meson setup build64 --backend vs2019 --default-library static

copy %PangoPath%\libffi\msvc_build\aarch64\aarch64_include\* build64\gobject\
copy %PangoPath%\libffi\src\aarch64\ffitarget.h build64\gobject\

echo dir build64
dir /S build64

echo compiling glib...
meson compile -C build64
