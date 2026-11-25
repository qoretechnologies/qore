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
# publishes the host's index again, after the routing index was made corrupt; the host's code
our *code q_restore_index;
# the refusals of a change of the published index, which only the host's code may change (qore#7611)
our hash<string, bool> q_denials = {
    "INVALID-FUNCTION-ACCESS": True,
    "PROVIDER-INDEX-REGISTRATION-DENIED": True,
};
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

# writing the index (the same index, so that the index of the process stays valid); only code that may change the
# published index may write it (qore#7611)
try {
    q_r.write_path = ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,),
        *hash<auto> sub (string app) { return {}; }, 0s, {}).path;
} catch (hash<ExceptionInfo> ex) {
    q_r.write_path = q_denials{ex.err} ? ex.err : ex.err + ": " + ex.desc;
}
# the index is published again: it can be written as YAML
q_restore_index();
try { q_r.dump = ProviderIndexUtil::dumpIndexYaml().find(q_app) >= 0; } catch (hash<ExceptionInfo> ex) { q_r.dump = ex.err; }
try { q_r.dump_dir = ProviderIndexUtil::dumpIndexYaml(q_index_dir).find(q_app) >= 0; } catch (hash<ExceptionInfo> ex) { q_r.dump_dir = ex.err; }
# an exception of the caller's code is passed on as it is
try {
    ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,),
        *hash<auto> sub (string app) { throw "Q7597-CALLER", "q7597 caller description"; }, 0s, {});
    q_r.write_caller = "written";
} catch (hash<ExceptionInfo> ex) {
    q_r.write_caller = q_denials{ex.err} ? ex.err : ex.err + ": " + ex.desc;
}

# the process-wide logger receives the module's diagnostics, which name the index directory
try { q_r.logger_get = exists ProviderIndexUtil::getGlobalLogger(); } catch (hash<ExceptionInfo> ex) { q_r.logger_get = ex.err; }
try {
    *Logger::LoggerInterface old = ProviderIndexUtil::getGlobalLogger();
    ProviderIndexUtil::setGlobalLogger(new Logger::Logger("q7597-child", Logger::LoggerLevel::getLevelAll()));
    # the host's logger is restored
    ProviderIndexUtil::setGlobalLogger(old);
    q_r.logger_set = "set";
} catch (hash<ExceptionInfo> ex) {
    q_r.logger_set = ex.err;
}

# the logger of the data providers, which the host also gives ProviderIndexUtil
try { q_r.dp_logger_get = exists DataProvider::DataProvider::getGlobalLogger(); } catch (hash<ExceptionInfo> ex) { q_r.dp_logger_get = ex.err; }
try {
    *Logger::LoggerInterface old = DataProvider::DataProvider::getGlobalLogger();
    DataProvider::DataProvider::setGlobalLogger(new Logger::Logger("q7597-child", Logger::LoggerLevel::getLevelAll()));
    # the host's logger is restored
    DataProvider::DataProvider::setGlobalLogger(old);
    q_r.dp_logger_set = "set";
} catch (hash<ExceptionInfo> ex) {
    q_r.dp_logger_set = ex.err;
}

# the index directory cannot be written to
q_set_writable(False);
on_exit q_set_writable(True);
try {
    ProviderIndexUtil::writeDataProviderIndexByApp(q_maps, (q_app,), *hash<auto> sub (string app) { return {}; }, 0s,
        {});
    q_r.write_denied = "written";
} catch (hash<ExceptionInfo> ex) {
    q_r.write_denied = q_denials{ex.err} ? ex.err : ex.err + ": " + ex.desc;
}
try {
    ProviderIndex::createDataProviderIndex();
    q_r.pi_denied = "created";
} catch (hash<ExceptionInfo> ex) {
    q_r.pi_denied = q_denials{ex.err} ? ex.err : ex.err + ": " + ex.desc;
}
try {
    ProviderIndex::createDataProviderIndex(<DataProviderIndexInfo>{}, NOTHING,
        <ProviderIndex::ProviderIndexCreateOptions>{"workers": 1});
    q_r.pi_workers = "created";
} catch (hash<ExceptionInfo> ex) {
    q_r.pi_workers = q_denials{ex.err} ? ex.err : ex.err + ": " + ex.desc;
}
try { q_r.writable_denied = ProviderIndexUtil::getWritableIndexDir(); } catch (hash<ExceptionInfo> ex) { q_r.writable_denied = ex.err == "INVALID-FUNCTION-ACCESS" ? ex.err : ex.err + ": " + ex.desc; }
