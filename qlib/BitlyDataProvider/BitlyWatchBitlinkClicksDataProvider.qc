# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyWatchBitlinkClicksDataProvider.qc Bitly watch bitlink clicks data provider

/*  BitlyWatchBitlinkClicksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly watch bitlink clicks data provider
/** Polls for bitlinks with new clicks by checking the modification time of bitlinks
    in a group and emitting events for recently modified ones.
*/
public class BitlyWatchBitlinkClicksDataProvider inherits BitlyWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_BITLINK_CLICK,
            "desc": "Watch for bitlinks with new clicks by polling for recently modified "
                "bitlinks in a group",
            "type": "BitlyWatchBitlinkClicksDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "group_guid": <DataProviderOptionInfo>{
                    "display_name": "Group GUID",
                    "short_desc": "Group to watch for new clicks",
                    "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                    "desc": "The group GUID to watch for bitlink click events. "
                        "If not provided, uses the default group",
                    "ref_data": "groups",
                },
            },
        };
    }

    private {
        #! The group GUID to watch
        *string group_guid;
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : BitlyWatchDataProviderBase(rest, options) {
        group_guid = options.group_guid;
    }

    #! Returns the provider name
    string getName() {
        return EVENT_BITLINK_CLICK;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for bitlinks with new clicks by polling for recently modified "
            "bitlinks in a group";
    }

    #! Fetches recently modified bitlinks (potential click activity)
    private list<hash<auto>> fetchRecords(date since) {
        # Resolve group_guid if not set
        if (!group_guid) {
            hash<auto> groups = doGet("groups");
            if (groups.groups) {
                group_guid = groups.groups[0].guid;
            }
            if (!group_guid) {
                throw "BITLY-ERROR", "Cannot watch for clicks: no group_guid provided and "
                    "no default group found";
            }
        }

        # Get recently modified bitlinks
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        hash<auto> params = {
            "modified_after": since_str,
        };

        return fetchAllPages("groups/" + group_guid + "/bitlinks", "links", params);
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_BITLINK_CLICK;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() {
        return new BitlyBitlinkDataType();
    }

    #! Returns the timestamp field
    private string getTimestampField() {
        return "created_at";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
