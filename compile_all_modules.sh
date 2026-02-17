#!/bin/bash
# Compile all qlib modules to AOT .qmod files
# Uses multi-pass approach to handle dependency ordering automatically

SRCDIR="$(cd "$(dirname "$0")" && pwd)"
BUILDDIR="${1:-$SRCDIR/build-debug}"
OUTDIR="$SRCDIR/qlib-bin"
QCC="$BUILDDIR/qcc"

if [ ! -x "$QCC" ]; then
    echo "ERROR: qcc not found at $QCC" >&2
    echo "Usage: $0 [build-dir]" >&2
    echo "  build-dir defaults to build-debug" >&2
    exit 1
fi

export LD_LIBRARY_PATH="$BUILDDIR"
# Source modules first for complete dependency resolution during compilation,
# then binary module dirs, then compiled output for already-compiled deps
export QORE_MODULE_DIR="$SRCDIR/qlib:$BUILDDIR/modules/reflection:$BUILDDIR/modules/astparser:$BUILDDIR/modules/logger_bin:$BUILDDIR/modules/linenoise:$OUTDIR"

mkdir -p "$OUTDIR"

# All modules in the order from CMakeLists.txt (path name pairs)
MODULES=(
    # base
    "qlib/DatasourceProvider.qm DatasourceProvider"
    "qlib/DebugUtil.qm DebugUtil"
    "qlib/FsUtil.qm FsUtil"
    "qlib/Qdx.qm Qdx"
    "qlib/Qorize.qm Qorize"
    "qlib/QoreHistory QoreHistory"
    "qlib/QoreRepl QoreRepl"
    "qlib/Util.qm Util"
    "qlib/WebSocketUtil.qm WebSocketUtil"
    "qlib/MapperUtil.qm MapperUtil"
    "qlib/ProviderIndexUtil ProviderIndexUtil"
    "qlib/QoreApiMetadata QoreApiMetadata"
    "qlib/QoreCodeCompletion QoreCodeCompletion"
    "qlib/QoreCodeFormat QoreCodeFormat"
    "qlib/QLS QLS"
    # base 2
    "qlib/FileLocationHandler FileLocationHandler"
    "qlib/Diff.qm Diff"
    "qlib/Logger.qm Logger"
    "qlib/Mime.qm Mime"
    "qlib/QUnit.qm QUnit"
    "qlib/TextWrap.qm TextWrap"
    "qlib/EdifactUtil EdifactUtil"
    # base 3
    "qlib/DataProvider DataProvider"
    "qlib/DpqlWebSocket DpqlWebSocket"
    "qlib/DebugProgramControl.qm DebugProgramControl"
    "qlib/HttpServerUtil.qm HttpServerUtil"
    "qlib/MailMessage.qm MailMessage"
    # base 4
    "qlib/AsyncSocketIo AsyncSocketIo"
    "qlib/FilePoller.qm FilePoller"
    "qlib/FileDataProvider FileDataProvider"
    "qlib/FtpClientDataProvider FtpClientDataProvider"
    "qlib/HttpClientDataProvider HttpClientDataProvider"
    "qlib/ServerSentEventHandler ServerSentEventHandler"
    "qlib/SqlUtil SqlUtil"
    "qlib/ConnectionProvider ConnectionProvider"
    "qlib/CsvUtil CsvUtil"
    "qlib/FixedLengthUtil FixedLengthUtil"
    "qlib/Mapper.qm Mapper"
    "qlib/FtpPollerUtil.qm FtpPollerUtil"
    "qlib/BulkSqlUtil BulkSqlUtil"
    "qlib/Schema.qm Schema"
    "qlib/TelnetClient.qm TelnetClient"
    "qlib/DebugCmdLine.qm DebugCmdLine"
    "qlib/DiscordWebSocketClient.qm DiscordWebSocketClient"
    "qlib/HttpServer.qm HttpServer"
    "qlib/HttpServerAsyncIo HttpServerAsyncIo"
    "qlib/Http2ClientIo Http2ClientIo"
    "qlib/Pop3Client.qm Pop3Client"
    "qlib/RestSchemaValidator.qm RestSchemaValidator"
    "qlib/SewioWebSocketClient.qm SewioWebSocketClient"
    "qlib/SmtpClient.qm SmtpClient"
    "qlib/WebSocketHandler.qm WebSocketHandler"
    "qlib/WebUtil.qm WebUtil"
    # base 5
    "qlib/ProviderIndex ProviderIndex"
    "qlib/ServerSentEventClient ServerSentEventClient"
    "qlib/MssqlSqlUtilBase.qm MssqlSqlUtilBase"
    "qlib/MysqlSqlUtil.qm MysqlSqlUtil"
    "qlib/OracleSqlUtilBase.qm OracleSqlUtilBase"
    "qlib/PgsqlSqlUtilBase.qm PgsqlSqlUtilBase"
    "qlib/Sqlite3SqlUtil.qm Sqlite3SqlUtil"
    "qlib/XdbcFirebirdSqlUtilBase.qm XdbcFirebirdSqlUtilBase"
    "qlib/WebSocketClient.qm WebSocketClient"
    "qlib/TableMapper.qm TableMapper"
    "qlib/DbDataProvider DbDataProvider"
    "qlib/FtpPoller.qm FtpPoller"
    "qlib/SchemaReverse.qm SchemaReverse"
    "qlib/DebugHandler.qm DebugHandler"
    "qlib/DebugLinenoiseCmdLine.qm DebugLinenoiseCmdLine"
    "qlib/AsyncApi.qm AsyncApi"
    "qlib/OpenApi3.qm OpenApi3"
    "qlib/RestHandler.qm RestHandler"
    "qlib/Swagger.qm Swagger"
    "qlib/VscDebugAdapter.qm VscDebugAdapter"
    "qlib/Pop3ClientDataProvider Pop3ClientDataProvider"
    # base 6
    "qlib/FreetdsSqlUtil.qm FreetdsSqlUtil"
    "qlib/JdbcMicrosoftSqlUtil.qm JdbcMicrosoftSqlUtil"
    "qlib/OracleSqlUtil.qm OracleSqlUtil"
    "qlib/JdbcOracleSqlUtil.qm JdbcOracleSqlUtil"
    "qlib/PgsqlSqlUtil.qm PgsqlSqlUtil"
    "qlib/JdbcPostgresqlSqlUtil.qm JdbcPostgresqlSqlUtil"
    "qlib/JdbcFirebirdSqlUtil.qm JdbcFirebirdSqlUtil"
    "qlib/OdbcFirebirdSqlUtil.qm OdbcFirebirdSqlUtil"
    "qlib/RestClient.qm RestClient"
    # base 7
    "qlib/SalesforceRestClient.qm SalesforceRestClient"
    "qlib/SewioRestClient.qm SewioRestClient"
    "qlib/ZeyosRestClient.qm ZeyosRestClient"
    "qlib/BillwerkRestClient.qm BillwerkRestClient"
    "qlib/Sap4HanaRestClient.qm Sap4HanaRestClient"
    "qlib/CdsRestClient.qm CdsRestClient"
    "qlib/DiscordRestClient.qm DiscordRestClient"
    "qlib/FreshBooksRestClient.qm FreshBooksRestClient"
    "qlib/LinearRestClient.qm LinearRestClient"
    "qlib/WaveRestClient.qm WaveRestClient"
    "qlib/GoogleRestClient.qm GoogleRestClient"
    "qlib/HueRestClient.qm HueRestClient"
    "qlib/JotformRestClient.qm JotformRestClient"
    "qlib/MailgunRestClient.qm MailgunRestClient"
    "qlib/MewsRestClient.qm MewsRestClient"
    "qlib/NetSuiteRestClient.qm NetSuiteRestClient"
    "qlib/OpenAiRestClient.qm OpenAiRestClient"
    "qlib/ServiceNowRestClient.qm ServiceNowRestClient"
    "qlib/ShipStationRestClient.qm ShipStationRestClient"
    "qlib/ShippoRestClient.qm ShippoRestClient"
    "qlib/SquareRestClient.qm SquareRestClient"
    "qlib/Cin7CoreRestClient.qm Cin7CoreRestClient"
    "qlib/UnleashedRestClient.qm UnleashedRestClient"
    "qlib/ZohoBooksRestClient.qm ZohoBooksRestClient"
    "qlib/ZohoInventoryRestClient.qm ZohoInventoryRestClient"
    "qlib/ZohoInvoiceRestClient.qm ZohoInvoiceRestClient"
    "qlib/AwsRestClient.qm AwsRestClient"
    "qlib/SwaggerDataProvider SwaggerDataProvider"
    "qlib/AsyncApiDataProvider AsyncApiDataProvider"
    "qlib/RestSchemaDataProvider RestSchemaDataProvider"
    "qlib/CdsRestDataProvider CdsRestDataProvider"
    "qlib/ServiceNowRestDataProvider ServiceNowRestDataProvider"
    "qlib/RestClientDataProvider RestClientDataProvider"
    "qlib/ElasticSearchDataProvider ElasticSearchDataProvider"
    "qlib/EmpathicBuildingDataProvider EmpathicBuildingDataProvider"
    "qlib/GeneratorDataProvider GeneratorDataProvider"
    "qlib/MemcachedDataProvider MemcachedDataProvider"
    "qlib/MongoDbDataProvider MongoDbDataProvider"
    "qlib/RedisDataProvider RedisDataProvider"
    # base 8
    "qlib/ElasticSearchLoggerAppender.qm ElasticSearchLoggerAppender"
    "qlib/SalesforceRestDataProvider SalesforceRestDataProvider"
    "qlib/AwsRestClientDataProvider AwsRestClientDataProvider"
    "qlib/GoogleDataProvider GoogleDataProvider"
    "qlib/DiscordDataProvider DiscordDataProvider"
    "qlib/FreshBooksDataProvider FreshBooksDataProvider"
    "qlib/LinearDataProvider LinearDataProvider"
    "qlib/WaveDataProvider WaveDataProvider"
    "qlib/JotformDataProvider JotformDataProvider"
    "qlib/MailgunDataProvider MailgunDataProvider"
    "qlib/MewsRestDataProvider MewsRestDataProvider"
    "qlib/OpenAiDataProvider OpenAiDataProvider"
    # base 9
    "qlib/GoogleCalendarDataProvider GoogleCalendarDataProvider"
    "qlib/GmailDataProvider GmailDataProvider"
    "qlib/ZohoBooksDataProvider ZohoBooksDataProvider"
    "qlib/ZohoInventoryDataProvider ZohoInventoryDataProvider"
    "qlib/ZohoInvoiceDataProvider ZohoInvoiceDataProvider"
    "qlib/ShipStationDataProvider ShipStationDataProvider"
    "qlib/ShippoDataProvider ShippoDataProvider"
    "qlib/SquareDataProvider SquareDataProvider"
    "qlib/Cin7CoreDataProvider Cin7CoreDataProvider"
    "qlib/UnleashedDataProvider UnleashedDataProvider"
    "qlib/QdrantDataProvider QdrantDataProvider"
)

TOTAL=${#MODULES[@]}

echo "=== AOT Compiling all qlib modules (multi-pass) ==="
echo "QCC: $QCC"
echo "Output: $OUTDIR"
echo "QORE_MODULE_DIR: $QORE_MODULE_DIR"
echo "Total modules: $TOTAL"
echo

MAX_PASSES=10
PASS=0
COMPILED=0
LAST_COMPILED=-1

while [ $PASS -lt $MAX_PASSES ] && [ $COMPILED -ne $LAST_COMPILED ]; do
    PASS=$((PASS + 1))
    LAST_COMPILED=$COMPILED
    PASS_NEW=0
    PASS_SKIP=0
    PASS_FAIL=0
    FAILED_THIS_PASS=""

    echo "=== Pass $PASS ==="
    for entry in "${MODULES[@]}"; do
        src="${entry% *}"
        name="${entry##* }"
        out="$OUTDIR/${name}.qmod"

        # Skip already compiled
        if [ -f "$out" ]; then
            PASS_SKIP=$((PASS_SKIP + 1))
            continue
        fi

        local_src="$SRCDIR/$src"
        if [ -d "$local_src" ]; then
            label="$src (dir)"
        elif [ -f "$local_src" ]; then
            label="$src"
        else
            continue
        fi

        printf "  %-45s ... " "$label"
        if OUTPUT=$("$QCC" -m "$local_src" -o "$out" 2>&1); then
            echo "OK"
            PASS_NEW=$((PASS_NEW + 1))
            COMPILED=$((COMPILED + 1))
        else
            echo "FAIL"
            # Show first meaningful error line
            ERR=$(echo "$OUTPUT" | grep -E '(ERROR|error:|cannot be found|PARSE-EXCEPTION)' | head -1)
            if [ -n "$ERR" ]; then
                echo "    $ERR"
            fi
            PASS_FAIL=$((PASS_FAIL + 1))
            FAILED_THIS_PASS="$FAILED_THIS_PASS $name"
        fi
    done

    echo "  Pass $PASS: $PASS_NEW new, $PASS_SKIP skipped, $PASS_FAIL failed"
    echo
done

REMAINING=$((TOTAL - COMPILED))
echo "=== Final Summary ==="
echo "  Compiled: $COMPILED / $TOTAL"
echo "  Remaining: $REMAINING"
echo "  Passes: $PASS"
if [ -n "$FAILED_THIS_PASS" ]; then
    echo "  Still failing:$FAILED_THIS_PASS"
fi
