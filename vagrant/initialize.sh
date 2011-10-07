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
 mysql-server-5.1 libboost1.40-dev bjam boost-build libboost-test-dev \
 libboost-thread1.40-dev rabbitmq-server libconfuse-dev

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


# Apt
sudo apt-get install libapt-pkg-dev
