BUILD_DIR=/home/vagrant

# Add rabbit debian repo
echo "deb http://www.rabbitmq.com/debian/ testing main"  | sudo tee -a /etc/apt/sources.list
wget http://www.rabbitmq.com/rabbitmq-signing-key-public.asc
sudo apt-key add rabbitmq-signing-key-public.asc
sudo apt-get update


pkg_install () {
    echo Installing $@...
    sudo -E DEBIAN_FRONTEND=noninteractive apt-get -y --allow-unauthenticated install $@
}

# Install deps
pkg_install mercurial \
  git-core autoconf libtool uuid-dev libmysqlcppconn-dev g++ valgrind \
 mysql-server-5.1 libboost-dev bjam boost-build libboost-test-dev \
 libboost-thread-dev libconfuse-dev libgc1c2 \
 libmysqlclient-dev # rabbitmq-server #<-- Reddwarf CI script will install this

mkdir $BUILD_DIR/build

# Installing Rabbit
cd $BUILD_DIR/build
hg clone http://hg.rabbitmq.com/rabbitmq-codegen/
hg clone http://hg.rabbitmq.com/rabbitmq-c/

cd rabbitmq-c
autoreconf -i
./configure
# Alter librabbitmq/amqp_connection.c, line 416 / 417 to include the flag
# MSG_NOSIGNAL. I'm having trouble ginoring the SIGPIPE signal in Sneaky-Pete
# so this is just a temporary kludge.
# res = send(state->sockfd, out_frame,
#            out_frame_len + HEADER_SIZE + FOOTER_SIZE, MSG_NOSIGNAL);
sed -i.bac 's/FOOTER_SIZE, 0);/FOOTER_SIZE, MSG_NOSIGNAL);/g' /home/vagrant/build/rabbitmq-c/librabbitmq/amqp_connection.c
make
sudo make install


# Install json stuff
cd $BUILD_DIR/build
git clone https://github.com/jehiah/json-c.git
cd json-c
sh autogen.sh
./configure
make
sudo make install

# On the Reddwarf VM this seems to be necessary.
sudo cp /usr/local/lib/libjson* /usr/lib/
sudo cp /usr/local/lib/librabbit* /usr/lib/

# Needed for static compile magic.
cd $BUILD_DIR
ln -s `g++ -print-file-name=libstdc++.a`
