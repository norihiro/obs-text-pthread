#! /bin/bash

export LANG=C
set -e

std_meson_args=(--buildtype=release --wrap-mode=nofallback)

# TODO: env CFLAGS="-arch arm64 -arch x86_64" does not work for cairo. Try lipo.

brew install meson ninja pkg-config

curl -o glib.tar.xz --location https://download.gnome.org/sources/glib/2.72/glib-2.72.3.tar.xz
mkdir glib
cd glib
tar -xJf ../glib.tar.xz --strip-components 1
meson setup build -Ddtrace=false "${std_meson_args[@]}"
meson compile -C build --verbose
cd -

# TODO: install lzo

curl -o cairo.tar.xz --location https://cairographics.org/releases/cairo-1.18.0.tar.xz
mkdir cairo
cd cairo
tar -xJf ../cairo.tar.xz --strip-components 1
meson setup build -Dglib=disabled "${std_meson_args[@]}"
meson compile -C build --verbose
cd -

curl -o pango.tar.xz --location https://download.gnome.org/sources/pango/1.50/pango-1.50.9.tar.xz
mkdir pango
cd pango
tar -xJf ../pango.tar.xz --strip-components 1

