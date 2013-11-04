#include "nova/Log.h"
#include "nova/process.h"
#include "nova/VolumeManager.h"
#include <boost/assign/list_of.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <string>

using std::string;
using boost::format;
using namespace std;
using namespace boost::assign;
namespace proc = nova::process;

namespace nova {

namespace {  // Begin anonymous namespace

/**---------------------------------------------------------------------------
 *- VolumeMountPoint
 *---------------------------------------------------------------------------*/

class VolumeMountPoint {
    public:
        VolumeMountPoint(
            const std::string device_path,
            const std::string mount_point)
        :   device_path(device_path),
            mount_point(mount_point)
        {
        }

        ~VolumeMountPoint() {
        }

        void mount(const std::string volume_fstype,
                   const std::string mount_options) {
            // Create directory if it doesn't exist already
            proc::execute(list_of("/usr/bin/sudo")
                                 ("mkdir")
                                 ("-p")
                                 (mount_point.c_str()));
            NOVA_LOG_INFO("Mounting Volume...");
            // TODO (joe.cruz) if no mount_options then no '-o' flag?
            proc::CommandList cmds = list_of("/usr/bin/sudo")
                                            ("mount")
                                            ("-t")
                                            (volume_fstype.c_str())
                                            ("-o")
                                            (mount_options.c_str())
                                            (device_path.c_str())
                                            (mount_point.c_str());
            std::stringstream output;
            try{
                proc::execute(output, cmds);
            }
            catch (proc::ProcessException &e) {
                NOVA_LOG_ERROR("Mounting Device FAILED:%s", e.what());
                NOVA_LOG_ERROR("%s", output.str())
                throw VolumeException(VolumeException::MOUNT_FAILURE);
            }
        }

        void write_to_fstab(const std::string volume_fstype,
                            const std::string mount_options) {
            const std::string fstab_file_name = "/etc/fstab";
            const std::string fstab_original_file_name = "/etc/fstab.orig";
            const std::string new_fstab_file_name = "/tmp/newfstab";

            std::string fstab_line = str(format(
                "%s\t%s\t%s\t%s\t0\t0")
                % device_path.c_str()
                % mount_point.c_str()
                % volume_fstype.c_str()
                % mount_options.c_str()
                );

            NOVA_LOG_INFO("Writing to fstab...");
            try {
                    proc::execute(list_of("/usr/bin/sudo")("cp")
                                         (fstab_file_name.c_str())
                                         (fstab_original_file_name.c_str()));
                    proc::execute(list_of("/usr/bin/sudo")("cp")
                                         (fstab_file_name.c_str())
                                         (new_fstab_file_name.c_str()));
                    proc::execute(list_of("/usr/bin/sudo")("chmod")
                                         ("666")(new_fstab_file_name.c_str()));

                    ofstream tmp_new_fstab_file;
                    // Open file in append mode
                    tmp_new_fstab_file.open(new_fstab_file_name.c_str(), ios::app);
                    if (!tmp_new_fstab_file.good()) {
                        NOVA_LOG_ERROR("Couldn't open tmp new fstab file");
                        throw VolumeException(VolumeException::WRITE_TO_FSTAB_FAILURE);
                    }
                    tmp_new_fstab_file << endl;
                    tmp_new_fstab_file << fstab_line << endl;
                    tmp_new_fstab_file.close();

                    proc::execute(list_of("/usr/bin/sudo")("chmod")
                                         ("640")(new_fstab_file_name.c_str()));
                    proc::execute(list_of("/usr/bin/sudo")("mv")
                                         (new_fstab_file_name.c_str())
                                         (fstab_file_name.c_str()));
            }
            catch (proc::ProcessException &e) {
                NOVA_LOG_ERROR("Writing to fstab FAILED:%s", e.what());
                throw VolumeException(VolumeException::WRITE_TO_FSTAB_FAILURE);
            }
        }

    private:

        const std::string device_path;
        const std::string mount_point;
};

} // end anonymous namespace

/**---------------------------------------------------------------------------
 *- VolumeDevice
 *---------------------------------------------------------------------------*/


VolumeDevice::VolumeDevice(
    const string device_path,
    const VolumeManager & manager)
:   device_path(device_path),
    manager(manager)
{
}

VolumeDevice::~VolumeDevice() {
}

void VolumeDevice::format() {
    check_device_exists();
    format_device();
    check_format();
}

void VolumeDevice::mount(const std::string mount_point) {
    VolumeMountPoint volume_mount_point(device_path, mount_point);
    std::string volume_filesystem_type = manager.get_volume_fstype();
    std::string mount_options = manager.get_mount_options();
    volume_mount_point.mount(volume_filesystem_type, mount_options);
    volume_mount_point.write_to_fstab(volume_filesystem_type, mount_options);
}

void VolumeDevice::check_device_exists() {
    NOVA_LOG_INFO("Checking if device exists...");
    unsigned int attempts = 0;
    unsigned int max_retries =  manager.get_num_tries_device_exists();
    // Take into account first attempt for max attempts
    unsigned int max_attempts = max_retries + 1;

    while(attempts < max_attempts) {
        std::stringstream output;
        try{
            proc::execute(output, list_of("/usr/bin/sudo")
                                         ("blockdev")
                                         ("--getsize64")
                                         (device_path.c_str()));
            return;
        }
        catch (proc::ProcessException &e) {
            NOVA_LOG_INFO("Checking if device exists FAILED: %s", e.what());
            attempts++;
            if (attempts >= max_attempts) {
                NOVA_LOG_ERROR("Checking if device exists FAILED after (%u) attempts", attempts);
                NOVA_LOG_ERROR("%s", output.str());
                throw VolumeException(VolumeException::DEVICE_DOES_NOT_EXIST);
            }
        }
    }
}

void VolumeDevice::format_device() {
    NOVA_LOG_INFO("Formatting device...");
    std::string volume_fstype = manager.get_volume_fstype();
    std::string format_options = manager.get_format_options();
    proc::CommandList cmds = list_of("/usr/bin/sudo")
                                    ("mkfs")("-F")("-t")
                                    (volume_fstype.c_str())
                                    (format_options.c_str())
                                    (device_path.c_str());
    std::stringstream output;
    try{
        proc::execute(output, cmds);
        // TODO (joe.cruz) expect EOF
    }
    catch (proc::ProcessException &e) {
        NOVA_LOG_ERROR("Formatting Device FAILED:%s", e.what());
        NOVA_LOG_ERROR("%s", output.str());
        throw VolumeException(VolumeException::FORMAT_DEVICE_FAILURE);
    }
}

void VolumeDevice::check_format() {
   NOVA_LOG_INFO("Checking device format...");
    proc::CommandList cmds = list_of("/usr/bin/sudo")
                                    ("dumpe2fs")
                                    (device_path.c_str());
    // proc::Process<proc::StdErrAndStdOut> process(cmds);
    std::stringstream output;
    try{
        proc::execute(output, cmds);
        // TODO (joe.cruz) expect patterns "has_journal", "Wrong magic number"
    }
    catch (proc::ProcessException &e) {
        NOVA_LOG_ERROR("Check formatting Device FAILED:%s", e.what());
        NOVA_LOG_ERROR("%s", output.str());
        throw VolumeException(VolumeException::CHECK_FORMAT_FAILURE);
    }
}



/**---------------------------------------------------------------------------
 *- VolumeManager
 *---------------------------------------------------------------------------*/

VolumeManager::VolumeManager(
    const unsigned int num_tries_device_exists,
    const std::string & volume_fstype,
    const std::string & format_options,
    const unsigned int volume_format_timeout,
    const std::string & mount_options)
:   num_tries_device_exists(num_tries_device_exists),
    volume_fstype(volume_fstype),
    format_options(format_options),
    volume_format_timeout(volume_format_timeout),
    mount_options(mount_options)
{
}

VolumeManager::~VolumeManager() {
}

unsigned int VolumeManager::get_num_tries_device_exists() const {
    return num_tries_device_exists;
}

std::string VolumeManager::get_volume_fstype() const {
    return volume_fstype;
}

std::string VolumeManager::get_format_options() const {
    return format_options;
}

unsigned int VolumeManager::get_volume_format_timeout() const {
    return volume_format_timeout;
}

std::string VolumeManager::get_mount_options() const {
    return mount_options;
}

VolumeDevice VolumeManager::create_volume_device(const string device_path) {
    return VolumeDevice(device_path, *this);
}

/**---------------------------------------------------------------------------
 *- VolumeException
 *---------------------------------------------------------------------------*/

VolumeException::VolumeException(Code code) throw()
: code(code) {
}

VolumeException::~VolumeException() throw() {
}

const char * VolumeException::what() const throw() {
    switch(code) {
        case DEVICE_DOES_NOT_EXIST:
            return "Device does not exist after retries.";
        case FORMAT_DEVICE_FAILURE:
            return "There was a failure while formatting device.";
        case CHECK_FORMAT_FAILURE:
            return "There was a failure checking format of device.";
        case MOUNT_FAILURE:
            return "There was a failure mounting device.";
        case WRITE_TO_FSTAB_FAILURE:
            return "There was a failure writing to fstab.";
        default:
            return "An error occurred.";
    }
}

} // end namespace nova
