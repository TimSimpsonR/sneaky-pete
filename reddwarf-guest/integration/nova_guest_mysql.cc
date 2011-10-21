#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python.hpp>

#include "nova/guest/mysql.h"
#include "nova/guest/mysql/MySqlPreparer.h"

char const* greet()
{
   return "hello, world";
}

using nova::guest::mysql::MySqlDatabase;
using nova::guest::mysql::MySqlDatabasePtr;
using nova::guest::mysql::MySqlGuest;
using nova::guest::mysql::MySqlGuestPtr;
using nova::guest::mysql::MySqlPreparer;
using nova::guest::mysql::MySqlPreparerPtr;
using nova::guest::mysql::MySqlUser;
using nova::guest::mysql::MySqlUserPtr;
using namespace boost::python;

// struct MySqlGuestPtrPy : public MySqlGuestPtr {
//     MySqlGuestPtrPy(const std::string & uri, const std::string & user,
//                     const std::string & password) {
//         this->reset(new MySqlGuest(uri, user, password));
//     }
// };

// struct MySqlGuestPy : public MySqlGuest {
//     MySqlGuestPy(const std::string & uri, const std::string & user,
//                  const std::string & password)
//     : MySqlGuest(uri, user, password) {}
// };

// struct MySqlPreparerPtrPy : public MySqlPreparerPtr {
//     MySqlPreparerPtrPy(MySqlGuestPtr & ptr) {
//         this->reset(new MySqlPreparer(ptr));
//     }
// };

BOOST_PYTHON_MODULE(nova_guest_mysql)
{
    // Its as easy as 1, 2, 3... 4, 5, 6, uhhh, maybe this was a bad idea, 7...
    class_<MySqlDatabase, boost::noncopyable, MySqlDatabasePtr >(
        "MySqlDatabase")
        .add_property("character_set",
                      make_function(&MySqlDatabase::get_character_set,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlDatabase::set_character_set)
        .add_property("collation",
                      make_function(&MySqlDatabase::get_collation,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlDatabase::set_collation)
        .add_property("name",
                      make_function(&MySqlDatabase::get_name,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlDatabase::set_name)
    ;

    class_<MySqlUser, boost::noncopyable, MySqlUserPtr >(
        "MySqlUser")
        .add_property("name",
                      make_function(&MySqlUser::get_name,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlUser::set_name)
        .add_property("password",
                      make_function(&MySqlUser::get_password,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlUser::set_password)
        .add_property("databases",
                      make_function(&MySqlUser::get_databases,
                                   return_value_policy<copy_const_reference>()),
                      &MySqlUser::set_databases)
    ;

    class_<MySqlGuest, boost::noncopyable, MySqlGuestPtr >("MySqlGuest",
        init<const std::string &, const std::string &, const std::string &>(
            args("uri", "user", "password")))
        .def("create_database", &MySqlGuest::create_database)
        .def("flush_privileges", &MySqlGuest::flush_privileges)
        .def("generate_password", &MySqlGuest::generate_password)
        .staticmethod("generate_password")
    ;
    // class_<MySqlGuestPy, MySqlGuestPtr>("MySqlGuest",
    //     init<const std::string &, const std::string &, const std::string &>(
    //         args("uri", "user", "password")))
    //     .def("flush_privileges", &MySqlGuest::flush_privileges)
    //     .def("generate_password", &MySqlGuest::generate_password)
    //     .staticmethod("generate_password")
    // ;

    class_<MySqlPreparer, boost::noncopyable, MySqlPreparerPtr>(
        "MySqlPreparer", init<MySqlGuestPtr &>())
        .def("create_admin_user", &MySqlPreparer::create_admin_user)
        .def("generate_root_password", &MySqlPreparer::generate_root_password)
        .def("init_mycnf", &MySqlPreparer::init_mycnf)
        .def("install_mysql", &MySqlPreparer::install_mysql)
        .def("remove_anonymous_user", &MySqlPreparer::remove_anonymous_user)
        .def("remove_remote_root_access",
            &MySqlPreparer::remove_remote_root_access)
        .def("restart_mysql", &MySqlPreparer::restart_mysql)
    ;

    def("greet", greet);
}


