# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxEventDataProviders.qc event providers for the DropboxDataProvider module

/*  DropboxEventDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Common base class for the Dropbox delta-feed watch providers
/** Dropbox exposes a delta feed: \c /2/files/list_folder returns a folder's contents and a cursor, and
    \c /2/files/list_folder/continue returns everything that has changed since that cursor along with a
    replacement.  That is far cheaper than re-listing, and it is how all three watches here are driven.

    @par Creations against modifications
    Dropbox reports a created file and a modified file identically -- both are simply an entry that has
    changed -- and a file's metadata carries no creation time to separate them with.  Telling them apart
    therefore needs an index of what the watch has already seen: an entry whose identity is not in the index
    is new, and one that is in it has been modified.

    The index is seeded by walking the watched folder when the cursor is first established, so the files that
    were already there are not all reported as new on the first poll.  It is stored in the checkpoint next to
    the cursor, so it survives a restart along with it.

    @par The index is bounded
    An index proportional to the watched folder is proportional to what the user asked to watch, which is the
    right shape, but it still has to have a limit.  \c max_tracked_entries sets it; reaching it is logged as a
    warning naming \c path and \c recursive as the way to narrow the watch.  Beyond the bound the oldest
    identities are dropped, so a file that has not changed for a long time may be reported as new the next
    time it does -- which is why the bound is reported rather than passed over.

    @par Delivery
    The replacement cursor is made durable only once every entry it supersedes has been delivered.  Storing it
    first would skip the whole batch if delivery then failed, so delivery is at-least-once.
*/
public class DropboxWatchDataProviderBase inherits DropboxDataProviderBase,
        DataProvider::AbstractWatchDataProviderBase {
    public {
        #! Options shared by every Dropbox watch provider
        const CommonWatchOptions = ConstructorOptions + {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to check for changes, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to check for changes, in seconds (minimum `" + MinPollInterval + "`, "
                    "default `" + DefaultPollInterval + "`)",
                "default_value": DefaultPollInterval,
            },
            "path": <DataProviderOptionInfo>{
                "display_name": "Folder",
                "short_desc": "The folder to watch",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The folder to watch; leave it unset to watch the whole account from its root",
                "example_value": "/incoming",
                "ref_data": "folders",
            },
            "recursive": <DataProviderOptionInfo>{
                "display_name": "Recursive",
                "short_desc": "Watch subfolders as well",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Watch the contents of subfolders as well as of the folder itself.\n\n"
                    "This widens both what is reported and the index the watch keeps to tell a new file from "
                    "a modified one, so leave it clear when only one folder matters",
                "default_value": True,
            },
            "max_tracked_entries": <DataProviderOptionInfo>{
                "display_name": "Maximum Tracked Entries",
                "short_desc": "How many entry identities the watch remembers",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How many entry identities the watch remembers in order to tell a new file from a "
                    "modified one (default `" + DefaultMaxTrackedEntries + "`).\n\n"
                    "Dropbox reports a creation and a modification identically, so the difference can only "
                    "come from remembering what has been seen.  A watch that reaches this bound logs a "
                    "warning; narrow it with `path` and `recursive` rather than raising this",
                "default_value": DefaultMaxTrackedEntries,
            },
        };
    }

    private {
        #! The delta-feed cursor
        *string cursor;

        #! The identities the watch has already seen, in the order they were first seen
        /** A hash rather than a list: membership is tested once per entry per poll, and %Qore hashes preserve
            insertion order, so the oldest identity to drop when the bound is reached is simply the first key
        */
        hash<string, bool> seen = {};

        #! The folder to watch
        string watch_path = RootPath;

        #! Watch subfolders as well
        bool watch_recursive = True;

        #! How many identities the watch remembers
        int max_tracked_entries = DefaultMaxTrackedEntries;
    }

    #! Creates the object from a DropboxRestClientIo client
    constructor(DropboxRestClient::DropboxRestClientIo rest, *hash<auto> options)
            : DropboxDataProviderBase(rest),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        setWatchOptions(options);
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options, hash<string, hash<DataProviderOptionInfo>> con_opts = CommonWatchOptions)
            : DropboxDataProviderBase(options, con_opts),
              AbstractWatchDataProviderBase(
                  max(options.poll_interval ?? DefaultPollInterval, MinPollInterval)) {
        setWatchOptions(options);
    }

    #! Applies the watch-specific options
    private setWatchOptions(*hash<auto> options) {
        if (exists options.path) {
            watch_path = DropboxDataProviderBase::normalizePath(options.path);
        }
        if (exists options.recursive) {
            watch_recursive = options.recursive;
        }
        if (options.max_tracked_entries) {
            max_tracked_entries = options.max_tracked_entries;
        }
    }

    #! Cancels an in-flight request so that a stop does not wait out the poll
    private cancelPollOperation() {
        rest.close();
    }

    #! The delta feed guarantees only that an entry's latest state comes last
    /** Dropbox does not document a total order over a delta page, so no ordering is claimed here; the whole
        page is delivered and the replacement cursor is made durable only once it has been.
    */
    string getSourceOrdering() {
        return WSO_UNKNOWN;
    }

    #! Returns the configuration that defines which feed this provider follows
    /** The folder and its recursion select what the feed covers, so each defines a different feed and a
        change to either must start a new cursor rather than resume the previous one against a different set
        of entries.  The poll interval and the index bound are excluded: neither changes what the feed covers.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return {
            "path": watch_path,
            "recursive": watch_recursive,
            "event": getEventName(),
        };
    }

    #! Restores the cursor and the index of identities already seen
    private restoreCheckpoint(hash<auto> checkpoint) {
        if (checkpoint.version.typeCode() != NT_INT || checkpoint.version != 1
                || checkpoint.cursor.typeCode() != NT_STRING || !checkpoint.cursor.val()
                || checkpoint.seen.typeCode() != NT_LIST) {
            throw "WATCH-CHECKPOINT-ERROR", "invalid Dropbox watch checkpoint; expecting version 1, a "
                "non-empty cursor, and a seen list";
        }
        hash<string, bool> restored = {};
        foreach auto id in (checkpoint.seen) {
            if (id.typeCode() != NT_STRING || !id.val()) {
                throw "WATCH-CHECKPOINT-ERROR", "invalid Dropbox watch checkpoint; entry identities must be "
                    "non-empty strings";
            }
            restored{id} = True;
        }
        cursor = checkpoint.cursor;
        seen = restored;
    }

    #! Durably advances the cursor and the index
    private commitCheckpoint(string next_cursor) {
        if (!next_cursor.val()) {
            throw "WATCH-CHECKPOINT-ERROR", "cannot persist an empty Dropbox cursor";
        }
        persistCheckpoint({
            "version": 1,
            "cursor": next_cursor,
            "seen": keys seen,
        });
        cursor = next_cursor;
    }

    #! Records an identity, dropping the oldest once the bound is reached
    /** @param id the entry identity

        @note Reaching the bound is reported rather than passed over: past it, a file that has not changed for
        a long time can be reported as new the next time it does, and the only other symptom would be an event
        that looks wrong for no visible reason
    */
    private addSeen(string id) {
        seen{id} = True;
        if (seen.size() <= max_tracked_entries) {
            return;
        }
        # %Qore hashes preserve insertion order, so the first key is the oldest identity
        string oldest = (keys seen)[0];
        seen -= oldest;
        warn("watch on %y is tracking %d entry identities, reaching the 'max_tracked_entries' bound; the "
            "oldest identity was dropped, so an entry that has not changed for a long time may be reported as "
            "new the next time it does. Narrow the watch with the 'path' and 'recursive' options",
            watch_path.val() ? watch_path : "the account root", max_tracked_entries);
    }

    #! Polls once, advancing the cursor only after every entry has been delivered
    private pollOnce() {
        try {
            if (!cursor.val()) {
                seedCursor();
                return;
            }

            list<hash<auto>> entries = ();
            string next_cursor = cursor;
            while (True) {
                if (isStopping()) {
                    # the cursor stays where it is, so this batch is read again
                    return;
                }
                hash<auto> body = doRpc("/2/files/list_folder/continue", {"cursor": next_cursor});
                if (body.entries) {
                    entries += body.entries;
                }
                next_cursor = body.cursor ?? next_cursor;
                if (!body.has_more) {
                    break;
                }
            }

            foreach hash<auto> e in (entries) {
                if (isStopping()) {
                    # the cursor stays where it is, so the remaining entries are read again
                    return;
                }
                *string id = e.id;
                string tag = e.".tag" ?? "";
                # a deletion leaves nothing to report, and the identity must stop being tracked or a file
                # recreated at the same path would be reported as a modification
                if (tag == "deleted") {
                    if (id.val()) {
                        seen -= id;
                    }
                    continue;
                }
                if (!id.val()) {
                    continue;
                }
                # NOTE: the "?? False" is required, not defensive: a key the index does not hold reads as
                # NOTHING, and assigning that to a "bool" lvalue raises RUNTIME-TYPE-ERROR -- which the catch
                # below would then report as a poll failure, so every entry would be dropped and the watch
                # would deliver nothing at all
                bool already = seen{id} ?? False;
                addSeen(id);
                if (!matches(tag, already)) {
                    continue;
                }
                notifyObservers(getEventName(), DropboxDataProviderBase::getFileInfo(e));
            }

            # the batch has been delivered, so the replacement cursor can be made durable
            commitCheckpoint(next_cursor);
        } catch (hash<ExceptionInfo> ex) {
            # NOTE: "LoggerWrapper::error()", not a bare "error()"; AbstractDataProvider declares
            # "error(string err, string fmt, ...)", which throws rather than logging and wins overload
            # resolution, so a bare call would raise an exception from the handler that reports the error
            LoggerWrapper::error(sprintf("Error polling for %s events: %s: %s", getEventName(), ex.err,
                ex.desc));
        }
    }

    #! Establishes the cursor and the index of what is already there, reporting nothing
    /** The folder is walked rather than the cursor simply taken, because the index of existing entries is
        what separates a file created later from one that was already there.  Taking only the latest cursor
        would leave the index empty, and the first change to any pre-existing file would then be reported as a
        creation.
    */
    private seedCursor() {
        hash<auto> body = doRpc("/2/files/list_folder", {
            "path": watch_path,
            "recursive": watch_recursive,
            "limit": MaxPageSize,
        });
        while (True) {
            foreach hash<auto> e in (body.entries) {
                if (e.id.val() && e.".tag" != "deleted") {
                    addSeen(e.id);
                }
            }
            if (!body.has_more || !body.cursor.val()) {
                break;
            }
            if (isStopping()) {
                # nothing is persisted, so the next start seeds again from the beginning
                return;
            }
            body = doRpc("/2/files/list_folder/continue", {"cursor": body.cursor});
        }
        if (!body.cursor.val()) {
            throw "DROPBOX-ERROR", sprintf("listing %y returned no cursor to watch from",
                watch_path.val() ? watch_path : "the account root");
        }
        debug("Dropbox watch on %y seeded with %d existing entries", watch_path, seen.size());
        commitCheckpoint(body.cursor);
    }

    #! Returns example event data
    /** Real data is used when the watched folder has any, since an example built from the account under test
        shows the caller the field values they will actually receive
    */
    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> body = doRpc("/2/files/list_folder", {
                "path": watch_path,
                "recursive": watch_recursive,
                "limit": MaxPageSize,
            });
            foreach hash<auto> e in (body.entries) {
                # a folder watch must show a folder and a file watch a file, or the example advertises the
                # wrong fields entirely
                if (wantsTag(e.".tag" ?? "")) {
                    return DropboxDataProviderBase::getFileInfo(e);
                }
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
                "type": DropboxFileDataType,
            },
        };
    }

    #! Returns @ref True if this watch reports entries of the given kind, whatever their history
    /** Used where only the kind matters -- choosing an example entry, and shaping fabricated example data --
        rather than whether a particular entry is a creation or a modification

        @param tag the Dropbox entry kind: \c "file" or \c "folder"

        @return @ref True if this watch reports entries of that kind
    */
    private bool wantsTag(string tag) {
        return matches(tag, False) || matches(tag, True);
    }

    #! Returns example data used when the watched folder holds nothing to build an example from
    private hash<auto> getFakeExampleData() {
        bool folder = wantsTag("folder");
        return {
            "id": folder ? "id:aBcDeFgHiJkLmNoPqRsTuV" : "id:1aBcDeFgHiJkLmNoPqRsTuV",
            "name": folder ? "Example Folder" : "example-file.txt",
            "path": folder ? "/incoming/Example Folder" : "/incoming/example-file.txt",
            "path_lower": folder ? "/incoming/example folder" : "/incoming/example-file.txt",
            "is_folder": folder,
            "size": folder ? NOTHING : 1024,
            "rev": folder ? NOTHING : "0123456789abcdef0123456789abcdef",
            "modified": folder ? NOTHING : 2026-04-09T12:00:00Z,
            "client_modified": folder ? NOTHING : 2026-04-09T12:00:00Z,
            "content_hash": folder
                ? NOTHING
                : "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        };
    }

    #! Returns @ref True if an entry of the given kind and history is one this watch reports
    /** This is the whole difference between the three Dropbox watches, and the only place the
        creation-against-modification rule is applied.

        @param tag the Dropbox entry kind: \c "file" or \c "folder"
        @param already_seen whether the entry's identity was already in the index

        @return @ref True if the entry is reported by this watch
    */
    abstract private bool matches(string tag, bool already_seen);

    #! Returns the name of the event this provider raises
    abstract private string getEventName();

    #! Returns the description of the event this provider raises
    abstract private string getEventDesc();
}

#! Reports each file created in the watched folder
public class DropboxWatchNewFileDataProvider inherits DropboxWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FILE,
            "type": "DropboxWatchNewFileDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest, *hash<auto> options)
            : DropboxWatchDataProviderBase(rest, options) {
    }
    constructor(*hash<auto> options) : DropboxWatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private bool matches(string tag, bool already_seen) {
        return tag == "file" && !already_seen;
    }

    private string getEventName() { return EVENT_NEW_FILE; }

    private string getEventDesc() {
        return "Fired when a file is created in the folder being watched.\n\n"
            "A file that was already there and has since been edited is reported by `file-modified` instead, "
            "and a new folder by `new-folder`.";
    }
}

#! Reports each folder created in the watched folder
public class DropboxWatchNewFolderDataProvider inherits DropboxWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_FOLDER,
            "type": "DropboxWatchNewFolderDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest, *hash<auto> options)
            : DropboxWatchDataProviderBase(rest, options) {
    }
    constructor(*hash<auto> options) : DropboxWatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private bool matches(string tag, bool already_seen) {
        return tag == "folder" && !already_seen;
    }

    private string getEventName() { return EVENT_NEW_FOLDER; }

    private string getEventDesc() {
        return "Fired when a folder is created in the folder being watched.\n\n"
            "A folder that was already there and has since been renamed or moved is not reported.";
    }
}

#! Reports each change to a file that was already in the watched folder
public class DropboxWatchFileModifiedDataProvider inherits DropboxWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FILE_MODIFIED,
            "type": "DropboxWatchFileModifiedDataProvider",
            "supports_observable": True,
            "constructor_options": CommonWatchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const WatchOptions = CommonWatchOptions;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest, *hash<auto> options)
            : DropboxWatchDataProviderBase(rest, options) {
    }
    constructor(*hash<auto> options) : DropboxWatchDataProviderBase(options, WatchOptions) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private bool matches(string tag, bool already_seen) {
        return tag == "file" && already_seen;
    }

    private string getEventName() { return EVENT_FILE_MODIFIED; }

    private string getEventDesc() {
        return "Fired when a file that was already in the folder being watched is changed.\n\n"
            "A file appearing for the first time is reported by `new-file` instead.  A file moved into the "
            "watched folder keeps its Dropbox identity, so it is reported here rather than as a new file.";
    }
}

#! Events container data provider
public class DropboxEventsDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "DropboxEventsDataProvider",
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
            EVENT_NEW_FILE: Class::forName("DropboxDataProvider::DropboxWatchNewFileDataProvider"),
            EVENT_NEW_FOLDER: Class::forName("DropboxDataProvider::DropboxWatchNewFolderDataProvider"),
            EVENT_FILE_MODIFIED: Class::forName("DropboxDataProvider::DropboxWatchFileModifiedDataProvider"),
        };
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

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
