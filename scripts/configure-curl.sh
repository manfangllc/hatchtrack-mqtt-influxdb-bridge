#!/bin/sh

BASE=$(dirname $0)

LIBMEDTLS=$(realpath $BASE/../build/lib/libmbedtls.a)
BUILD_DIR=$(realpath $BASE/../build)

if [ -e $BASE/../build/bin/curl ]; then
  exit 0
fi

cd $BASE/../lib/curl*

export LDFLAGS="-Wl,$LIBMEDTLS"
./configure \
  --without-ssl \
  --with-mbedtls=$BUILD_DIR \
  --disable-shared \
  --prefix=$BUILD_DIR \
  --exec-prefix=$BUILD_DIR
