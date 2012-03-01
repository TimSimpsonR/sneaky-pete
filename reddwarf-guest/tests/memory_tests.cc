#define BOOST_TEST_MODULE Memory_Tests
#include <boost/test/unit_test.hpp>

#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include <json/json.h>
#include <iostream>
#include "nova/rpc/receiver.h"
#include <signal.h>
#include "nova/guest/mysql.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


using nova::rpc::Receiver;
using namespace nova::guest;

/*
The goal of these tests are to show the memory usage of various components.
*/


// Runs a process, saves the output in "out".
void run_proc(const char * command, std::stringstream & out) {
    char line[256];
    FILE * pipe = (FILE *) popen(command,"r");
    BOOST_REQUIRE_NE(pipe, (FILE *) 0);
    while (fgets(line, sizeof(line), pipe)) {
        out << line;
    }
    pclose(pipe);
}


struct MemoryInfo {
    int mapped;
    int writeable_private;
    int shared;

    MemoryInfo(const int pid) {
        std::stringstream str;
        str << "pmap -d " << pid << " | "
            << "awk '/^mapped:/ { print $2 \" \" $4 \" \" $6 }'";
        std::stringstream out;
        run_proc(str.str().c_str(), out);
        // Output will be in the form:
        // #####K ###K ###K
        out >> mapped;
        char k;
        out >> k;
        // Make sure we see Ks!
        BOOST_REQUIRE_EQUAL(k, 'K');
        out >> writeable_private;
        out >> k;
        BOOST_REQUIRE_EQUAL(k, 'K');
        out >> shared;
        out >> k;
        BOOST_REQUIRE_EQUAL(k, 'K');
    }

    MemoryInfo() {
    }

    MemoryInfo operator-(MemoryInfo & rhs) {
        MemoryInfo rtn;
        rtn.mapped = this->mapped - rhs.mapped;
        rtn.writeable_private = this->writeable_private - rhs.writeable_private;
        rtn.shared = this->shared - rhs.shared;
        return rtn;
    }
};


typedef void (* proc_func_ptr)(void);

MemoryInfo run_fork(proc_func_ptr func) {
    // Remember 0 is for reading, 1 is for writing.
    int	child_pipe[2] = {-1,-1};
	int parent_pipe[2] = {-1,-1};
	BOOST_REQUIRE_GE(pipe(child_pipe), 0);
	BOOST_REQUIRE_GE(pipe(parent_pipe), 0);

    char buf[5];
    buf[4] = '\0';
    pid_t pid = fork();
    BOOST_REQUIRE_MESSAGE(pid >= 0, "Couldn't fork!");
    if (pid == 0) {  // Child.
        close(child_pipe[1]);
        close(parent_pipe[0]);
        func();
        // The child does its work, then informs the parent it is ready.
        write(parent_pipe[1], "DONE", 4);
        close(parent_pipe[1]);
        // Afterwards it awaits orders to shut down.
        read(child_pipe[0], buf, 3);
        close(child_pipe[0]);
        _exit(0);
    }
    close(parent_pipe[1]);
    close(child_pipe[0]);

    // Wait for Child to finish work.
    ssize_t size = read(parent_pipe[0], buf, 4);
    BOOST_REQUIRE_MESSAGE(size == 4,
        "Could not read from pipe! This probably means the forked program "
        "crashed.");
    BOOST_REQUIRE_EQUAL(buf, "DONE");
    close(parent_pipe[0]);

    // Take memory reading.
    MemoryInfo memory(pid);
    // Tell child to shutdown.
    write(child_pipe[1], "END", 3);
    close(child_pipe[1]);

    //kill(pid, SIGINT);
    int child_exit_status;
    /*pid_t ws = */waitpid(pid, &child_exit_status, 0);
    BOOST_REQUIRE_MESSAGE(WIFEXITED(child_exit_status),
                          "Process must exit normally.");
    return memory;
}

const int mapped_min = 46000; //16000; //8000;
const int write_min = 9040; //844;
const int shared_min = 0;

void empty() {
}

/** Gets the base memory this should use. */
MemoryInfo & base_memory() {
    static bool initialized = false;
    static MemoryInfo base;
    if (!initialized) {
        base = run_fork(empty);
        BOOST_CHECK_LE(base.mapped, mapped_min);
        BOOST_CHECK_LE(base.writeable_private, write_min);
        BOOST_CHECK_LE(base.shared, shared_min);
    }
    return base;
}


/** Runs a process and tests that the expected diff is in bounds. */
void test_proc(const char * name, proc_func_ptr func,
               MemoryInfo & expected_diff) {
    MemoryInfo & base = base_memory();
    MemoryInfo memory = run_fork(func);
    MemoryInfo diff = memory - base;

    std::cout << name << ":" << std::endl;
    std::cout << "    mapped............:" << memory.mapped << "K" << std::endl;
    std::cout << "        diff..........:+" << diff.mapped << "K" << std::endl;
    std::cout << "    writeable/private.:" << memory.writeable_private << "K"
              << std::endl;
    std::cout << "        diff..........:+" << diff.writeable_private << "K"
              << std::endl;
    std::cout << "    shared............:" << memory.shared << "K" << std::endl;
    std::cout << "        diff..........:+" << diff.shared << "K" << std::endl;
    std::cout << std::endl;

    BOOST_CHECK_LE(diff.mapped, expected_diff.mapped);
    BOOST_CHECK(diff.mapped >= 0);
    BOOST_CHECK_LE(diff.writeable_private, expected_diff.writeable_private);
    BOOST_CHECK(diff.writeable_private >= 0);
    BOOST_CHECK_LE(diff.shared, expected_diff.shared);
    BOOST_CHECK(diff.shared >= 0);
}


BOOST_AUTO_TEST_CASE(baseline)
{
    MemoryInfo expected_diff;
    expected_diff.mapped = 0;
    expected_diff.writeable_private = 0;
    expected_diff.shared = 0;
    test_proc("Baseline (some memory is taken up by Boost.Test)", empty,
              expected_diff);
}

void open_sql_connection() {
    sql::Driver * driver = get_driver_instance();
    try {
        sql::Connection *con = driver->connect("tcp://127.0.0.1:3306",
                                               "root", "");
        con = 0;
    } catch(const std::exception & e) {
        // ignore
    }
}

BOOST_AUTO_TEST_CASE(SqlTakesUpAFewBytes)
{
    MemoryInfo expected_diff;
    expected_diff.mapped = (int)(8.5 * 1024);
    expected_diff.writeable_private = (int)(8.5 * 1024);
    expected_diff.shared = 0;
    test_proc("Merely opening a SQL connection", open_sql_connection,
              expected_diff);
}

void open_sql_socket_connection() {
    sql::Driver * driver = get_driver_instance();
    //TODO(hub-cap): fix this to read from my.cnf!
    sql::Connection *con = driver->connect("unix:///var/run/mysqld/mysqld.sock",
                                           "root", "test2");
    con = 0;
}
/*

BOOST_AUTO_TEST_CASE(SqlSocketTakesUpAFewBytesMaybe)
{
    MemoryInfo expected_diff;
    expected_diff.mapped = (int)(8.5 * 1024);
    expected_diff.writeable_private = (int)(8.5 * 1024);
    expected_diff.shared = 0;
    test_proc("Merely opening a SQL Socket connection", open_sql_socket_connection,
              expected_diff);
}*/

#define JSON_OBJECT_COUNT 10000
#define STR_VALUE(arg)  #arg
#define STR_MACRO(name) STR_VALUE(name)
#define JSON_OBJECT_COUNT_STR  STR_MACRO(JSON_OBJECT_COUNT)


void run_some_json() {
    json_object * objs[JSON_OBJECT_COUNT];
    for (size_t j = 0; j < JSON_OBJECT_COUNT; j ++) {
        std::stringstream s;
        s << "{'status': '200', 'x-api-version': '1.0.11', "
             "'content-length': '392', "
             "'content-location': 'https://blah.com',"
             " 'server': 'Jetty(7.3.1.v20110307)', "
             "'date': 'Mon, 19 Sep 2011 21:02:59 GMT', "
             "'content-type': 'application/json'"
             "'body':{'records': ["
             "{'updated': '2011-09-15T17:49:46.000+0000', "
             "'name': 'bfgsdgshdfh.org', "
             "'created': '2011-09-15T17:49:46.000+0000', "
             "'type': 'NS', 'ttl': 5600, "
             "'data': 'dns1.stabletransit.com', 'id': u'NS-6353155'},"
             " {'updated': '2011-09-15T17:49:46.000+0000', "
             "'name': 'paulydbaas.org', "
             "'created': '2011-09-15T17:49:46.000+0000', 'type': 'NS', "
             "'ttl': 5600, 'data': 'dns2.stabletransit.com', "
             "'id': 'NS-6353156'}], 'totalEntries': 2} "
             "} ";
        objs[j] = json_object_new_string(s.str().c_str());
    }
    //char * five_hundred = new  char[50 * 1024];
    //five_hundred[6] = 'h';
    /*sleep(3);
    for (size_t j = 0; j < 100; j ++) {
        json_object_put(objs[j]);
    }*/
}

BOOST_AUTO_TEST_CASE(Json)
{
    // Lets say 7/10ths of a kb per one of the objects above is acceptable.
    const double OBJ_SIZE = 0.7;
    MemoryInfo expected_diff;
    expected_diff.mapped = (int) (OBJ_SIZE * JSON_OBJECT_COUNT);
    expected_diff.writeable_private = (int) (OBJ_SIZE * JSON_OBJECT_COUNT);
    expected_diff.shared = 0;
    test_proc("Some arbitrary JSON * " JSON_OBJECT_COUNT_STR, run_some_json,
              expected_diff);
}

const char JSON_OBJECT_STRING [] =
"{"
"    'object':{"
"        'string':'hello',"
"        'number':42,"
"        'bool':true,"
"        'array':[1,2,3,4,5]"
"    }"
"}";


void run_json_object() {
    json_object * objs[JSON_OBJECT_COUNT];
    for (size_t j = 0; j < JSON_OBJECT_COUNT; j ++) {
        objs[j] = json_object_new_string(JSON_OBJECT_STRING);
    }
}

BOOST_AUTO_TEST_CASE(JsonObject) {
    // Lets say 1/10ths of a kb per one of the objects above is acceptable.
    const double OBJ_SIZE = 0.15;
    MemoryInfo expected_diff;
    expected_diff.mapped = (int) (OBJ_SIZE * JSON_OBJECT_COUNT);
    expected_diff.writeable_private = (int) (OBJ_SIZE * JSON_OBJECT_COUNT);
    expected_diff.shared = 0;
    test_proc("Plain JSON object * " JSON_OBJECT_COUNT_STR, run_json_object,
              expected_diff);
}


#define BOOST_PTR_COUNT 10000
#define BOOST_PTR_COUNT_STR  STR_MACRO(BOOST_PTR_COUNT)

void run_boost_smart_ptr_stuff() {
    for (size_t j = 0; j < BOOST_PTR_COUNT; j ++) {
        boost::shared_ptr<char> * dumb_ptr_to_smart_ptr =
            new boost::shared_ptr<char>(new char('a'));
        dumb_ptr_to_smart_ptr = 0;
    }
}


BOOST_AUTO_TEST_CASE(BoostSharedPtrTest) {
    // Each one of these pointers is <54 bytes (wow).
    const double expected_size = 55.0 / 1024.0;
    MemoryInfo expected_diff;
    expected_diff.mapped = (int) (expected_size * BOOST_PTR_COUNT);
    expected_diff.writeable_private = (int) (expected_size * BOOST_PTR_COUNT);
    expected_diff.shared = 0;
    test_proc("Making " BOOST_PTR_COUNT_STR " boost::shared_ptrs to char.",
              run_boost_smart_ptr_stuff, expected_diff);
}


#define AUTO_PTR_COUNT 10000
#define AUTO_PTR_COUNT_STR  STR_MACRO(AUTO_PTR_COUNT)

void run_auto_ptr_stuff() {
    for (size_t j = 0; j < AUTO_PTR_COUNT; j ++) {
        std::auto_ptr<char> * dumb_ptr_to_smart_ptr =
            new std::auto_ptr<char>(new char('a'));
        dumb_ptr_to_smart_ptr = 0;
    }
}


BOOST_AUTO_TEST_CASE(AutoPtrTest) {
    // Each one of these pointers is <27 bytes (just above 26).
    const double expected_size = 28 / 1024.0;
    MemoryInfo expected_diff;
    expected_diff.mapped = (int) (expected_size * AUTO_PTR_COUNT);
    expected_diff.writeable_private = (int) (expected_size * AUTO_PTR_COUNT);
    expected_diff.shared = 0;
    test_proc("Making " AUTO_PTR_COUNT_STR " std::auto_ptrs to char.",
              run_auto_ptr_stuff, expected_diff);
}


#define INTRUSIVE_PTR_COUNT 10000
#define INTRUSIVE_PTR_COUNT_STR  STR_MACRO(INTRUSIVE_PTR_COUNT)

void intrusive_ptr_add_ref(char * ptr) {
}

void run_intrusive_ptr_stuff() {
    for (size_t j = 0; j < INTRUSIVE_PTR_COUNT; j ++) {
        boost::intrusive_ptr<char> * dumb_ptr_to_smart_ptr =
            new boost::intrusive_ptr<char>(new char('a'));
        dumb_ptr_to_smart_ptr = 0;
    }
}

BOOST_AUTO_TEST_CASE(IntrusivePtrTest) {
    // Each one of these pointers is <27 bytes (just above 26).
    const double expected_size = 28 / 1024.0;
    MemoryInfo expected_diff;
    expected_diff.mapped = (int) (expected_size * INTRUSIVE_PTR_COUNT);
    expected_diff.writeable_private = (int) (expected_size * INTRUSIVE_PTR_COUNT);
    expected_diff.shared = 0;
    test_proc("Making " INTRUSIVE_PTR_COUNT_STR
              " boost::intrusive_ptrs to char.",
              run_intrusive_ptr_stuff, expected_diff);
}
