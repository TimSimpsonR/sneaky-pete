#ifndef _NOVA_UTILS_THREADS_H
#define _NOVA_UTILS_THREADS_H

#include <functional>
#include <pthread.h>
#include <boost/utility.hpp>


namespace nova { namespace utils {

class Thread : boost::noncopyable
{
public:

    // Starting point of a thread.
    class Runner {
    public:
        virtual ~Runner()
        {
        }

        virtual void operator()() = 0;
    };

    Thread(const size_t stack_size, Runner & runner);

    ~Thread();

private:

    pthread_t thread_id;

};

class ThreadException : public std::exception {

    public:
        enum Code {
            ATTR_DESTROY_ERROR,
            ATTR_DETACH_SET_ERROR,
            ATTR_INIT_ERROR,
            ATTR_SET_STACKSIZE_ERROR,
            CTOR_ERROR,
            DTOR_ERROR
        };

        ThreadException(Code code) throw();

        virtual ~ThreadException() throw();

        virtual const char * what() const throw();

    private:
        Code code;
};


} } // end namespace nova::utils

#endif
