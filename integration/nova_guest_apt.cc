#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python.hpp>

#include "nova/guest/apt.h"

using namespace boost::python;


BOOST_PYTHON_MODULE(nova_guest_apt)
{
    def("install", nova::guest::apt::install);
    def("remove", nova::guest::apt::remove);
    def("version", nova::guest::apt::version);
}
