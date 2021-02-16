export CFLAGS="${CFLAGS} -g -O0 -Wall -Wno-format -Wno-format-security -Wno-pointer-sign -Wfatal-errors -Werror"

./configure \
    --enable-swanctl \
    --enable-tss-trousers \
    --enable-systemd \
    --enable-openssl \
    --enable-aesni \
    --enable-ccm \
    --enable-gcm \
    --disable-tss-trousers
