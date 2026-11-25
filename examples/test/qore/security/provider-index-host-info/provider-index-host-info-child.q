# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the code of the Programs of provider-index-host-info-worker.q (qore#7597): the worker preloads ProviderIndexUtil and
# ProviderIndex into each Program before it restricts it (or the code requires them), sets the q_* variables and runs
# it; the top-level code calls the methods of the modules that use the data provider index and stores what it gets
# (or the error) in q_r

# set by the worker before it runs the Program
our *string q_index_dir;
our *string q_app;
our *hash<auto> q_maps;
# makes the index directory writable (True) or not (False); the host's code
our *code q_set_writable;
our hash<auto> q_r;

# the methods that return the index directory, paths under it or the environment of the index
try { q_r.dir = ProviderIndexUtil::tryGetIndexDir(); } catch (hash<ExceptionInfo> ex) { q_r.dir = ex.err; }
try { q_r.dir_throw = ProviderIndexUtil::tryGetIndexDir(True); } catch (hash<ExceptionInfo> ex) { q_r.dir_throw = ex.err; }
try { q_r.dir_write = ProviderIndexUtil::tryGetIndexDir(True, True); } catch (hash<ExceptionInfo> ex) { q_r.dir_write = ex.err; }
try { q_r.writable = ProviderIndexUtil::getWritableIndexDir(); } catch (hash<ExceptionInfo> ex) { q_r.writable = ex.err; }
try { q_r.path = ProviderIndexUtil::getIndexPath("routing"); } catch (hash<ExceptionInfo> ex) { q_r.path = ex.err; }
# a directory that the caller gives is the caller's own
try { q_r.path_dir = ProviderIndexUtil::getIndexPath("routing", "/q7597"); } catch (hash<ExceptionInfo> ex) { q_r.path_dir = ex.err; }
try { q_r.max = ProviderIndexUtil::getMaxCanonicalIndexSize(); } catch (hash<ExceptionInfo> ex) { q_r.max = ex.err; }

# the lookups in the index work for every caller
try { q_r.lookup = ProviderIndexUtil::tryGetAppInfo(q_app).desc; } catch (hash<ExceptionInfo> ex) { q_r.lookup = ex.err; }
try { q_r.apps = (map $1.name, ProviderIndexUtil::getCachedApps(), $1.name == q_app); } catch (hash<ExceptionInfo> ex) { q_r.apps = ex.err; }

# the routing index of the published index is corrupt: the reload fails, and the process keeps its index
try { q_r.reload_error = ProviderIndexUtil::reloadIndex().error; } catch (hash<ExceptionInfo> ex) { q_r.reload_error = ex.err; }
try { q_r.read_error = ProviderIndexUtil::getIndexReadError(); } catch (hash<ExceptionInfo> ex) { q_r.read_error = ex.err; }
try { q_r.lookup_after = ProviderIndexUtil::tryGetAppInfo(q_app).desc; } catch (hash<ExceptionInfo> ex) { q_r.lookup_after = ex.err; }

# writing the index (the same index, so that the index of the process stays valid)
try {
    q_r.write_path = ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,),
        *hash<auto> sub (string app) { return {}; }, 0s, {}).path;
} catch (hash<ExceptionInfo> ex) {
    q_r.write_path = ex.err + ": " + ex.desc;
}
# the index is published again: it can be written as YAML
try { q_r.dump = ProviderIndexUtil::dumpIndexYaml().find(q_app) >= 0; } catch (hash<ExceptionInfo> ex) { q_r.dump = ex.err; }
try { q_r.dump_dir = ProviderIndexUtil::dumpIndexYaml(q_index_dir).find(q_app) >= 0; } catch (hash<ExceptionInfo> ex) { q_r.dump_dir = ex.err; }
# an exception of the caller's code is passed on as it is
try {
    ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,),
        *hash<auto> sub (string app) { throw "Q7597-CALLER", "q7597 caller description"; }, 0s, {});
    q_r.write_caller = "written";
} catch (hash<ExceptionInfo> ex) {
    q_r.write_caller = ex.err + ": " + ex.desc;
}

# the index directory cannot be written to
q_set_writable(False);
on_exit q_set_writable(True);
try {
    ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,), *hash<auto> sub (string app) { return {}; }, 0s,
        {});
    q_r.write_denied = "written";
} catch (hash<ExceptionInfo> ex) {
    q_r.write_denied = ex.err + ": " + ex.desc;
}
try {
    ProviderIndex::createDataProviderIndex();
    q_r.pi_denied = "created";
} catch (hash<ExceptionInfo> ex) {
    q_r.pi_denied = ex.err + ": " + ex.desc;
}
try {
    ProviderIndex::createDataProviderIndex(<DataProviderIndexInfo>{}, NOTHING,
        <ProviderIndex::ProviderIndexCreateOptions>{"workers": 1});
    q_r.pi_workers = "created";
} catch (hash<ExceptionInfo> ex) {
    q_r.pi_workers = ex.err == "INVALID-FUNCTION-ACCESS" ? ex.err : ex.err + ": " + ex.desc;
}
try { q_r.writable_denied = ProviderIndexUtil::getWritableIndexDir(); } catch (hash<ExceptionInfo> ex) { q_r.writable_denied = ex.err == "INVALID-FUNCTION-ACCESS" ? ex.err : ex.err + ": " + ex.desc; }
