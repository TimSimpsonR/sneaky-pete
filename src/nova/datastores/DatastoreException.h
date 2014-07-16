#ifndef __NOVA_DATASTORES_DATASTOREEXCEPTION_H
#define __NOVA_DATASTORES_DATASTOREEXCEPTION_H

#include <exception>

namespace nova { namespace datastores {


    class DatastoreException : public std::exception {

        public:
            enum Code {
                COULD_NOT_START,
                COULD_NOT_STOP
            };

            DatastoreException(Code code) throw();

            virtual ~DatastoreException() throw();

            virtual const char * what() throw();

        private:
            Code code;

    };

} }

#endif

