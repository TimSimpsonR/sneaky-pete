#include "pch.hpp"
#include "nova/guest/diagnostics.h"

#include "nova/guest/GuestException.h"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::json_string;
using nova::JsonData;
using nova::JsonDataPtr;
using nova::json_obj;
using nova::JsonObjectBuilder;
using nova::Log;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using namespace boost;

namespace nova { namespace guest { namespace diagnostics {

namespace {
    JsonObjectBuilder diagnostics_to_json_object(DiagInfoPtr diagnostics) {
        return json_obj(
            "version", diagnostics->version,
            "fd_size", diagnostics->fd_size,
            "vm_size", diagnostics->vm_size,
            "vm_peak", diagnostics->vm_peak,
            "vm_rss", diagnostics->vm_rss,
            "vm_hwm", diagnostics->vm_hwm,
            "threads", diagnostics->threads
        );
    }

    JsonObjectBuilder fs_stats_to_json_object(const FileSystemStatsPtr fs_stats) {
        return json_obj(
            "used", fs_stats->used,
            "free", fs_stats->free,
            "total", fs_stats->total
        );
    }

    JsonObjectBuilder hwinfo_to_json_object(const HwInfoPtr hwinfo) {
        return json_obj(
            "mem_total", hwinfo->mem_total,
            "num_cpus", hwinfo->num_cpus
        );
    }
} // end of anonymous namespace

InterrogatorMessageHandler::InterrogatorMessageHandler(const Interrogator & interrogator)
: interrogator(interrogator) {
}

JsonDataPtr InterrogatorMessageHandler::handle_message(const GuestInput & input) {
    NOVA_LOG_DEBUG("entering the handle_message method now ");
    if (input.method_name == "get_diagnostics") {
        NOVA_LOG_DEBUG("handling the get_diagnostics method");
        DiagInfoPtr diagnostics = interrogator.get_diagnostics();
        NOVA_LOG_DEBUG("returned from the get_diagnostics");
        if (diagnostics.get() != 0) {
            JsonDataPtr rtn(new JsonObject(
                diagnostics_to_json_object(diagnostics)));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else if (input.method_name == "get_filesystem_stats") {
        NOVA_LOG_DEBUG("handling the get_filesystem_stats method");
        string fs_path = input.args->get_string("fs_path");
        FileSystemStatsPtr fs_stats = interrogator.get_filesystem_stats(fs_path);
        JsonDataPtr rtn(new JsonObject(fs_stats_to_json_object(fs_stats)));
        return rtn;
    } else if (input.method_name == "get_hwinfo") {
        NOVA_LOG_DEBUG("handling the get_hwinfo method");
        HwInfoPtr hwinfo = interrogator.get_hwinfo();
        NOVA_LOG_DEBUG("returned from get_hwinfo");
        if (hwinfo.get() != 0) {
            JsonDataPtr rtn(new JsonObject(hwinfo_to_json_object(hwinfo)));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else {
        return JsonDataPtr();
    }
}



} } } // end namespace nova::guest::diagnostics
