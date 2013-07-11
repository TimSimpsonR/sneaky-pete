#define BOOST_TEST_MODULE SwiftTests
#include <boost/test/unit_test.hpp>

#include "nova/Log.h"
#include <boost/thread.hpp>
#include "nova/utils/threads.h"

using nova::LogApiScope;
using nova::LogOptions;
using namespace nova::utils;


void sleep_one() {
    boost::posix_time::seconds time(1);
    boost::this_thread::sleep(time);
}


namespace {
    int start_count;
    int finish_count;
    bool quit;

    struct TestJob : public Job {

        int cycle_count;
        bool finished;

        TestJob()
        :   cycle_count(0),
            finished(false)
        {}

        virtual void operator()() {
            ++ start_count;
            while(!quit) {
                ++ cycle_count;
                sleep_one();
            }
            ++ finish_count;
        }

        virtual Job * clone() const {
            return new TestJob(*this);
        }

    };
}

BOOST_AUTO_TEST_CASE(job_runner_tests)
{
    LogApiScope log(LogOptions::simple());

    ThreadBasedJobRunner runner;
    Thread thread(1024 * 1024, runner);


    BOOST_REQUIRE(runner.is_idle());

    start_count = finish_count = 0;
    quit = false;

    TestJob job;

    runner.run(job);
    // Wait an unbearably long (from the tests perspective) time.
    sleep_one();
    BOOST_REQUIRE(!runner.is_idle());
    BOOST_REQUIRE_EQUAL(start_count, 1);
    BOOST_REQUIRE_EQUAL(1, start_count);
    BOOST_REQUIRE(job.cycle_count == 0); // Prove job is being copied.

    quit = true;  // End job.
    sleep_one();
    sleep_one();

    BOOST_REQUIRE_EQUAL(1, finish_count);;
    BOOST_REQUIRE(runner.is_idle());

    // Now, set number back to 0 so the job won't quit, and as it is run,
    // try to run other jobs.
    quit = false;
    runner.run(job);
    sleep_one();
    BOOST_REQUIRE_EQUAL(start_count, 2);
    BOOST_REQUIRE_EQUAL(finish_count, 1);
    BOOST_REQUIRE(job.cycle_count == 0); // Prove job is being copied.

    for (unsigned int i = 0; i < 10000; i ++) {
        bool result = runner.run(job);
        BOOST_REQUIRE_EQUAL(result, false);
        BOOST_REQUIRE(!runner.is_idle());
        // Make sure no other jobs are run.
        BOOST_REQUIRE_EQUAL(start_count, 2);
        BOOST_REQUIRE_EQUAL(finish_count, 1);
    }

    // Let the job finish.
    quit = true;
    runner.shutdown(); // Avoid errors due to thread still running dead object.
}
