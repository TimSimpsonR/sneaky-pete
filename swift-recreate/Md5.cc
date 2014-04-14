#include "Md5.h"


using std::string;



/**---------------------------------------------------------------------------
 *- nova::utils::Md5
 *---------------------------------------------------------------------------*/

Md5::Md5()
:   finalized(false)
{
    MD5_Init(&context);
}

std::string Md5::finalize() {
    if (finalized) {
        throw Md5FinalizedException();
    }
    unsigned char digest[16];
    MD5_Final(digest, &context);
    finalized = true;
    char mdString[33];
    for(int i = 0; i < 16; i++) {
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    }
    return string(mdString);
}

void Md5::update(const char * buffer, size_t buffer_size) {
    if (finalized) {
        throw Md5FinalizedException();
    }
    MD5_Update(&context, buffer, buffer_size);
}

/**---------------------------------------------------------------------------
 *- nova::utils::Md5FinalizedException
 *---------------------------------------------------------------------------*/

const char * Md5FinalizedException::what() const throw() {
    return "Md5 was already finalized and cannot be updated.";
}


