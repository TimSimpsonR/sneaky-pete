#ifndef __NOVA_DB_API
#define __NOVA_DB_API

#include "nova/flags.h"
#include "nova/db/mysql.h"
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <string>


namespace nova { namespace db {

struct NewService {
    std::string availability_zone;
    std::string binary;
    std::string host;
    std::string topic;
};

struct Service : public NewService {
    int id;
    boost::optional<bool> disabled;
    int report_count;
};

typedef boost::shared_ptr<Service> ServicePtr;


class Api {

public:
    virtual ~Api() {}

    virtual ServicePtr service_create(const NewService & new_service) = 0;

    virtual ServicePtr service_get_by_args(const NewService & new_service) = 0;

    virtual void service_update(Service & service) = 0;

};

typedef boost::shared_ptr<Api> ApiPtr;


ApiPtr create_api(nova::db::mysql::MySqlConnectionWithDefaultDbPtr con);


}} // nova::db

#endif
