#ifndef _NOVA_UTILS_MD5
#define _NOVA_UTILS_MD5


#include <string>


namespace nova { namespace utils {

// TODO(tim.simpson): Replace placeholder with something real.
class Md5 {

public:
    Md5();

    void add(const char * buffer, size_t buffer_size);

    /* Close this object out and return Md5. No future additions will be
       possible. */
    std::string finish();

};

} } // end nova::utils

#endif
