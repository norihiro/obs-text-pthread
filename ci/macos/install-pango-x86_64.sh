#! /bin/sh

set -ex

brew install pkg-config

brew install pango cairo libpng
cp /usr/local/opt/libpng/LICENSE data/LICENSE-libpng
cp /usr/local/opt/pango/COPYING data/COPYING-pango
cp /usr/local/opt/cairo/COPYING data/COPYING-cairo
