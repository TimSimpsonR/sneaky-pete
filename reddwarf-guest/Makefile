CXX      = g++
CFLAGS   = -Wall

AMQPCPP = /usr/local/lib

CPPFLAGS = $(CFLAGS) -I/usr/local/include -L/usr/local/lib -Iinclude/

LIBRARIES= rabbitmq mysqlcppconn json
LIBS     = $(addprefix -l,$(LIBRARIES))

GUEST_SOURCES = src/guest.cc
RECEIVER_SOURCES = src/receiver.cc

OBJECTS = $(GUEST_OBJECTFILE:.cc=.o)

SOURCES_DIR = src
GUEST_OBJECTFILE = src/guest.o
RECEIVER_OBJECTFILE = src/receiver.o

AMQPCPP_A_FILE  = $(AMQPCPP)/libamqpcpp.a
COMPILE_ARGS = $(AMQPCPP_A_FILE) -lrabbitmq -lmysqlcppconn -ljson -luuid

guest:
	$(CXX) $(CPPFLAGS) -g -c -o $(GUEST_OBJECTFILE) $(SOURCES_DIR)/guest.cc $(COMPILE_ARGS)

receiver:
	$(CXX) $(CPPFLAGS) -g -c -o $(RECEIVER_OBJECTFILE) $(SOURCES_DIR)/receiver.cc $(COMPILE_ARGS)

receiver_daemon: $(GUEST_OBJECTFILE) $(RECEIVER_OBJECTFILE)
	$(CXX) $(CPPFLAGS) -g $(SOURCES_DIR)/receiver_daemon.cc -o receiver $(GUEST_OBJECTFILE) $(RECEIVER_OBJECTFILE) $(COMPILE_ARGS)

sender: $(GUEST_OBJECTFILE)
	$(CXX) $(CPPFLAGS) -g $(SOURCES_DIR)/test_sender.cc -o sender $(GUEST_OBJECTFILE) $(COMPILE_ARGS)

clean:
	rm -f receiver sender $(GUEST_OBJECTFILE) $(RECEIVER_OBJECTFILE)
