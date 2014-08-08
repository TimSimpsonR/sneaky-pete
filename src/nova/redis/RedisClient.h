#ifndef __NOVA_REDIS_REDISCLIENT_H
#define __NOVA_REDIS_REDISCLIENT_H


class redisContext;


namespace nova { namespace redis {

class ReplyPtr;

class RedisClient {
public:
    // Connects to an assumed port, host, etc using a password grabbed from
    // the Redis file. If more is needed feel free to write another
    // constructor.
    RedisClient();

    ~RedisClient();

    void auth();

    void config_rewrite();

    void config_set(const std::string & key, const std::string & value);

    void ping();
private:
    ::redisContext * context;

    ReplyPtr command(const char * format, ...);
};

}}//end nova::redis namespace

#endif // __NOVA_REDIS_REDISCLIENT_H
