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



} } // end namespace nova::utils
