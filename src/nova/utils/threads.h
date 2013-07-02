#ifndef _NOVA_UTILS_THREADS_H
#define _NOVA_UTILS_THREADS_H

#include <boost/thread/condition_variable.hpp>
#include <functional>
#include <pthread.h>
#include <boost/shared_ptr.hpp>
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


/* A class which runs an action when called and can be safely cloned. */
class Job {
public:
    virtual ~Job()
    {
    }

    virtual void operator()() = 0;

    virtual Job * clone() const = 0;
};


/* A simple class that runs Jobs. */
class JobRunner {
public:
    ~JobRunner() {}

    virtual bool is_idle() = 0;

    virtual bool run(const Job & job) = 0;
};


/* A class which operates a job runner on another thread. */
class ThreadBasedJobRunner
:   public JobRunner,
    public Thread::Runner,
    private boost::noncopyable
{
public:
    ThreadBasedJobRunner();

    virtual ~ThreadBasedJobRunner();

    virtual bool is_idle();

    virtual void operator()();

    virtual bool run(const Job & job);

    /* Shuts down the runner, causing its thread to exit.
     * This is not a big deal in production but is necessary for unit tests. */
    void shutdown();
private:
    boost::condition_variable condition;

    void execute_job();

    bool _is_idle();

    Job * job;

    boost::mutex mutex;

    // True if the main routine has finished.
    bool is_shutdown_completed();

    // True if the main routine is currently running.
    bool is_shutdown_requested();

    bool shutdown_completed;

    bool shutdown_requested;
};



} } // end namespace nova::utils

#endif
