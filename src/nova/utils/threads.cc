#include "pch.hpp"
#include "threads.h"
#include "../Log.h"

namespace nova { namespace utils {


namespace {

    void * thread_start(void * arg) {
        Thread::Runner & runner = *(reinterpret_cast<Thread::Runner *>(arg));
        runner();
        return NULL;
    }

    class Attr {
    public:
        Attr() {
            if (0 != pthread_attr_init(&attr)) {
                throw ThreadException(ThreadException::ATTR_INIT_ERROR);
            }
        }

        ~Attr() {
            if (0 != pthread_attr_destroy(&attr)) {
                NOVA_LOG_ERROR("Error destroying thread attrs. Weird!");
            }
        }

        pthread_attr_t * get() {
            return &attr;
        }

        void set_detach_state() {
            if (0 != pthread_attr_setdetachstate(&attr,
                                                 PTHREAD_CREATE_DETACHED)) {
                throw ThreadException(ThreadException::ATTR_DETACH_SET_ERROR);
            }
        }

        void set_stack_size(const size_t size) {
            if (0 != pthread_attr_setstacksize(&attr, size)) {
                throw ThreadException(ThreadException::ATTR_SET_STACKSIZE_ERROR);
            }
        }

    private:
        pthread_attr_t attr;
    };

} // end anonymous namespace

/**---------------------------------------------------------------------------
 *- Thread
 *---------------------------------------------------------------------------*/

Thread::Thread(const size_t stack_size, Thread::Runner & runner) {
    Attr attr;
    attr.set_detach_state();
    attr.set_stack_size(stack_size);
    if (0 != pthread_create(&thread_id, attr.get(), &thread_start, &runner)) {
        throw ThreadException(ThreadException::CTOR_ERROR);
    }
}

Thread::~Thread() {
}


/**---------------------------------------------------------------------------
 *- ThreadException
 *---------------------------------------------------------------------------*/

ThreadException::ThreadException(Code code) throw()
: code(code) {
}

ThreadException::~ThreadException() throw() {
}

const char * ThreadException::what() const throw() {
    switch(code) {
        case ATTR_DESTROY_ERROR:
            return "Error destroying attribute.";
        case ATTR_DETACH_SET_ERROR:
            return "Error setting detached state.";
        case ATTR_INIT_ERROR:
            return "Error initializing thread.";
        case ATTR_SET_STACKSIZE_ERROR:
            return "Error setting stack size.";
        case CTOR_ERROR:
            return "Error constructing thread.";
        case DTOR_ERROR:
            return "Error destroying thread.";
        default:
            return "An error occurred.";
    }
}


/**---------------------------------------------------------------------------
 *- ThreadBasedJobRunner
 *---------------------------------------------------------------------------*/

ThreadBasedJobRunner::ThreadBasedJobRunner()
:   condition(),
    job(0),
    mutex(),
    shutdown_completed(false),
    shutdown_requested(false)
{
}

ThreadBasedJobRunner::~ThreadBasedJobRunner() {
    if (0 != job) {
        NOVA_LOG_ERROR("Destroying the job runner, but the last job was never "
                       "finished!");
        delete job;
        job = 0;
    }
}

void ThreadBasedJobRunner::execute_job() {
    if (0 == job) {
        NOVA_LOG_ERROR("Error- can't execute null job!");
        return;
    }
    NOVA_LOG_INFO("Running job!");
#ifndef _DEBUG
    try {
#endif
        (*job)();
        NOVA_LOG_INFO("Job finished successfully.");
#ifndef _DEBUG
    } catch (const std::exception & e) {
        NOVA_LOG_ERROR("Error running job!: %s", e.what());
    } catch(...) {
        NOVA_LOG_ERROR("Error executing job! Exception type unknown.");
    }
#endif
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        delete job;  // Kill job variable.
        job = 0;
    }
}

void ThreadBasedJobRunner::operator()() {
    Log::initialize_job_thread();
    NOVA_LOG_INFO("Starting Job Runner thread...");
    while(!is_shutdown_requested()) {
        NOVA_LOG_INFO("Waiting for job...");
        {
            while (_is_idle() && !is_shutdown_requested()) {
                boost::posix_time::seconds time(1);
                boost::this_thread::sleep(time);
            }
        }
        if (!is_shutdown_requested()) {
            execute_job();
        }
    }
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        shutdown_completed = true;
    }
}

bool ThreadBasedJobRunner::is_idle() {
    boost::lock_guard<boost::mutex> lock(mutex);
    return _is_idle();
}

bool ThreadBasedJobRunner::_is_idle() {
    return 0 == job;
}

bool ThreadBasedJobRunner::is_shutdown_completed() {
    boost::lock_guard<boost::mutex> lock(mutex);
    return shutdown_completed;
}

bool ThreadBasedJobRunner::is_shutdown_requested() {
    boost::lock_guard<boost::mutex> lock(mutex);
    return shutdown_requested;
}

bool ThreadBasedJobRunner::run(const Job & job) {
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        if (!_is_idle()) {
            NOVA_LOG_INFO("Can't run job because thread isn't idle.");
            return false;
        }
        this->job = job.clone();
    }
    condition.notify_one();
    return true;
}

void ThreadBasedJobRunner::shutdown() {
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        shutdown_requested = true;
    }
    while(!is_shutdown_completed()) {
        boost::posix_time::seconds time(1);
        boost::this_thread::sleep(time);
    }

    {
        boost::lock_guard<boost::mutex> lock(mutex);
        shutdown_completed = true;
    }
}


} } // end namespace nova::utils
