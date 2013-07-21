#define BOOST_TEST_MODULE zlib_tests
#include <boost/test/unit_test.hpp>

#include "nova/Log.h"
#include "nova/utils/zlib.h"
#include <iostream>

using nova::LogApiScope;
using nova::LogOptions;
using namespace nova::utils;

using namespace nova::utils::zlib;


#define CHECKPOINT BOOST_REQUIRE_EQUAL(1, 1);


const size_t letters_in_a_row = 3;


/* This ridiculous thing represents a string with hilariously compressable
 * output. It goes through the alphabet, stating each letter the number of
 * times given by the constant "letters_in_a_row" above, before going to the
 * next letter. Once 'z' has finished, it returns to 'a'. It produces the
 * amount of output specified by "max".
 */
struct RepeatingAlphabetInput {
    size_t index;
    size_t max;

    RepeatingAlphabetInput(size_t max)
    :   index(0),
        max(max)
    {}

    bool finished() {
        return index >= max;
    }

    char next_char() {
        unsigned char letter_index = ((index / letters_in_a_row) % 26);
        ++ index;
        char base = 'a';
        return base + letter_index;
    }

    size_t write(char * buffer, size_t size) {
        size_t limit = std::min(size, max - index);
        for (size_t i = 0; i < limit; i ++) {
            buffer[i] = next_char();
        }
        return limit;
    }
};


/* This function makes sure that a stringstream is filled with input that
 * has come from the class above. */
void confirm_stringstream_matches_input(std::stringstream & stream,
                                        size_t expected_size) {
    char alpha_buffer[letters_in_a_row];
    char current_char = 'a';
    for (size_t index = 0; index < expected_size;) {
        stream.read(alpha_buffer, letters_in_a_row);
        size_t result = stream.gcount();
        BOOST_REQUIRE_EQUAL(letters_in_a_row, result);
        for (size_t j = 0; j < letters_in_a_row && index < expected_size; j ++)
        {
            BOOST_REQUIRE_EQUAL(current_char, alpha_buffer[j]);
            ++ index;
        }
        current_char += 1;
        if (current_char >= ('a' + 26)) {
            current_char -= 26;
        }
    }
}

BOOST_AUTO_TEST_CASE(showing_how_alphabet_class_works)
{
    // This class just goes through the alphabet, printing out hundreds of
    // a's, then b's, etc etc.
    RepeatingAlphabetInput alphabet(letters_in_a_row * 2);
    char buffer[letters_in_a_row * 2];
    const auto result = alphabet.write(buffer, sizeof(buffer));
    BOOST_REQUIRE_EQUAL(letters_in_a_row * 2, result);
    for (size_t i = 0; i < letters_in_a_row; i ++) {
        BOOST_REQUIRE_EQUAL('a', buffer[i]);
    }
    for (size_t i2 = letters_in_a_row; i2 < letters_in_a_row * 2; i2 ++) {
        BOOST_REQUIRE_EQUAL('b', buffer[i2]);
    }
}

void compress_test(std::stringstream & compressed_buffer, size_t source_size) {
    RepeatingAlphabetInput alphabet(source_size);

    class Reader : public InputStream {
        public:
            Reader(RepeatingAlphabetInput & alphabet)
            :   alphabet(alphabet)
            {
            }

            virtual ZlibBufferStatus advance() {
                if (alphabet.finished()) {
                    NOVA_LOG_DEBUG2("Reader: FINISHED");
                    return FINISHED;
                }
                current_read_count = alphabet.write(buffer, sizeof(buffer) - 1);
                buffer[current_read_count] = '\0';
                return OK;
            }

            virtual char * get_buffer() {
                return buffer;
            }

            virtual size_t get_buffer_size() {
                return current_read_count;
            }

        private:
            char buffer[1024];
            size_t current_read_count;
            RepeatingAlphabetInput & alphabet;
    };
    InputStreamPtr reader;
    //Note(tim.simpson): This cast is insane and probably due to a compiler
    //                   error.
    reader.reset(static_cast<InputStream *>(new Reader(alphabet)));


    class Writer : public OutputStream {
        public:
            Writer(std::stringstream & compressed_buffer)
            :   compressed_buffer(compressed_buffer),
                seen_finished(false)
            {
            }

            virtual ZlibBufferStatus advance() {
                return OK;  // Pretty simple am I right?
            }

            virtual char * get_buffer() {
                return output_buffer;
            }

            virtual size_t get_buffer_size() {
                return sizeof(output_buffer) - 1;
            }

            virtual ZlibBufferStatus notify_written(const size_t count) {
                compressed_buffer.write(output_buffer, count);
                if (seen_finished) {
                BOOST_REQUIRE_MESSAGE(false, "is_finished() was true and yet "
                                             "decompress still returned data.");
                }
                output_buffer[count] = '\0';
                return OK;
            }

            void set_seen_finished() {
                seen_finished = true;
            }

        private:
            std::stringstream & compressed_buffer;
            char output_buffer[1024];
            bool seen_finished;
    };
    OutputStreamPtr writer(static_cast<OutputStream *>(new Writer(compressed_buffer)));


    // char output_buffer[1024];
    // bool seen_finished = false;
    ZlibCompressor zlib;
    // size_t write_count;

    while (zlib.is_finished() == false) {
        zlib.run_with_streams(reader, writer);
    }

    // while ((write_count = zlib.compress(output_buffer, sizeof(output_buffer)))
    //        > 0)
    // {
    //     compressed_buffer.write(output_buffer, write_count);
    //     if (seen_finished) {
    //         BOOST_REQUIRE_MESSAGE(false, "is_finished() was true and yet "
    //                                      "decompress still returned data.");
    //     }
    //     seen_finished = zlib.is_finished();
    // }
    BOOST_REQUIRE(zlib.is_finished());
} //;


// This version uses the char buffer.
void compress_test2(std::stringstream & compressed_buffer, size_t source_size) {
    RepeatingAlphabetInput alphabet(source_size);

    class Reader : public InputStream {
        public:
            Reader(RepeatingAlphabetInput & alphabet)
            :   alphabet(alphabet)
            {
            }

            virtual ZlibBufferStatus advance() {
                if (alphabet.finished()) {
                    NOVA_LOG_DEBUG2("Reader: FINISHED");
                    return FINISHED;
                }
                current_read_count = alphabet.write(buffer, sizeof(buffer) - 1);
                buffer[current_read_count] = '\0';
                return OK;
            }

            virtual char * get_buffer() {
                return buffer;
            }

            virtual size_t get_buffer_size() {
                return current_read_count;
            }

        private:
            char buffer[1024];
            size_t current_read_count;
            RepeatingAlphabetInput & alphabet;
    };
    InputStreamPtr reader;
    //Note(tim.simpson): This cast is insane and probably due to a compiler
    //                   error.
    reader.reset(static_cast<InputStream *>(new Reader(alphabet)));

    ZlibCompressor zlib;

    char output_buffer[1024];
    // size_t last_write_count;
    // OutputStreamPtr writer;
    // writer.reset(new BufferStream(output_buffer, last_write_count));

    // ZlibCompressor zlib(reader, writer);
    while(!zlib.is_finished()) {
        size_t last_write_count = zlib.run_write_into(reader, output_buffer,
                                                      sizeof(output_buffer));
        compressed_buffer.write(output_buffer, last_write_count);
    }

    BOOST_REQUIRE(zlib.is_finished());
} //;


void decompress_test(std::stringstream & compressed_buffer,
                     std::stringstream & decompressed_buffer) {
     class Reader : public InputStream {
        public:
            Reader(std::stringstream & compressed_buffer)
            :   compressed_buffer(compressed_buffer)
            {
            }

            virtual ZlibBufferStatus advance() {
                if (!compressed_buffer.good()) {
                    NOVA_LOG_DEBUG2("Decompress FINISHED");
                    return FINISHED;
                }
                compressed_buffer.read(buffer, sizeof(buffer) - 1);
                read_count = compressed_buffer.gcount();
                buffer[read_count] = '\0';
                NOVA_LOG_DEBUG2("Decompress reader:%s!", buffer);
                // if (read_count == 0) {
                //     if (!compressed_buffer.good()) {
                //         return FINISHED;
                //     }
                // }
                return OK;
            }

            virtual char * get_buffer() {
                return buffer;
            }

            virtual size_t get_buffer_size() {
                return read_count;
            }

        private:
            char buffer[1024];
            std::stringstream & compressed_buffer;
            size_t read_count;
    };
    InputStreamPtr reader;
    reader.reset(static_cast<InputStream *>(new Reader(compressed_buffer)));

    class Writer : public OutputStream {
        public:
            Writer(std::stringstream & decompressed_buffer)
            :   decompressed_buffer(decompressed_buffer),
                seen_finished(false)
            {
            }

            virtual ZlibBufferStatus advance() {
                return OK;  // Pretty simple am I right?
            }

            virtual char * get_buffer() {
                return output_buffer;
            }

            virtual size_t get_buffer_size() {
                return sizeof(output_buffer) - 1;
            }

            virtual ZlibBufferStatus notify_written(const size_t write_count) {
                decompressed_buffer.write(output_buffer, write_count);
                output_buffer[write_count] = '\0';
                return OK;
            }

            void set_seen_finished() {
                seen_finished = true;
            }

        private:
            std::stringstream & decompressed_buffer;
            char output_buffer[1024];
            bool seen_finished;
    };
    OutputStreamPtr writer(static_cast<OutputStream *>(
        new Writer(decompressed_buffer)));

    // char output_buffer[1024];

    // bool seen_finished = false;

    // ZlibDecompressor zlib(reader);
    // size_t write_count;
    // CHECKPOINT;
    // while ((write_count = zlib.decompress(output_buffer, sizeof(output_buffer)))
    //        > 0)
    // {
    //     // std::cerr << "YO\n\n";
    //     // std::cerr.write(output_buffer, write_count);
    //     // std::cerr << "^^^\n\n";
    //     decompressed_buffer.write(output_buffer, write_count);
    //     if (seen_finished) {
    //         BOOST_REQUIRE_MESSAGE(false, "is_finished() was true and yet "
    //                                      "decompress still returned data.");
    //     }
    //     seen_finished = zlib.is_finished();
    // }
    ZlibDecompressor zlib;
    while(!zlib.is_finished()) {
        zlib.run_with_streams(reader, writer);
    }
    BOOST_REQUIRE(zlib.is_finished());
} ;


void decompress_test2(std::stringstream & compressed_buffer,
                      std::stringstream & decompressed_buffer) {

    class Writer : public OutputStream {
        public:
            Writer(std::stringstream & decompressed_buffer)
            :   decompressed_buffer(decompressed_buffer),
                seen_finished(false)
            {
            }

            virtual ZlibBufferStatus advance() {
                return OK;  // Pretty simple am I right?
            }

            virtual char * get_buffer() {
                return output_buffer;
            }

            virtual size_t get_buffer_size() {
                return sizeof(output_buffer) - 1;
            }

            virtual ZlibBufferStatus notify_written(const size_t write_count) {
                decompressed_buffer.write(output_buffer, write_count);
                output_buffer[write_count] = '\0';
                return OK;
            }

            void set_seen_finished() {
                seen_finished = true;
            }

        private:
            std::stringstream & decompressed_buffer;
            char output_buffer[1024];
            bool seen_finished;
    };
    OutputStreamPtr writer(static_cast<OutputStream *>(
        new Writer(decompressed_buffer)));

    // char output_buffer[1024];

    // bool seen_finished = false;

    // ZlibDecompressor zlib(reader);
    // size_t write_count;
    // CHECKPOINT;
    // while ((write_count = zlib.decompress(output_buffer, sizeof(output_buffer)))
    //        > 0)
    // {
    //     // std::cerr << "YO\n\n";
    //     // std::cerr.write(output_buffer, write_count);
    //     // std::cerr << "^^^\n\n";
    //     decompressed_buffer.write(output_buffer, write_count);
    //     if (seen_finished) {
    //         BOOST_REQUIRE_MESSAGE(false, "is_finished() was true and yet "
    //                                      "decompress still returned data.");
    //     }
    //     seen_finished = zlib.is_finished();
    // }
    ZlibDecompressor zlib;

    char read_buffer[1024];

    CHECKPOINT;
    while(!zlib.is_finished()) {
        if (!compressed_buffer.good()) {
            CHECKPOINT;
            zlib.finish_input_stream(writer);
            break;
        }
        CHECKPOINT;
        compressed_buffer.read(read_buffer, sizeof(read_buffer) - 1);
        const size_t read_count = compressed_buffer.gcount();
        read_buffer[read_count] = '\0';
        zlib.run_read_from(read_buffer, read_count, writer);
    }
    BOOST_REQUIRE(zlib.is_finished());
} ;

BOOST_AUTO_TEST_CASE(compress_and_decompress)
{
    LogApiScope log(LogOptions::simple());

    const size_t source_size = 2000 * 26 * letters_in_a_row;

    std::stringstream compressed_buffer;
    compress_test(compressed_buffer, source_size);

    NOVA_LOG_DEBUG("Compression completed, decompressing...");

    std::stringstream decompressed_buffer;
    decompress_test(compressed_buffer, decompressed_buffer);

    CHECKPOINT

    // Confirm that the decompressed stream is equivalent to what was zipped.
    confirm_stringstream_matches_input(decompressed_buffer, source_size);

    // Make sure that the compression ratio is as desired.
    const size_t original = source_size;
    const size_t zipped = compressed_buffer.str().size();
    const double ratio = (double) zipped / (double) original;
    NOVA_LOG_DEBUG2("Ratio is %d", ratio);
    BOOST_REQUIRE_MESSAGE(ratio < .014, "Compression was less than expected.");
}


BOOST_AUTO_TEST_CASE(compress_and_decompress_2)
{
    LogApiScope log(LogOptions::simple());

    const size_t source_size = 2000 * 26 * letters_in_a_row;

    std::stringstream compressed_buffer;
    compress_test2(compressed_buffer, source_size);

    NOVA_LOG_DEBUG("Compression completed, decompressing...");

    std::stringstream decompressed_buffer;
    decompress_test(compressed_buffer, decompressed_buffer);

    CHECKPOINT

    // Confirm that the decompressed stream is equivalent to what was zipped.
    confirm_stringstream_matches_input(decompressed_buffer, source_size);

    // Make sure that the compression ratio is as desired.
    const size_t original = source_size;
    const size_t zipped = compressed_buffer.str().size();
    const double ratio = (double) zipped / (double) original;
    NOVA_LOG_DEBUG2("Ratio is %d", ratio);
    BOOST_REQUIRE_MESSAGE(ratio < .014, "Compression was less than expected.");
}


BOOST_AUTO_TEST_CASE(compress_and_decompress_3)
{
    LogApiScope log(LogOptions::simple());

    const size_t source_size = 2000 * 26 * letters_in_a_row;

    std::stringstream compressed_buffer;
    compress_test(compressed_buffer, source_size);

    NOVA_LOG_DEBUG("Compression completed, decompressing...");

    std::stringstream decompressed_buffer;
    decompress_test2(compressed_buffer, decompressed_buffer);

    CHECKPOINT

    // Confirm that the decompressed stream is equivalent to what was zipped.
    confirm_stringstream_matches_input(decompressed_buffer, source_size);

    // Make sure that the compression ratio is as desired.
    const size_t original = source_size;
    const size_t zipped = compressed_buffer.str().size();
    const double ratio = (double) zipped / (double) original;
    NOVA_LOG_DEBUG2("Ratio is %d", ratio);
    BOOST_REQUIRE_MESSAGE(ratio < .014, "Compression was less than expected.");
}
