#! /bin/bash

curl http://repo.msys2.org/msys/x86_64/zstd-1.4.5-2-x86_64.pkg.tar.xz | tar CvxJf / -
curl http://repo.msys2.org/msys/x86_64/rsync-3.2.2-2-x86_64.pkg.tar.zst | tar Cxvf / - --zstd
curl http://repo.msys2.org/msys/x86_64/openbsd-netcat-1.206_1-1-x86_64.pkg.tar.xz | tar CxvJf / -
curl http://repo.msys2.org/msys/x86_64/libzstd-1.4.5-2-x86_64.pkg.tar.xz | tar CxvJf / -
curl http://repo.msys2.org/msys/x86_64/libxxhash-0.8.0-1-x86_64.pkg.tar.zst | tar Cvxf / - --zstd

./prepare.sh
