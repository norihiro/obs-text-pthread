@echo off
SETLOCAL EnableDelayedExpansion

set PATH=C:\Program Files\Meson;%PATH%

mkdir %PangoPath%
cd /D %PangoPath%

echo downloading pango...
curl -o pango.tar.xz http://ftp.gnome.org/pub/GNOME/sources/pango/1.46/pango-1.46.0.tar.xz
7z x pango.tar.xz -so | 7z x -si -ttar -aoa > nul
rename pango-1.46.0 pango

echo configuring pango...
cd /D %PangoPath%\pango
mkdir build64
cd build64
meson ..
