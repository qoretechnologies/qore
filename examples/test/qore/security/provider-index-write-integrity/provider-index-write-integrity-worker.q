#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of provider-index-write-integrity.qtest (qore#7611): a host that publishes a data provider index,
# preloads ProviderIndexUtil and ProviderIndex into Programs and then restricts them from external information or
# sandboxes them (or creates them restricted, and their code requires the modules); the code of each Program
# (provider-index-write-integrity-child.q, at top level) calls the methods that write, replace or hide the published
# index; the host then checks that the published index, its directory, the build mode and the cached app are as
# before, unless the code may change them; prints one "<kind>/<mode>/<case>=ok" line per check (or
# "...=expected <x>, got <y>")
#
# the environment is set by the test: QORE_PROVIDER_INDEX_DIR (a directory of the test), Q7611_CHILD (the path of the
# code of the Programs) and Q7611_TMP (a directory of the test for the directories of the Programs)

%modern
%no-child-restrictions

%requires ProviderIndexUtil
%requires ProviderIndex
%requires DataProvider
%requires FsUtil

#! the time after which the worker ends itself, in case a check never returns
const Deadline = 240s;

# the worker ends itself if it does not finish in time, so that it never outlives the test
Mutex deadline_lock();
Condition deadline_cond();
bool done;
background sub () {
    deadline_lock.lock();
    on_exit deadline_lock.unlock();
    date end = now_us() + Deadline;
    while (!done) {
        if (now_us() >= end) {
            stderr.printf("provider-index-write-integrity-worker: no result after %y; exiting\n", Deadline);
            exit(2);
        }
        deadline_cond.wait(deadline_lock, end - now_us());
    }
}();
on_exit {
    deadline_lock.lock();
    done = True;
    deadline_cond.signal();
    deadline_lock.unlock();
}

#! the modules that the host preloads into each Program
const Modules = ("ProviderIndexUtil", "ProviderIndex", "DataProvider");

#! the kinds of Programs
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

#! the app of the published index
const App = "Q7611App";
const AppInfo = {"name": App, "desc": "q7611 app"};

list<string> results = ();

sub check(string key, auto expected, auto actual) {
    push results, key + "=" + (expected === actual ? "ok" : sprintf("expected %y, got %y", expected, actual));
}

string child_source = ReadOnlyFile::readTextFile(ENV.Q7611_CHILD);
string index_dir = ProviderIndexUtil::tryGetIndexDir(True);
bool msgpack = ProviderIndexUtil::hasMessagePack();

# the published index
hash<auto> maps = {
    "factorymap": {},
    "appmap": {App: {}},
    "schememap": {},
    "appinfomap": {App: AppInfo},
};

#! publishes the host's index and makes the process use it
sub publish() {
    ProviderIndexUtil::setEnvironmentVariable(index_dir);
    ProviderIndexUtil::setBuildMode(False);
    ProviderIndexUtil::writeDataProviderIndexByApp(maps, (App,), *hash<auto> sub (string app) {
        return {};
    }, 0s, {});
    ProviderIndexUtil::reloadIndex();
    # the app is cached again, if the code of a Program removed it
    ProviderIndexUtil::addAppToCache(AppInfo);
}

#! the files of a directory and their digests
hash<string, string> sub get_files(string dir) {
    hash<string, string> rv = {};
    foreach string path in (sort(glob(dir + DirSep + "*") ?? ())) {
        rv{basename(path)} = digest("sha256", ReadOnlyFile::readBinaryFile(path)).toHex();
    }
    return rv;
}

publish();
if (ProviderIndexUtil::tryGetAppInfo(App).desc != AppInfo.desc) {
    check("setup/lookup", AppInfo.desc, ProviderIndexUtil::tryGetAppInfo(App).desc);
}
#! the files of the published index; the same index is always written identically
hash<string, string> published = get_files(index_dir);

#! what the code of each kind of Program gets
const Refused = {
    "write_by_app": True,
    "write": True,
    "create": True,
    "build_mode": True,
    "worker": True,
    "run_worker": True,
    "add_app": True,
    "add_action": True,
    "remove_app": True,
    "create_dir": True,
    "set_env": True,
};

TmpDir tmp("qore-provider-index-write-integrity-");
string requires = (map sprintf("%%requires %s\n", $1), Modules).join("");

foreach hash<auto> mode in (Modes.pairIterator()) {
    foreach hash<auto> kind in (Kinds.pairIterator()) {
        string key = kind.key + "/" + mode.key;
        bool may_change = !kind.value.restricted && !kind.value.sandboxed;
        string own_dir = tmp.path + DirSep + kind.key + "-" + mode.key;
        mkdir(own_dir);

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
            p.parse(requires + child_source, "provider-index-write-integrity-child");
        } else {
            map p.loadModule($1), Modules;
            if (kind.value.restricted) {
                p.setParseOptions(PO_NO_EXTERNAL_INFO);
            }
            p.parse(child_source, "provider-index-write-integrity-child");
        }
        p.setGlobalVarValue("q_index_dir", index_dir);
        p.setGlobalVarValue("q_own_dir", own_dir);
        p.setGlobalVarValue("q_app", App);
        p.setGlobalVarValue("q_r", {});

        p.run();
        hash<auto> r = p.getGlobalVariable("q_r");
        delete p;

        # the published index can be written as YAML to a file of the caller's own by every kind of code that may
        # write files there
        check(key + "/dump_own", msgpack ? True : "PROVIDER-INDEX-ERROR", r.dump_own);
        foreach string k in (keys Refused) {
            *string expected;
            if (may_change) {
                expected = "not refused";
            } else if (kind.value.sandboxed) {
                expected = "PROVIDER-INDEX-REGISTRATION-DENIED";
            } else {
                expected = "INVALID-FUNCTION-ACCESS";
            }
            check(key + "/" + k, expected, r{k});
        }

        # the published index, its directory, the build mode and the cached app are unchanged, unless the code may
        # change them
        check(key + "/published", may_change ? "changed" : "unchanged",
            get_files(index_dir) == published ? "unchanged" : "changed");
        check(key + "/index_dir", may_change ? own_dir + DirSep + "env" : index_dir,
            ProviderIndexUtil::tryGetIndexDir());
        check(key + "/in_build_mode", False, ProviderIndexUtil::inBuildMode());
        check(key + "/lookup", may_change ? NOTHING : AppInfo.desc, ProviderIndexUtil::tryGetAppInfo(App).desc);
        # no index was written to the directory of the caller's own when it is refused
        # (the directory set by the code that may change it is created when the host looks up the app)
        list<string> own_files = msgpack ? ("dump.yaml",) : ();
        if (may_change) {
            own_files = sort(own_files + ("create", "env"));
        }
        check(key + "/own_dir", own_files, sort(map basename($1), glob(own_dir + DirSep + "*")) ?? ());

        # the host publishes its index again, so that the next Program finds it as before
        publish();
        if (may_change) {
            check(key + "/restored", published, get_files(index_dir));
        }
    }
}

#! creates a Program of the given kind in the given mode that runs the given code
Program sub tenant(bool restricted, bool sandboxed, Qore::ExecMode exec_mode, string code, string label) {
    Program p(PO_MODERN);
    p.setExecMode(exec_mode);
    if (sandboxed) {
        SandboxManager sm();
        sm.setNetworkDefaultPolicy(True);
        sm.setFilesystemDefaultPolicy(True);
        p.setSandboxManager(sm);
    }
    map p.loadModule($1), Modules;
    if (restricted) {
        p.setParseOptions(PO_NO_EXTERNAL_INFO);
    }
    p.parse(code, label);
    return p;
}

#! discards the in-memory index state, so that the next lookup reads the published index first
sub reset_index() {
    ProviderIndexUtil::setBuildMode(True);
    ProviderIndexUtil::setBuildMode(False);
}

# the test hook of the files that sandboxed code reads (debug builds only): code restricted from external
# information may not set it, so it can neither get the paths of the index files nor hide the published index from
# a sandboxed tenant that reads it first
bool have_hook = (select Class::forName("ProviderIndexUtil::ProviderIndexUtil").getStaticVariants(),
    $1.getMethod().getName() == "setIndexFileOpenTestHook").size() > 0;
string hook_code = "our list<string> seen = (); our auto res;
try {
    ProviderIndexUtil::setIndexFileOpenTestHook(sub (string f) { seen += f; throw \"Q7611\", \"hidden\"; });
    res = \"set\";
} catch (hash<ExceptionInfo> ex) {
    res = ex.err;
}";
string reader_code = "our auto res;
try { res = ProviderIndexUtil::tryGetAppInfo(\"" + App + "\").desc; } catch (hash<ExceptionInfo> ex) { res = ex.err; }";
# the claim of the modules of the indexed apps: code restricted from external information may not claim them, so
# the host gets them
string claim_code = "our auto res;
try { res = ProviderIndexUtil::getAllAppModulesToLoad(); } catch (hash<ExceptionInfo> ex) { res = ex.err; }";

foreach hash<auto> mode in (Modes.pairIterator()) {
    string key = "tenants/" + mode.key;
    if (!have_hook) {
        push results, key + "/hook=skipped";
        push results, key + "/hook_paths=skipped";
        push results, key + "/sandboxed_lookup=skipped";
    } else {
        reset_index();
        Program r = tenant(True, False, mode.value, hook_code, "restricted-hook");
        r.run();
        Program s = tenant(False, True, mode.value, reader_code, "sandboxed-reader");
        s.run();
        check(key + "/hook", "INVALID-FUNCTION-ACCESS", r.getGlobalVariable("res"));
        check(key + "/hook_paths", (), r.getGlobalVariable("seen"));
        check(key + "/sandboxed_lookup", AppInfo.desc, s.getGlobalVariable("res"));
        # in case the hook was set
        call_static_method("ProviderIndexUtil::ProviderIndexUtil", "setIndexFileOpenTestHook");
    }
}

# an index whose app has a module to load
const ClaimModule = "Q7611NoSuchModule";
maps.appmap{App} = {ClaimModule: True};
publish();
foreach hash<auto> mode in (Modes.pairIterator()) {
    string key = "tenants/" + mode.key;
    reset_index();
    Program r = tenant(True, False, mode.value, claim_code, "restricted-claim");
    r.run();
    check(key + "/claim", "INVALID-FUNCTION-ACCESS", r.getGlobalVariable("res"));
    check(key + "/host_claim", (ClaimModule,), ProviderIndexUtil::getAllAppModulesToLoad());
}

map printf("%s\n", $1), results;
