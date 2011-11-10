#ifndef __NOVA_GUEST_MYSQL_MYSQLGUESTEXCEPTION_H
#define __NOVA_GUEST_MYSQL_MYSQLGUESTEXCEPTION_H


#include <exception>


namespace nova { namespace guest { namespace mysql {

    class MySqlGuestException : public std::exception {

        public:
            enum Code {
                CANT_WRITE_TMP_MYCNF,
                GENERAL,
                GUEST_INSTANCE_ID_NOT_FOUND,
                NO_PASSWORD_FOR_CREATE_USER
            };

            MySqlGuestException(Code code) throw();

            virtual ~MySqlGuestException() throw();

            static const char * code_to_string(Code code);

            virtual const char * what() const throw();

            const Code code;
    };

} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
