#include "pch.hpp"
#include "nova/Log.h"
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <exception>
#include <boost/lexical_cast.hpp>
#include "nova/utils/subsecond.h"
#include <boost/format.hpp>

using namespace nova;
using boost::format;
using nova::utils::subsecond::now;

int main(int argc, char* argv[]) {
    LogApiScope log(LogOptions::simple());

    int repetitions = 2;

    if (argc > 1) {
        int reps = boost::lexical_cast<int>(argv[1]);
        repetitions = (reps > repetitions) ? reps : repetitions;
    }

    std::cout << "Start the clock. Running " << repetitions << " times." << std::endl;

    while(repetitions > 0){
        std::cout << "    " << str(format("%8.8f") % now()) << std::endl;
        repetitions--;
    }

    std::cout << "Done." << std::endl;

}
