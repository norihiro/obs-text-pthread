@echo off
SETLOCAL EnableDelayedExpansion

set PATH=C:\Program Files\Meson;%PATH%

mkdir %PangoPath%
cd /D %PangoPath%

echo downloading pango...
curl -L -o pango.tar.xz https://download.gnome.org/sources/pango/1.48/pango-1.48.0.tar.xz
7z x pango.tar.xz -so | 7z x -si -ttar -aoa > nul
rename pango-1.48.0 pango

echo configuring pango...
cd /D %PangoPath%\pango
mkdir build64
cd build64
meson ..
