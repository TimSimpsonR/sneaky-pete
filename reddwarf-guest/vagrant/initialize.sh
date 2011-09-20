# Add rabbit debian repo
echo "deb http://www.rabbitmq.com/debian/ testing main"  | sudo tee -a /etc/apt/sources.list
wget http://www.rabbitmq.com/rabbitmq-signing-key-public.asc
sudo apt-key add rabbitmq-signing-key-public.asc
sudo apt-get update

# Install deps
sudo DEBIAN_FRONTEND=noninteractive apt-get --allow-unauthenticated -y install mercurial \
 git-core autoconf libtool uuid-dev libmysqlcppconn-dev g++ valgrind mysql-server-5.1 \
 libboost1.40-dev bjam boost-build libboost-test-dev libboost-thread1.40-dev rabbitmq-server

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
sudo make install


# Install Boost



# Install some rabbit goodness!
cd ~/
