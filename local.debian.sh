#!/bin/sh
DIR=$(cd "$(dirname "$0")" && pwd)
"$DIR/configure" --build=x86_64-linux-gnu --prefix=/usr \
      --includedir=\${prefix}/include \
      --mandir=\${prefix}/share/man \
      --infodir=\${prefix}/share/info \
      --sysconfdir=/etc \
      --localstatedir=/var \
      --disable-silent-rules \
      --libdir=/usr/lib \
      --libexecdir=/usr/lib \
      --disable-maintainer-mode \
      --disable-dependency-tracking \
      --enable-openssl \
      --disable-gmp \
      --enable-cmd \
      --disable-blowfish \
      --disable-des \
      --enable-rdrand \
      --enable-systemd \
      --disable-socket-default \
      --enable-socket-dynamic \
      --with-piddir=/run/strongswan
