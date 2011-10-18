#define BOOST_TEST_MODULE process_tests
#include <boost/test/unit_test.hpp>

#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"

using namespace nova;
using std::string;
using std::stringstream;
using namespace boost::assign;

const char * parrot_path() {
    return "/src/bin/gcc-4.4.3/debug/parrot";
}

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
    Process::execute(list_of(parrot_path())("chirp"), TIME_OUT);

    try {
        Process::execute(list_of(parrot_path()), TIME_OUT);
        BOOST_FAIL("Should have thrown.");
    } catch(const ProcessException & pe) {
        BOOST_REQUIRE_EQUAL(ProcessException::EXIT_CODE_NOT_ZERO, pe.code);
    }
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
