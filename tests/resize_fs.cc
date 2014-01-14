#include <stdio.h>
#include <iostream>
#include "nova/Log.h"
#include "nova/VolumeManager.h"

using namespace std;
using nova::Log;
using nova::LogApiScope;
using nova::LogOptions;
using nova::VolumeManager;
using nova::VolumeDevice;


int main(int argc, char **argv)
{
    LogApiScope log(LogOptions::simple());

    if (argc < 3) {
        cerr << "Usage: " << (argc > 0 ? argv[0] : "resize_fs")
        << " device_path mount_point" << endl;
        return 1;
    }

    const unsigned int num_tries_device_exists = 3;
    const string volume_fstype = "ext3";
    const string format_options = "-m 5";
    const unsigned int volume_format_timeout = 120;
    const string mount_options = "defaults,noatime";

    const string device_path = argv[1];
    const string mount_point = argv[2];

    /* Create Volume Manager. */
    VolumeManager volumeManager(
        num_tries_device_exists,
        volume_fstype,
        format_options,
        volume_format_timeout,
        mount_options
    );

    NOVA_LOG_INFO("Creating volume device with device_path: %s", device_path.c_str());
    VolumeDevice vol_device = volumeManager.create_volume_device(device_path);

    NOVA_LOG_INFO("Resizing device filesystem.");
    vol_device.resize_fs(mount_point);

    NOVA_LOG_INFO("Resized filesystem.");

    return 0;
}
