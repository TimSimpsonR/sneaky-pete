CURL_INCLUDE_DIR=/opt/sp-deps/libcurl/include
LIBS=
FLAGS="-std=c++0x -I$CURL_INCLUDE_DIR"

g++ -std=c++0x -static-libgcc -I$CURL_INCLUDE_DIR Curl.cc Md5.cc swift.cc upload_file.cc \
    -L"/opt/sp-deps/libcurl/lib" -lboost_thread -lpthread -lssl -lcrypto -lcurl -lidn -ldl
