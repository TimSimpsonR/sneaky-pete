#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>


namespace nova { namespace redis {

//Redis responses.


//This is the base good response code from redis.
//You will only see this response code when getting data from redis.
static const std::string STRING_RESPONSE = "+";

//This is the base error response code from redis.
//You will only see this response code when getting data from redis
//and there is an error
static const std::string ERROR_RESPONSE = "-";

//This is the base response code for an integer response.
//You will only see this response code when getting data from redis
//and only an integer is returned.
static const std::string INTEGER_RESPONSE = ":";

//This is the base response for a multipart redis response.
//You will only see this response when getting data from redis
//and only if there is a multipart response from redis.
static const std::string MULTIPART_RESPONSE = "*";

//This is the base client error response code.
//This is returned when the client has an error.
static const std::string CERROR_RESPONSE = "cerror";

//This is returned when an unsupported response comes from a redis server.
//I.E not string error int or multipart response
static const std::string UNSUPPORTED_RESPONSE = "unsupported";


//This is returned when a redis server has a an server error
//that is unrelated to the redis protocol itself.
static const std::string SERROR_RESPONSE = "serror";

// //This is the status description when the redis server returns a server error
// //that is unrelated to the redis protocol itself.
// static const std::string SERROR_DESCRIPTION = "Redis server error.";

//This status is returned when the client has a connection error.
static const std::string CCONNECTION_ERR_RESPONSE = "cconnection_err";

//TODO(tim.simpson): This looks like it may be useless.
//This status is returned when a message has been sent to a redis server.
static const std::string CMESSAGE_SENT_RESPONSE = "cmessage_sent";

// //This status description is returned when a message has been
// //sent to the redis server.
// static const std::string CMESSAGE_SENT_DESCRIPTION = "Redis client"
//                                                         "sent message.";

//This is returned when a command results in no response nothing to parse.
static const std::string CNOTHING_TO_DO_RESPONSE  = "cnothing";

//Socket error status code.
static const int SOCK_ERROR = -1;

//The length of one byte.
static const int FIRST_BYTE_READ = 1;

//Max read length in bytes, for the client.
static const int READ_LEN = 2048;

//CRLF constant.
static const std::string CRLF = "\r\n";

//This is bad but it is a value that is never used by redis.
//So it means the item does not exist in the vector.
static const int INT_NULL = -42;

//Config keys.

//Hash disables a config value in the config file.
static const std::string DISABLE = "#";

//Other config files to include.
static const std::string INCLUDE_FILE = "include";

//Run as a daemon can be yes or no
static const std::string DAEMONIZE = "daemonize";

//Path to the pidfile.
static const std::string PIDFILE = "pidfile";

//The redis server port.
static const std::string PORT = "port";

//tcp backlog length
static const std::string TCP_BACKLOG = "tcp-backlog";

//The bind addresses for this redis instance.
static const std::string BIND_ADDR = "bind";

//Path to the unix socket.
static const std::string UNIX_SOCKET = "unixsocket";

//socket perms for the socket file in numnumnum format
static const std::string UNIX_SOCKET_PERMISSION = "unixsocketperm";

//the tcp keepalive in seconds.
static const std::string TCP_KEEPALIVE = "tcp-keepalive";

//default log level for redis.
static const std::string LOG_LEVEL = "loglevel";

//path to the redis log file.
static const std::string LOG_FILE = "logfile";

//enable or disable syslog can be yes or no.
static const std::string SYSLOG = "syslog-enabled";

//the syslog user name.
static const std::string SYSLOG_IDENT = "syslog-ident";

//what syslog faculty are you using.
static const std::string SYSLOG_FACILITY = "syslog-facility";

//the number of databases in this redis db.
static const std::string DATABASES = "databases";

//The save interval for RDB dumps.
static const std::string SAVE = "save";

//Stops writing if bgsave fails can be yes or no.
static const std::string STOP_WRITES_ON_BGSAVE_ERROR = "stop-writes-on-bgsave"
                                                        "-error";

//Should we use rdbcompression can be yes or no.
static const std::string RDB_COMPRESSION = "rdbcompression";

//Should we create a checksum when writing the rdbfile can be yes or no.
static const std::string RDB_CHECKSUM = "rdbchecksum";

//name of the rdb database file.
static const std::string DB_FILENAME = "dbfilename";

//The default path that the rdb and aof databases files are written to.
static const std::string DB_DIR = "dir";

//host or ip address of the master for this redis instance.
static const std::string SLAVE_OF = "slaveof";

//the password for the redis master server.
static const std::string MASTER_AUTH = "masterauth";

//Should we serve stale slave data? can be yes or no.
static const std::string SLAVE_SERVE_STALE_DATA = "slave-serve-stale-data";

//Set a slave to read only can be yes or no.
static const std::string SLAVE_READ_ONLY = "slave-read-only";

//timeout for a slave to respond. Set in seconds.
static const std::string REPL_PING_SLAVE_PERIOD = "repl-ping-slave-period";

//General replication timeout. Set in seconds.
static const std::string REPL_TIMEOUT = "repl-timeout";

//Turn of replication delay can be set to yes or no.
static const std::string REPL_DISABLE_TCP_NODELAY = "repl-disable-tcp-nodelay";

//How large of a replication backlog to keep set in bytes.
static const std::string REPL_BACKLOG_SIZE = "repl-backlog-size";

//How long to save the replication backlog in seconds.
static const std::string REPL_BACKLOG_TTL = "repl-backlog-ttl";

//Set your instances save priority.
static const std::string SLAVE_PRIORITY = "slave-priority";

//Set the min number of slaves to write to. Set in bytes.
static const std::string MIN_SLAVES_TO_WRITE = "min-slaves-to-write";

//What is min replication lag time for slaves. Set in seconds.
static const std::string MIN_SLAVES_MAX_LAG = "min-slaves-max-lag";

//Enable password protection.
static const std::string REQUIRE_PASS = "requirepass";

//Rename a base redis command.
static const std::string RENAME_COMMAND = "rename-command";

//Max number of connected clients.
static const std::string MAX_CLIENTS = "maxclients";

//Max memory consumed by the redis daemon.
static const std::string MAX_MEMORY = "maxmemory";

//Max memory policy used for key eviction when the db fills up.
static const std::string MAX_MEMORY_POLICY = "maxmemory-policy";

//Number of samples to take for volitile eviction policies.
static const std::string MAX_MEMORY_SAMPLES = "maxmemory-samples";

//Enable AOF file.
static const std::string APPEND_ONLY = "appendonly";

//path to write the aof file to.
static const std::string APPEND_FILENAME = "appendfilename";

static const std::string APPEND_FSYNC = "appendfsync";

static const std::string NO_APPEND_FSYNC_ON_REWRITE = "no-appendfsync-on-"
                                                        "rewrite";

static const std::string AUTO_AOF_REWRITE_PERCENTAGE = "auto-aof-rewrite-"
                                                    "percentage";

static const std::string AUTO_AOF_REWRITE_MIN_SIZE = "auto-aof-rewrite-"
                                                        "min-size";

static const std::string LUA_TIME_LIMIT = "lua-time-limit";

static const std::string SLOWLOG_LOG_SLOWER_THAN = "slowlog-log-slower-"
                                                    "than";

static const std::string SLOWLOG_MAX_LEN = "slowlog-max-len";

static const std::string NOTIFY_KEYSPACE_EVENTS = "notify-keyspace-events";

static const std::string HASH_MAX_ZIP_LIST_ENTRIES = "hash-max-ziplist-"
                                                        "entries";

static const std::string HASH_MAX_ZIP_LIST_VALUE = "hash-max-ziplist-"
                                                    "value";

static const std::string LIST_MAX_ZIP_LIST_ENTRIES = "list-max-ziplist-"
                                                        "entries";

static const std::string LIST_MAX_ZIP_LIST_VALUE = "list-max-ziplist-"
                                                    "value";

static const std::string SET_MAX_INTSET_ENTRIES = "set-max-intset-entries";

static const std::string ZSET_MAX_ZIP_LIST_ENTRIES = "zset-max-ziplist-entries";

static const std::string ZSET_MAX_ZIP_LIST_VALUE = "zset-max-ziplist-value";

static const std::string ACTIVE_REHASHING = "activerehashing";

static const std::string CLIENT_OUTPUT_BUFFER_LIMIT = "client-output-"
                                                        "buffer-limit";

static const std::string HZ = "hz";

static const std::string AOF_REWRITE_INCREMENTAL_FSYNC = "aof-rewrite-"
                                                            "incremental-fsync";

//Config value types.

static const std::string TYPE_STRING = "string";

static const std::string TYPE_INT = "int";

static const std::string TYPE_MEMORY = "memory";

static const std::string TYPE_BYTES = "bytes";

static const std::string TYPE_KILOBYTES = "kilobytes";

static const std::string TYPE_MEGABYTES = "megabytes";

static const std::string TYPE_GIGABYTES = "gigabytes";

static const std::string TYPE_BOOL = "bool";

static const std::string TYPE_TRUE = "yes";

static const std::string TYPE_FALSE = "no";

//Save keys.
static const std::string SAVE_SECONDS = "seconds";

static const std::string SAVE_CHANGES = "changes";

//Loglevel.
static const std::string LOG_LEVEL_DEBUG = "debug";

static const std::string LOG_LEVEL_VERBOSE = "verbose";

static const std::string LOG_LEVEL_NOTICE = "notice";

static const std::string LOG_LEVEL_WARNING = "warning";

//Slave of keys.
static const std::string MASTER_IP = "master_ip";

static const std::string MASTER_PORT = "master_port";

//Rename command keys.
static const std::string RENAMED_COMMAND = "renamed_command";

static const std::string NEW_COMMAND_NAME = "new_command_name";

//Max memory policy values.
static const std::string VOLATILE_LRU = "volatile-lru";

static const std::string ALLKEYS_LRU = "allkeys-lru";

static const std::string VOLATILE_RANDOM = "volatile-random";

static const std::string ALLKEYS_RANDOM = "allkeys-random";

static const std::string VOLATILE_TTL = "volatile-ttl";

static const std::string NO_EVICTION = "noevection";

//Append sync values.
static const std::string APPEND_FSYNC_ALWAYS = "always";

static const std::string APPEND_FSYNC_EVERYSEC = "everysec";

static const std::string APPEND_FSYNC_NO = "no";

//Client output buffer limit class values.
static const std::string NORMAL_CLIENTS = "normal";

static const std::string SLAVE_CLIENTS = "slave";

static const std::string PUBSUB_CLIENTS = "pubsub";

//Client output buffer limit keys.
static const std::string CLIENT_BUFFER_LIMIT_CLASS = "class";

static const std::string CLIENT_BUFFER_LIMIT_HARD_LIMIT = "hard_limit";

static const std::string CLIENT_BUFFER_LIMIT_SOFT_LIMIT = "soft_limit";

static const std::string CLIENT_BUFFER_LIMIT_SOFT_SECONDS = "soft_seconds";

//Memory units.
static const std::string KILOBYTES = "kb";

static const std::string MEGABYTES = "mb";

static const std::string GIGABYTES = "gb";

//Redis key command names.
static const std::string COMMAND_DEL = "DEL";

static const std::string COMMAND_DUMP = "DUMP";

static const std::string COMMAND_EXISTS = "EXISTS";

static const std::string COMMAND_EXPIRE = "EXPIRE";

static const std::string COMMAND_EXPIRE_AT = "EXPIREAT";

static const std::string COMMAND_KEYS = "KEYS";

static const std::string COMMAND_MIGRATE = "MIGRATE";

static const std::string COMMAND_MOVE = "MOVE";

static const std::string COMMAND_OBJECT = "OBJECT";

static const std::string COMMAND_PERSIST = "PERSIST";

static const std::string COMMAND_PEXIPRE = "PEXPIRE";

static const std::string COMMAND_PEXPIRE_AT = "PEXPIREAT";

static const std::string COMMAND_PTTL = "PTTL";

static const std::string COMMAND_RANDOMKEY = "RANDOMKEY";

static const std::string COMMAND_RENAME = "RENAME";

static const std::string COMMAND_RENAMENX = "RENAMENX";

static const std::string COMMAND_RESTORE = "RESTORE";

static const std::string COMMAND_SORT = "SORT";

static const std::string COMMAND_TTL = "TTL";

static const std::string COMMAND_TYPE = "TYPE";

static const std::string COMMAND_SCAN = "SCAN";

//Redis string command names.

static const std::string COMMAND_APPEND = "APPEND";

static const std::string COMMAND_BITCOUNT = "BITCOUT";

static const std::string COMMAND_BITOP = "BITOP";

static const std::string COMMAND_DECR = "DECR";

static const std::string COMMAND_DECR_BY = "DECRBY";

static const std::string COMMAND_GET = "GET";

static const std::string COMMAND_GET_BIT = "GETGIT";

static const std::string COMMAND_GET_RANGE = "GETRANGE";

static const std::string COMMAND_GET_SET = "GETSET";

static const std::string COMMAND_INCR = "INCR";

static const std::string COMMAND_INCR_BY = "INCRBY";

static const std::string COMMAND_INCR_BY_FLOAT = "INCRBYFLOAT";

static const std::string COMMAND_MGET = "MGET";

static const std::string COMMAND_MSET = "MSET";

static const std::string COMMAND_MSETNX = "MSETNX";

static const std::string COMMAND_PSETEX = "PSETEX";

static const std::string COMMAND_SET = "SET";

static const std::string COMMAND_SET_BIT = "SETBIT";

static const std::string COMMAND_SETEX = "SETEX";

static const std::string COMMAND_SET_RANGE = "SETRANGE";

static const std::string COMMAND_STRLEN = "STRLEN";

//Redis hash command names.

static const std::string COMMAND_HDEL = "HDEL";

static const std::string COMMAND_HEXISTS = "HEXISTS";

static const std::string COMMAND_HGET = "HGET";

static const std::string COMMAND_HGET_ALL = "HGETALL";

static const std::string COMMAND_HINCR_BY = "HINCRBY";

static const std::string COMMAND_HINCR_BY_FLOAT = "HINCRBYFLOAT";

static const std::string COMMAND_HKEYS = "HKEYS";

static const std::string COMMAND_HLEN = "HLEN";

static const std::string COMMAND_HMGET = "HMGET";

static const std::string COMMAND_HMSET = "HMSET";

static const std::string COMMAND_HSET = "HSET";

static const std::string COMMAND_HSETNX = "HSETNX";

static const std::string COMMAND_HVALS = "HVALS";

static const std::string COMMAND_HSCAN = "HSCAN";

//Redis list command names.
static const std::string COMMAND_BLPOP = "BLPOP";

static const std::string COMMAND_BRPOP = "BRPOP";

static const std::string COMMAND_BRPOPLPUSH = "BRPOPLPUSH";

static const std::string COMMAND_LINDEX = "LINDEX";

static const std::string COMMAND_LINSTER = "LINSTER";

static const std::string COMMAND_LLEN = "LLEN";

static const std::string COMMAND_LPOP = "LPOP";

static const std::string COMMAND_LPUSH = "LPUSH";

static const std::string COMMAND_LPUSHX = "LPUSHX";

static const std::string COMMAND_LRANGE = "LRANGE";

static const std::string COMMAND_LREM = "LREM";

static const std::string COMMAND_LSET = "LSET";

static const std::string COMMAND_LTRIM = "LTRIM";

static const std::string COMMAND_RPOP = "RPOP";

static const std::string COMMAND_RPOPLPUSH = "RPOPLPUSH";

static const std::string COMMAND_RPUSH = "RPUSH";

static const std::string COMMAND_RPUSHX = "RPUSHX";

//Redis set command names.
static const std::string COMMAND_SADD = "SADD";

static const std::string COMMAND_SCARD = "SCARD";

static const std::string COMMAND_SDIFF = "SDIFF";

static const std::string COMMAND_SDIFFSTORE = "SDIFFSTORE";

static const std::string COMMAND_SINTER = "SINTER";

static const std::string COMMAND_SINTERSORE = "SINTERSTORE";

static const std::string COMMAND_SISMEMEBER = "SISMEMBER";

static const std::string COMMAND_SMEMBERS = "SMEMBERS";

static const std::string COMMAND_SMOVE = "SMOVE";

static const std::string COMMAND_SPOP = "SPOP";

static const std::string COMMAND_SRANDMEMEBER = "SRANDMEMEBER";

static const std::string COMMAND_SREM = "SREM";

static const std::string COMMAND_SUNION = "SUNION";

static const std::string COMMAND_SUINIONSTORE = "SUNIONSTORE";

static const std::string COMMAND_SSCAN = "SSCAN";

//Redis sorted set command names.
static const std::string COMMAND_ZADD = "ZADD";

static const std::string COMMAND_ZCARD = "ZCARD";

static const std::string COMMAND_ZCOUNT = "ZCOUNT";

static const std::string COMMAND_ZINCR_BY = "ZINCRBY";

static const std::string COMMAND_ZINTERSTORE = "ZINTERSTORE";

static const std::string COMMAND_ZRANGE = "ZRANGE";

static const std::string COMMAND_ZRANGEBYSCORE = "ZRANGEBYSCORE";

static const std::string COMMAND_ZRANK = "ZRANK";

static const std::string COMMAND_ZREM = "ZREM";

static const std::string COMMAND_ZREMRANGEBYRANK = "ZREMRANGEBYRANK";

static const std::string COMMAND_ZREMRANGEBYSCORE = "ZREMRANGEBYSCORE";

static const std::string COMMAND_ZREVRANK = "ZREVERANK";

static const std::string COMMAND_ZSCORE = "ZSCORE";

static const std::string COMMAND_ZUNIONSTORE = "ZUNIONSTORE";

static const std::string COMMAND_ZSCAN = "ZSCAN";

//Redis pub/sub command names.
static const std::string COMMAND_PSUBSCRIBE = "PSUBSCRIBE";

static const std::string COMMAND_PUBSUB = "PUBSUB";

static const std::string COMMAND_PUBLISH = "PUBLISH";

static const std::string COMMAND_PUNSUBSCRIBE = "PUNSUBSCRIBE";

static const std::string COMMAND_SUBSCRIBE = "SUBSCRIBE";

static const std::string COMMAND_UNSUBSCRIBE = "UNSUBSCRIBE";

//Redis transaction command names.
static const std::string COMMAND_DISCARD = "DISCARD";

static const std::string COMMAND_EXEC = "EXEC";

static const std::string COMMAND_MULTI = "MULTI";

static const std::string COMMAND_UNWATCH = "UNWATCH";

static const std::string COMMAND_WATCH = "WATCH";

//Redis scripting command namees.

static const std::string COMMAND_EVAL = "EVAL";

static const std::string COMMAND_EVALSHA = "EVALSHA";

static const std::string COMMAND_SCRIPT_EXISTS = "SCRIPT EXISTS";

static const std::string COMMAND_SCRIPT_FLUSH = "SCRIPT FLUSH";

static const std::string COMMAND_SCRIPT_KILL = "SCRIPT KILL";

static const std::string COMMAND_SCRIPT_LOAD = "SCRIPT LOAD";

//Redis connection command names.

static const std::string COMMAND_AUTH = "AUTH";

static const std::string COMMAND_ECHO = "ECHO";

static const std::string COMMAND_PING = "PING";

static const std::string COMMAND_QUIT = "QUIT";

static const std::string COMMAND_SELECT = "SELECT";

//Redis server command names.

static const std::string COMMAND_BGREWRITEAOF = "BGREWRITEAOF";

static const std::string COMMAND_BGSAVE = "BGSAVE";

static const std::string COMMAND_CLIENT_KILL = "CLIENT KILL";

static const std::string COMMAND_CLIENT_LIST = "CLIENT LIST";

static const std::string COMMAND_CLIENT_GETNAME = "CLIENT GETNAME";

static const std::string COMMAND_CLIENT_SETNAME = "CLIENT SETNAME";

static const std::string COMMAND_CLIENT = "CLIENT";

static const std::string COMMAND_CONFIG_GET = "CONFIG GET";

static const std::string COMMAND_CONFIG_REWRITE = "CONFIG REWRITE";

static const std::string COMMAND_CONFIG_SET = "CONFIG SET";

static const std::string COMMAND_CONFIG_RESETSTAT = "CONFIG RESETSTAT";

static const std::string COMMAND_CONFIG = "CONFIG";

static const std::string COMMAND_DBSIZE = "DBSIZE";

static const std::string COMMAND_DEBUG_OBJECT = "DEBUG OBJECT";

static const std::string COMMAND_DEBUG_SEGFAULT = "DEBUG SEGFAULT";

static const std::string COMMAND_FLUSHALL = "FLUSHALL";

static const std::string COMMAND_FLUSHDB = "FLUSHDB";

static const std::string COMMAND_INFO = "INFO";

static const std::string COMMAND_LASTSAVE = "LASTSAVE";

static const std::string COMMAND_MONITOR = "MONITOR";

static const std::string COMMAND_SAVE = "SAVE";

static const std::string COMMAND_SHUTDOWN = "SHUTDOWN";

static const std::string COMMAND_SLAVEOF = "SLAVEOF";

static const std::string COMMAND_SLOWLOG = "SLOWLOG";

static const std::string COMMAND_SYNC = "SYNC";

static const std::string COMMAND_TIME = "TIME";

}}//end of nova::redis


#endif /* CONSTANTS_H */
