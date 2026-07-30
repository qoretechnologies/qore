#!/bin/bash
# Regression: the Qorus logger cache path must preserve nested hashdecl casts
# when logger params are parsed from YAML, stored in a typed hash, and then
# recast by another class compiled as a separate source-deferred .qo object.

set -euo pipefail

QORE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${QORE_ROOT}"

TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

QCC="${QCC:-./build/qcc}"
mkdir -p "${TMP}/src" "${TMP}/qo"

cat >"${TMP}/src/provider.q" <<'QORE'
%new-style
%requires yaml
%strict-args
%require-types

hashdecl AppenderParams {
    *softstring name;
    *string layoutPattern;
    *softstring filename;
    *int rotationCount;
    *string appenderType;
    *string encoding;
    *string archivePattern;
    *bool lifecycleMarkers;
    *string url;
    *string index;
    *int batchSize;
    *int flushIntervalMs;
    *string pipeline;
    *int retentionDays;
}

hashdecl LoggerParams {
    int level;
    *softstring name;
    *bool additivity;
    *hash<string, hash<AppenderParams>> appenders;
}

class AbstractLogger {
    hash<auto> getLoggerMap(string params_yaml) {
        hash<auto> loggerMap = {};
        hash<auto> params;
        hash<auto> tmp_params = parse_yaml(params_yaml);
        if (tmp_params.appenders) {
            params.appenders = map {$1.key: cast<hash<AppenderParams>>($1.value)},
                tmp_params.appenders.pairIterator();
        }
        params += tmp_params - "appenders";
        loggerMap{143}.params = cast<hash<LoggerParams>>(params);
        loggerMap{143}.interface_table_name = "system";

        return {
            "loggerMap": loggerMap,
            "loggerAliases": {
                "system": 143,
            },
        };
    }
}

class AbstractQorusClientProcess inherits AbstractLogger {
}

class AbstractQorusProcessManager inherits AbstractQorusClientProcess {
}
QORE

cat >"${TMP}/src/common.q" <<'QORE'
%new-style
%strict-args
%require-types

class QorusMasterCoreQsvcCommon {
    static *hash<LoggerParams> buildLoggerParams(*hash<auto> input_params) {
        if (!input_params) {
            return;
        }
        hash<auto> params;
        if (input_params.appenders) {
            params.appenders = map {$1.key: cast<hash<AppenderParams>>($1.value)},
                input_params.appenders.pairIterator();
        }
        params += input_params - "appenders";
        return cast<hash<LoggerParams>>(params);
    }
}
QORE

cat >"${TMP}/src/main.q" <<'QORE'
%new-style
%strict-args
%require-types

class QorusMaster inherits AbstractQorusProcessManager, QorusMasterCoreQsvcCommon {
    constructor() {
        string logger_yaml = "%YAML 1.2\n--- {level: -9223372036854775808, name: \"DefaultSystemLogger\", "
            "additivity: false, appenders: {1: {name: \"DefaultSystemAppender\", "
            "appenderType: \"LoggerAppenderFileRotate\", "
            "layoutPattern: \"%d{YYYY-MM-DD HH:mm:SS.xx} %h:%P T%t [%p]: %m%n\", "
            "encoding: \"UTF-8\", rotationCount: 10, archivePattern: \"%p%f.%i\", "
            "filename: \"$path/OMQ-$instance-$name.log\"}}}\n";
        hash<auto> init_logger_map = getLoggerMap(logger_yaml);
        if (*int loggerid = init_logger_map.loggerAliases.system) {
            hash<LoggerParams> output = QorusMasterCoreQsvcCommon::buildLoggerParams(
                init_logger_map.loggerMap{loggerid}.params);
            printf("%y\n", output);
        }
    }
}

int sub main() {
    QorusMaster q();
    return 0;
}
QORE

cat >"${TMP}/source-symbols.manifest" <<QORE
format=1
hashdecl	AppenderParams	${TMP}/src/provider.q
hashdecl	LoggerParams	${TMP}/src/provider.q
class	AbstractLogger	${TMP}/src/provider.q
class	AbstractQorusClientProcess	${TMP}/src/provider.q
class	AbstractQorusProcessManager	${TMP}/src/provider.q
class	QorusMasterCoreQsvcCommon	${TMP}/src/common.q
QORE

echo "=== Step 1: compile hashdecl provider ==="
"${QCC}" -c -o "${TMP}/qo/provider.qo" "${TMP}/src/provider.q" | tail -2

echo ""
echo "=== Step 2: compile source-deferred common helper ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/common.idx.json" \
    -o "${TMP}/qo/common.qo" \
    "${TMP}/src/common.q" | tail -2

echo ""
echo "=== Step 3: compile source-deferred main executable source ==="
"${QCC}" -c \
    -L "${TMP}/qo" \
    --source-symbol-manifest="${TMP}/source-symbols.manifest" \
    --write-index-json="${TMP}/main.idx.json" \
    -o "${TMP}/qo/main.qo" \
    "${TMP}/src/main.q" | tail -2

echo ""
echo "=== Step 4: link separate .qo files ==="
"${QCC}" -o "${TMP}/hash-map-hashdecl-cast" \
    "${TMP}/qo/provider.qo" "${TMP}/qo/common.qo" "${TMP}/qo/main.qo" | tail -2

echo ""
echo "=== Step 5: run compiled regression ==="
set +e
out="$(LD_LIBRARY_PATH="${QORE_ROOT}/build${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${TMP}/hash-map-hashdecl-cast" 2>&1)"
status=$?
set -e
printf '%s\n' "${out}"
echo "status=${status}"
test "${status}" -eq 0
test "${out}" = '{level: -9223372036854775808, name: "DefaultSystemLogger", additivity: False, appenders: {1: {name: "DefaultSystemAppender", layoutPattern: "%d{YYYY-MM-DD HH:mm:SS.xx} %h:%P T%t [%p]: %m%n", filename: "$path/OMQ-$instance-$name.log", rotationCount: 10, appenderType: "LoggerAppenderFileRotate", encoding: "UTF-8", archivePattern: "%p%f.%i"}}}'

echo ""
echo "OK: hash-map hashdecl cast preserved typed appender values."
