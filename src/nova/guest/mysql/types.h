#ifndef __NOVA_GUEST_MYSQL_TYPES_H
#define __NOVA_GUEST_MYSQL_TYPES_H


#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <string>
#include <vector>


namespace nova { namespace guest { namespace mysql {

    class MySqlDatabase {

        public:
            MySqlDatabase();

            static const char * default_character_set();

            static const char * default_collation();

            inline const std::string & get_character_set() const {
                return character_set;
            }

            inline const std::string & get_collation() const {
                return collation;
            }

            inline const std::string & get_name() const {
                return name;
            }

            void set_character_set(const std::string & value);

            void set_collation(const std::string & value);

            void set_name(const std::string & value);

        private:
            std::string character_set;
            std::string collation;
            std::string name;
    };

    typedef boost::shared_ptr<MySqlDatabase> MySqlDatabasePtr;
    typedef std::vector<MySqlDatabasePtr> MySqlDatabaseList;
    typedef boost::shared_ptr<MySqlDatabaseList> MySqlDatabaseListPtr;


    class MySqlUser {
    public:
        MySqlUser();

        inline const std::string & get_name() const {
            return name;
        }

        const boost::optional<std::string> get_password() const {
            return password;
        }

        inline MySqlDatabaseListPtr get_databases() {
            return databases;
        }

        void set_name(const std::string & value);

        void set_password(const boost::optional<std::string> & value);

    private:
        std::string name;
        boost::optional<std::string> password;
        MySqlDatabaseListPtr databases;
    };


    typedef boost::shared_ptr<MySqlUser> MySqlUserPtr;
    typedef std::vector<MySqlUserPtr> MySqlUserList;
    typedef boost::shared_ptr<MySqlUserList> MySqlUserListPtr;


} } }  // end namespace

#endif //__NOVA_GUEST_SQL_GUEST_H
