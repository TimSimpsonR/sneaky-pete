#ifndef __NOVA_GUEST_HEARTBEAT
#define __NOVA_GUEST_HEARTBEAT

#include "nova/db/mysql.h"
#include <string>
#include <boost/utility.hpp>


namespace nova { namespace guest {

/** Updates the agent_heartbeats table with the current time. Used to determine
 *  if Sneaky Pete is still alive. */
class HeartBeat : boost::noncopyable  {
public:
    HeartBeat(nova::db::mysql::MySqlConnectionWithDefaultDbPtr con,
              const char * guest_id);

    ~HeartBeat();

    void update();

private:

    nova::db::mysql::MySqlConnectionWithDefaultDbPtr con;

    bool exists();

    const std::string guest_id;
};

}} // nova::guest

#endif
