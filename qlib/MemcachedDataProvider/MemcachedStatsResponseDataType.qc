# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedStatsResponseDataType class definition

/** MemcachedStatsResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

%modern

public namespace MemcachedDataProvider {
#! Data type for memcached stats responses
public class MemcachedStatsResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "pid": {
                "type": StringOrNothingType,
                "display_name": "PID",
                "short_desc": "Server process ID",
                "desc": "Process id of this server process",
            },
            "uptime": {
                "type": StringOrNothingType,
                "display_name": "Uptime",
                "short_desc": "Seconds since start",
                "desc": "Number of seconds this server has been running",
            },
            "time": {
                "type": StringOrNothingType,
                "display_name": "Time",
                "short_desc": "Current UNIX timestamp",
                "desc": "Current UNIX time according to the server",
            },
            "version": {
                "type": StringOrNothingType,
                "display_name": "Version",
                "short_desc": "Server version",
                "desc": "Version string of this server",
            },
            "curr_items": {
                "type": StringOrNothingType,
                "display_name": "Current Items",
                "short_desc": "Items in cache",
                "desc": "Current number of items stored",
            },
            "total_items": {
                "type": StringOrNothingType,
                "display_name": "Total Items",
                "short_desc": "Total items ever stored",
                "desc": "Total number of items stored since server started",
            },
            "bytes": {
                "type": StringOrNothingType,
                "display_name": "Bytes Used",
                "short_desc": "Memory used",
                "desc": "Current number of bytes used to store items",
            },
            "max_connections": {
                "type": StringOrNothingType,
                "display_name": "Max Connections",
                "short_desc": "Maximum connections",
                "desc": "Maximum number of simultaneous connections",
            },
            "curr_connections": {
                "type": StringOrNothingType,
                "display_name": "Current Connections",
                "short_desc": "Open connections",
                "desc": "Number of open connections",
            },
            "total_connections": {
                "type": StringOrNothingType,
                "display_name": "Total Connections",
                "short_desc": "Total connections ever",
                "desc": "Total number of connections opened since server started",
            },
            "cmd_get": {
                "type": StringOrNothingType,
                "display_name": "Get Commands",
                "short_desc": "Total get commands",
                "desc": "Cumulative number of retrieval requests",
            },
            "cmd_set": {
                "type": StringOrNothingType,
                "display_name": "Set Commands",
                "short_desc": "Total set commands",
                "desc": "Cumulative number of storage requests",
            },
            "cmd_flush": {
                "type": StringOrNothingType,
                "display_name": "Flush Commands",
                "short_desc": "Total flush commands",
                "desc": "Cumulative number of flush requests",
            },
            "cmd_touch": {
                "type": StringOrNothingType,
                "display_name": "Touch Commands",
                "short_desc": "Total touch commands",
                "desc": "Cumulative number of touch requests",
            },
            "get_hits": {
                "type": StringOrNothingType,
                "display_name": "Get Hits",
                "short_desc": "Cache hits",
                "desc": "Number of keys that have been requested and found present",
            },
            "get_misses": {
                "type": StringOrNothingType,
                "display_name": "Get Misses",
                "short_desc": "Cache misses",
                "desc": "Number of items that have been requested and not found",
            },
            "delete_hits": {
                "type": StringOrNothingType,
                "display_name": "Delete Hits",
                "short_desc": "Successful deletes",
                "desc": "Number of deletion requests resulting in an item being removed",
            },
            "delete_misses": {
                "type": StringOrNothingType,
                "display_name": "Delete Misses",
                "short_desc": "Failed deletes",
                "desc": "Number of deletions for items not found",
            },
            "incr_hits": {
                "type": StringOrNothingType,
                "display_name": "Incr Hits",
                "short_desc": "Successful increments",
                "desc": "Number of successful increment requests",
            },
            "incr_misses": {
                "type": StringOrNothingType,
                "display_name": "Incr Misses",
                "short_desc": "Failed increments",
                "desc": "Number of increment requests for non-existent keys",
            },
            "decr_hits": {
                "type": StringOrNothingType,
                "display_name": "Decr Hits",
                "short_desc": "Successful decrements",
                "desc": "Number of successful decrement requests",
            },
            "decr_misses": {
                "type": StringOrNothingType,
                "display_name": "Decr Misses",
                "short_desc": "Failed decrements",
                "desc": "Number of decrement requests for non-existent keys",
            },
            "cas_hits": {
                "type": StringOrNothingType,
                "display_name": "CAS Hits",
                "short_desc": "Successful CAS operations",
                "desc": "Number of successful CAS requests",
            },
            "cas_misses": {
                "type": StringOrNothingType,
                "display_name": "CAS Misses",
                "short_desc": "CAS key not found",
                "desc": "Number of CAS requests for non-existent keys",
            },
            "cas_badval": {
                "type": StringOrNothingType,
                "display_name": "CAS Bad Value",
                "short_desc": "CAS token mismatch",
                "desc": "Number of CAS requests with mismatched CAS token",
            },
            "touch_hits": {
                "type": StringOrNothingType,
                "display_name": "Touch Hits",
                "short_desc": "Successful touches",
                "desc": "Number of keys that have been touched and found present",
            },
            "touch_misses": {
                "type": StringOrNothingType,
                "display_name": "Touch Misses",
                "short_desc": "Failed touches",
                "desc": "Number of items that have been touched and not found",
            },
            "evictions": {
                "type": StringOrNothingType,
                "display_name": "Evictions",
                "short_desc": "Items evicted",
                "desc": "Number of valid items removed from cache to free memory",
            },
            "reclaimed": {
                "type": StringOrNothingType,
                "display_name": "Reclaimed",
                "short_desc": "Items reclaimed",
                "desc": "Number of times an entry was stored using memory from an expired entry",
            },
            "bytes_read": {
                "type": StringOrNothingType,
                "display_name": "Bytes Read",
                "short_desc": "Total bytes read",
                "desc": "Total number of bytes read by this server from network",
            },
            "bytes_written": {
                "type": StringOrNothingType,
                "display_name": "Bytes Written",
                "short_desc": "Total bytes written",
                "desc": "Total number of bytes sent by this server to network",
            },
            "limit_maxbytes": {
                "type": StringOrNothingType,
                "display_name": "Max Bytes",
                "short_desc": "Memory limit",
                "desc": "Number of bytes this server is allowed to use for storage",
            },
            "threads": {
                "type": StringOrNothingType,
                "display_name": "Threads",
                "short_desc": "Worker threads",
                "desc": "Number of worker threads requested",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        # Allow additional stats fields not in the known list
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."*string");
    }
}

#! Constant for the stats response type
public const MemcachedStatsResponseDataType = new MemcachedStatsResponseDataType();
}
