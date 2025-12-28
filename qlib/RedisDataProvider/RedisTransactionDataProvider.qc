# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedisDataProvider module definition

/** RedisTransactionDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedisDataProvider module
public namespace RedisDataProvider {
#! The Redis transaction data provider
public class RedisTransactionDataProvider inherits RedisDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "transactions",
            "desc": "Redis transaction data provider; transaction operations (MULTI, EXEC, DISCARD, WATCH, UNWATCH)",
            "type": "RedisTransactionDataProvider",
            "constructor_options": RedisDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = new RedisTransactionRequestDataType();
        const ResponseType = new RedisTransactionResponseDataType();
    }

    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", RedisDataProvider::ConstructorOptions, options);
        redis = RedisDataProvider::getRedisClient(options);
    }

    constructor(RedisClient redis) : RedisDataProviderBase(redis) {
    }

    string getName() { return ProviderInfo.name; }
    *string getDesc() { return sprintf("Redis transaction provider for `%s`", redis.getSafeURL()); }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        switch (req.operation) {
            case "multi":
                string result = redis.command("MULTI");
                return {"queued": result == "OK"};

            case "exec":
                *list<auto> results = redis.command("EXEC");
                return {"results": results};

            case "discard":
                redis.command("DISCARD");
                return {"discarded": True};

            case "watch":
                redis.commandWithArgs("WATCH", req.keys);
                return {"watching": True};

            case "unwatch":
                redis.command("UNWATCH");
                return {"unwatched": True};

            default:  # execute_transaction - convenience method
                # Execute a list of commands in a transaction
                if (!req.commands) {
                    throw "REDIS-ERROR", "commands list is required for execute_transaction";
                }

                # WATCH keys if specified
                if (req.watch_keys) {
                    redis.commandWithArgs("WATCH", req.watch_keys);
                }

                # Start transaction
                redis.command("MULTI");

                try {
                    # Queue all commands
                    foreach list<auto> cmd in (req.commands) {
                        redis.commandWithArgs(cmd[0], cmd.size() > 1 ? cmd[1..] : ());
                    }

                    # Execute
                    *list<auto> results = redis.command("EXEC");

                    if (!exists results) {
                        return {"aborted": True, "reason": "WATCH key was modified"};
                    }

                    return {"results": results};
                } catch (hash<ExceptionInfo> ex) {
                    # Try to discard on error
                    try {
                        redis.command("DISCARD");
                    } catch () {
                        # Ignore discard errors
                    }
                    rethrow;
                }
        }
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class RedisTransactionRequestDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "operation": {"type": StringOrNothingType, "desc": "Operation: 'execute_transaction' (default), 'multi', 'exec', 'discard', 'watch', 'unwatch'"},
            "commands": {"type": SoftListOrNothingType, "desc": "List of commands for execute_transaction, each is [cmd, arg1, arg2, ...]"},
            "keys": {"type": SoftListOrNothingType, "desc": "Keys to WATCH"},
            "watch_keys": {"type": SoftListOrNothingType, "desc": "Keys to watch before execute_transaction"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}

public class RedisTransactionResponseDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "queued": {"type": SoftBoolType, "desc": "Transaction started (MULTI)"},
            "results": {"type": SoftListOrNothingType, "desc": "Results from EXEC"},
            "discarded": {"type": SoftBoolType, "desc": "Transaction was discarded"},
            "watching": {"type": SoftBoolType, "desc": "Now watching keys"},
            "unwatched": {"type": SoftBoolType, "desc": "Keys unwatched"},
            "aborted": {"type": SoftBoolType, "desc": "Transaction was aborted (WATCH key modified)"},
            "reason": {"type": StringOrNothingType, "desc": "Reason for abort"},
        };
    }
    constructor() { map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type)), Fields.pairIterator(); }
}
}
