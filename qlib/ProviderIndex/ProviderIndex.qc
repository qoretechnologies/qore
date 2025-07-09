# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProviderIndex.qc module for indexing data providers

/*  ProviderIndex.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ProviderIndex module
public namespace ProviderIndex {
#! The main API class
public class ProviderIndex inherits ProviderIndexUtil {
    public {}

    #! Create the data provider index
    public static hash<auto> createDataProviderIndex(hash<DataProviderIndexInfo> index = <DataProviderIndexInfo>{},
            *string dir) {
        date start = now_us();

        # setup logger
        setupLogger();

        if (dir.val()) {
            setEnvironmentVariable(dir);
        }

        # check if environment variable is set
        tryGetIndexDir(True);

        # Handle the old index file cleanly
        IndexFileHelper ifh("data");
        on_error ifh.abort();

        # make sure that all data providers are initialized immediately
        DataProvider::setOptions(DPO_DisableOnDemandInitialization | DPO_EnableOnDemandActions);

        # load all known data provider factories
        DataProvider::registerKnownFactories();

        # start with the default factory map
        index.factorymap += DataProvider::FactoryMap;

        if (ENV.QORE_DATA_PROVIDERS) {
            # load all data providers from the QORE_DATA_PROVIDERS environment variable
            DataProvider::loadProvidersFromEnvironment();

            addDataProviderFactories(\index);
        }

        # try to load and initialize TypeScriptActionInterface manually
        tryLoadTypeScriptActionInterface();

        # iterate all apps to initialize the app and scheme maps
        foreach hash<DataProviderAppInfo> app in (DataProviderActionCatalog::getAllAppsNoInit()) {
            *list<string> paths = DataProviderActionCatalog::getModulePathsForApp(app.name);
            if (!paths) {
                log(LoggerLevel::LevelError, "Data provider app %y has no module path", app.name);
                continue;
            }
            if (!index.appmap{app.name}) {
                map index.appmap{app.name}{$1} = True, paths;
                map log(LoggerLevel::LevelDebug, "Registered data provider app %y -> %y", app.name, $1), paths;
            }
            if (app.scheme) {
                *hash<ConnectionSchemeInfo> info = ConnectionSchemeCache::getScheme(app.scheme);
                if (!info) {
                    log(LoggerLevel::LevelError, "Data provider app %y has unregistered scheme %y", app.name, app.scheme);
                    continue;
                }
                foreach string path in (paths) {
                    map index.schememap{$1}{path} = True, keys info.schemes;
                    map log(LoggerLevel::LevelDebug, "Registered scheme %y -> %y", $1, path), keys info.schemes;
                }
            }
        }

        return writeDataProviderIndex(index, now_us() - start, {
            "factories": index.factorymap.size(),
            "apps": index.appmap.size(),
            "schemes": index.schememap.size(),
        });
    }

    #! Setup logger
    private static setupLogger() {
        Logger logger("provider index", LoggerLevel::LevelDebug);
        logger.addAppender(new StdoutAppender());
        DataProvider::setGlobalLogger(logger);
        ProviderIndexUtil::setGlobalLogger(logger);
    }

    #! Try to load and initialize TypeScriptActionInterface
    private static tryLoadTypeScriptActionInterface() {
        try {
            load_module("TypeScriptActionInterface");
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::LevelError, "Failed to load and initialize TypeScriptActionInterface: %s: %s", ex.err,
                ex.desc);
        }
    }

    #! Iterate all data provider factories and add them to the index
    private static addDataProviderFactories(reference<hash<DataProviderIndexInfo>> index) {
         # Iterate all factories and add any new data providers
        foreach string factory in (DataProvider::listFactories()) {
            # get the factory object
            AbstractDataProviderFactory f = DataProvider::getFactory(factory);
            if (!f) {
                log(LoggerLevel::LevelError, "Data provider factory %y registered but cannot be returned", factory);
                continue;
            }

            string factory = f.getName();
            if (index.factorymap{factory}) {
                # already registered
                continue;
            }

            # get the module name for the factory
            *string mod = Class::getClass(f).getModuleName();
            if (!mod) {
                log(LoggerLevel::LevelError, "Data provider factory %y has no module name", factory);
                continue;
            }

            # get the path to the module
            *string path = get_module_hash(){mod}.filename;
            if (!path.val()) {
                log(LoggerLevel::LevelError, "Data provider factory %y module %y has no path", factory, mod);
                continue;
            }

            index.factorymap{factory} = path;
            log(LoggerLevel::LevelDebug, "Registered factory %y -> %y", factory, path);
        }
    }
}
}

namespace Priv {
class IndexFileHelper {
    public {}

    private {
        string orig_path;
        string new_path;
        bool restore;
    }

    #! Constructor
    constructor(string type) {
        orig_path = ProviderIndexUtil::getIndexPath(type);

        # check if the index file exists and is readable
        if (hstat(orig_path) == "REGULAR") {
            # if so, make a backup
            new_path = orig_path + ".orig";
            rename(orig_path, new_path);
        }
    }

    destructor() {
        # delete the backup file, if any
        if (new_path.val()) {
            unlink(new_path);
        }
    }

    abort() {
        if (new_path.val()) {
            # restore the backup
            rename(new_path, orig_path);
            remove new_path;
        }
    }
}
}