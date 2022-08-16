#! /bin/sh

set -ex

brew install pkg-config
removes=(lzo libpng jpeg-turbo)
for pkg in "${removes[@]}"; do
	brew uninstall --ignore-dependencies $pkg || true
done

b='http://www.nagater.net/obs-studio'
ff=(
	$b/012782bc02fa513bc2c205676d119d1873c1e839da92a8f5fa14a439f4dbb3d5--icu4c--70.1.arm64_big_sur.bottle.tar.gz
	$b/0b22b3a294ae9a0adeaa234fa8439696e2484f2494a1fbad722cc5fd2dc2f68e--lzo--2.10.arm64_big_sur.bottle.tar.gz
	$b/0f96e131aa631d433553a8ac492da24cdb18d9100257e7c9cc6c22062cb5b915--libxau--1.0.9.arm64_big_sur.bottle.tar.gz
	$b/1368560eaab08ce79b4aea96559d5ca40bb719dd5855bafb765ff319bb640689--cairo--1.16.0_5.arm64_big_sur.bottle.tar.gz
	$b/1f1627dde5114605c3eb317505fd68f936c9c4e1d768bea9623489ff94037dc0--util-linux--2.38_1.arm64_big_sur.bottle.tar.gz
	$b/1f9b5738a934578fa99c29009ad22bfb0c2bdde4a63ef30f9022f92809f44c1e--pygobject3--3.42.2.arm64_big_sur.bottle.tar.gz
	$b/20e31bc0c284afef2af5b203b13c98b859e905c5546a4bdd17bb4984ad9a5bf2--libpthread-stubs--0.4.arm64_big_sur.bottle.tar.gz
	$b/2d56e746f5a3121427f2a75bfc39bc22c0ff418e73dee2fea3bad7a8106455a6--libpng--1.6.37.arm64_big_sur.bottle.tar.gz
	$b/30daea65577a5ca01db7166e48390944b1277e7a75f7943bf0fd93af59641a8b--xz--5.2.5_1.arm64_big_sur.bottle.tar.gz
	$b/3e88b29f2cd6477579e19f8d26ece38881c58f316548d3a32d30850ebb1466f5--gettext--0.21.arm64_big_sur.bottle.tar.gz
	$b/3f80d366807a8af5fd4d817e8be35d7b4593136b64cc431652b59b4b9715bae7--libxcb--1.15.arm64_big_sur.bottle.tar.gz
	$b/487621be5b7dbbf9ba68062c651803b8434652b496a3492b7d004416993f0808--pango--1.50.8.arm64_big_sur.bottle.tar.gz
	$b/56b398b19d29e98258688d94584835c236e3a092b7f9a7b3137fc3787679588d--libxext--1.3.4.arm64_big_sur.bottle.tar.gz
	$b/5f9dcd2bdd37acd8a39aa26494aa4b69cfa5d789a4939aece1d9e9924845718f--readline--8.1.2.arm64_big_sur.bottle.tar.gz
	$b/721ba5eecc1bf642955757ff072f50ef96f86bb20a2045284db199b810c709a7--libffi--3.4.2.arm64_big_sur.bottle.tar.gz
	$b/7b9bace0fdfb378d4bd0f3dbcc2352667e7352f1c04bbe6dc594352b1eb5ec4f--xorgproto--2022.1.arm64_big_sur.bottle.tar.gz
	$b/8003141aa4f8fd28d11f6348a05cc82e188a3db4bce30baeac5fc9ed9cf19f9e--pcre--8.45.arm64_big_sur.bottle.tar.gz
	$b/8deff0ce8800f353a5c12dd9f65a4a21033ac06a9abdd1edf637015bddf070dd--libxdmcp--1.1.3.arm64_big_sur.bottle.tar.gz
	$b/a8b3ab5336902ba107565060130e47e930e1f4a1f50f3133a9e7a9d4363cc311--graphite2--1.3.14.arm64_big_sur.bottle.tar.gz
	$b/ae7ee98fe7a664f7cbce7e9c1603bcd07cad14c5c6c6a6b1a3a7d590e2a69a26--fribidi--1.0.12.arm64_big_sur.bottle.tar.gz
	$b/bb970f1da177ba009e42c6bf2a61471a1803a5ff6756698c4a80c42e5c96efef--mpdecimal--2.5.1.arm64_big_sur.bottle.tar.gz
	$b/bff6a9e9ebcec1855583a512ee4a3729718613e23ed94489cb08f07bb0898f34--pixman--0.40.0.arm64_big_sur.bottle.tar.gz
	$b/c3c85605f159c54dc7ffc01a0553fa307f00ba5089d461e0d9fc90b7b3f4b339--libx11--1.8.1.arm64_big_sur.bottle.tar.gz
	$b/c4170a1c9041eb5a0c9398dc43ac3aaf2d54b91fe265d41b93c5751e15721fa7--gdbm--1.23.arm64_big_sur.bottle.tar.gz
	$b/c4f7ef1bae054673c3f0dd5030615277258f799ba2e54e31b1e5d37e28ed169a--freetype--2.12.1.arm64_big_sur.bottle.tar.gz
	$b/cd7df9ce5f3349144db9c452017090a8c3dc59a58600b69d02aa9cf44875979a--sqlite--3.39.2.arm64_big_sur.bottle.tar.gz
	$b/cf087d9a9024093ae8946ea083c93655b3ef656eeda95f4bf4118b889c9ab636--harfbuzz--5.1.0.arm64_big_sur.bottle.tar.gz
	$b/d036c7c43786d6ac87079eaeb069d167c2b324a69673989dce66cd97ee7642f1--fontconfig--2.14.0.arm64_big_sur.bottle.tar.gz
	$b/dd87bc4cbe8c59fcc022fe6dbc6daf794aaa1b21a790c9b3e2235ff1c06b4035--openssl@1.1--1.1.1q.arm64_big_sur.bottle.tar.gz
	$b/e6702ef9c08750270f4f396d2c48d270744081e65060dcb1056093f200214d7e--libxrender--0.9.10.arm64_big_sur.bottle.tar.gz
	$b/ee5e1c87f0bf4b850158ab0b249ab5eae55a230bf191f5cfed19f40eeacffcbc--py3cairo--1.21.0_1.arm64_big_sur.bottle.tar.gz
	$b/fe639a17a206adf0f8c65f250bb71b6b7fb4cfaf50858f6abbabf7cd25d4ed5e--glib--2.72.3.arm64_big_sur.bottle.tar.gz
)

dest='/usr/local/opt/arm64/'
sudo mkdir -p $dest
sudo chown $USER:staff $dest
cd $dest
for f in ${ff[@]}; do
	curl -O $f
	b=$(basename "$f")
	tar --strip-components=2 -xzf $b

	case "$b" in
		*pango*)
			cp COPYING $OLDPWD/COPYING-pango
			;;
		*cairo*)
			cp COPYING $OLDPWD/COPYING-cairo
			;;
		*libpng*)
			cp LICENSE $OLDPWD/LICENSE-libpng
			;;
	esac
done

sha256sum -c <<-EOF
c3c22a25dd864a6494d2371bea6b8b9d5e49f8c401b2f6cda00f4c349f57e975  012782bc02fa513bc2c205676d119d1873c1e839da92a8f5fa14a439f4dbb3d5--icu4c--70.1.arm64_big_sur.bottle.tar.gz
76d0933f626d8a1645b559b1709396a2a6fd57dbd556d2f1f1848b5fddfcd327  0b22b3a294ae9a0adeaa234fa8439696e2484f2494a1fbad722cc5fd2dc2f68e--lzo--2.10.arm64_big_sur.bottle.tar.gz
c266397e5e2417a4dc5827a504f27153c12a3938a91b19697abf24d3cfba8ac5  0f96e131aa631d433553a8ac492da24cdb18d9100257e7c9cc6c22062cb5b915--libxau--1.0.9.arm64_big_sur.bottle.tar.gz
2fc4da6029167f696fc0b3c0553d36abb8e77c75f0096396d4eb89d0ea912612  1368560eaab08ce79b4aea96559d5ca40bb719dd5855bafb765ff319bb640689--cairo--1.16.0_5.arm64_big_sur.bottle.tar.gz
af364bc9ec694952b1d7a24754c06b42dbf29a6d177b6a17d2f6d0142b9c996f  1f1627dde5114605c3eb317505fd68f936c9c4e1d768bea9623489ff94037dc0--util-linux--2.38_1.arm64_big_sur.bottle.tar.gz
b23869dc59112723ab755d023f95afd23e43b7e3425a4d085f98d45d3801e4c8  1f9b5738a934578fa99c29009ad22bfb0c2bdde4a63ef30f9022f92809f44c1e--pygobject3--3.42.2.arm64_big_sur.bottle.tar.gz
66f717674d23f63fae9357bc6432f98c9e40702a1112af2b65ba4b3b22ed3192  20e31bc0c284afef2af5b203b13c98b859e905c5546a4bdd17bb4984ad9a5bf2--libpthread-stubs--0.4.arm64_big_sur.bottle.tar.gz
766a7136ee626b411fb63da0c7e5bc1e848afb6e224622f25ea305b2d1a4a0f1  2d56e746f5a3121427f2a75bfc39bc22c0ff418e73dee2fea3bad7a8106455a6--libpng--1.6.37.arm64_big_sur.bottle.tar.gz
3441afab81c2f9ee9c82cac926edcf77be0bca61664c6acedfaba79774742ac2  30daea65577a5ca01db7166e48390944b1277e7a75f7943bf0fd93af59641a8b--xz--5.2.5_1.arm64_big_sur.bottle.tar.gz
339b62b52ba86dfa73091d37341104b46c01ae354ca425000732df689305442b  3e88b29f2cd6477579e19f8d26ece38881c58f316548d3a32d30850ebb1466f5--gettext--0.21.arm64_big_sur.bottle.tar.gz
dbb71439521a388431894b8ba9ea8b9ee628046ccc71cc94acdd3511eceb4df1  3f80d366807a8af5fd4d817e8be35d7b4593136b64cc431652b59b4b9715bae7--libxcb--1.15.arm64_big_sur.bottle.tar.gz
2c4c85f58ee32b3290427b0571dedf0e93603392ea054fb72c3cb513f64fb914  487621be5b7dbbf9ba68062c651803b8434652b496a3492b7d004416993f0808--pango--1.50.8.arm64_big_sur.bottle.tar.gz
24e44ef107138f015271fcd5aaa400403594adf7c64cf4a628b0cfe44d4e9fc6  56b398b19d29e98258688d94584835c236e3a092b7f9a7b3137fc3787679588d--libxext--1.3.4.arm64_big_sur.bottle.tar.gz
08efc469d237689a9619ec6b3ea931793d03597e89bd622ebd122b7256d7a446  5f9dcd2bdd37acd8a39aa26494aa4b69cfa5d789a4939aece1d9e9924845718f--readline--8.1.2.arm64_big_sur.bottle.tar.gz
2166e9d5178197a84ec721b40e22d8c42e30bd0c4808bd38b1ca768eb03f62a5  721ba5eecc1bf642955757ff072f50ef96f86bb20a2045284db199b810c709a7--libffi--3.4.2.arm64_big_sur.bottle.tar.gz
59bc0ef6734f03b980c00fed98dd2d607dda9abdfb9a0c0fb3b00ba8b5db26ac  7b9bace0fdfb378d4bd0f3dbcc2352667e7352f1c04bbe6dc594352b1eb5ec4f--xorgproto--2022.1.arm64_big_sur.bottle.tar.gz
2d6bfcafce9da9739e32ee433087e69a78cda3f18291350953e6ad260fefc50b  8003141aa4f8fd28d11f6348a05cc82e188a3db4bce30baeac5fc9ed9cf19f9e--pcre--8.45.arm64_big_sur.bottle.tar.gz
6c17c65a3f5768a620bc177f6ee189573993df7337c6614050c28e400dc6320c  8deff0ce8800f353a5c12dd9f65a4a21033ac06a9abdd1edf637015bddf070dd--libxdmcp--1.1.3.arm64_big_sur.bottle.tar.gz
544e2c344f6c0a7c2c3cb6541150f0d0d91cd1100460dac9c6a08578823f91c3  a8b3ab5336902ba107565060130e47e930e1f4a1f50f3133a9e7a9d4363cc311--graphite2--1.3.14.arm64_big_sur.bottle.tar.gz
70fd8d0bf3cae1b973c8f580159fa8079dc93a050d19d8032ad0f0288c3f4ee2  ae7ee98fe7a664f7cbce7e9c1603bcd07cad14c5c6c6a6b1a3a7d590e2a69a26--fribidi--1.0.12.arm64_big_sur.bottle.tar.gz
eebbc5c7e71710c848eb60b90f946aefdee1b5269c840c30b8098d6bb758500b  bb970f1da177ba009e42c6bf2a61471a1803a5ff6756698c4a80c42e5c96efef--mpdecimal--2.5.1.arm64_big_sur.bottle.tar.gz
da951aa8e872276034458036321dfa78e7c8b5c89b9de3844d3b546ff955c4c3  bff6a9e9ebcec1855583a512ee4a3729718613e23ed94489cb08f07bb0898f34--pixman--0.40.0.arm64_big_sur.bottle.tar.gz
fe550c503a924fd78a9865793706a8e208752b890713a6699282630b28d7ad50  c3c85605f159c54dc7ffc01a0553fa307f00ba5089d461e0d9fc90b7b3f4b339--libx11--1.8.1.arm64_big_sur.bottle.tar.gz
09f52f15b2a2d126213ea5631bdd35722006540f0086bd285a4f611a4b4b8a78  c4170a1c9041eb5a0c9398dc43ac3aaf2d54b91fe265d41b93c5751e15721fa7--gdbm--1.23.arm64_big_sur.bottle.tar.gz
deb09510fb83adf76d9bb0d4ac4a3d3a2ddfff0d0154e09d3719edb73b058278  c4f7ef1bae054673c3f0dd5030615277258f799ba2e54e31b1e5d37e28ed169a--freetype--2.12.1.arm64_big_sur.bottle.tar.gz
0f1857a5b00b477cbc71f4e8b47db0909ac4591ed162fa68b85bb0b2366a2f74  cd7df9ce5f3349144db9c452017090a8c3dc59a58600b69d02aa9cf44875979a--sqlite--3.39.2.arm64_big_sur.bottle.tar.gz
e028adb6c912631dca4beddada6e63f66f47a169cfde3193d2e10bfd1df6a9dc  cf087d9a9024093ae8946ea083c93655b3ef656eeda95f4bf4118b889c9ab636--harfbuzz--5.1.0.arm64_big_sur.bottle.tar.gz
c603b1b3842e3de3e0681914d01063e923c50bbb5b5f17c4f93611541b28aee5  d036c7c43786d6ac87079eaeb069d167c2b324a69673989dce66cd97ee7642f1--fontconfig--2.14.0.arm64_big_sur.bottle.tar.gz
f0b206023866473514bd5540dc8d2ba18967625d3befee6191bab8f1878f9b6c  dd87bc4cbe8c59fcc022fe6dbc6daf794aaa1b21a790c9b3e2235ff1c06b4035--openssl@1.1--1.1.1q.arm64_big_sur.bottle.tar.gz
46243f05a17674c00950dddc105b33aa479af7d605533d1aeada27d4d89d4275  e6702ef9c08750270f4f396d2c48d270744081e65060dcb1056093f200214d7e--libxrender--0.9.10.arm64_big_sur.bottle.tar.gz
2483f93ce39583d9fde00e42c0081c86572ec15de319ff15d4ea705aefc7fda0  ee5e1c87f0bf4b850158ab0b249ab5eae55a230bf191f5cfed19f40eeacffcbc--py3cairo--1.21.0_1.arm64_big_sur.bottle.tar.gz
a3265f48c2c88487e5f1db1230ec8f9382a2fb68d5eafe90a9ad75fd7c71de85  fe639a17a206adf0f8c65f250bb71b6b7fb4cfaf50858f6abbabf7cd25d4ed5e--glib--2.72.3.arm64_big_sur.bottle.tar.gz
EOF

find $dest -name '*.dylib' |
while read dylib; do
	opt_inst=($(otool -L $dylib | awk -v dest="$dest" '$1~/^@@HOMEBREW_PREFIX@@/{
		chg0 = $1
		chg1 = chg0
		gsub(/^@@HOMEBREW_PREFIX@@\/opt\/[^\/]*\//, dest "/", chg1)
		print "-change", chg0, chg1
	}'))
	install_name_tool "${opt_inst[@]}" -id $dylib "$dylib"
done

for pc in $dest/lib/pkgconfig/*.pc; do
	test -L "$pc" && continue
	sed -i -e "s;^prefix=.*;prefix=$dest;" "$pc"
	sed -i -e "s;@@HOMEBREW_PREFIX@@/opt/[a-z]*/lib/;$dest/lib/;" "$pc"
done

export PKG_CONFIG_PATH=/usr/local/opt/arm64/lib/pkgconfig:$PKG_CONFIG_PATH
for pkg in pango cairo pangocairo; do
	pkg-config --cflags $pkg
	pkg-config --libs $pkg
done

cd $dest/include && ln -s pango-1.0/pango pango
