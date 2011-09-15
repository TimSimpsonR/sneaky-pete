sudo apt-get install mercurial
sudo apt-get install git-core
sudo apt-get install autoconf
sudo apt-get install libtool
sudo apt-get install uuid-dev


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

# Copy stuff to /usr/local/include
sudo cp include/* /usr/local/include
sudo cp libamqpcpp.a /usr/local/lib

# Install json stuff
cd ~/build
git clone https://github.com/jehiah/json-c.git
cd json-c
sh autogen.sh
./configure
make
make install


# Install Boost
sudo apt-get install libboost1.40-dev
sudo apt-get install bjam
sudo apt-get install boost-build
sudo apt-get install libboost-test-dev


# Install some rabbit goodness!
cd ~/
echo "deb http://www.rabbitmq.com/debian/ testing main"  | sudo tee -a /etc/apt/sources.list
wget http://www.rabbitmq.com/rabbitmq-signing-key-public.asc
sudo apt-key add rabbitmq-signing-key-public.asc
sudo apt-get update
sudo apt-get install rabbitmq-server

# Needed?
sudo apt-get install mysql-server-5.1

sudo apt-get install valgrind
