#ifndef __NOVA_GUEST_MYSQL_MYSQLDATABASE_H
#define __NOVA_GUEST_MYSQL_MYSQLDATABASE_H

#include <boost/smart_ptr.hpp>
#include <string>
#include <vector>

namespace nova { namespace guest { namespace mysql {

class MySqlDatabase {
    public:
        MySqlDatabase(const std::string & name);

        const std::string & get_character_set() const {
            return character_set;
        }

        const std::string & get_collation() const {
            return collation;
        }

        inline const std::string & get_name() const {
            return name;
        }

        void set_character_set(const std::string & value) {
            this->character_set = value;
        }

        void set_collation(const std::string & value) {
            this->collation = value;
        }

        void set_name(const std::string & value);

    private:
        std::string character_set;
        std::string collation;
        std::string name;
};

typedef boost::shared_ptr<MySqlDatabase> MySqlDatabasePtr;
typedef std::vector<MySqlDatabasePtr> MySqlDatabaseList;
typedef boost::shared_ptr<MySqlDatabaseList> MySqlDatabaseListPtr;

} } }  // end nova::guest::mysql

#endif
