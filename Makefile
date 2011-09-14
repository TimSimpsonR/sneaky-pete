CXX      = g++
CFLAGS   = -Wall

AMQPCPP = $(LIB_HOME)/amqpcpp

CPPFLAGS = $(CFLAGS) -I/usr/local/include -L/usr/local/lib -Iinclude/ -I$(AMQPCPP)/include -I$(BOOST_HOME)

LIBRARIES= rabbitmq mysqlcppconn json
LIBS     = $(addprefix -l,$(LIBRARIES))

GUEST_SOURCES = src/guest.cc
RECEIVER_SOURCES = src/receiver.cc

OBJECTS  = $(GUEST_SOURCES:.cc=.o)

SOURCES_DIR = src
GUEST_OBJECTFILE = src/guest.o

AMQPCPP_A_FILE  = $(AMQPCPP)/libamqpcpp.a
COMPILE_ARGS = $(AMQPCPP_A_FILE) -lrabbitmq -lmysqlcppconn

guest: $(AMQPCPP_A_FILES)
        $(CXX) $(CPPFLAGS) -g -c -o $(GUEST_OBJECTFILE) $(SOURCES_DIR)/guest.cc $(COMPILE_ARGS) 

receiver: guest.o
        $(CXX) $(CPPFLAGS) $(SOURCES_DIR)/receiver.cc -o receiver $(GUEST_OBJECTFILE) $(COMPILE_ARGS) 

send: guest.o first_c_tests/send.cc
        $(CXX) $(CPPFLAGS) $(SOURCES_DIR)/sender.cc -o sender $(GUEST_OBJECTFILE) $(COMPILE_ARGS) 

clean:
        rm -f $(OBJECTS) $(EXAMPLES) $(LIBFILE)
