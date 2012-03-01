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

    LogTestsFixture(boost::optional<double> max_time_in_seconds=boost::none,
                    bool test_append=false)
    : log_file()
    {
        if (test_append) {
            // If testing append, re-use the last opened log file.
            -- test_count;
        }
        log_file = str(format("bin/log_tests_%d") % test_count);
        if (!test_append) {
            // Otherwise, first destroy the log file.
            remove(log_file.c_str());
        }
        LogFileOptions file_options(log_file, boost::optional<size_t>(10000),
                    max_time_in_seconds, 3);
        LogOptions options(optional<LogFileOptions>(file_options), false);
        nova::Log::initialize(options);
        ++ test_count;
    }

    ~LogTestsFixture() {
        nova::Log::shutdown();
    }

    void read_file(vector<string> & lines, int index=0) {
        string path = (index == 0) ? log_file
                      : (str(format("%s.%d") % log_file % index));
        std::ifstream file(path.c_str());
        if (!file.good()) {
            string msg = str(format("Could not open logging file %s!") % path);
            BOOST_FAIL(msg.c_str());
        }
        while(!file.eof()) {
            string line;
            std::getline(file, line);
            lines.push_back(line);
        }
        file.close();
    }

};

void check_log_line(const string msg, const string & actual_line,
                    const string & level, const string & expected_value) {
    string regex_string = str(format(
        "[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
        "\\s{1}%s %s for tests/log_tests.cc:[0-9]+") % level % expected_value);
    Regex regex(regex_string.c_str());
    RegexMatchesPtr matches = regex.match(actual_line.c_str());
    BOOST_CHECK_MESSAGE(!!matches, msg);
}

void check_expected_data_written(vector<string> & lines, const char * msg) {
    check_log_line(str(format("%s Line 0") % msg).c_str(),
                   lines[0], "DEBUG",
                   "Hello from the tests\\. How\'re you doing\\?");

    check_log_line(str(format("%s Line 1") % msg).c_str(),
                   lines[1], "ERROR",
                   "Hi");

    check_log_line(str(format("%s Line 2") % msg).c_str(),
                   lines[2], "INFO ",
                   "Bye");
}


// BOOST_FIXTURE_TEST_SUITE(LogTestsSuite,
//                          LogTestsFixture);

BOOST_AUTO_TEST_CASE(writing_some_lines) {
    LogTestsFixture log_fixture;
    NOVA_LOG_DEBUG("Hello from the tests. How're you doing?");
    NOVA_LOG_ERROR("Hi");
    NOVA_LOG_INFO("Bye");

    /* Read the log file and confirm it works. */
    vector<string> lines;
    log_fixture.read_file(lines);
    check_expected_data_written(lines, "writing_some_lines");
}

BOOST_AUTO_TEST_CASE(writing_some_lines_and_appending_on_restart) {
    {
        LogTestsFixture log_fixture;
        NOVA_LOG_DEBUG("Hello from the tests. How're you doing?");
        NOVA_LOG_ERROR("Hi");
        NOVA_LOG_INFO("Bye");
        NOVA_LOG_INFO("ps, this was written in the append test");

        /* Read the log file and confirm it works. */
        vector<string> lines;
        log_fixture.read_file(lines);
        check_expected_data_written(lines, "write_and_append");
    }

    {
        // Act like Sneaky Pete has restarted. Make sure it appends to the
        // original file.
        LogTestsFixture log_fixture(boost::none, true);
        NOVA_LOG_INFO("Hi again. I hope I'm not overwriting anything.");
        NOVA_LOG_DEBUG(":)");
        NOVA_LOG_ERROR("Adios.");

        /* Make sure the old lines are still there... */
        vector<string> lines;
        log_fixture.read_file(lines);
        check_expected_data_written(lines, "write_after_append_2");
        check_log_line("write_and_append_2 line 4",
                       lines[4], "INFO ",
                       "Hi again\\. I hope I\'m not overwriting anything\\.");

        check_log_line("write_and_append_2 line 5",
                       lines[5], "DEBUG", "\\:\\)");

        check_log_line("write_and_append_2 line 6",
                       lines[6], "ERROR", "Adios\\.");
    }
}

BOOST_AUTO_TEST_CASE(writing_some_lines_and_rotating) {
    LogTestsFixture log_fixture;
    NOVA_LOG_DEBUG("Hello from the tests. How're you doing?");
    nova::Log::rotate_files();
    NOVA_LOG_ERROR("Hi");
    nova::Log::rotate_files();
    NOVA_LOG_INFO("Bye");

    /* Read the log file and confirm it works. */
    {
        vector<string> lines;
        log_fixture.read_file(lines, 2);
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]{2}\\s{1}[0-9]{2}:[0-9]{2}:[0-9]{2}"
                    "\\s{1}DEBUG Hello from the tests\\. How\'re you doing\\? "
                    "for tests/log_tests.cc:[0-9]+");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        vector<string> lines;
        log_fixture.read_file(lines, 1);
        Regex regex("[0-9]{4}-[0-9]{2}-[0-9]");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }

    {
        vector<string> lines;
        log_fixture.read_file(lines);
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
            if (id == 50) {
                nova::Log::rotate_logs_if_needed();
            }
            NOVA_LOG_DEBUG2("id=%d stage=%c", id, stage);
            NOVA_LOG_ERROR2("id=%d stage=%c", id, stage);
            NOVA_LOG_INFO2("id=%d stage=%c", id, stage);
            boost::posix_time::milliseconds time(100);
            boost::this_thread::sleep(time);
        }
    }
};

BOOST_AUTO_TEST_CASE(writing_lines_in_a_thread) {
    LogTestsFixture log_fixture;
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
        BOOST_FOREACH(Worker & worker, workers) {
            CHECK_POINT();
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
    LogTestsFixture log_fixture;
    NOVA_LOG_INFO("Welcome to Test #3.");
    const int worker_count = 10;
    vector<boost::thread *> threads;
    vector<Worker> workers;
    for (int i = 20; i < worker_count + 20; i ++) {
        workers.push_back(Worker(i));
    }
    BOOST_FOREACH(Worker & worker, workers) {
        CHECK_POINT();
        threads.push_back(new boost::thread(&Worker::work, worker));
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
        BOOST_FOREACH(Worker & worker, workers) {
            CHECK_POINT();
            worker.next_state();
        }
        nova::Log::rotate_files();
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

BOOST_AUTO_TEST_CASE(writing_lines_to_threads_while_switching_files_2) {
    LogTestsFixture log_fixture;
    NOVA_LOG_INFO("Welcome to Test #4.");
    const int worker_count = 10;
    vector<boost::thread *> threads;
    vector<Worker> workers;
    // The ID of 50 will cause that thread to rotate the logs if needed.
    for (int i = 50; i < worker_count + 50; i ++) {
        workers.push_back(Worker(i));
    }
    BOOST_FOREACH(Worker & worker, workers) {
        CHECK_POINT();
        threads.push_back(new boost::thread(&Worker::work, worker));
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
        BOOST_FOREACH(Worker & worker, workers) {
            CHECK_POINT();
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
    boost::this_thread::sleep(boost::posix_time::seconds(1));
}

BOOST_AUTO_TEST_CASE(writing_some_lines_and_waiting_for_rotate) {
    LogTestsFixture log_fixture(boost::optional<double>(2));
    NOVA_LOG_DEBUG("START!");
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    nova::Log::rotate_logs_if_needed();
    NOVA_LOG_ERROR("Finish");
    boost::this_thread::sleep(boost::posix_time::seconds(2));
    nova::Log::rotate_logs_if_needed();
    // rotate
    NOVA_LOG_DEBUG("I am the next log.");
    nova::Log::rotate_logs_if_needed();
    {
        vector<string> lines;
        log_fixture.read_file(lines, 1);
        Regex regex("START");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }
    {
        vector<string> lines;
        log_fixture.read_file(lines);
        Regex regex("I am the next log");
        RegexMatchesPtr matches = regex.match(lines[0].c_str());
        BOOST_CHECK_EQUAL(!!matches, true);
    }


}
