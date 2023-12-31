#! /bin/sh

set -ex

brew install pkg-config
removes=(lzo libpng jpeg-turbo)
for pkg in "${removes[@]}"; do
	brew uninstall --ignore-dependencies $pkg || true
done

curl -L -o deps.tar.gz http://www.nagater.net/obs-studio/obs-text-pthread-brew-apple-deps-20231231.tar.gz
sha256sum -c <<-EOF
9a645fd3be53e30b6ab0b7834f1dfc2e444b943039f93ef9f9e5529e1d4d0c65  deps.tar.gz
EOF
(cd / && sudo tar xz) < deps.tar.gz
