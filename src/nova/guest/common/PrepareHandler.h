#ifndef __NOVA_GUEST_COMMON_PREPAREHANDLER_H
#define __NOVA_GUEST_COMMON_PREPAREHANDLER_H


#include "nova/guest/apt.h"
#include "nova/datastores/DatastoreApp.h"
#include "nova/datastores/DatastoreStatus.h"
#include "nova/guest/monitoring/monitoring.h"
#include <boost/shared_ptr.hpp>
#include "nova/VolumeManager.h"


namespace nova { namespace guest { namespace common {

class PrepareHandler {
public:
    PrepareHandler(nova::datastores::DatastoreAppPtr app,
                   nova::guest::apt::AptGuestPtr apt,
                   nova::datastores::DatastoreStatusPtr status,
                   VolumeManagerPtr volumeManager);

    void prepare(const GuestInput & input);

private:
    nova::datastores::DatastoreAppPtr app;
    nova::guest::apt::AptGuestPtr apt;
    bool skip_install_for_prepare;
    nova::datastores::DatastoreStatusPtr status;
    VolumeManagerPtr volume_manager;
};

typedef boost::shared_ptr<PrepareHandler> PrepareHandlerPtr;

} } }

#endif
