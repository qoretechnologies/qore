# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of the Programs of provider-index-write-integrity-worker.q (qore#7611): the worker preloads
# ProviderIndexUtil and ProviderIndex into each Program before it restricts it (or the code requires them), sets the
# q_* variables and runs it; the top-level code calls every method that writes, replaces or hides the data provider
# index that the host published, and stores in q_r the error code of a refusal, or "not refused"

# set by the worker before it runs the Program
our *string q_index_dir;
our *string q_own_dir;
our *string q_app;
our hash<auto> q_r;

# the refusals of a change of the published index
our hash<string, bool> q_denials = {
    "INVALID-FUNCTION-ACCESS": True,
    "PROVIDER-INDEX-REGISTRATION-DENIED": True,
};

# an index of the caller's own: another app than the host's
our hash<auto> q_maps = {
    "factorymap": {},
    "appmap": {"Q7611Child": {}},
    "schememap": {},
    "appinfomap": {"Q7611Child": {"name": "Q7611Child", "desc": "q7611 child app"}},
};

our hash<auto> q_opts = <ProviderIndex::ProviderIndexCreateOptions>{
    "load_known_factories": False,
    "load_environment": False,
    "load_typescript_actions": False,
};

# the published index written as YAML to a file of the caller's own: a directory that the caller gives is its own
try {
    ProviderIndexUtil::dumpIndexYaml(q_index_dir, NOTHING, q_own_dir + DirSep + "dump.yaml");
    q_r.dump_own = is_file(q_own_dir + DirSep + "dump.yaml");
} catch (hash<ExceptionInfo> ex) {
    q_r.dump_own = ex.err;
}

# the index replaced with one of the caller's own
try {
    ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, ("Q7611Child",), *hash<auto> sub (string app) {
        return {};
    }, 0s, {});
    q_r.write_by_app = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.write_by_app = q_denials{ex.err} ? ex.err : "not refused";
}

# the index replaced with an empty index
try {
    ProviderIndexUtil::writeDataProviderIndex(<DataProviderIndexInfo>{}, 0s, {});
    q_r.write = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.write = q_denials{ex.err} ? ex.err : "not refused";
}

# the index rebuilt in the configured index directory
try {
    ProviderIndex::createDataProviderIndex(<DataProviderIndexInfo>{}, NOTHING, q_opts);
    q_r.create = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.create = q_denials{ex.err} ? ex.err : "not refused";
}

# build mode: the published index is not read
try {
    ProviderIndexUtil::setBuildMode(True);
    q_r.build_mode = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.build_mode = q_denials{ex.err} ? ex.err : "not refused";
}
try { ProviderIndexUtil::setBuildMode(False); } catch (hash<ExceptionInfo> ex) {}

# a worker enters build mode before it reads its batch file, which does not exist
try {
    ProviderIndex::prepareIndexWorker(q_own_dir + DirSep + "no-batch");
    q_r.worker = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.worker = q_denials{ex.err} ? ex.err : "not refused";
}
try { ProviderIndexUtil::setBuildMode(False); } catch (hash<ExceptionInfo> ex) {}
try {
    # the worker fails (2), as the batch file does not exist
    q_r.run_worker = ProviderIndex::runIndexWorker(q_own_dir + DirSep + "no-batch") == 2 ? "not refused" : "ran";
} catch (hash<ExceptionInfo> ex) {
    q_r.run_worker = q_denials{ex.err} ? ex.err : "not refused";
}
try { ProviderIndexUtil::setBuildMode(False); } catch (hash<ExceptionInfo> ex) {}

# the cached info of the host's app replaced
try {
    ProviderIndexUtil::addAppToCache({"name": q_app, "desc": "q7611 replaced"});
    q_r.add_app = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.add_app = q_denials{ex.err} ? ex.err : "not refused";
}

# an action of the host's app replaced
try {
    ProviderIndexUtil::addActionToCache({"app": q_app, "action": "q7611-action"});
    q_r.add_action = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.add_action = q_denials{ex.err} ? ex.err : "not refused";
}

# the host's app hidden
try {
    ProviderIndexUtil::removeCachedApp(q_app);
    q_r.remove_app = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.remove_app = q_denials{ex.err} ? ex.err : "not refused";
}

# the index rebuilt in a directory of the caller's own, which becomes the configured index directory of the process
try {
    ProviderIndex::createDataProviderIndex(<DataProviderIndexInfo>{}, q_own_dir + DirSep + "create", q_opts);
    q_r.create_dir = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.create_dir = q_denials{ex.err} ? ex.err : "not refused";
}

# the configured index directory of the process replaced by one of the caller's own
try {
    ProviderIndexUtil::setEnvironmentVariable(q_own_dir + DirSep + "env");
    q_r.set_env = "not refused";
} catch (hash<ExceptionInfo> ex) {
    q_r.set_env = q_denials{ex.err} ? ex.err : "not refused";
}
