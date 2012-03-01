/*
 * This is a program used to test the Process class which is like "cat" with
 * more noise.
 */
#include <iostream>
#include "nova/Log.h"
#include <stdlib.h>
#include <string>
#include <string.h>


using namespace nova;
using namespace std;

static void speak(const char * msg) {
    cout << "(@'> <( " << msg << " AWK! )" << endl;;
}

static void error(const char * msg) {
    cerr << "(@'> <( " << msg << " )" << endl;;
}

/* If the first argument is not "wake" the parrot snores and exits.
 * Otherwise, it repeats back every line entered in stdin until it sees the
 * word "die" at which point it does so (don't worry, its just a neat trick,
 * the real parrot doesn't actually die). */
int main(int argc, const char* argv[]) {
    if (argc >= 2 && strncmp(argv[1], "wake", 4) == 0) {
        speak("Hello!");
        string next = "";
        while(true) {
            next = "0";
            getline(cin, next);
            if (cin.eof()) {
                error("End of stdin.");
                return 1;
            } else if (cin.bad() || cin.fail()) {
                error("Something went wrong with stdin!");
                return 217;
            } else if (next == "die") {
                speak("I am dead!");
                return 0;
            } else {
                speak(next.c_str());
            }
        }
    } else if (argc >= 2 && strncmp(argv[1], "babble", 6) == 0) {
        while(true) {
            speak("blah");
        }
    } else if (argc >= 2 && strncmp(argv[1], "chirp", 5) == 0) {
        cout << "(@'> < * chirp * )" << endl;
        return 0;
    } else if (argc >= 2 && strncmp(argv[1], "eat", 3) == 0) {
        char * food = getenv("food");
        if (food == 0 || strncmp(food, "birdseed", 8) != 0) {
            speak("I can't eat this!");
        } else {
            cout << "(@'> < * crunch * )" << endl;
        }
    } else {
        error("zzz");
        exit(57);
    }
}
