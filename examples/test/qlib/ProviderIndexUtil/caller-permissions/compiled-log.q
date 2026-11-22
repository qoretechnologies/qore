%modern
%no-terminal-io
%requires ProviderIndexUtil
# Debug level has no output without a configured logger.
int failures = 0;
try {
    ProviderIndexUtil::log(LoggerLevel::LevelDebug, "");
    ++failures;
} catch (hash<ExceptionInfo> ex) {
    if (ex.err != "PERMISSION-DENIED") {
        rethrow;
    }
}
try {
    ProviderIndexUtil::logArgs(LoggerLevel::LevelDebug, "", ());
    ++failures;
} catch (hash<ExceptionInfo> ex) {
    if (ex.err != "PERMISSION-DENIED") {
        rethrow;
    }
}
return failures;
