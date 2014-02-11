#!/usr/bin/env bash
##########################################################################
# Setup Dependencies required for sneaky-pete
##########################################################################

set -o xtrace

# Bail on errors.
set -e

BUILD_DIR=${BUILD_DIR:-/home/vagrant/}sneaky_deps

pkg_install () {
    echo Installing $@...
    sudo -E DEBIAN_FRONTEND=noninteractive apt-get -y --allow-unauthenticated install $@
}

pkg_checkout () {
    PKG_DIR=$BUILD_DIR/$2
    if [ ! -d $PKG_DIR ]; then
        git clone $1 $PKG_DIR
    fi
    pushd $PKG_DIR
    git reset --hard $3
    git clean -f -d -x
    git submodule update --init
    popd
}

# Install deps
pkg_install \
    autoconf \
    git-core \
    g++ \
    libboost-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libconfuse-dev \
    libgc1c2 \
    libmysqlclient-dev \
    libmysqlcppconn-dev \
    libtool \
    make \
    mercurial \
    uuid-dev \
    valgrind
    # rabbitmq-server #<-- Reddwarf CI script will install this

if [ "$(cat /etc/*-release | grep wheezy)" != "" ]; then
    pkg_install \
        libboost1.49-dev \
        mysql-server-5.5

else
    pkg_install \
        bjam \
        boost-build \
        mysql-server-5.1
fi

mkdir -p $BUILD_DIR

# Installing Rabbit
pkg_checkout git://github.com/rabbitmq/rabbitmq-codegen.git rabbitmq-codegen 80fdd87358
pkg_checkout git://github.com/alanxz/rabbitmq-c.git rabbitmq-c 3ff795807f

cd $BUILD_DIR/rabbitmq-c
autoreconf -i
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
pkg_checkout https://github.com/jehiah/json-c.git json-c 276123efe0
cd $BUILD_DIR/json-c
sh autogen.sh
./configure
make
sudo make install

# On the Reddwarf VM this seems to be necessary.
sudo cp /usr/local/lib/libjson* /usr/lib/
sudo cp /usr/local/lib/librabbit* /usr/lib/


# Install Curl
set +e

pkg_install \
    libcrypto++-dev \
    libgcrypt11-dev \
    libidn11-dev \
    libssh-dev \
    libssl-dev

set -e

mkdir -p /opt/sp-deps/libcurl

# Now build the library Sneaky Pete will use from source.
pkg_checkout git://github.com/bagder/curl.git curl e305f5ec71
cd $BUILD_DIR/curl
./buildconf
# TODO(tim.simpson): Add SSL / SSH back... of course.
LIBS="-ldl" ./configure \
    --disable-shared \
    --enable-static \
    --disable-ldap \
    --disable-ldaps \
    --without-gnutls \
    --without-libssh2 \
    --prefix=/opt/sp-deps/libcurl

make LDFLAGS="-all-static -ldl"
make install


# Needed for static compile magic.
cd $BUILD_DIR
ln -sf $(g++ -print-file-name=libstdc++.a)

# Implements a fix for Boost Build's precompiled header feature so it will pass
# necessary C++ flags when building the precompiled header initially.
# Basically, we have to put $(USER_OPTIONS) into the "actions compile.c++.pch"
# for gcc.
# See the following link for more info:
# http://en.it-usenet.org/thread/14892/13006/
sudo sed -i.bac 's/$(CONFIG_COMMAND)" -x c++-header $(OPTIONS) -D$(DEFINES) -I"$(INCLUDES)" -c -o "$(<)" "$(>)/$(CONFIG_COMMAND)" -x c++-header $(OPTIONS) -D$(DEFINES) -I"$(INCLUDES)" $(USER_OPTIONS) -c -o "$(<)" "$(>)/g' /usr/share/boost-build/tools/gcc.jam
