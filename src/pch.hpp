#ifndef __NOVA_PRECOMPILED_HEADER_H
#define __NOVA_PRECOMPILED_HEADER_H

// Defines essential configuration constants for Sneaky Pete.

#ifdef BOOST_BUILD_PCH_ENABLED

#include <algorithm>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/list.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>
#include <cstdlib>
#include <curl/curl.h>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <ifaddrs.h>
#include <initializer_list>
#include <iostream>
#include <json/json.h>
#include <limits>
#include <list>
#include <malloc.h>
#include <map>
#include <memory>
#include <mysql/mysql.h>
#include <regex.h>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>
#include <vector>
#include <unistd.h>
#include <uuid/uuid.h>

extern "C" {
    #include <amqp.h>
    #include <amqp_framing.h>
}

#endif // end BOOST_BUILD_PCH_ENABLED

#endif // end __NOVA_PRECOMPILED_HEADER_H
