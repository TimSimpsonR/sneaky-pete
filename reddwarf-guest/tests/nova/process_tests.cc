#define BOOST_TEST_MODULE process_tests
#include <boost/test/unit_test.hpp>

#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"
#include <stdlib.h>

using namespace nova;
using std::string;
using std::stringstream;
using namespace boost::assign;


static string path;

const char * parrot_path() {
    const char * value = getenv("AGENT_DIR");
    if (value != 0) {
        std::stringstream path_s;
        path_s << value << "/parrot/parrot_e";
        path = path_s.str();
        return path.c_str();
    } else {
        return "parrot/parrot_e";
    }
}


struct GlobalFixture {

    LogApiScope log;

    GlobalFixture()
    : log(LogOptions::simple()) {
    }

};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

/**---------------------------------------------------------------------------
 *- Process Tests
 *---------------------------------------------------------------------------*/
BOOST_AUTO_TEST_CASE(sending_to_stdin)
{
    Process::CommandList cmds = list_of(parrot_path())("wake");
    Process process(cmds);

    // Parrot program sends a first message to its stdout, then waits
    // for a response.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1, 4.0);
        BOOST_REQUIRE_EQUAL(bytes_read, 22);
        BOOST_REQUIRE_EQUAL("(@'> <( Hello! AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(process.eof(), false);
    }

    // Polling a second time will show no new input.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1, 4.0);
        BOOST_REQUIRE_EQUAL(bytes_read, 0);
        BOOST_REQUIRE_EQUAL("", std_out.str());
        BOOST_REQUIRE_EQUAL(process.eof(), false);
    }

    process.write("hi...\n");
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1, 4.0);
        BOOST_REQUIRE_EQUAL(bytes_read, 21);
        BOOST_REQUIRE_EQUAL("(@'> <( hi... AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(process.eof(), false);
    }

    // Again, polling a second time will show no new input.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 2, 4.0);
        BOOST_REQUIRE_EQUAL(bytes_read, 0);
        BOOST_REQUIRE_EQUAL("", std_out.str());
        BOOST_REQUIRE_EQUAL(process.eof(), false);
    }

    process.write("die\n");
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 5, 4.0);
        BOOST_REQUIRE_EQUAL("(@'> <( I am dead! AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(bytes_read, 26);
        BOOST_REQUIRE_EQUAL(process.eof(), true);
    }
}

BOOST_AUTO_TEST_CASE(sending_to_stderr)
{
    Process::CommandList cmds = list_of(parrot_path());
    Process process(cmds);
    stringstream std_out;
    size_t bytes_read = process.read_until_pause(std_out, 2, 4.0);
    BOOST_REQUIRE_EQUAL("(@'> <( zzz )\n", std_out.str());
    BOOST_REQUIRE_EQUAL(bytes_read, 14);
    BOOST_REQUIRE_EQUAL(process.eof(), true);
}

BOOST_AUTO_TEST_CASE(execute_test) {
    const double TIME_OUT = 4.0;
    // Returns zero exit code.
    Process::CommandList cmds = list_of(parrot_path())("chirp");
    Process::execute(cmds, TIME_OUT);

    try {
        Process::CommandList cmds = list_of(parrot_path());
        Process::execute(cmds, TIME_OUT);
        BOOST_FAIL("Should have thrown.");
    } catch(const ProcessException & pe) {
        BOOST_REQUIRE_EQUAL(ProcessException::EXIT_CODE_NOT_ZERO, pe.code);
    }
}

BOOST_AUTO_TEST_CASE(environment_variables_should_transfer) {
    const double TIME_OUT = 4.0;
    // Returns zero exit code.
    Process::CommandList cmds = list_of(parrot_path())("eat");

    stringstream out;
    Process::execute(out, cmds, TIME_OUT);
    BOOST_CHECK_EQUAL(out.str(), "(@'> <( I can't eat this! AWK! )\n");

    setenv("food", "birdseed", 1);
    stringstream out2;
    Process::execute(out2, cmds, TIME_OUT);
    BOOST_CHECK_EQUAL(out2.str(), "(@'> < * crunch * )\n");
}


//TODO: Need a test for a process which outputs infinite data to standard out.

/*
BOOST_AUTO_TEST_CASE(read_until_pause_will_time_out_even_with_infinite_input)
{
    const char * program_path = "/src/bin/gcc-4.4.3/debug/parrot";
    const char * const argv[] = {"babble", (char *) 0};
    Process process(program_path, argv);
    stringstream std_out;
    size_t bytes_read = process.read_until_pause(std_out, 6, 3.0);
    BOOST_REQUIRE(bytes_read > 1024);
    BOOST_REQUIRE_EQUAL("(@'> <( zzz )\n", std_out.str().substr(0, 30));
    BOOST_REQUIRE_EQUAL(process.eof(), false);
}


BOOST_AUTO_TEST_CASE(gdfhd)
{
    const char * program_path = "/src/bin/gcc-4.4.3/debug/parrot";
    const char * const argv[] = {"wake", (char *) 0};
    Process process(program_path, argv);
    stringstream std_out;
    size_t bytes_read = process.read_until_pause(std_out, 9, 3.0);
    bytes_read = process.read_until_pause(std_out, 9, 3.0);
    BOOST_REQUIRE(bytes_read > 1024);
    BOOST_REQUIRE_EQUAL("(@'> <( zzz )\n", std_out.str().substr(0, 30));
    BOOST_REQUIRE_EQUAL(process.eof(), false);
}*/
