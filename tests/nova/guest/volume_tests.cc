#define BOOST_TEST_MODULE volume_tests
#include <boost/test/unit_test.hpp>

#include "nova/Log.h"
#include "nova/VolumeManager.h"
#include <string>

using namespace nova;
using nova::VolumeManager;
using nova::VolumeDevice;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;


/* These are currently negative tests to test out proper exceptions are thrown
 * and handled related to volume formatting and mounting.
 * All test cases use bad device paths and mount points to trigger bad return
 * codes for formatting and mouting processes.
 */


#define CHECK_VOLUME_EXCEPTION(statement, ex_code) try { \
        statement ; \
        BOOST_FAIL("Should have thrown."); \
    } catch(const VolumeException & ve) { \
        BOOST_REQUIRE_EQUAL(ve.code, VolumeException::ex_code); \
    }


const std::string device_path= "/DEVICE_DOESNT_EXIST";
const std::string mount_point= "/MOUNT_POINT_DOESNT_EXIST";

const unsigned int num_tries_device_exists = 3;
const std::string volume_fstype = "ext3";
const std::string format_options = "-m 5";
const unsigned int volume_format_timeout = 120;
const std::string mount_options = "defaults,noatime";


BOOST_AUTO_TEST_CASE(check_device_exists_test)
{
    LogApiScope log(LogOptions::simple());


    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.check_device_exists(), DEVICE_DOES_NOT_EXIST);
}


BOOST_AUTO_TEST_CASE(format_device_test)
{
    LogApiScope log(LogOptions::simple());

    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.format_device(), FORMAT_DEVICE_FAILURE);
}

BOOST_AUTO_TEST_CASE(check_format_test)
{
    LogApiScope log(LogOptions::simple());

    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.check_format(), CHECK_FORMAT_FAILURE);
}

BOOST_AUTO_TEST_CASE(mount_test)
{
    LogApiScope log(LogOptions::simple());

    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.mount(mount_point), MOUNT_FAILURE);
}

BOOST_AUTO_TEST_CASE(unmount_test)
{
    LogApiScope log(LogOptions::simple());

    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.unmount(mount_point), UNMOUNT_FAILURE);
}

BOOST_AUTO_TEST_CASE(check_fs_test)
{
    LogApiScope log(LogOptions::simple());

    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);
    CHECK_VOLUME_EXCEPTION(vol_device.check_filesystem(), CHECK_FS_FAILURE);
}
