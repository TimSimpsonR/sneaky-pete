#include "pch.hpp"
#include "nova/guest/HeartBeat.h"
#include <boost/format.hpp>
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include <string.h>
#include "nova/guest/utils.h"

using boost::format;
using namespace nova::db::mysql;
using boost::optional;
using nova::guest::utils::IsoDateTime;
using std::string;

namespace nova { namespace guest {


HeartBeat::HeartBeat(MySqlConnectionWithDefaultDbPtr con, const char * guest_id)
: con(con), guest_id(guest_id) {
}

HeartBeat::~HeartBeat() {
}


bool HeartBeat::exists() {
    con->ensure();
    string query = str(format("SELECT updated_at FROM agent_heartbeats "
                              "WHERE instance_id='%s'")
                       % con->escape_string(guest_id.c_str()));
    MySqlResultSetPtr results = con->query(query.c_str());
    return results->next();
}

void HeartBeat::update() {
    IsoDateTime now;
    const char * query = exists()
        ? "UPDATE agent_heartbeats SET updated_at = ? "
              "WHERE agent_heartbeats.instance_id = ?"
        : "INSERT INTO agent_heartbeats (id, updated_at, instance_id) "
              "VALUES(UUID(), ?, ?);"
        ;
    con->ensure();
    MySqlPreparedStatementPtr stmt = con->prepare_statement(query);
    stmt->set_string(0, now.c_str());
    stmt->set_string(1, guest_id.c_str());
    stmt->execute();
}


} } // end nova::guest
