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

        std::vector<std::string> get_include_files();

        bool get_daemonize();

        std::string get_pidfile();

        int get_port();

        int get_tcp_backlog();

        std::vector<std::string> get_bind_addrs();

        std::string get_unix_socket();

        int get_unix_socket_permission();

        int get_tcp_keepalive();

        std::string get_log_level();

        std::string get_log_file();

        bool get_syslog_enabled();

        std::string get_syslog_ident();

        std::string get_syslog_facility();

        int get_databases();

        std::vector<std::map <std::string, std::string> > get_save_intervals();

        bool get_stop_writes_on_bgsave_error();

        bool get_rdb_compression();

        bool get_rdb_checksum();

        std::string get_db_filename();

        std::string get_db_dir();

        std::map<std::string, std::string>get_slave_of();

        std::string get_master_auth();

        bool get_slave_serve_stale_data();

        bool get_slave_read_only();

        int get_repl_ping_slave_period();

        int get_repl_timeout();

        bool get_repl_disable_tcp_nodelay();

        std::string get_repl_backlog_size();

        int get_repl_backlog_ttl();

        int get_slave_priority();

        int get_min_slaves_to_write();

        int get_min_slaves_max_lag();

        std::string get_require_pass();

        std::vector<std::map <std::string, std::string> >
            get_renamed_commands();

        int get_max_client();

        int get_max_memory();

        std::string get_max_memory_policy();

        int get_max_memory_samples();

        bool get_append_only();

        std::string get_append_filename();

        std::string get_append_fsync();

        bool get_no_append_fsync_on_rewrite();

        int get_auto_aof_rewrite_percentage();

        std::string get_auto_aof_rewrite_min_size();

        int get_lua_time_limit();

        int get_slowlog_log_slower_than();

        int get_slowlog_max_len();

        std::string get_notify_keyspace_events();

        int get_hash_max_zip_list_entries();

        int get_hash_max_zip_list_value();

        int get_list_max_zip_list_entries();

        int get_list_max_zip_list_value();

        int get_set_max_intset_entries();

        int get_zset_max_zip_list_entries();

        int get_zset_max_zip_list_value();

        bool get_active_rehashing();

        std::vector<std::map <std::string, std::string> >
            get_client_output_buffer_limit();

        int get_hz();

        bool get_aof_rewrite_incremental_fsync();
};


}}//end nova::redis
#endif /* CONFIG_H */
