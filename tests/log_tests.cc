#define BOOST_TEST_MODULE ConfigFile_Tests
#include <boost/test/unit_test.hpp>

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include "nova/Log.h"
#include "nova/utils/regex.h"
#include <string>
#include <boost/thread.hpp>
#include <vector>

// Confirm the macros works everywhere by not using nova::Log.
using boost::format;
using nova::LogException;
using nova::LogFileOptions;
using nova::LogOptions;
using boost::optional;
using nova::utils::Regex;
using nova::utils::RegexMatchesPtr;
using std::string;
using std::vector;

#define CHECK_POINT() BOOST_CHECK_EQUAL(2,2);

#define CHECK_LOG_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown LogException."); \
    } catch(const LogException & le) { \
        const char * actual = LogException::code_to_string(LogException::ex_code); \
        const char * expected = LogException::code_to_string(le.code); \
        BOOST_REQUIRE_EQUAL(actual, expected); \
    }

BOOST_AUTO_TEST_CASE(shutdown_should_be_idempotent)
{
    nova::Log::shutdown();
    nova::Log::shutdown();
}

BOOST_AUTO_TEST_CASE(should_throw_if_uninitialized)
{
    nova::Log::shutdown();

    CHECK_LOG_EXCEPTION({
        NOVA_LOG_DEBUG2("Arrggghhh!");
    }, NOT_INITIALIZED);
}


static int test_count = 0;

struct LogTestsFixture {

    string log_file;

    LogTestsFixture() : log_file(str(format("log_tests_%d") % test_count)) {
        remove(log_file.c_str());
        LogFileOptions file_options(log_file, 3);
        LogOptions options(optional<LogFileOptions>(file_options), true, false);
        nova::Log::initialize(options);
        test_count ++;
    }

    ~LogTestsFixture() {
        nova::Log::shutdown();
    }

    void read_file(vector<string> & lines, int index=0) {
        string path = (index == 0) ? log_file
                      : (str(format("%s.%d") % log_file % index));
        std::ifstream file(path.c_str());
        while(!file.eof()) {
            string line;
            std::getline(file, line);
            lines.push_back(line);
        }
        file.close();
    }

};

BOOST_FIXTURE_TEST_SUITE(LogTestsSuite,
                         LogTestsFixture);

BOOST_AUTO_TEST_CASE(writing_some_lines) {
    NOVA_LOG_DEBUG("Hello from the tests. How're you doing?");
    NOVA_LOG_ERROR("Hi");
    NOVA_LOG_INFO("Bye");

    /* Read the log file and confirm it works. */
    vector<string> lines;
    read_file(lines);

    {
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
                    "\\s{1}DEBUG Hello from the tests\\. How\'re you doing\\? "
                    "for tests/log_tests.cc:[0-9]+");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]");
        RegexMatchesPtr matches = regex.match(lines[1].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
                "\\s{1}INFO  Bye "
                "for tests/log_tests.cc:[0-9]+");
        RegexMatchesPtr matches = regex.match(lines[2].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }
}

BOOST_AUTO_TEST_CASE(writing_some_lines_and_rotating) {
    NOVA_LOG_DEBUG("Hello from the tests. How're you doing?");
    nova::Log::rotate_files();
    NOVA_LOG_ERROR("Hi");
    nova::Log::rotate_files();
    NOVA_LOG_INFO("Bye");

    /* Read the log file and confirm it works. */
    {
        vector<string> lines;
        read_file(lines, 2);
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
                    "\\s{1}DEBUG Hello from the tests\\. How\'re you doing\\? "
                    "for tests/log_tests.cc:[0-9]+");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        vector<string> lines;
        read_file(lines, 1);
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        vector<string> lines;
        read_file(lines);
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
                "\\s{1}INFO  Bye "
                "for tests/log_tests.cc:[0-9]+");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

}

struct Worker {
    int id;
    char stage;
    bool quit;

    Worker(int id) : id(id), stage('a'), quit(false) {
    }

    void next_state() {
        stage ++;
    }

    void work() {
        while(!quit) {
            std::cerr << "WORK id=" << id << " stage=" << stage << std::endl;
            NOVA_LOG_DEBUG2("id=%d stage=%c", id, stage);
            NOVA_LOG_ERROR2("id=%d stage=%c", id, stage);
            NOVA_LOG_INFO2("id=%d stage=%c", id, stage);
            boost::posix_time::milliseconds time(100);
            boost::this_thread::sleep(time);
        }
    }
};


BOOST_AUTO_TEST_CASE(writing_lines_in_a_thread) {
    std::cerr << "Welcome to Test #2." << std::endl;
    NOVA_LOG_INFO("Welcome to Test #2.");
    const int worker_count = 10;
    vector<boost::thread *> threads;
    vector<Worker> workers;
    for (int i = 0; i < worker_count; i ++) {
        workers.push_back(Worker(i));
    }
    for (int i = 0; i < worker_count; i ++) {
        CHECK_POINT();
        threads.push_back(new boost::thread(&Worker::work, &workers[i]));
    }

    CHECK_POINT();

    boost::posix_time::seconds time(0);
    boost::this_thread::sleep(time);
    for (char s = 'a'; s <= 'e'; s = s + (char) 1) {
        // We give all the workers one whole second to print out their letters.
        CHECK_POINT();
        boost::posix_time::seconds time(1);
        boost::this_thread::sleep(time);
        CHECK_POINT();
        std::cout << "Time to enter a world of pain." << std::endl;
        BOOST_FOREACH(Worker & worker, workers) {
            CHECK_POINT();
            std::cout << "next state for : " << worker.id << std::endl;
            worker.next_state();
        }
    }

    BOOST_CHECK_EQUAL(2,2);

    BOOST_FOREACH(Worker & worker, workers) {
        worker.quit = true;
    }

    BOOST_CHECK_EQUAL(2,2);

    BOOST_FOREACH(boost::thread * thread, threads) {
        thread->interrupt();
        thread->join();
        delete thread;
    }

    BOOST_CHECK_EQUAL(2,2);
    boost::this_thread::sleep(boost::posix_time::seconds(2));
}

BOOST_AUTO_TEST_CASE(writing_lines_to_threads_while_switching_files) {
    std::cerr << "Welcome to Test #3." << std::endl;
    NOVA_LOG_INFO("Welcome to Test #3.");
    const int worker_count = 50;
    vector<boost::thread *> threads;
    vector<Worker> workers;
    for (int i = 20; i < worker_count + 20; i ++) {
        workers.push_back(Worker(i));
    }
    for (int i = 20; i < worker_count + 20; i ++) {
        CHECK_POINT();
        threads.push_back(new boost::thread(&Worker::work, &workers[i]));
    }

    CHECK_POINT();

    boost::posix_time::seconds time(0);
    boost::this_thread::sleep(time);
    for (char s = 'a'; s <= 'e'; s = s + (char) 1) {
        // We give all the workers one whole second to print out their letters.
        CHECK_POINT();
        boost::posix_time::seconds time(1);
        boost::this_thread::sleep(time);
        CHECK_POINT();
        std::cout << "Time for waffles." << std::endl;
        BOOST_FOREACH(Worker & worker, workers) {
            CHECK_POINT();
            std::cout << "next state for : " << worker.id << std::endl;
            worker.next_state();
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            nova::Log::rotate_files();
        }
    }

    BOOST_CHECK_EQUAL(2,2);

    BOOST_FOREACH(Worker & worker, workers) {
        worker.quit = true;
    }

    BOOST_CHECK_EQUAL(2,2);

    BOOST_FOREACH(boost::thread * thread, threads) {
        thread->interrupt();
        thread->join();
        delete thread;
    }

    BOOST_CHECK_EQUAL(2,2);
    boost::this_thread::sleep(boost::posix_time::seconds(1));
}

BOOST_AUTO_TEST_SUITE_END();
