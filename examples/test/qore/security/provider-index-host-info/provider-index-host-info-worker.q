#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of provider-index-host-info.qtest (qore#7597): a host that publishes a data provider index, preloads
# ProviderIndexUtil and ProviderIndex into Programs and then restricts them from external information (or creates
# them restricted, and their code requires the modules); the code of each Program (provider-index-host-info-child.q,
# at top level) calls the methods of the modules that use the index; prints one "<kind>/<mode>/<case>=ok" line per
# check (or "...=expected <x>, got <y>")
#
# the environment is set by the test: QORE_PROVIDER_INDEX_DIR (a directory of the test),
# QORE_PROVIDER_INDEX_MAX_CANONICAL_BYTES and Q7597_CHILD (the path of the code of the Programs)

%modern
%no-child-restrictions

%requires ProviderIndexUtil
%requires ProviderIndex
%requires DataProvider

#! the modules that the host preloads into each Program
const Modules = ("ProviderIndexUtil", "ProviderIndex", "DataProvider");

#! the kinds of Programs: whether the code may read external information
const Kinds = {
    "unrestricted": {"restricted": False, "sandboxed": False},
    "restricted": {"restricted": True, "sandboxed": False},
    # restricted from the start: the code requires the modules that the host's process has loaded, which are not
    # parsed again for it
    "restricted-requires": {"restricted": True, "sandboxed": False, "requires": True},
    "sandboxed-restricted": {"restricted": True, "sandboxed": True},
    # a sandbox that allows everything: it governs the code without restricting it from external information
    "sandboxed": {"restricted": False, "sandboxed": True},
};

#! the execution modes of the Programs
const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT, "tiered": EM_TIERED};

#! the app of the index
const App = "Q7597App";

list<string> results = ();

sub check(string key, auto expected, auto actual) {
    push results, key + "=" + (expected === actual ? "ok" : sprintf("expected %y, got %y", expected, actual));
}

#! records the messages of the host's logger
class Capture inherits Logger::LoggerAppenderWithLayout {
    public {
        list<string> got = ();
    }

    constructor() : LoggerAppenderWithLayout("q7597", new Logger::LoggerLayoutPattern("%m")) {
        open();
    }

    processEventImpl(int type, auto params) {
        if (type == Logger::LoggerAppender::EVENT_LOG) {
            got += params;
        }
    }
}

# the module's diagnostics go to the host's logger, which records them, instead of stderr; the host gives the data
# providers the same logger
Capture host_capture();
{
    Logger::Logger host_logger("q7597", Logger::LoggerLevel::getLevelInfo());
    host_logger.addAppender(host_capture);
    ProviderIndexUtil::setGlobalLogger(host_logger);
    DataProvider::DataProvider::setGlobalLogger(host_logger);
}

string child_source = ReadOnlyFile::readTextFile(ENV.Q7597_CHILD);
string index_dir = ProviderIndexUtil::tryGetIndexDir(True);
string uid = sprintf("uid %d", getuid());

# the published index
hash<auto> maps = {
    "factorymap": {},
    "appmap": {App: {}},
    "schememap": {},
    "appinfomap": {App: {"name": App, "desc": "q7597 app"}},
};
string write_path = ProviderIndexUtil::writeDataProviderIndexByApp(maps, (App,), *hash<auto> sub (string app) {
    return {};
}, 0s, {}).path;
# the process reads it and its metadata
ProviderIndexUtil::reloadIndex();
if (ProviderIndexUtil::tryGetAppInfo(App).desc != "q7597 app") {
    check("setup/lookup", "q7597 app", ProviderIndexUtil::tryGetAppInfo(App).desc);
}
string routing_path = ProviderIndexUtil::getIndexPath("routing");
string data_path = ProviderIndexUtil::getIndexPath("data");
#! the routing index of the published index, restored by restore_published(); the same index is written identically
binary routing_data = ReadOnlyFile::readBinaryFile(routing_path);
#! without MessagePack, the index is published with its canonical index file only, which is read instead of a
#! routing index that cannot be read
bool msgpack = ProviderIndexUtil::hasMessagePack();

#! makes the published index unreadable, so that a reload fails and names a file of the index in its error: a
#! corrupt routing index, and without MessagePack also a canonical index file that cannot be opened
sub corrupt_index() {
    File f();
    f.open2(routing_path, O_CREAT | O_WRONLY | O_TRUNC);
    f.write("q7597: [");
    f.close();
    if (!msgpack) {
        chmod(data_path, 0);
    }
}

#! restores the permissions of the canonical index file, if a check left it unreadable
sub restore_index() {
    if (!msgpack && is_file(data_path)) {
        chmod(data_path, 0644);
    }
}

#! publishes the index again after corrupt_index(), as the code of the Programs may not write it (qore#7611)
code restore_published = sub () {
    File f();
    f.open2(routing_path, O_CREAT | O_WRONLY | O_TRUNC);
    f.write(routing_data);
    f.close();
    restore_index();
};

#! what an error text names: the index directory and the IDs of the process, or neither
auto sub classify(auto v) {
    if (v.typeCode() != NT_STRING) {
        return v;
    }
    bool has_dir = v.find(index_dir) >= 0;
    bool has_ids = v.find(uid) >= 0;
    if (has_dir && has_ids) {
        return "names the directory and the IDs";
    }
    if (has_dir) {
        return "names the directory";
    }
    # an error code with a description that names neither
    if (v =~ /^[-A-Z0-9_]+: / && v !~ /uid [0-9]/) {
        return "generic";
    }
    return v;
}

#! True if the user can write to a directory without write permission (root), so it cannot be denied; the checks
#! of a directory that cannot be written to (and without MessagePack, of a file that cannot be read) are then
#! reported as skipped
bool denied_skipped = !getuid();

#! what code that may read external information gets
hash<auto> may_read = {
    "dir": index_dir,
    "dir_throw": index_dir,
    "dir_write": index_dir,
    "writable": index_dir,
    "path": routing_path,
    "path_dir": "/q7597" + DirSep + "qore-routing-index.yaml",
    "max": ENV.QORE_PROVIDER_INDEX_MAX_CANONICAL_BYTES.toInt(),
    "dump": msgpack ? True : "PROVIDER-INDEX-ERROR",
    "dump_dir": msgpack ? True : "PROVIDER-INDEX-ERROR",
    "lookup": "q7597 app",
    "apps": (App,),
    "reload_error": "names the directory",
    "read_error": "names the directory",
    "lookup_after": "q7597 app",
    "write_path": write_path,
    "write_caller": "Q7597-CALLER: q7597 caller description",
    "write_denied": "names the directory and the IDs",
    "pi_denied": "names the directory and the IDs",
    "pi_workers": "names the directory and the IDs",
    "writable_denied": "names the directory and the IDs",
    "logger_get": True,
    "logger_set": "set",
    "dp_logger_get": True,
    "dp_logger_set": "set",
};

#! what code restricted from external information gets: no index directory, no paths under it and no IDs
hash<auto> may_not_read = may_read + {
    "dir": "INVALID-FUNCTION-ACCESS",
    "dir_throw": "INVALID-FUNCTION-ACCESS",
    "dir_write": "INVALID-FUNCTION-ACCESS",
    "writable": "INVALID-FUNCTION-ACCESS",
    "path": "INVALID-FUNCTION-ACCESS",
    "max": "INVALID-FUNCTION-ACCESS",
    "dump": "INVALID-FUNCTION-ACCESS",
    "reload_error": "generic",
    "read_error": "generic",
    # only code that may read external information may change the published index (qore#7611)
    "write_path": "INVALID-FUNCTION-ACCESS",
    "write_caller": "INVALID-FUNCTION-ACCESS",
    "write_denied": "INVALID-FUNCTION-ACCESS",
    "pi_denied": "INVALID-FUNCTION-ACCESS",
    "pi_workers": "INVALID-FUNCTION-ACCESS",
    "writable_denied": "INVALID-FUNCTION-ACCESS",
    "logger_get": False,
    "logger_set": "INVALID-FUNCTION-ACCESS",
    "dp_logger_get": False,
    "dp_logger_set": "INVALID-FUNCTION-ACCESS",
};

#! what sandboxed code gets in addition: the process-wide logger is not shared with it, whether it may read external
#! information or not
const Sandboxed = {
    # sandboxed code may not change the published index (qore#6747, qore#7611)
    "write_path": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "write_caller": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "write_denied": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "pi_denied": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "pi_workers": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "logger_get": False,
    "logger_set": "PROVIDER-INDEX-REGISTRATION-DENIED",
    "dp_logger_get": False,
    "dp_logger_set": "DATA-PROVIDER-CALLBACK-ERROR",
};

const Classified = ("reload_error", "read_error", "write_denied", "pi_denied", "pi_workers", "writable_denied");
list<string> denied = ("write_denied", "pi_denied", "pi_workers", "writable_denied");
if (!msgpack) {
    denied += ("reload_error", "read_error");
}

# the values must differ, or the checks would not show what the code gets
foreach string k in (keys may_read) {
    if (may_read{k} === may_not_read{k} && k !~ /^(path_dir|dump_dir|lookup|apps|lookup_after|write_caller)$/) {
        check("setup/" + k, "distinct values", "the same value");
    }
}

code set_writable = sub (bool writable) {
    chmod(index_dir, writable ? 0755 : 0555);
};

string requires = (map sprintf("%%requires %s\n", $1), Modules).join("");

foreach hash<auto> mode in (Modes.pairIterator()) {
    foreach hash<auto> kind in (Kinds.pairIterator()) {
        string key = kind.key + "/" + mode.key;
        # the host creates the Program, preloads the modules into it and then restricts it, or creates it restricted
        # and the code requires the modules
        Program p(PO_MODERN | (kind.value.requires ? PO_NO_EXTERNAL_INFO : 0));
        p.setExecMode(mode.value);
        if (kind.value.sandboxed) {
            SandboxManager sm();
            sm.setNetworkDefaultPolicy(True);
            sm.setFilesystemDefaultPolicy(True);
            p.setSandboxManager(sm);
        }
        if (kind.value.requires) {
            p.parse(requires + child_source, "provider-index-host-info-child");
        } else {
            map p.loadModule($1), Modules;
            if (kind.value.restricted) {
                p.setParseOptions(PO_NO_EXTERNAL_INFO);
            }
            p.parse(child_source, "provider-index-host-info-child");
        }
        p.setGlobalVarValue("q_index_dir", index_dir);
        p.setGlobalVarValue("q_app", App);
        p.setGlobalVarValue("q_maps", maps);
        p.setGlobalVarValue("q_set_writable", set_writable);
        p.setGlobalVarValue("q_restore_index", restore_published);
        p.setGlobalVarValue("q_r", {});
        corrupt_index();
        on_exit restore_index();
        p.run();
        hash<auto> r = p.getGlobalVariable("q_r");
        map r{$1} = classify(r{$1}), Classified;
        hash<auto> expected = (kind.value.restricted ? may_not_read : may_read) + (kind.value.sandboxed ? Sandboxed : {});
        foreach string k in (keys expected) {
            if (denied_skipped && inlist(k, denied)) {
                push results, key + "/" + k + "=skipped";
                continue;
            }
            check(key + "/" + k, expected{k}, r{k});
        }
        delete p;
    }
}

# the host's own logger got the module's diagnostics, with the paths of the index files, also while the module
# served restricted code: each Program's reload of the corrupt index was logged
check("host/logger", True, (select host_capture.got, $1.find(index_dir) >= 0).size() >= Kinds.size() * Modes.size());

map printf("%s\n", $1), results;
