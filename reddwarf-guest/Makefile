CXX      = g++
CFLAGS   = -Wall

AMQPCPP = $(LIB_HOME)

CPPFLAGS = $(CFLAGS) -I/usr/local/include -L/usr/local/lib -Iinclude/

LIBRARIES= rabbitmq mysqlcppconn json
LIBS     = $(addprefix -l,$(LIBRARIES))

GUEST_SOURCES = src/guest.cc
RECEIVER_SOURCES = src/receiver.cc

OBJECTS = $(GUEST_OBJECTFILE:.cc=.o)

SOURCES_DIR = src
GUEST_OBJECTFILE = src/guest.o

AMQPCPP_A_FILE  = $(AMQPCPP)/libamqpcpp.a
COMPILE_ARGS = $(AMQPCPP_A_FILE) -lrabbitmq -lmysqlcppconn -ljson -luuid

guest:
	$(CXX) $(CPPFLAGS) -g -c -o $(GUEST_OBJECTFILE) $(SOURCES_DIR)/guest.cc $(COMPILE_ARGS) 

receiver: $(GUEST_OBJECTFILE)
	$(CXX) $(CPPFLAGS) -g $(SOURCES_DIR)/receiver.cc -o receiver $(GUEST_OBJECTFILE) $(COMPILE_ARGS) 

sender: $(GUEST_OBJECTFILE) 
	$(CXX) $(CPPFLAGS) -g $(SOURCES_DIR)/test_sender.cc -o sender $(GUEST_OBJECTFILE) $(COMPILE_ARGS) 

clean:
	rm -f receiver sender $(GUEST_OBJECTFILE)
