#ifndef __NOVA_REDIS_REDISCLIENT_H
#define __NOVA_REDIS_REDISCLIENT_H

#include <boost/optional.hpp>
#include <string>

class redisContext;
class redisReply;

namespace nova { namespace redis {


class RedisClient {
public:
    /* Redis Info string. */
    class Info {
    public:
        Info(const std::string & info);

        Info(const Info & other);

        std::string get(const std::string & key);

        boost::optional<std::string> get_optional(const std::string & key);

    private:
        std::stringstream info_stream;
    };

    /* Manages a reply object to ensure it's always freed. */
    class Reply {
    public:
        Reply(redisReply * r);

        ~Reply();

        std::string expect_status() const;

        std::string expect_string() const;

        void expect_ok() const;

        long long expect_int() const;

        redisReply * get();

        void throw_if_error() const;

    private:
        redisReply * r;
    };

    // Connects to an assumed port, host, etc using a password grabbed from
    // the Redis file. If more is needed feel free to write another
    // constructor.
    RedisClient();

    ~RedisClient();

    void auth();

    Reply command(const char * format, ...);

    void config_rewrite();

    void config_set(const std::string & key, const std::string & value);

    boost::optional<std::string> get_role();

    Info info();

    void ping();

private:
    ::redisContext * context;
};

}}//end nova::redis namespace

#endif // __NOVA_REDIS_REDISCLIENT_H
