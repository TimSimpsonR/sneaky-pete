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
 libboost-regex1.40-dev libboost-program-options1.40-dev \
 libboost-filesystem1.40-dev swig

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

# Install QPID
pkg_install automake help2man libtool doxygen graphviz ruby
pkg_install python-dev
pkg_install libperl-dev

#e2fsprogs-devel ? pkgconfig?
sudo apt-get install subversion
sudo apt-get install cmake

cd ~/build
svn co https://svn.apache.org/repos/asf/qpid/trunk/qpid
mkdir ~/build/qpid_build
cd ~/build/qpid_build
cmake ~/build/qpid/cpp/CMakeLists.txt
cd ~/build/qpid/cpp
make # Build
#make tests
sudo make install # Run tests


# Install some rabbit goodness!
cd ~/
