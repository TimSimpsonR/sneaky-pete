#ifndef __NOVA_VOLUME_MANAGER_H
#define __NOVA_VOLUME_MANAGER_H

#include <string>
#include <boost/utility.hpp>


namespace nova {

// Forward Declaration so VolumeDevice can take VolumeManager reference
// Passing VolumeManager since many of its attributes are passed to VolumeDevice
class VolumeManager;

class VolumeDevice {
public:
    VolumeDevice(const std::string device_path,
                 const VolumeManager & manager);

    ~VolumeDevice();

    /** This runs the entire format process which includes:
      * check_device_exists
      * format_device
      * check_format **/
    void format();

    /** This runs the entire mount process which includes:
      * mount device
      * write to fstab **/
    void mount(const std::string mount_point);

    /** Check that the device path exists.
     * Verify that the device path has actually been created and can report
     * it's size, only then can it be available for formatting, retry
     * num_tries_device_exists to account for the time lag.  **/
    void check_device_exists();

    /** Calls mkfs to format the device at device_path.  **/
    void format_device();

    /** Checks that an unmounted volume is formatted.  **/
    void check_format();

private:

    const std::string device_path;
    const VolumeManager & manager;

};

class VolumeManager : boost::noncopyable  {
public:
    VolumeManager(const unsigned int num_tries_device_exists,
                  const std::string & volume_fstype,
                  const std::string & format_options,
                  const unsigned int volume_format_timeout,
                  const std::string & mount_options);

    ~VolumeManager();

    unsigned int get_num_tries_device_exists() const;

    std::string get_volume_fstype() const;

    std::string get_format_options() const;

    unsigned int get_volume_format_timeout() const;

    std::string get_mount_options() const;

    VolumeDevice create_volume_device(const std::string device_path);


private:

    const unsigned int num_tries_device_exists;
    const std::string volume_fstype;
    const std::string format_options;
    const unsigned int volume_format_timeout;
    const std::string mount_options;
};

typedef boost::shared_ptr<VolumeManager> VolumeManagerPtr;

class VolumeException : public std::exception {

    public:
        enum Code {
            DEVICE_DOES_NOT_EXIST,
            FORMAT_DEVICE_FAILURE,
            CHECK_FORMAT_FAILURE,
            MOUNT_FAILURE,
            WRITE_TO_FSTAB_FAILURE
        };

        VolumeException(Code code) throw();

        virtual ~VolumeException() throw();

        const Code code;

        virtual const char * what() const throw();
};

} // end namespace nova

#endif // __NOVA_VOLUME_MANAGER_H
