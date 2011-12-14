#include "nova/db/api.h"
#include "nova/flags.h"
#include "nova/Log.h"
#include "nova/db/mysql.h"
#include <sstream>
#include <string.h>
#include "nova/guest/utils.h"

using nova::flags::FlagValues;
using namespace nova::db::mysql;
using boost::optional;
using nova::guest::utils::IsoDateTime;
using std::string;
using std::stringstream;

namespace nova { namespace db {


class ApiMySql : public Api {

private:
    MySqlConnectionWithDefaultDbPtr con;

public:
    ApiMySql(MySqlConnectionWithDefaultDbPtr con)
    : con(con)
    {
    }

    virtual ~ApiMySql() {}

    void ensure() {
        con->ensure();
    }

    virtual ServicePtr service_create(const NewService & new_service) {
        ServicePtr service = service_get_by_args(new_service);
        if (!!service) {
            return service;
        }
        ensure();
        MySqlPreparedStatementPtr stmt = con->prepare_statement(
            "INSERT INTO services "
            "(created_at, updated_at, deleted, report_count, disabled, "
            " availability_zone, services.binary, host, topic) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);");
        IsoDateTime now;
        stmt->set_string(0, now.c_str());
        stmt->set_string(1, now.c_str());
        stmt->set_int(2, 0);
        stmt->set_int(3, 0);
        stmt->set_int(4, 0);
        stmt->set_string(5, new_service.availability_zone.c_str());
        stmt->set_string(6, new_service.binary.c_str());
        stmt->set_string(7, new_service.host.c_str());
        stmt->set_string(8, new_service.topic.c_str());
        stmt->execute();
        return service_get_by_args(new_service);
    }

    virtual ServicePtr service_get_by_args(const NewService & search) {
        ensure();
        //TODO(tim.simpson): Get the preparedstatement result set to work with
        // int return values.
        stringstream query;
        query << "SELECT disabled, id, report_count "
            "FROM services WHERE services.binary= \""
            << con->escape_string(search.binary.c_str())
            << "\" AND host= \"" << con->escape_string(search.host.c_str())
            << "\" AND services.topic = \""
            << con->escape_string(search.topic.c_str())
            << "\" AND availability_zone = \""
            << con->escape_string(search.availability_zone.c_str()) << "\"";
        MySqlResultSetPtr results = con->query(query.str().c_str());
        if (!results->next()) {
            return ServicePtr();
        } else {
            ServicePtr service(new Service());
            service->availability_zone = search.availability_zone;
            service->binary = search.binary;
            optional<int> d_int = results->get_int(0);
            if (d_int) {
                service->disabled = (d_int.get() != 0);
            } else {
                service->disabled = boost::none;
            }
            service->host = search.host;
            service->id = results->get_int_non_null(1);
            service->report_count = results->get_int_non_null(2);
            service->topic = search.topic;
            return service;
        }
    }

    virtual void service_update(Service & service) {
        ensure();
        stringstream query;
        query << "UPDATE services "
                 "SET updated_at = ? , report_count = ? ";
        if (service.disabled) {
            query << ", disabled = ? ";
        }
        query << "WHERE services.binary= ? AND host= ? "
                 "AND services.topic = ? AND availability_zone = ?";
        MySqlPreparedStatementPtr stmt = con->prepare_statement(
            query.str().c_str());
        IsoDateTime now;
        int index = 0;
        stmt->set_string(index ++, now.c_str());
        stmt->set_int(index ++, service.report_count);
        if (service.disabled) {
            stmt->set_int(index ++, service.disabled.get() ? 1 : 0);
        }
        stmt->set_string(index ++, service.binary.c_str());
        stmt->set_string(index ++, service.host.c_str());
        stmt->set_string(index ++, service.topic.c_str());
        stmt->set_string(index ++, service.availability_zone.c_str());
        MySqlResultSetPtr results = stmt->execute();
    }

};

ApiPtr create_api(MySqlConnectionWithDefaultDbPtr con) {
    ApiPtr ptr(new ApiMySql(con));
    return ptr;
}

} } // end nova::db
