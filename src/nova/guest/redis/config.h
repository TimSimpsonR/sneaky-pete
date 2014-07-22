#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <vector>
#include <string>


namespace nova { namespace redis {


class Config
{
    private:
        std::map<std::string, std::string>_options;

        std::vector<std::string> _include;

        std::vector<std::string> _bind_addrs;

        std::vector<std::map <std::string, std::string> > _save;

        std::map<std::string, std::string> _slave_of;

        std::vector<std::map <std::string, std::string> > _renamed_commands;

        std::vector<std::map <std::string, std::string> >
            _client_output_buffer_limit;

        std::string _redis_config;

        std::string _get_string_value(std::string key);

        int _get_int_value(std::string key);

        bool _get_bool_value(std::string key);

    public:
        Config(std::string config);

        bool write_new_config(std::string config);

        std::vector<std::string> get_include_files();

        void set_include_files(std::string value,
                          bool clear_existing_values);

        bool get_daemonize();

        void set_daemonize(bool value);

        std::string get_pidfile();

        void set_pidfile(std::string value);

        int get_port();

        void set_port(int value);

        int get_tcp_backlog();

        void set_tcp_backlog(int value);

        std::vector<std::string> get_bind_addrs();

        void set_bind_addrs(std::string value,
                            bool clear_existing_values);

        std::string get_unix_socket();

        void set_unix_socket(std::string value);

        int get_unix_socket_permission();

        void set_unix_socket_permission(int value);

        int get_tcp_keepalive();

        void set_tcp_keepalive(int value);

        std::string get_log_level();

        void set_log_level(std::string value);

        std::string get_log_file();

        void set_log_file(std::string value);

        bool get_syslog_enabled();

        void set_syslog_enabled(bool value);

        std::string get_syslog_ident();

        void set_syslog_ident(std::string value);

        std::string get_syslog_facility();

        void set_syslog_facility(std::string value);

        int get_databases();

        void set_databases(int value);

        std::vector<std::map <std::string, std::string> > get_save_intervals();

        void set_save_intervals (std::string changes,
                                 std::string seconds,
                                 bool clear_existing_values);

        bool get_stop_writes_on_bgsave_error();

        void set_stop_writes_on_bgsave_error(bool value);

        bool get_rdb_compression();

        void set_rdb_compression(bool value);

        bool get_rdb_checksum();

        void set_rdb_checksum(bool value);

        std::string get_db_filename();

        void set_db_filename(std::string value);

        std::string get_db_dir();

        void set_db_dir(std::string value);

        std::map<std::string, std::string>get_slave_of();

        void set_slave_of(std::string master_ip,
                          std::string master_port);

        std::string get_master_auth();

        void set_master_auth(std::string value);

        bool get_slave_serve_stale_data();

        void set_slave_serve_stale_data(bool value);

        bool get_slave_read_only();

        void set_slave_read_only(bool value);

        int get_repl_ping_slave_period();

        void set_repl_ping_slave_period(int value);

        int get_repl_timeout();

        void set_repl_timeout(int value);

        bool get_repl_disable_tcp_nodelay();

        void set_repl_disable_tcp_nodelay(bool value);

        std::string get_repl_backlog_size();

        void set_repl_backlog_size(std::string value);

        int get_repl_backlog_ttl();

        void set_repl_backlog_ttl(int value);

        int get_slave_priority();

        void set_slave_priority(int value);

        int get_min_slaves_to_write();

        void set_min_slaves_to_write(int value);

        int get_min_slaves_max_lag();

        void set_min_slaves_max_lag(int value);

        std::string get_require_pass();

        void set_require_pass(std::string value);

        std::vector<std::map <std::string, std::string> >
            get_renamed_commands();

        void set_renamed_commands(std::string command,
                                  std::string renamed_command,
                                  bool clear_existing_values);

        int get_max_clients();

        void set_max_clients(int value);

        std::string get_max_memory();

        void set_max_memory(std::string value);

        std::string get_max_memory_policy();

        void set_max_memory_policy(std::string value);

        int get_max_memory_samples();

        void set_max_memory_samples(int value);

        bool get_append_only();

        void set_append_only(bool value);

        std::string get_append_filename();

        void set_append_filename(std::string value);

        std::string get_append_fsync();

        void set_append_fsync(std::string value);

        bool get_no_append_fsync_on_rewrite();

        void set_no_append_fsync_on_rewrite(bool value);

        int get_auto_aof_rewrite_percentage();

        void set_auto_aof_rewrite_percentage(int value);

        std::string get_auto_aof_rewrite_min_size();

        void set_auto_aof_rewrite_min_size(std::string value);

        int get_lua_time_limit();

        void set_lua_time_limit(int value);

        int get_slowlog_log_slower_than();

        void set_slowlog_log_slower_than(int value);

        int get_slowlog_max_len();

        void set_slowlog_max_len(int value);

        std::string get_notify_keyspace_events();

        void set_notify_keyspace_events(std::string value);

        int get_hash_max_zip_list_entries();

        void set_hash_max_zip_list_entries(int value);

        int get_hash_max_zip_list_value();

        void set_hash_max_zip_list_value(int value);

        int get_list_max_zip_list_entries();

        void set_list_max_zip_list_entries(int value);

        int get_list_max_zip_list_value();

        void set_list_max_zip_list_value(int value);

        int get_set_max_intset_entries();

        void set_set_max_intset_entries(int value);

        int get_zset_max_zip_list_entries();

        void set_zset_max_zip_list_entries(int value);

        int get_zset_max_zip_list_value();

        void set_zset_max_zip_list_value(int value);

        bool get_active_rehashing();

        void set_active_rehashing(bool value);

        std::vector<std::map <std::string, std::string> >
            get_client_output_buffer_limit();

        void set_client_output_buffer_limit(std::string buff_soft_seconds,
                                            std::string buff_hard_limit,
                                            std::string buff_soft_limit,
                                            std::string buff_class,
                                            bool clear_existing_values);

        int get_hz();

        void set_hz(int value);

        bool get_aof_rewrite_incremental_fsync();

        void set_aof_rewrite_incremental_fsync(bool value);
};


}}//end nova::redis
#endif /* CONFIG_H */
