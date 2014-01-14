#include "pch.hpp"
#include "nova/guest/diagnostics.h"

#include "nova/guest/GuestException.h"
#include <boost/foreach.hpp>
#include "nova/Log.h"
#include <sstream>
#include <string>

using nova::JsonData;
using nova::JsonDataPtr;
using nova::Log;
using nova::guest::GuestException;
using boost::optional;
using std::string;
using std::stringstream;
using namespace boost;

namespace nova { namespace guest { namespace diagnostics {

namespace {
    string diagnostics_to_stream(DiagInfoPtr diagnostics) {
        stringstream out;
        out << "{";
        out << JsonData::json_string("version") << ": ";
        out << JsonData::json_string(diagnostics->version);
        out << ",";
        out << JsonData::json_string("fd_size") << ": " << diagnostics->fd_size;
        out << ",";
        out << JsonData::json_string("vm_size") << ": " << diagnostics->vm_size;
        out << ",";
        out << JsonData::json_string("vm_peak") << ": " << diagnostics->vm_peak;
        out << ",";
        out << JsonData::json_string("vm_rss") << ": " << diagnostics->vm_rss;
        out << ",";
        out << JsonData::json_string("vm_hwm") << ": " << diagnostics->vm_hwm;
        out << ",";
        out << JsonData::json_string("threads") << ": " << diagnostics->threads;
        out << "}";
        return out.str();
    }

    string fs_stats_to_stream(const FileSystemStatsPtr fs_stats) {
        stringstream out;
        out << "{";
        out << JsonData::json_string("used") << ": " << fs_stats->used;
        out << ",";
        out << JsonData::json_string("free") << ": " << fs_stats->free;
        out << ",";
        out << JsonData::json_string("total") << ": " << fs_stats->total;
        out << "}";
        return out.str();
    }

    string hwinfo_to_stream(const HwInfoPtr hwinfo) {
        stringstream out;
        out << "{";
        out << JsonData::json_string("mem_total") << ": " << hwinfo->mem_total;
        out << ",";
        out << JsonData::json_string("num_cpus") << ": " << hwinfo->num_cpus;
        out << "}";
        return out.str();
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
            //convert from map to string
            string output = diagnostics_to_stream(diagnostics);
            NOVA_LOG_DEBUG("output = %s", output.c_str());
            JsonDataPtr rtn(new JsonObject(output.c_str()));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else if (input.method_name == "get_filesystem_stats") {
        NOVA_LOG_DEBUG("handling the get_filesystem_stats method");
        string fs_path = input.args->get_string("fs_path");
        FileSystemStatsPtr fs_stats = interrogator.get_filesystem_stats(fs_path);
        string json = fs_stats_to_stream(fs_stats);
        JsonDataPtr rtn(new JsonObject(json.c_str()));
        return rtn;
    } else if (input.method_name == "get_hwinfo") {
        NOVA_LOG_DEBUG("handling the get_hwinfo method");
        HwInfoPtr hwinfo = interrogator.get_hwinfo();
        NOVA_LOG_DEBUG("returned from get_hwinfo");
        if (hwinfo.get() != 0) {
            string output = hwinfo_to_stream(hwinfo);
            NOVA_LOG_DEBUG("output = %s", output.c_str());
            JsonDataPtr rtn(new JsonObject(output.c_str()));
            return rtn;
        } else {
            return JsonData::from_null();
        }
    } else {
        return JsonDataPtr();
    }
}



} } } // end namespace nova::guest::diagnostics
