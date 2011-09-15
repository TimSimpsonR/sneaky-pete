#define BOOST_TEST_MODULE Send_and_Receive
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(Test1)
{
	char * mem = new char[20];
	int x(1);
	int y(2);
	BOOST_CHECK_EQUAL(x, 1);
	BOOST_CHECK_EQUAL(y, 2);
	delete[] mem;
}


BOOST_AUTO_TEST_CASE(TestFail)
{
	char * mem = new char[20];
	mem[0] = 'x';
	int x(255);
	int y(255);
	BOOST_CHECK_EQUAL(x, 255);
	BOOST_CHECK_EQUAL(y, 255);

	BOOST_CHECK_EQUAL(x, 255);
	BOOST_CHECK_EQUAL(y, 255);
	delete[] mem;
}
