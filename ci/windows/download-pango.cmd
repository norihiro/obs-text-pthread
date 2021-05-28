@echo on

git clone https://github.com/kkartaltepe/pango-win32-build.git %PangoDir%
python3 -m pip install meson
python3 -m pip install ninja
