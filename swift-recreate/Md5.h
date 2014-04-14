#ifndef _NOVA_UTILS_MD5
#define _NOVA_UTILS_MD5


#include <exception>
#include <openssl/md5.h>
#include <string>


// TODO(tim.simpson): Replace placeholder with something real.
class Md5 {

public:
    Md5();

    /* Close this object out and return Md5. No future additions will be
       possible. */
    std::string finalize();

    void update(const char * buffer, size_t buffer_size);

private:
    MD5_CTX context;
    bool finalized;
};


class Md5FinalizedException : public std::exception {
    public:
        virtual const char * what() const throw();
};


#endif
