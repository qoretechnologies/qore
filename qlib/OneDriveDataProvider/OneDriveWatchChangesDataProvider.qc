# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveWatchChangesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! Watch provider for file changes using delta query
public class OneDriveWatchChangesDataProvider inherits OneDriveDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Watch constructor options
        const WatchConstructorOptions = ConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "Polling interval in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
                    + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
        };

        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FILE_CHANGE,
            "type": "OneDriveWatchChangesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Delta link for tracking changes
        *string delta_link;

        #! Flag to track first poll (seed cursor without firing events)
        bool first_poll = True;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : OneDriveDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : OneDriveDataProviderBase(options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! Polls for changes using delta query
    private pollOnce() {
        try {
            string path;
            if (delta_link) {
                # Use the delta link from the previous poll
                # Strip the base URL prefix if present
                path = delta_link;
                path =~ s/^https:\/\/graph\.microsoft\.com\/v1\.0\///;
            } else {
                # Initial delta request — get latest token
                path = "me/drive/root/delta?token=latest";
            }

            # Fetch all pages of changes
            list<hash<auto>> changes = ();
            while (True) {
                if (isStopping()) {
                    break;
                }
                hash<auto> response = doRestCommand("GET", path);
                if (response.body.value) {
                    changes += response.body.value;
                }
                if (*string next_link = response.body."@odata.nextLink") {
                    next_link =~ s/^https:\/\/graph\.microsoft\.com\/v1\.0\///;
                    path = next_link;
                } else {
                    # Save the delta link for next poll
                    delta_link = response.body."@odata.deltaLink";
                    break;
                }
            }

            if (first_poll) {
                # On first poll, seed the cursor without firing events
                first_poll = False;
                return;
            }

            # Notify observers for each change
            foreach hash<auto> item in (changes) {
                if (isStopping()) {
                    break;
                }
                notifyObservers(EVENT_FILE_CHANGE, item);
            }
        } catch (hash<ExceptionInfo> ex) {
            error("Error polling for %s events: %s: %s", EVENT_FILE_CHANGE, ex.err, ex.desc);
        }
    }

    #! Returns example event data
    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> response = doRestCommand("GET", "me/drive/root/children?$top=1");
            if (response.body.value) {
                return response.body.value[0];
            }
        } catch () {
        }
        return {
            "id": "0123456789abc",
            "name": "example-file.txt",
            "size": 1024,
            "webUrl": "https://onedrive.live.com/...",
            "createdDateTime": "2026-04-09T12:00:00Z",
            "lastModifiedDateTime": "2026-04-09T12:00:00Z",
            "file": {
                "mimeType": "text/plain",
            },
        };
    }

    #! Returns the event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_FILE_CHANGE: <DataProviderMessageInfo>{
                "desc": "Fired when a file or folder is created, modified, or deleted in OneDrive",
                "type": OneDriveDriveItemDataType,
            },
        };
    }
}

#! Events container data provider
public class OneDriveEventsDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "OneDriveEventsDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_observers": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            EVENT_FILE_CHANGE:
                Class::forName("OneDriveDataProvider::OneDriveWatchChangesDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
