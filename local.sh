export CFLAGS="${CFLAGS} -g -O0 -Wall -Wno-format -Wno-format-security -Wno-pointer-sign -Wfatal-errors -Werror"

./configure --libdir=/usr/lib --libexecdir=/usr/lib --sysconfdir=/etc/ \
	--enable-openssl \
	--enable-bypass-lan
