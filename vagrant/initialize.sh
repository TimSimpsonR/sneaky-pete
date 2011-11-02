cd ~/

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
 libboost-thread-dev libconfuse-dev \
 libmysqlclient-dev # rabbitmq-server #<-- Reddwarf CI script will install this

mkdir ~/build

# Installing Rabbit
cd ~/build
hg clone http://hg.rabbitmq.com/rabbitmq-codegen/
hg clone http://hg.rabbitmq.com/rabbitmq-c/

cd rabbitmq-c
autoreconf -i
./configure
make
sudo make install


# Install json stuff
cd ~/build
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
cd ~/
ln -s `g++ -print-file-name=libstdc++.a`

# To disable the Python guest from installing, run this:
# sudo -E reprepro -Vb /var/www/debian remove squeeze nova-guest
