#include "pch.hpp"
#include "PrepareHandler.h"
#include <boost/foreach.hpp>
#include "nova/guest/GuestException.h"
#include "nova/Log.h"
#include <boost/optional.hpp>

using nova::guest::apt::AptGuest;
using nova::guest::apt::AptGuestPtr;
using nova::backup::BackupRestoreInfo;
using nova::datastores::DatastoreAppPtr;
using nova::datastores::DatastoreStatusPtr;
using nova::guest::GuestException;
using nova::guest::monitoring::MonitoringManagerPtr;
using boost::optional;
using std::string;
using nova::VolumeManagerPtr;
using std::vector;

namespace nova { namespace guest { namespace common {


namespace {
    const double TIME_OUT = 500;

    optional<BackupRestoreInfo> get_restore_info(const GuestInput & input) {
        // Restore the database?
        optional<BackupRestoreInfo> restore;
        const auto backup_url = input.args->get_optional_string("backup_url");
        if (backup_url && backup_url.get().length() > 0) {
            NOVA_LOG_INFO("Calling Restore...")
            if (!input.token) {
                NOVA_LOG_ERROR("No token given! Cannot do this restore!");
                throw GuestException(GuestException::MALFORMED_INPUT);
            }
            const auto token = input.token;
            const auto backup_checksum = input.args->get_optional_string("backup_checksum");
            restore = optional<BackupRestoreInfo>(
                BackupRestoreInfo(token.get(), backup_url.get(),
                                  backup_checksum.get()));
        }
        return restore;
    }
    // Grabs the packages argument from a JSON object.
    vector<string> get_packages_argument(JsonObjectPtr arg) {
        try {
            const auto packages = arg->get_array("packages")->to_string_vector();
            return packages;
        } catch(const JsonException) {
            NOVA_LOG_DEBUG("Interpretting \"packages\" as a single string.");
            vector<string> packages;
            packages.push_back(arg->get_string("packages"));
            return packages;
        }
    }

    void install_packages(AptGuest & apt, const vector<string> & packages) {
        NOVA_LOG_INFO("Installing datastore.");
        BOOST_FOREACH(const string & package, packages) {
            NOVA_LOG_INFO("Installing package %s...", package);
            apt.install(package.c_str(), TIME_OUT);
        }
    }

    void mount_volume(VolumeManagerPtr volume_manager, JsonObjectPtr args) {
        if (volume_manager) {
            const auto device_path = args->get_optional_string("device_path");
            const auto mount_point = volume_manager->get_mount_point();
            if (device_path && device_path.get().length() > 0) {
                NOVA_LOG_INFO("Mounting volume for prepare call...");
                bool write_to_fstab = true;
                VolumeDevice vol_device = volume_manager->create_volume_device(device_path.get());
                vol_device.format();
                vol_device.mount(mount_point, write_to_fstab);
                NOVA_LOG_INFO("Mounted the volume.");
            }
        }
    }

}  // end anonymous namespace


PrepareHandler::PrepareHandler(DatastoreAppPtr app,
                               AptGuestPtr apt,
                               DatastoreStatusPtr status,
                               VolumeManagerPtr volume_manager)
:   app(app),
    apt(apt),
    status(status),
    volume_manager(volume_manager) {
}

void PrepareHandler::prepare(const GuestInput & input) {
    if (status->is_installed()) {
        NOVA_LOG_ERROR("Datastore is already installed! Can't prepare.");
        throw GuestException(GuestException::MALFORMED_INPUT);
    }
    NOVA_LOG_INFO("Updating status to BUILDING...");
    status->begin_install();
    try {
        const auto root_password =
            input.args->get_optional_string("root_password");

        auto restore = get_restore_info(input);
        const auto config_contents = input.args->get_string("config_contents");
        const auto overrides = input.args->get_optional_string("overrides");

        mount_volume(volume_manager, input.args);

        apt->write_repo_files(input.args->get_optional_string("preferences_file"),
                              input.args->get_optional_string("sources_file"));

        const auto packages = get_packages_argument(input.args);
        install_packages(*apt, packages);

        app->prepare(root_password, config_contents, overrides, restore);
        JsonObjectPtr ssl_info = input.args->get_optional_object("ssl");
        if (ssl_info) {
            app->enable_ssl(
                ssl_info->get_string("ca_certificate"),
                ssl_info->get_string("private_key"),
                ssl_info->get_string("public_key")
            );
        }

        status->end_install_or_restart();
        NOVA_LOG_INFO("Preparation of datastore finished successfully.");
    } catch(const std::exception & e) {
        NOVA_LOG_ERROR("Error installing datastore!: %s", e.what());
        status->end_failed_install();
        throw;
    }
}

} } }
