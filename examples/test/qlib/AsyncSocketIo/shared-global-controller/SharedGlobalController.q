# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The process-wide AsyncSocketIoController wrapper is marked as shared by every Program, so that sandboxed code that
# obtains it cannot change its state or use the logger that trusted code set on it, also when a sandboxed Program is
# the first to load the module (issue #7666); see shared-global-controller.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Logger

# counts the messages that reach the host's logger
class CountingAppender inherits LoggerAppenderNull {
    public {
        int count = 0;
    }

    private auto serializeImpl(LoggerEvent event) {
        return event.getMessage();
    }

    processEventImpl(int type, auto params) {
        if (type == LoggerAppender::EVENT_LOG) {
            ++count;
        }
    }
}

# the code of a tenant (a sandboxed Program), which is the first to load the module
const TenantCode = "
%modern
%requires AsyncSocketIo
%requires Logger
%requires reflection

# the logger that the tenant tries to give the process-wide wrapper
Logger tenant_logger(\"tenant\");

Logger sub get_logger() {
    return tenant_logger;
}

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub try_use(code c) {
    try {
        c();
        return \"no exception\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# calls the setLogger() method that the wrapper inherits from LoggerWrapper, not its override
sub base_set_logger(AsyncSocketIoController c, Logger l) {
    cast<Reflection::NormalMethod>(Reflection::Class::forName(\"Logger::LoggerWrapper\").getMethod(
        \"setLogger\")).call(c, l);
}

# the changes of the wrapper that a tenant can attempt, by path
hash<string, code> sub changes() {
    return {
        \"its own setLogger()\": sub (AsyncSocketIoController c) { c.setLogger(tenant_logger); },
        \"the inherited LoggerWrapper::setLogger() through Reflection\": sub (AsyncSocketIoController c) {
            base_set_logger(c, tenant_logger);
        },
        \"Reflection NormalMember::setValue() of the private member\": sub (AsyncSocketIoController c) {
            Reflection::Class cls = Reflection::Class::forName(\"Logger::LoggerWrapper\");
            cast<Reflection::NormalMember>(cls.getMember(\"logger\")).setValue(c, tenant_logger);
        },
        \"delete\": sub (AsyncSocketIoController c) { delete c; },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, AsyncSocketIoController c) {
    return try_change(sub () { changes(){name}(c); });
}

# uses the logger that trusted code set on the wrapper: its level and its messages
string sub use_logger(AsyncSocketIoController c) {
    # the level is state of the shared wrapper: the shared-object rule refuses it (qore#6843, before the owner rule
    # of qore#6842)
    string level = try_use(sub () { c.setLevel(\"ALL\"); });
    # the wrapper has no logger for the tenant, so its messages reach no logger
    string logging = try_use(sub () {
        c.info(\"a message of the tenant\");
        c.log(\"ERROR\", \"another message of the tenant\");
    });
    return sprintf(\"setLevel(): %s, info() and log(): %s, info enabled: %y\", level, logging, c.isInfoEnabled());
}

# uses the logger through a copy of the wrapper, which is not shared and has no logger; the copy keeps the provenance
# of the wrapper (qore#7033), so it is not the tenant's either, and the tenant cannot change its level or logger
string sub use_copy(AsyncSocketIoController c, Logger host_logger) {
    AsyncSocketIoController copy;
    string rv = try_use(sub () { copy = c.copy(); });
    if (!copy) {
        return rv;
    }
    # setLogger() of the class refuses first, as the logger of any wrapper is also the logger of the process-wide
    # controller; the inherited LoggerWrapper::setLogger() refuses as the copy is not the tenant's
    return sprintf(\"%s, copy shared: %y, copy owned: %y, the copy has the host's logger: %y, setLevel() on the \"
        \"copy: %s, setLogger() on the copy: %s, LoggerWrapper::setLogger() on the copy: %s, info() through the \"
        \"copy: %s\", rv, SandboxManager::isSharedObject(copy), SandboxManager::isObjectOwnedByCaller(copy),
        copy.isLogger(host_logger), try_use(sub () { copy.setLevel(\"ALL\"); }),
        try_use(sub () { copy.setLogger(tenant_logger); }), try_use(sub () { base_set_logger(copy, tenant_logger); }),
        try_use(sub () { copy.info(\"a message of the tenant through a copy\"); }));
}

# asks whether the wrapper uses a logger
bool sub is_logger(AsyncSocketIoController c, Logger l) {
    return c.isLogger(l);
}

# calls code of the host
sub call_host(code c) {
    c();
}

# the tenant's own wrapper is not shared
string sub own_wrapper() {
    AsyncSocketIoController c = getGlobalAsyncIoController();
    base_set_logger(c, tenant_logger);
    return sprintf(\"shared: %y, own logger: %y\", SandboxManager::isSharedObject(c), c.isLogger(tenant_logger));
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");
# creates the tenant's logger
tenant.run();

# trusted host code that gets the process-wide wrapper after the tenant loaded the module
const HostCode = "
%modern
%requires AsyncSocketIo
object sub global_controller() {
    return getGlobalAsyncIoController();
}
";
Program host(PO_NEW_STYLE | PO_REQUIRE_TYPES);
host.parse(HostCode, "host");

object ctrl = host.callFunction("global_controller");
object tenant_logger = tenant.callFunction("get_logger");
printf("the process-wide wrapper is shared: %y\n", SandboxManager::isSharedObject(ctrl));

foreach string name in (tenant.callFunction("change_names")) {
    string rv;
    try {
        rv = tenant.callFunction("try_one", name, ctrl);
    } catch (hash<ExceptionInfo> ex) {
        rv = ex.err;
    }
    string state;
    try {
        state = sprintf("the tenant's logger is set: %y", ctrl.isLogger(tenant_logger));
        # trusted code restores the state for the next change
        ctrl.setLogger();
    } catch (hash<ExceptionInfo> ex) {
        state = ex.err;
    }
    printf("%s: %s, %s\n", name, rv, state);
}

# the host's logger, which trusted code sets on the process-wide wrapper
CountingAppender appender();
appender.open();
Logger host_logger("host", LoggerLevel::getLevelWarn());
host_logger.addAppender(appender);
ctrl.setLogger(host_logger);
printf("the tenant uses the host's logger: %s; host level: %s, messages: %d\n", tenant.callFunction("use_logger", ctrl),
    host_logger.getLevel().getStr(), appender.count);
printf("the tenant uses the host's logger through a copy: %s; host level: %s, messages: %d\n",
    tenant.callFunction("use_copy", ctrl, host_logger), host_logger.getLevel().getStr(), appender.count);
printf("the tenant calls isLogger() with the host's logger: %y\n",
    tenant.callFunction("is_logger", ctrl, host_logger));
# trusted code that acts for the tenant: a callback of the host that the tenant calls
tenant.callFunction("call_host", sub () { ctrl.warn("a warning of the host for the tenant"); });
printf("the host's code logs for the tenant: messages: %d\n", appender.count);
ctrl.info("a message of the host");
ctrl.warn("a warning of the host");
printf("the host logs through the wrapper: messages: %d, uses its logger: %y\n", appender.count,
    ctrl.isLogger(host_logger));
ctrl.setLogger();

printf("the tenant's own wrapper: %s\n", tenant.callFunction("own_wrapper"));

# trusted code still sets the logger of the process-wide wrapper
printf("trusted code sets and clears the logger: %s\n", (string sub () {
    try {
        ctrl.setLogger(tenant_logger);
        bool set = ctrl.isLogger(tenant_logger);
        ctrl.setLogger();
        return sprintf("set: %y, cleared: %y", set, !ctrl.isLogger(tenant_logger));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
