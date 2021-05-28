#! /bin/bash

set -x
env # check PATH, etc.

curl http://repo.msys2.org/msys/x86_64/zstd-1.4.5-2-x86_64.pkg.tar.xz | tar CvxJf / -
curl http://repo.msys2.org/msys/x86_64/rsync-3.2.2-2-x86_64.pkg.tar.zst | tar Cxvf / - --zstd
curl http://repo.msys2.org/msys/x86_64/openbsd-netcat-1.206_1-1-x86_64.pkg.tar.xz | tar CxvJf / -
curl http://repo.msys2.org/msys/x86_64/libzstd-1.4.5-2-x86_64.pkg.tar.xz | tar CxvJf / -
curl http://repo.msys2.org/msys/x86_64/libxxhash-0.8.0-1-x86_64.pkg.tar.zst | tar Cvxf / - --zstd
curl http://repo.msys2.org/msys/x86_64/wget-1.21.1-2-x86_64.pkg.tar.zst | tar Cvxf / - --zstd
curl http://repo.msys2.org/msys/x86_64/libpcre2_8-10.36-1-x86_64.pkg.tar.zst | tar Cvxf / - --zstd
curl http://repo.msys2.org/msys/x86_64/libmetalink-0.1.3-3-x86_64.pkg.tar.zst | tar Cvxf / - --zstd
curl http://repo.msys2.org/msys/x86_64/libgpgme-1.15.1-2-x86_64.pkg.tar.zst | tar Cvxf / - --zstd

# -G 'Ninja'
# -G "Visual Studio 16 2019"
sed -i \
	-e "s;-G 'Ninja';-G 'Visual Studio 16 2019';g" \
	-e 's/ninja install$/cmake --config Release --build . ; cmake --config Release --install ./g' \
	./prepare.sh
. ./prepare.sh
