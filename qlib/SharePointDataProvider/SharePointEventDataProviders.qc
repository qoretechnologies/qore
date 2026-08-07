# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointEventDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! Common base for the SharePoint watch providers, which all follow a Graph delta feed
/** A delta feed hands back an opaque cursor - a delta link - that resumes exactly where the last
    read stopped.  That is what makes these events an improvement on the TypeScript app they
    replace, which polled the list newest-first and reconciled against the IDs it had already seen:
    a delta feed reports deletes and moves, which that approach cannot see at all, and it does not
    re-read the whole head of the list on every tick.
*/
public class SharePointWatchDataProviderBase inherits SharePointDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    private {
        #! Delta link for tracking changes
        *string delta_link;

        #! Flag to track first poll (seed cursor without firing events)
        bool first_poll = True;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : SharePointDataProviderBase(options),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
    }

    #! Cancels in-flight REST operations for graceful shutdown
    private cancelPollOperation() {
        rest.close();
    }

    #! The Graph delta feed returns changes in ascending change order
    /** The delta link is an opaque cursor, so this provider is a documented exemption from the
        shared ordered template (@ref DataProvider::AbstractOrderedWatchDataProvider): there is no
        per-change key to sort or compare against.
    */
    string getSourceOrdering() {
        return WSO_ASCENDING;
    }

    #! Returns the path of the delta feed this provider follows, with no query string
    private abstract string getDeltaPath();

    #! Returns True if the given change should be raised as this provider's event
    private abstract bool acceptChange(hash<auto> change);

    #! Returns the name of the event this provider raises
    private abstract string getEventName();

    #! Returns the configuration that defines which change feed this provider follows
    /** The feed is defined by the library or list the provider was configured with, so the cursor
        is keyed on that and survives an unrelated option edit such as a change of poll interval.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "feed": getDeltaPath(),
        };
    }

    #! Restores the delta link
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.delta_link.typeCode() != NT_STRING || !checkpoint.delta_link.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid SharePoint watch checkpoint; expecting version 1 and "
                "a non-empty delta_link";
        }
        delta_link = checkpoint.delta_link;
        # a restored delta link means the baseline has already been established
        first_poll = False;
    }

    #! Durably advances the delta link
    private commitCheckpoint(string next_delta_link) {
        if (!next_delta_link.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist an empty SharePoint delta link";
        }
        persistCheckpoint({
            "version": 1,
            "delta_link": next_delta_link,
        });
        delta_link = next_delta_link;
    }

    #! Polls once, advancing the delta link only after every change has been delivered
    /** @note The replacement delta link is stored only after every change it supersedes has been
        raised.  Storing it first would skip the whole batch if delivery then failed; storing it
        afterwards means a failure re-reads the batch, so delivery is at-least-once.
    */
    private pollOnce() {
        try {
            string path = delta_link.val()
                ? SharePointDataProviderBase::stripGraphBaseUrl(delta_link)
                : getDeltaPath() + "/delta";

            list<hash<auto>> changes = ();
            *string next_delta_link;
            while (True) {
                if (isStopping()) {
                    # the delta link stays where it is, so this batch is read again
                    return;
                }
                hash<auto> response = doRestCommand("GET", path);
                if (response.body.value) {
                    changes += response.body.value;
                }
                if (*string next_link = response.body."@odata.nextLink") {
                    path = SharePointDataProviderBase::stripGraphBaseUrl(next_link);
                } else {
                    next_delta_link = response.body."@odata.deltaLink";
                    break;
                }
            }

            if (first_poll) {
                # on the first poll the cursor is seeded without firing events, so that turning a
                # trigger on does not replay the entire history of the library or list
                first_poll = False;
                if (next_delta_link.val()) {
                    commitCheckpoint(next_delta_link);
                }
                return;
            }

            foreach hash<auto> change in (changes) {
                if (isStopping()) {
                    # the delta link stays where it is, so the remaining changes are read again
                    return;
                }
                if (acceptChange(change)) {
                    notifyObservers(getEventName(), change);
                }
            }

            # the batch has been delivered, so the replacement delta link can be made durable
            if (next_delta_link.val()) {
                commitCheckpoint(next_delta_link);
            }
        } catch (hash<ExceptionInfo> ex) {
            # NOTE: "LoggerWrapper::error()", not a bare "error()"; AbstractDataProvider declares
            # "error(string err, string fmt, ...)", which throws rather than logging and wins overload
            # resolution, so a bare call would raise an exception from the handler reporting the error
            LoggerWrapper::error(sprintf("Error polling for %s events: %s: %s", getEventName(), ex.err,
                ex.desc));
        }
    }
}

#! Constructor options for the document library watch providers
public const DriveWatchConstructorOptions = SharePointDataProviderBase::ConstructorOptions + {
    "site_id": SharePointDataProviderBase::ConstructorOptions."site_id" + <DataProviderOptionInfo>{
        "required": True,
        "ref_data": "sites",
        "has_dependents": True,
        "on_change": ("refetch",),
    },
    "drive_id": SharePointDataProviderBase::ConstructorOptions."drive_id" + <DataProviderOptionInfo>{
        "required": True,
        "ref_data": "drives",
        "depends_on": ("site_id",),
    },
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
            + DefaultPollInterval + "`)",
        "default_value": DefaultPollInterval,
    },
};

#! Watch provider for documents added to a document library
public class SharePointNewFileDataProvider inherits SharePointWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FILE,
            "type": "SharePointNewFileDataProvider",
            "supports_observable": True,
            "constructor_options": DriveWatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointWatchDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointWatchDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEventName() { return EVENT_NEW_FILE; }
    private string getDeltaPath() { return getDriveBasePath(NOTHING) + "/root"; }

    #! Accepts files that have just appeared
    /** A delta entry carries the item as it now stands, not what changed about it, so "new" is read
        as an item whose creation and last modification are the same moment.  A deleted item carries
        a \c deleted facet and is not a new file whatever its timestamps say.
    */
    private bool acceptChange(hash<auto> change) {
        return !exists change.deleted && exists change.file
            && change.createdDateTime == change.lastModifiedDateTime;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_FILE: <DataProviderMessageInfo>{
                "desc": "Fired when a file is added to the document library",
                "type": OneDriveDataProvider::OneDriveDriveItemDataType,
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        return SharePointEventExamples::getDriveItemExample(True);
    }
}

#! Watch provider for documents modified in a document library
public class SharePointFileModifiedDataProvider inherits SharePointWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FILE_MODIFIED,
            "type": "SharePointFileModifiedDataProvider",
            "supports_observable": True,
            "constructor_options": DriveWatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointWatchDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointWatchDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEventName() { return EVENT_FILE_MODIFIED; }
    private string getDeltaPath() { return getDriveBasePath(NOTHING) + "/root"; }

    #! Accepts files that existed before this change
    private bool acceptChange(hash<auto> change) {
        return !exists change.deleted && exists change.file
            && change.createdDateTime != change.lastModifiedDateTime;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_FILE_MODIFIED: <DataProviderMessageInfo>{
                "desc": "Fired when a file in the document library is modified",
                "type": OneDriveDataProvider::OneDriveDriveItemDataType,
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        return SharePointEventExamples::getDriveItemExample(False);
    }
}

#! Watch provider for folders added to a document library
public class SharePointNewFolderDataProvider inherits SharePointWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FOLDER,
            "type": "SharePointNewFolderDataProvider",
            "supports_observable": True,
            "constructor_options": DriveWatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointWatchDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointWatchDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEventName() { return EVENT_NEW_FOLDER; }
    private string getDeltaPath() { return getDriveBasePath(NOTHING) + "/root"; }

    #! Accepts folders that have just appeared
    /** The library's own root folder appears in the first delta page of a fresh feed and is not a
        newly created folder, so it is excluded by its missing parent reference path.
    */
    private bool acceptChange(hash<auto> change) {
        return !exists change.deleted && exists change.folder && exists change.parentReference.path
            && change.createdDateTime == change.lastModifiedDateTime;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_FOLDER: <DataProviderMessageInfo>{
                "desc": "Fired when a folder is added to the document library",
                "type": OneDriveDataProvider::OneDriveDriveItemDataType,
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        return SharePointEventExamples::getFolderExample();
    }
}

#! Constructor options for the list watch provider
public const ListWatchConstructorOptions = SharePointDataProviderBase::ConstructorOptions + {
    "site_id": SharePointDataProviderBase::ConstructorOptions."site_id" + <DataProviderOptionInfo>{
        "required": True,
        "ref_data": "sites",
        "has_dependents": True,
        "on_change": ("refetch",),
    },
    "list_id": <DataProviderOptionInfo>{
        "display_name": "List",
        "short_desc": "The list to watch",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The list to watch for new items",
        "required": True,
        "ref_data": "lists",
        "depends_on": ("site_id",),
    },
    "poll_interval": <DataProviderOptionInfo>{
        "display_name": "Poll Interval",
        "short_desc": "Polling interval in seconds",
        "type": AbstractDataProviderType::get(SoftIntOrNothingType),
        "desc": "Polling interval in seconds (minimum `" + MinPollInterval + "`, default `"
            + DefaultPollInterval + "`)",
        "default_value": DefaultPollInterval,
    },
};

#! Watch provider for items added to a list
/** The event keeps the \c new-row name the TypeScript trigger used, so flows built on it keep
    resolving.
*/
public class SharePointNewRowDataProvider inherits SharePointWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_ROW,
            "type": "SharePointNewRowDataProvider",
            "supports_observable": True,
            "constructor_options": ListWatchConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The list this provider watches
        *string watch_list_id;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointWatchDataProviderBase(rest, options) {
        watch_list_id = options.list_id;
    }

    constructor(*hash<auto> options) : SharePointWatchDataProviderBase(options) {
        watch_list_id = options.list_id;
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private string getEventName() { return EVENT_NEW_ROW; }

    private string getDeltaPath() {
        return getListBasePath({"list_id": watch_list_id}) + "/items";
    }

    #! Accepts items that have just appeared
    private bool acceptChange(hash<auto> change) {
        return !exists change.deleted
            && change.createdDateTime == change.lastModifiedDateTime;
    }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_ROW: <DataProviderMessageInfo>{
                "desc": "Fired when an item is added to the list",
                "type": SharePointListItemDataType,
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        try {
            string path = getListBasePath({"list_id": watch_list_id})
                + "/items?$expand=fields&$top=1&$orderby=createdDateTime desc";
            list<hash<auto>> items = getAllPages(path, 1, NonIndexedQueryHeaders);
            if (items) {
                return items[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getExampleEventDataImpl: %s: %s", ex.err, ex.desc);
        }
        return SharePointEventExamples::getListItemExample();
    }
}

#! Events container data provider
public class SharePointEventsDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "SharePointEventsDataProvider",
            "supports_children": True,
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "children_can_support_observers": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            EVENT_NEW_FILE: Class::forName("SharePointDataProvider::SharePointNewFileDataProvider"),
            EVENT_FILE_MODIFIED:
                Class::forName("SharePointDataProvider::SharePointFileModifiedDataProvider"),
            EVENT_NEW_FOLDER: Class::forName("SharePointDataProvider::SharePointNewFolderDataProvider"),
            EVENT_NEW_ROW: Class::forName("SharePointDataProvider::SharePointNewRowDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

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
        return cls.newObject(rest, {
            "site_id": default_site_id,
            "drive_id": default_drive_id,
        });
    }
}

#! Example event payloads, used when a live sample cannot be read
public class SharePointEventExamples {
    #! Returns an example drive item
    /** @param created if the example should look newly created

        @return an example drive item
    */
    static hash<auto> getDriveItemExample(bool created) {
        string ts = created ? "2026-04-09T12:00:00Z" : "2026-04-09T12:00:00Z";
        return {
            "id": "01SOISHHU5OH3QHANXWZGJWXLHS5THDK4K",
            "name": "quarterly-report.pdf",
            "size": 148213,
            "webUrl": "https://contoso.sharepoint.com/Shared%20Documents/quarterly-report.pdf",
            "createdDateTime": ts,
            "lastModifiedDateTime": created ? ts : "2026-04-10T09:15:00Z",
            "file": {
                "mimeType": "application/pdf",
            },
            "parentReference": {
                "driveId": "b!abcdef0123456789",
                "path": "/drive/root:",
            },
        };
    }

    #! Returns an example folder
    /** @return an example folder
    */
    static hash<auto> getFolderExample() {
        return {
            "id": "01SOISHHU5OH3QHANXWZGJWXLHS5THDK5L",
            "name": "Contracts",
            "webUrl": "https://contoso.sharepoint.com/Shared%20Documents/Contracts",
            "createdDateTime": "2026-04-09T12:00:00Z",
            "lastModifiedDateTime": "2026-04-09T12:00:00Z",
            "folder": {
                "childCount": 0,
            },
            "parentReference": {
                "driveId": "b!abcdef0123456789",
                "path": "/drive/root:",
            },
        };
    }

    #! Returns an example list item
    /** @return an example list item
    */
    static hash<auto> getListItemExample() {
        return {
            "id": "1",
            "webUrl": "https://contoso.sharepoint.com/Lists/Orders/1_.000",
            "eTag": "\"a1b2c3d4-0000-0000-0000-000000000000,1\"",
            "createdDateTime": "2026-04-09T12:00:00Z",
            "lastModifiedDateTime": "2026-04-09T12:00:00Z",
            "fields": {
                "Title": "Order 1001",
            },
        };
    }
}
}
