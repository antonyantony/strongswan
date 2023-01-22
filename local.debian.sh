export prefix="/usr"
export CFLAGS="-g -O0 -Wall -Wno-format -Wno-format-security -Wno-pointer-sign -Wfatal-errors -Werror"
./configure --prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--sysconfdir=/etc/  \
	--disable-defaults \
	--enable-ikev2 \
	--enable-systemd \
	--enable-swanctl \
	--enable-socket-default \
	--enable-kernel-netlink  \
	--enable-nonce \
	--enable-openssl
