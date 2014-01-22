#include "pch.hpp"
#include <string.h>
#include <time.h>

namespace nova { namespace utils { namespace subsecond {

double now(){
    time_t seconds = time(NULL);
    struct timespec subs;
    tm * tmp = gmtime(&seconds);
    if(tmp == NULL) {return 0.0;}
    clock_gettime(CLOCK_MONOTONIC, &subs);
    double subseconds = subs.tv_nsec / 1000000000.0;
    return ((double)((long int)seconds)) + subseconds;
}

}}}
