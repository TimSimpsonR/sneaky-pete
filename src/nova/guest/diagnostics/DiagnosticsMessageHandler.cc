#include "nova/guest/diagnostics.h"

#include "nova/guest/GuestException.h"
#include "nova/Log.h"

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;

namespace nova { namespace guest { namespace diagnostics {

namespace {
    struct Chunk {
        Chunk * next;
        char block[1024 * 1024];

        Chunk() {
        }

        ~Chunk() {
            if (0 != next) {
                delete next;
            }
        }
    };
}  // end anonymous namespace


// This recursive function is doomed to meet an end most foul.
int cursed_recursion() {
    char block[1024 * 1024];
    int * random = reinterpret_cast<int *>(&block);
    NOVA_LOG_DEBUG("The curse continues...");
    int answer = cursed_recursion() +  *random;
    return answer;
}


void crash_via_heap() {
    NOVA_LOG_DEBUG("Heap exhaustion time!");
    // This continously creates objects on the heap until memory
    // is exhausted. At that point, an out of memory exception
    // should be thrown and all the memory will be reclaimed.
    Chunk original;
    Chunk * itr = &original;
    while(true) {
        NOVA_LOG_DEBUG("This is exhausting!");
        itr->next = new Chunk();
        itr = itr->next;
    }
}


void crash_via_stack() {
    NOVA_LOG_DEBUG("Stack exhaustion time!");
    const auto answer = cursed_recursion();
    NOVA_LOG_DEBUG2("Answer from pointless function: %d", answer);
}


DiagnosticsMessageHandler::DiagnosticsMessageHandler(bool enabled)
: enabled(enabled) {
}


JsonDataPtr DiagnosticsMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the handle_message method now ");
    if (enabled) {
        if (input.method_name == "crash_via_heap") {
            crash_via_heap();
            return JsonData::from_null();
        } else if (input.method_name == "crash_via_stack") {
            crash_via_stack();
            return JsonData::from_null();
        }
    }  // end if enabled
    return JsonDataPtr();
}


} } } // end namespace nova::guest::diagnostics
