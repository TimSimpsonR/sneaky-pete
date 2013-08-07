
#include "pch.hpp"
#include "nova/utils/zlib.h"

#include <nova/Log.h>
#include <memory>
#include <zlib.h>

// For a good example of how Zlib work check out this:
// http://www.zlib.net/zpipe.c
// For more examples, try Google or your local library!

namespace nova { namespace utils { namespace zlib {


#define MY_Z_STREAM (reinterpret_cast<z_stream *>(this->zlib_object))


/**---------------------------------------------------------------------------
 *- ZlibBase
 *---------------------------------------------------------------------------*/

ZlibBase::ZlibBase()
:   finished(false),
    zlib_object(new z_stream()),
    input_buffer(),
    input_buffer_finished(false),
    output_buffer(),
    output_buffer_finished(false)
{
}

ZlibBase::~ZlibBase() {
    delete MY_Z_STREAM;
}

void ZlibBase::finish(bool can_throw) {
    if (!finished) {
        finished = true;
        finalize(can_throw);
    }
}

ZlibBufferStatus ZlibBase::get_input() {
    if (input_buffer_finished) {
        return FINISHED;
    }
    const ZlibBufferStatus status = input_buffer->advance();
    switch(status) {
        case FINISHED:
            input_buffer_finished = true;
            NOVA_LOG_DEBUG("End of zlib input detected.");
            break;
        case WAIT:
            NOVA_LOG_TRACE("Waiting for more input buffer space.");
            break;
        case OK:
            MY_Z_STREAM->next_in =
                reinterpret_cast<uint8_t *>(input_buffer->get_buffer());
            MY_Z_STREAM->avail_in = input_buffer->get_buffer_size();
            break;
        default:
            NOVA_LOG_ERROR("Unknown status in get_input!");
            throw ZlibException();
            break;
    }
    return status;
}

ZlibBufferStatus ZlibBase::get_output() {
    if (output_buffer_finished) {
        return FINISHED;
    }
    const ZlibBufferStatus status = output_buffer->advance();
    switch(status) {
        case FINISHED:
            output_buffer_finished = true;
            NOVA_LOG_DEBUG("End of zlib output detected.");
            break;
        case WAIT:
            NOVA_LOG_TRACE("Waiting for more output buffer space.");
            break;
        case OK:
            MY_Z_STREAM->next_out =
                reinterpret_cast<uint8_t *>(output_buffer->get_buffer());
            MY_Z_STREAM->avail_out = output_buffer->get_buffer_size();
            break;
        default:
            NOVA_LOG_ERROR("Unknown status in get_output!");
            throw ZlibException();
            break;
    }
    return status;
}

bool ZlibBase::is_finished() const {
    return finished;
}

bool ZlibBase::needs_input() const {
    return MY_Z_STREAM->avail_in == 0;
}

bool ZlibBase::needs_output() const {
    return MY_Z_STREAM->avail_out == 0;
}

ZlibBufferStatus ZlibBase::notify_output_written() {
    const size_t count = output_buffer->get_buffer_size() - MY_Z_STREAM->avail_out;
    const ZlibBufferStatus status = output_buffer->notify_written(count);
    switch(status) {
        case FINISHED:
            output_buffer_finished = true;
            NOVA_LOG_DEBUG("Notified of finished during output written.");
            break;
        case WAIT:
            break;
        case OK:
            break;
        default:
            NOVA_LOG_ERROR("Unknown status in get_output!");
            throw ZlibException();
            break;
    }
    return status;
}

void ZlibBase::run_with_streams(InputStreamPtr input, OutputStreamPtr output) {
    set_input_buffer(input);
    set_output_buffer(output);
    run();
}

void ZlibBase::run_read_from(char * in_buffer, size_t max_size,
                             OutputStreamPtr output) {

    struct InputCharacterArrayAdapter: public InputStream {

        InputCharacterArrayAdapter(char * arg_buffer, size_t max_size)
        :   advance_count(0),
            buffer(arg_buffer),
            max_size(max_size)
        {}

        virtual ZlibBufferStatus advance() {
            advance_count += 1;
            if (advance_count > 1) {
                return WAIT;  // This gives us time to kill it.
            }
            return OK;
        }

        virtual char * get_buffer() {
            if (advance_count != 1) {
                NOVA_LOG_ERROR("Why was this called twice?");
                throw ZlibException();
            }
            return buffer;
        }

        virtual size_t get_buffer_size() {
            return max_size;
        }

        int advance_count;
        char * buffer;
        size_t max_size;
    };

    InputStreamPtr reader;
    reader.reset(static_cast<InputStream *>(
        new InputCharacterArrayAdapter(in_buffer, max_size)));
    run_with_streams(reader, output);
}

size_t ZlibBase::run_write_into(InputStreamPtr input,
                                char * out_buffer, size_t max_size) {
    struct BufferStream : public OutputStream {

        BufferStream(char * arg_buffer, size_t max_size,
                     size_t & last_write_count)
        :   buffer(arg_buffer),
            last_write_count(last_write_count),
            max_size(max_size)
        {}

        virtual ZlibBufferStatus advance() {
            return OK;
        }

        virtual char * get_buffer() {
            return buffer;
        }

        virtual size_t get_buffer_size() {
            return max_size;
        }

        virtual ZlibBufferStatus notify_written(const size_t write_count) {
            last_write_count = write_count;
            return WAIT;
        }

        char * buffer;
        size_t & last_write_count;
        size_t max_size;
    };

    size_t last_write_count = 0;
    OutputStreamPtr writer;
    writer.reset(static_cast<OutputStream *>(
        new BufferStream(out_buffer, max_size, last_write_count)));
    run_with_streams(input, writer);
    return last_write_count;
}

void ZlibBase::finish_input_stream(OutputStreamPtr output) {

    struct StreamFinish : public InputStream {
        virtual ZlibBufferStatus advance() {
            return FINISHED;
        }

        virtual char * get_buffer() {
            return 0;
        }

        virtual size_t get_buffer_size() {
            return 0;
        }
    };

    InputStreamPtr reader;
    reader.reset(static_cast<InputStream *>(new StreamFinish()));
    run_with_streams(reader, output);
}


void ZlibBase::set_input_buffer(InputStreamPtr input) {
    input_buffer = input;
}

void ZlibBase::set_output_buffer(OutputStreamPtr output) {
    output_buffer = output;
}


/**---------------------------------------------------------------------------
 *- ZlibCompressor
 *---------------------------------------------------------------------------*/

ZlibCompressor::ZlibCompressor()
:   ZlibBase(),
    last_input(false)
{
    MY_Z_STREAM->zalloc = Z_NULL;
    MY_Z_STREAM->zfree = Z_NULL;
    MY_Z_STREAM->opaque = Z_NULL;
    const int result = deflateInit(MY_Z_STREAM, Z_BEST_COMPRESSION);
    if (Z_OK != result) {
        NOVA_LOG_ERROR("Error initializing deflate operation!");
        throw ZlibException();
    }
    MY_Z_STREAM->avail_in = 0; // Prompt for more input.
}

ZlibCompressor::~ZlibCompressor() {
    if (!finished) {
        finish(false);
    }
}


void ZlibCompressor::run() {
    if (is_finished()) {
        return;
    }
    while(!is_finished()) {
        if (needs_input()) {
            const ZlibBufferStatus status = get_input();
            if (OK != status) {
                if (FINISHED == status) {
                    last_input = true; // Go ahead and finish this up.
                } else {
                    // Return, and presumably let more data get put into input.
                    return;
                }
            }
        }

        if (needs_output()) {
            const ZlibBufferStatus status = get_output();
            if (OK != get_output()) {
                if (FINISHED == status) {
                    finish(true);
                }
                return;
            }
        }

        // Calling deflate changes avail_out and avail_in.
        const int flush = last_input ? Z_FINISH : Z_BLOCK;
        const int result = deflate(MY_Z_STREAM, flush);
        if (Z_STREAM_ERROR == result) {
            NOVA_LOG_ERROR("Error deflating zlib stream!");
            throw ZlibException();
        }
        if (last_input) {
            finish(true);
        }

        const ZlibBufferStatus status = notify_output_written();
        MY_Z_STREAM->avail_out = 0; //  This forces need input to be set before
                                    //  anything else can happen.
        if (OK != status) {
            if (FINISHED == status) {
                NOVA_LOG_ERROR("Output buffer finished? How can that be?");
                finish(true);
            }
            return;
        }
    }
};

void ZlibCompressor::finalize(bool can_throw) {
    if (Z_OK != deflateEnd(MY_Z_STREAM)) {
        NOVA_LOG_ERROR("Error ending deflate operation!");
        if (can_throw) {
            throw ZlibException();
        }
    }
}


/**---------------------------------------------------------------------------
 *- ZlibDecompressor
 *---------------------------------------------------------------------------*/

ZlibDecompressor::ZlibDecompressor()
:   ZlibBase()
{
    MY_Z_STREAM->zalloc = Z_NULL;
    MY_Z_STREAM->zfree = Z_NULL;
    MY_Z_STREAM->opaque = Z_NULL;
    MY_Z_STREAM->avail_in = 0;
    MY_Z_STREAM->next_in = 0;

    const int result = inflateInit(MY_Z_STREAM);
    if (Z_OK != result) {
        NOVA_LOG_ERROR("Error initializing inflate operation!");
        throw ZlibException();
    }
}

ZlibDecompressor::~ZlibDecompressor() {
    if (!finished) {
        finish(false);
    }
}

void ZlibDecompressor::run() {
    if (is_finished()) {
        return;
    }
    while(!is_finished()) {
        if (needs_input()) {
            const ZlibBufferStatus status = get_input();
            if (OK != status) {
                if (FINISHED == status) {
                    finish(true);
                }
                return;
            }
        }
        if (needs_output()) {
            const ZlibBufferStatus status = get_output();
            if (OK != status) {
                if (FINISHED == status) {
                    finish(true);
                }
                return;
            }
        }

        // Calling inflate changes avail_out and avail_in.
        const int result = inflate(MY_Z_STREAM, Z_NO_FLUSH);
        if (Z_OK != result && Z_STREAM_END != result) {
            NOVA_LOG_ERROR("Error inflating zlib stream!");
            if (Z_NULL != MY_Z_STREAM->msg) {
                NOVA_LOG_ERROR("Zlib error: %s", MY_Z_STREAM->msg);
            }
            throw ZlibException();
        }
        if (Z_STREAM_END == result) {
            // Zlib itself knows what the stream should look like, so in this case
            // it has figured out there's no more relevant data. So it will finish
            // things even if the various buffers think they need to send more.
            NOVA_LOG_DEBUG("Detected zlib Z_STREAM_END");
            finish(true);
        }
        const ZlibBufferStatus status = notify_output_written();
        MY_Z_STREAM->avail_out = 0; //  This forces need input to be set before
                                //  anything else can happen.
        if (OK != status) {
            if (FINISHED == status) {
                finish(true);
            }
            return;
        }
    }
}

void ZlibDecompressor::finalize(bool can_throw) {
    if (Z_OK != inflateEnd(MY_Z_STREAM)) {
        NOVA_LOG_ERROR("Error ending inflate operation!");
        if (Z_NULL != MY_Z_STREAM->msg) {
            NOVA_LOG_ERROR("Zlib error: %s", MY_Z_STREAM->msg);
        }
        if (can_throw) {
            throw ZlibException();
        }
    }
}


/**---------------------------------------------------------------------------
 *- ZlibException
 *---------------------------------------------------------------------------*/

ZlibException::ZlibException() throw() {
}

ZlibException::~ZlibException() throw() {
}

const char * ZlibException::what() const throw() {
    return "Zlib error!";
}

} } } // end nova::utils::zlib
