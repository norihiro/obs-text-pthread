@echo off
SETLOCAL EnableDelayedExpansion

echo downloading and installing meson.msi...
curl -o meson.msi -kLO https://github.com/mesonbuild/meson/releases/download/0.55.0/meson-0.55.0-64.msi -f --retry 5
msiexec /i meson.msi /passive /qn /lv meson-install.log
type meson-install.log
