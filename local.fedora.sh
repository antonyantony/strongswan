export prefix="/usr"
export CFLAGS="-g -O0 -Wall -Wno-format -Wno-format-security -Wno-pointer-sign -Wfatal-errors -Werror"
./configure --prefix=${prefix} \
    --with-ipsec-script=strongswan \
    --sysconfdir=/etc/strongswan \
    --with-ipsecdir=${prefix}/libexec/strongswan \
    --bindir=${libexecdir}/strongswan \
    --with-ipseclibdir=${prefix}/lib/strongswan \
    --with-piddir=/run/strongswan \
    --enable-swanctl \
    --enable-tss-trousers \
    --enable-systemd \
    --enable-openssl \
    --enable-aesni \
    --enable-ccm \
    --enable-gcm
