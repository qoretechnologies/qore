%modern
%no-process-control
%requires ProviderIndexUtil
# Compilation checks access. The function is never executed.
nothing sub check_initialization_access() {
    ProviderIndexUtil::getAllAppModulesToLoad();
}
