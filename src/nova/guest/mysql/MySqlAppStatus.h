#ifndef __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H
#define __NOVA_GUEST_MYSQL_MYSQLNOVAUPDATER_H

#include <list>
#include "nova/datastores/DatastoreStatus.h"
#include "nova/db/mysql.h"
#include "nova/rpc/sender.h"
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <sstream>
#include <string>
#include "nova/utils/subsecond.h"


namespace nova { namespace guest { namespace mysql {

    class MySqlAppStatus : public nova::datastores::DatastoreStatus {
        public:
            MySqlAppStatus(nova::rpc::ResilientSenderPtr sender,
                           bool is_mysql_installed);

            virtual ~MySqlAppStatus();

        protected:
            virtual Status determine_actual_status() const;

            virtual void execute(std::stringstream & out,
                                 const std::list<std::string> & cmds) const;

            boost::optional<std::string> find_mysql_pid_file() const;

            virtual bool is_file(const char * file_path) const;
    };

    typedef boost::shared_ptr<MySqlAppStatus> MySqlAppStatusPtr;

} } }

#endif
