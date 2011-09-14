sudo apt-get install mercurial
sudo apt-get install git-core
sudo apt-get install autoconf
sudo apt-get install libtool
sudo apt-get install libboost1.40-dev
sudo apt-get update

# Install mysql stuff
sudo apt-get install libmysqlcppconn-dev --fix-missing

# Install rabbitmq stuff
mkdir ~/build
cd ~/build
hg clone http://hg.rabbitmq.com/rabbitmq-codegen/
hg clone http://hg.rabbitmq.com/rabbitmq-c/

git clone https://github.com/akalend/amqpcpp.git

cd rabbitmq-c
autoreconf -i
./configure
make
sudo make install

cd ~/build/amqpcpp
make
cp include/* /usr/local/include
cp libamqpcpp.a /usr/local/lib

# Install json stuff
# TODO do this!
