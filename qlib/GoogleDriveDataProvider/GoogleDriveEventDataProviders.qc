# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveEventDataProviders.qc event providers for the GoogleDriveDataProvider module

/*  GoogleDriveEventDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! Common base class for the Google Drive change-feed watch providers
/** Drive exposes a change feed rather than a listing to scan: \c changes.list returns everything that has
    changed since an opaque page token, and hands back a replacement token to use next time.  That is far
    cheaper than polling a listing, and it is the only way to see a change to a file held in a folder the watch
    was never told about.

    @par Creations against modifications
    The change feed reports creations and modifications alike, so a change entry on its own does not say
    whether the file is new.  Each entry is therefore checked against the moment the cursor was first
    established: a file whose \c createdTime is at or after that moment is new, and one created earlier and
    merely edited since is not.  That instant is stored in the checkpoint next to the page token, so it
    survives a restart along with it -- keeping it only in memory would make every restart re-classify the
    whole backlog as new.

    @par Delivery
    The replacement page token is made durable only once every change it supersedes has been delivered.
    Storing it first would skip the whole batch if delivery then failed, so delivery is at-least-once.
*/
public class GoogleDriveWatchDataProviderBase inherits GoogleDriveDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Options shared by every Drive watch provider
        const CommonWatchOptions = ConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for changes, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to check for changes, in seconds (minimum `" + MinPollInterval + "`, "
                    "default `" + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
            "folder_id": FolderIdField + {
                "display_name": "Folder",
                "short_desc": "Report only items in this folder",
                "desc": "Report only items held directly in this folder.\n\n"
                    "Drive's change feed covers the whole drive and cannot be narrowed at the server, so this "
                    "filters what arrives rather than what is fetched.  Leave it unset to watch everything the "
                    "connection can see",
            },
            "drive_id": SharedDriveIdField + {
                "short_desc": "Watch this shared drive instead of My Drive",
                "desc": "Watch this shared drive (formerly a \"Team Drive\") instead of the user's own `My "
                    "Drive`.\n\n"
                    "A shared drive keeps its own change feed, so a watch covers one or the other rather than "
                    "both",
            },
        };
    }

    private {
        #! The change-feed page token
        *string page_token;

        #! The instant the cursor was established; an item created before it is not new
        *date since;

        #! Report only items held directly in this folder
        *string watch_folder_id;

        #! Watch this shared drive rather than "My Drive"
        *string watch_drive_id;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : GoogleDriveDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        setWatchOptions(options);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<string, hash<DataProviderOptionInfo>> con_opts = CommonWatchOptions)
            : GoogleDriveDataProviderBase(options, con_opts),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        setWatchOptions(options);
    }

    #! Applies the watch-specific options
    private setWatchOptions(*hash<auto> options) {
        watch_folder_id = options.folder_id;
        watch_drive_id = options.drive_id;
    }

    #! Cancels an in-flight request so that a stop does not wait out the poll
    private cancelPollOperation() {
        rest.close();
    }

    #! The change feed returns changes in ascending change order, and they are dispatched as received
    /** The page token is an opaque cursor with no per-change key to sort on, so this provider is a documented
        exemption from the shared ordered template: it dispatches in the order the feed returns, and makes the
        replacement token durable only after the whole batch has been delivered.
    */
    string getSourceOrdering() {
        return WSO_ASCENDING;
    }

    #! Returns the configuration that defines which change feed this provider follows
    /** A shared drive keeps a change feed of its own, so the drive selects the feed and a change to it must
        start a new cursor.  The folder filter is included for the same reason: a cursor established under one
        filter has already passed over everything the other filter would have wanted.  The poll interval is
        excluded, since it changes how often the same feed is read rather than what it covers.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "drive": watch_drive_id ?? "me",
            "folder_id": watch_folder_id,
            "event": getEventName(),
        };
    }

    #! Restores the page token and the instant the cursor was established
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.page_token.typeCode() != NT_STRING || !checkpoint.page_token.val()
                || checkpoint.since.typeCode() != NT_DATE || !checkpoint.since.absolute()) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid Google Drive watch checkpoint; expecting version 1, a "
                "non-empty page_token, and an absolute since date/time value";
        }
        page_token = checkpoint.page_token;
        since = checkpoint.since;
    }

    #! Durably advances the page token
    private commitCheckpoint(string next_page_token) {
        if (!next_page_token.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist an empty Google Drive change page token";
        }
        # the instant the cursor was established never moves; it is what separates a file created since the
        # watch started from one that merely changed
        date established = since ?? now_us();
        persistCheckpoint({
            "version": 1,
            "page_token": next_page_token,
            "since": established,
        });
        page_token = next_page_token;
        since = established;
    }

    #! Polls once, advancing the page token only after every change has been delivered
    private pollOnce() {
        try {
            if (!page_token.val()) {
                # seed the cursor without reporting anything: an existing drive would otherwise produce a flood
                # of events the first time the watch runs
                commitCheckpoint(getStartPageToken());
                return;
            }

            list<hash<auto>> changes = ();
            *string next_token;
            string token = page_token;
            while (True) {
                if (isStopping()) {
                    # the page token stays where it is, so this batch is read again
                    return;
                }
                hash<auto> body = doRestCommand("GET", getChangesPath(token)).body;
                if (body.changes) {
                    changes += body.changes;
                }
                if (body.nextPageToken.val()) {
                    token = body.nextPageToken;
                    continue;
                }
                next_token = body.newStartPageToken;
                break;
            }

            foreach hash<auto> change in (changes) {
                if (isStopping()) {
                    # the page token stays where it is, so the remaining changes are read again
                    return;
                }
                *hash<auto> item = getMatchingItem(change);
                if (!item) {
                    continue;
                }
                notifyObservers(getEventName(), item);
            }

            # the batch has been delivered, so the replacement token can be made durable
            if (next_token.val()) {
                commitCheckpoint(next_token);
            }
        } catch (hash<ExceptionInfo> ex) {
            # NOTE: "LoggerWrapper::error()", not a bare "error()"; AbstractDataProvider declares
            # "error(string err, string fmt, ...)", which throws rather than logging and wins overload
            # resolution, so a bare call would raise an exception from the handler that reports the error
            LoggerWrapper::error(sprintf("Error polling for %s events: %s: %s", getEventName(), ex.err,
                ex.desc));
        }
    }

    #! Returns the change-feed item this provider reports, or nothing if the change is not one of them
    /** @param change a \c changes.list entry

        @return the item in this module's representation, or @ref nothing if the change is not reported
    */
    private *hash<auto> getMatchingItem(hash<auto> change) {
        return GoogleDriveWatchDataProviderBase::matchChange(change, wantsFolders(), since, watch_folder_id);
    }

    #! Decides whether one change-feed entry is an item this watch reports, and returns it if so
    /** This is where a creation is told from a modification, which is the whole difficulty of building a
        "new file" trigger on a change feed: \c changes.list reports both, and a change entry carries nothing
        that says which it is.  A file is new when it was created at or after the moment the cursor was
        established -- anything older was already there and has merely been touched since.  Verified against
        the live feed, where an ordinary working day produces changes to files created years earlier.

        Written as a static method taking everything it needs so that the rule can be exercised directly: it
        is pure classification, and driving it through a polling thread to test it would be both slow and less
        thorough.

        @param change a \c changes.list entry
        @param want_folders whether folders rather than files are wanted
        @param since the instant the cursor was established; @ref nothing before it is
        @param folder_id report only items held directly in this folder, if set

        @return the item in this module's representation, or @ref nothing if the change is not reported
    */
    static *hash<auto> matchChange(hash<auto> change, bool want_folders, *date since, *string folder_id) {
        # a removal carries no file resource at all, so there is nothing to report
        if (change.removed || !change.file) {
            return;
        }
        hash<auto> f = GoogleDriveDataProviderBase::getFileInfo(change.file);
        # a trashed file is a deletion as far as a user is concerned, whatever Drive still holds
        if (f.trashed) {
            return;
        }
        if (f.is_folder != want_folders) {
            return;
        }
        # the change feed reports modifications as well as creations, so an item that existed before the
        # cursor was established is not new
        if (since && (!exists f.created || f.created < since)) {
            return;
        }
        # Drive's change feed covers the whole drive and cannot be narrowed at the server, so a folder filter
        # is applied here
        if (folder_id.val() && !inlist(folder_id, f.parents)) {
            return;
        }
        return f;
    }

    #! Returns the page token identifying the current end of the change feed
    private string getStartPageToken() {
        string path = "/drive/v3/changes/startPageToken";
        if (watch_drive_id.val()) {
            path += "?driveId=" + encode_url(watch_drive_id, True);
        }
        *string token = doRestCommand("GET", path).body.startPageToken;
        if (!token.val()) {
            throw "GOOGLE-DRIVE-ERROR", "the Drive change feed returned no start page token";
        }
        return token;
    }

    #! Returns the change-feed request path for the given page token
    private string getChangesPath(string token) {
        string path = sprintf("/drive/v3/changes?pageToken=%s&pageSize=%d&includeRemoved=true&fields=%s",
            encode_url(token, True), DefaultPageSize,
            encode_url("nextPageToken,newStartPageToken,changes(fileId,removed,time,changeType,file("
                + GoogleDriveDataProviderBase::getFileFields() + "))", True));
        # a shared drive keeps its own change feed, and reading it needs the drive named on every request
        if (watch_drive_id.val()) {
            path += "&driveId=" + encode_url(watch_drive_id, True) + "&includeItemsFromAllDrives=true"
                + "&corpora=drive";
        } else {
            path += "&includeItemsFromAllDrives=true";
        }
        return path;
    }

    #! Returns example event data
    /** Real data is used when the drive has any, since an example built from the account under test shows the
        caller the field values they will actually receive
    */
    private auto getExampleEventDataImpl(string event_id) {
        try {
            string q = wantsFolders()
                ? "mimeType = '" + FolderMimeType + "' and trashed = false"
                : "mimeType != '" + FolderMimeType + "' and trashed = false";
            hash<auto> resp = doRestCommand("GET", sprintf("/drive/v3/files?pageSize=1&q=%s&fields=%s",
                encode_url(q, True),
                encode_url("files(" + GoogleDriveDataProviderBase::getFileFields() + ")", True)));
            if (resp.body.files) {
                return GoogleDriveDataProviderBase::getFileInfo(resp.body.files[0]);
            }
        } catch (hash<ExceptionInfo> ex) {
            debug("getExampleEventDataImpl(): %s: %s", ex.err, ex.desc);
        }
        return getFakeExampleData();
    }

    #! Returns the event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getEventDesc(),
                "type": GoogleDriveFileDataType,
            },
        };
    }

    #! Returns example data used when the drive holds nothing to build an example from
    private hash<auto> getFakeExampleData() {
        bool folder = wantsFolders();
        return {
            "id": "1a2B3cD4eF5gH6iJ7kL8mN9oP0qR",
            "name": folder ? "Example Folder" : "example-file.txt",
            "mime_type": folder ? FolderMimeType : "text/plain",
            "size": folder ? NOTHING : 1024,
            "created": 2026-04-09T12:00:00Z,
            "modified": 2026-04-09T12:00:00Z,
            "parents": ("0aBcDeFgHiJkLmNoPqRsTuVwXyZ",),
            "web_view_link": "https://drive.google.com/file/d/1a2B3cD4eF5gH6iJ7kL8mN9oP0qR/view",
            "web_content_link": folder
                ? NOTHING
                : "https://drive.google.com/uc?id=1a2B3cD4eF5gH6iJ7kL8mN9oP0qR&export=download",
            "trashed": False,
            "description": NOTHING,
            "md5": folder ? NOTHING : "d41d8cd98f00b204e9800998ecf8427e",
            "drive_id": NOTHING,
            "shortcut_target_id": NOTHING,
            "shortcut_target_mime_type": NOTHING,
            "is_folder": folder,
            "is_shortcut": False,
            "is_google_doc": False,
        };
    }

    #! Returns @ref True if this provider reports folders rather than files
    abstract private bool wantsFolders();

    #! Returns the name of the event this provider raises
    abstract private string getEventName();

    #! Returns the description of the event this provider raises
    abstract private string getEventDesc();
}

#! Reports each file created in the drive
public class GoogleDriveWatchNewFileDataProvider inherits GoogleDriveWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FILE,
            "type": "GoogleDriveWatchNewFileDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : GoogleDriveWatchDataProviderBase(rest, options) {
    }
    constructor(*hash<auto> options) : GoogleDriveWatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private bool wantsFolders() { return False; }
    private string getEventName() { return EVENT_NEW_FILE; }

    private string getEventDesc() {
        return "Fired when a file is created in the drive being watched.\n\n"
            "A file that already existed and has merely been edited is not reported, and neither is a folder; "
            "use `new-folder` for those.";
    }
}

#! Reports each folder created in the drive
public class GoogleDriveWatchNewFolderDataProvider inherits GoogleDriveWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FOLDER,
            "type": "GoogleDriveWatchNewFolderDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : GoogleDriveWatchDataProviderBase(rest, options) {
    }
    constructor(*hash<auto> options) : GoogleDriveWatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private bool wantsFolders() { return True; }
    private string getEventName() { return EVENT_NEW_FOLDER; }

    private string getEventDesc() {
        return "Fired when a folder is created in the drive being watched.\n\n"
            "A folder that already existed and has merely been renamed or moved is not reported.";
    }
}

#! Events container data provider
public class GoogleDriveEventsDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "GoogleDriveEventsDataProvider",
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
            EVENT_NEW_FILE: Class::forName("GoogleDriveDataProvider::GoogleDriveWatchNewFileDataProvider"),
            EVENT_NEW_FOLDER: Class::forName("GoogleDriveDataProvider::GoogleDriveWatchNewFolderDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

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
        return cls.newObject(rest, NOTHING);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
