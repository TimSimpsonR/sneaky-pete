#!/usr/bin/env bash
##########################################################################
# Setup Dependencies required for sneaky-pete
##########################################################################

set -o xtrace

# Bail on errors.
set -e

BUILD_DIR=${BUILD_DIR:-/home/vagrant}/sneaky_deps

pkg_install () {
    echo Installing $@...
    sudo -E DEBIAN_FRONTEND=noninteractive apt-get -y --allow-unauthenticated install $@
}

# Install deps
pkg_install mercurial \
  git-core autoconf libtool uuid-dev libmysqlcppconn-dev g++ valgrind \
 mysql-server-5.1 libboost-dev bjam boost-build libboost-test-dev \
 libboost-thread-dev libconfuse-dev libgc1c2 make \
 libmysqlclient-dev # rabbitmq-server #<-- Reddwarf CI script will install this

rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Installing Rabbit
cd $BUILD_DIR
git clone git://github.com/rabbitmq/rabbitmq-codegen.git
git clone git://github.com/alanxz/rabbitmq-c.git

cd rabbitmq-c
git reset --hard 3ff795807f102aa866aec4d0921dd83ab3b9757d
git submodule init
git submodule update

autoreconf -i
ls
./configure
# Alter librabbitmq/amqp_connection.c, line 416 / 417 to include the flag
# MSG_NOSIGNAL. I'm having trouble ginoring the SIGPIPE signal in Sneaky-Pete
# so this is just a temporary kludge.
# res = send(state->sockfd, out_frame,
#            out_frame_len + HEADER_SIZE + FOOTER_SIZE, MSG_NOSIGNAL);
sed -i.bac 's/FOOTER_SIZE, 0);/FOOTER_SIZE, MSG_NOSIGNAL);/g' $BUILD_DIR/rabbitmq-c/librabbitmq/amqp_connection.c
make
sudo make install

# Install json stuff
cd $BUILD_DIR
git clone https://github.com/jehiah/json-c.git
cd json-c
sh autogen.sh
./configure
make
sudo make install

# On the Reddwarf VM this seems to be necessary.
sudo cp /usr/local/lib/libjson* /usr/lib/
sudo cp /usr/local/lib/librabbit* /usr/lib/


# Install Curl
set +e

pkg_install libssl-dev
pkg_install libidn11-dev
pkg_install libcrypto++-dev
pkg_install libssh-dev
pkg_install libgcrypt11-dev

set -e

mkdir -p /opt/sp-deps/libcurl

# Now build the library Sneaky Pete will use from source.
cd $BUILD_DIR
git clone git://github.com/bagder/curl.git
cd curl
git reset --hard e305f5ec715f967bdfaa0c9bf8f102f9185b9aa2
./buildconf
# TODO(tim.simpson): Add SSL / SSH back... of course.
./configure --disable-shared --enable-static --with-ssl=/usr/local/openssl \
     --disable-ldap --disable-ldaps --without-zlib --without-ssl \
     --without-gnutls --without-libssh2 --prefix=/opt/sp-deps/libcurl

make LDFLAGS=-all-static
make install


# Needed for static compile magic.
cd $BUILD_DIR
ln -s `g++ -print-file-name=libstdc++.a`
