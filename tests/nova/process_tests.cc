#define BOOST_TEST_MODULE process_tests
#include <boost/test/unit_test.hpp>

#include <boost/assign/list_of.hpp>
#include "nova/Log.h"
#include "nova/process.h"
#include <stdlib.h>

using namespace nova;
using namespace nova::process;
using std::string;
using std::stringstream;
using namespace boost::assign;

#define CHECK_POINT BOOST_REQUIRE_EQUAL(2,2);

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
        setenv("food", "", 1);
    }

};

BOOST_GLOBAL_FIXTURE(GlobalFixture);

/**---------------------------------------------------------------------------
 *- Process Tests
 *---------------------------------------------------------------------------*/
BOOST_AUTO_TEST_CASE(sending_to_stdin)
{
    CommandList cmds = list_of(parrot_path())("wake");
    Process<StdErrAndStdOut, StdIn> process(cmds);

    // Parrot program sends a first message to its stdout, then waits
    // for a response.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1);
        BOOST_REQUIRE_EQUAL(bytes_read, 22);
        BOOST_REQUIRE_EQUAL("(@'> <( Hello! AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(process.is_finished(), false);
    }

    // Polling a second time will show no new input.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1);
        BOOST_REQUIRE_EQUAL(bytes_read, 0);
        BOOST_REQUIRE_EQUAL("", std_out.str());
        BOOST_REQUIRE_EQUAL(process.is_finished(), false);
    }

    process.write("hi...\n");
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 1);
        BOOST_REQUIRE_EQUAL(bytes_read, 21);
        BOOST_REQUIRE_EQUAL("(@'> <( hi... AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(process.is_finished(), false);
    }

    // Again, polling a second time will show no new input.
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 2);
        BOOST_REQUIRE_EQUAL(bytes_read, 0);
        BOOST_REQUIRE_EQUAL("", std_out.str());
        BOOST_REQUIRE_EQUAL(process.is_finished(), false);
    }

    process.write("die\n");
    {
        stringstream std_out;
        size_t bytes_read = process.read_until_pause(std_out, 5);
        BOOST_REQUIRE_EQUAL("(@'> <( I am dead! AWK! )\n", std_out.str());
        BOOST_REQUIRE_EQUAL(bytes_read, 26);
        BOOST_REQUIRE_EQUAL(process.is_finished(), true);
    }
}

BOOST_AUTO_TEST_CASE(sending_to_stderr)
{
    CHECK_POINT;
    CommandList cmds = list_of(parrot_path());
    Process<StdErrAndStdOut> process(cmds);
    stringstream std_out;
    size_t bytes_read = process.read_until_pause(std_out, 2);
    BOOST_REQUIRE_EQUAL("(@'> <( zzz )\n", std_out.str());
    BOOST_REQUIRE_EQUAL(bytes_read, 14);
    BOOST_REQUIRE_EQUAL(process.is_finished(), true);
}

BOOST_AUTO_TEST_CASE(execute_test) {
    CHECK_POINT;
    const double TIME_OUT = 4.0;
    // Returns zero exit code.
    CommandList cmds = list_of(parrot_path())("chirp");
    execute(cmds, TIME_OUT);

    try {
        CommandList cmds = list_of(parrot_path());
        execute(cmds, TIME_OUT);
        BOOST_FAIL("Should have thrown.");
    } catch(const ProcessException & pe) {
        BOOST_REQUIRE_EQUAL(ProcessException::EXIT_CODE_NOT_ZERO, pe.code);
    }
}

BOOST_AUTO_TEST_CASE(environment_variables_should_transfer_part_1_it_fails) {
    CHECK_POINT;
    const double TIME_OUT = 4.0;
    // Returns zero exit code.
    CommandList cmds = list_of(parrot_path())("eat");

    stringstream out;
    execute(out, cmds, TIME_OUT);
    BOOST_CHECK_EQUAL(out.str(), "(@'> <( I can't eat this! AWK! )\n");
}

BOOST_AUTO_TEST_CASE(environment_variables_should_transfer_part_2_it_fails) {
    CHECK_POINT;
    NOVA_LOG_TRACE("\n\nENVIRO TEST\n\n\n");
    const double TIME_OUT = 4.0;
    CommandList cmds = list_of(parrot_path())("eat");
    setenv("food", "birdseed", 1);
    stringstream out2;
    execute(out2, cmds, TIME_OUT);
    BOOST_CHECK_EQUAL(out2.str(), "(@'> < * crunch * )\n");
    CHECK_POINT;
}

void test_independent_redirects(CommandList & cmds) {
    Process<IndependentStdErrAndStdOut> process(cmds);
    stringstream err, out;
    char buffer[5];
    IndependentStdErrAndStdOut::ReadResult result;
    while((result = process.read_into(buffer, sizeof(buffer) - 1, 60))
          .file != IndependentStdErrAndStdOut::ReadResult::NA)
    {
        NOVA_LOG_TRACE("HI!")
        stringstream & stream =
            (result.file == IndependentStdErrAndStdOut::ReadResult::StdOut
             ?  out
             :  err);
        buffer[result.write_length] = '\0';
        stream << buffer;
    }
    BOOST_CHECK_EQUAL("(@'> <( Hi from StdOut. AWK! )\n", out.str().c_str());
    BOOST_CHECK_EQUAL("(@'> <( Hi from StdErr. )\n", err.str().c_str());
}

BOOST_AUTO_TEST_CASE(independent_redirects) {
    CommandList cmds = list_of(parrot_path())("duo");
    test_independent_redirects(cmds);
}

BOOST_AUTO_TEST_CASE(independent_redirects2) {
    CommandList cmds = list_of(parrot_path())("duo2");
    test_independent_redirects(cmds);
}

BOOST_AUTO_TEST_CASE(test_giga_flood) {
    CommandList cmds = list_of(parrot_path())("giga-flood");
    Process<IndependentStdErrAndStdOut> process(cmds);
    stringstream err, out;
    char buffer[1024];
    IndependentStdErrAndStdOut::ReadResult result;

    bool expecting_eof_stdout = false;
    bool expecting_eof_stderr = false;

    size_t actual_stdout_count = 0;
    size_t actual_stderr_count = 0;

    while(!process.is_finished())
    // while((result = process.read_into(buffer, sizeof(buffer) - 1, 60))
    //       .file != IndependentStdErrAndStdOut::ReadResult::NA)
    {
        result = process.read_into(buffer, sizeof(buffer) - 1, 60);
        if (IndependentStdErrAndStdOut::ReadResult::StdOut == result.file) {
            //BOOST_REQUIRE_EQUAL(expecting_eof_stdout, false);
            actual_stdout_count += result.write_length;
        } else {
            //BOOST_REQUIRE_EQUAL(expecting_eof_stderr, false);
            actual_stderr_count += result.write_length;
        }
        const char expected_char =
            IndependentStdErrAndStdOut::ReadResult::StdOut == result.file
            ? '1' : '2';
        for (size_t i = 0; i < result.write_length; i ++) {
            if (expected_char != buffer[i]) {
                NOVA_LOG_ERROR2("Failure at index %d", i);
                BOOST_REQUIRE_EQUAL(true, false);
            }
        }
        if (result.write_length < sizeof(buffer) - 1) {
            if (IndependentStdErrAndStdOut::ReadResult::StdOut == result.file) {
                expecting_eof_stdout = true;
            } else {
                expecting_eof_stderr = true;
            }
        }
    }
    BOOST_REQUIRE_EQUAL(actual_stdout_count, 1024 * 1024);
    BOOST_REQUIRE_EQUAL(actual_stderr_count, 1024 * 1024);
}
