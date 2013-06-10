#!/usr/bin/env bash
##########################################################################
# Setup Dependencies required for sneaky-pete
##########################################################################

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


sudo -E DEBIAN_FRONTEND=noninteractive $HTTP_PROXY apt-get install libcurl4-openssl-dev
sudo updatedb

# # Alternative- use this if we ever need to build from source.
# # Install curl.
# # First, remove the installed one.
# set +e
# sudo -E DEBIAN_FRONTEND=noninteractive $HTTP_PROXY apt-get purge curl
# set -e
# # Now build the library Sneaky Pete will use from source.
# cd $BUILD_DIR
# git clone git://github.com/bagder/curl.git
# cd curl
# ./buildconf
# ./configure
# make
# sudo make install
# # Remove the binary file the above process creates as it won't work correctly.
# sudo rm /usr/local/bin/curl
# # Now reinstall.
# sudo -E DEBIAN_FRONTEND=noninteractive $HTTP_PROXY apt-get install curl

# Needed for static compile magic.
cd $BUILD_DIR
ln -s `g++ -print-file-name=libstdc++.a`
