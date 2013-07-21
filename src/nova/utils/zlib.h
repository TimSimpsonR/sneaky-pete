#ifndef _NOVA_UTILS_ZLIB_H
#define _NOVA_UTILS_ZLIB_H

#include <exception>
#include <memory>
#include <boost/optional.hpp>


namespace nova { namespace utils { namespace zlib {


enum ZlibBufferStatus {
    WAIT = 1,  // Tells zlib to return since more data isn't available yet.
               // Think of it like yielding from a coroutine.
    OK = 2,  // The buffer is ready to be used.
    FINISHED = 3  // The buffer is entirely finished and the routine should
                  // stop (like exiting from a coroutine). For input buffers,
                  // one additional write may be processed.
};


/**
 *  A data stream to be used by the classes below.
 *  Note that it returns a non-const buffer- this means zlib will write to it
 *  and in general complicates things. So the buffer needs to exist for
 *  at least as long as zlib since zlib will be using it.
 */
struct InputStream {
    virtual ~InputStream() {}

    virtual ZlibBufferStatus advance() = 0;
    virtual char * get_buffer() = 0;
    virtual size_t get_buffer_size() = 0;
};

struct OutputStream : public InputStream{
    virtual ~OutputStream() {}

    /* It is possible to stop a routine after being notified of the output,
     * just in case someone would want this. */
    virtual ZlibBufferStatus notify_written(const size_t count) = 0;
};


typedef boost::shared_ptr<InputStream> InputStreamPtr;

typedef boost::shared_ptr<OutputStream> OutputStreamPtr;


class ZlibBase {
    public:
        ZlibBase();

        virtual ~ZlibBase();

        /* This runs the routine with a input stream that simply states it's
         * finished. Use this if you have no more input data. */
        void finish_input_stream(OutputStreamPtr output);

        bool is_finished() const;

        bool needs_input() const;

        bool needs_output() const;

        /* Run the operation, reading from input and writing to output,
         * until one of the Streams returns FINISHED or WAIT. */
        void run_with_streams(InputStreamPtr input, OutputStreamPtr output);

        /* Run the operation, reading from the character array and writing
         * whatever can be written to the output stream. */
        void run_read_from(char * in_buffer, size_t max_size,
                           OutputStreamPtr output);

        /* Run the operation, reading from input and writing to the character
         * array until input is FINISHED or WAIT or the buffer is full. */
        size_t run_write_into(InputStreamPtr input,
                              char * out_buffer, size_t max_size);


        void set_input_buffer(InputStreamPtr input);

        void set_output_buffer(OutputStreamPtr output);

    protected:
        bool finished;

        void finish(bool can_throw=true);

        virtual void finalize(bool can_throw) = 0;

        ZlibBufferStatus get_input();

        ZlibBufferStatus get_output();

        ZlibBufferStatus notify_output_written();

        virtual void run() = 0;

        void * zlib_object;

    private:

        InputStreamPtr input_buffer;

        bool input_buffer_finished;

        OutputStreamPtr output_buffer;

        bool output_buffer_finished;
};


class ZlibCompressor : public ZlibBase {
    public:
        ZlibCompressor();

        ~ZlibCompressor();

        /* Compresses data from input, and writes it to output.
         * This runs until one of the streams returns FINISHED.
         * In theory, it could run forever. */
        //void run(InputStreamPtr input, OutputStreamPtr output);

        /* Compresses data from the input stream, and writes to the output
         * buffer given. May not be finished after the function exits. */
        //size_t run_write_into(InputStreamPtr input,
        //                      char * out_buffer, size_t max_size);

    protected:
        virtual void finalize(bool can_throw);

    private:
        bool last_input;

        virtual void run();
};


class ZlibDecompressor : public ZlibBase {
    public:
        ZlibDecompressor();

        ~ZlibDecompressor();

        //void run(InputStreamPtr input, OutputStreamPtr output);

        size_t decompress(char * output, const size_t output_size);

    protected:
        virtual void finalize(bool can_throw);

        virtual void run();
};


class ZlibException : public std::exception {
    public:
        ZlibException() throw();

        virtual ~ZlibException() throw();

        virtual const char * what() const throw();
};


} } } // end nova::utils::zlib;

#endif  // End _NOVA_UTILS_ZLIB_H
