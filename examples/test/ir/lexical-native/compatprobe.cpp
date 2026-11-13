// Copyright (C) 2026 Qore Technologies, s.r.o.
#include <qore/Qore.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

// Native boundary fixture for #5831. The rendezvous stores synchronization state only, never a semantic policy.
static QoreNamespace* probe_ns = nullptr;

class Rendezvous {
public:
    bool wait(ExceptionSink* xsink) {
        std::unique_lock<std::mutex> lock(mutex);
        unsigned old_phase = phase;
        if (!failed && ++arrivals == 2) {
            arrivals = 0;
            ++phase;
            changed.notify_all();
            return true;
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!failed && old_phase == phase) {
            changed.wait_for(lock, std::chrono::milliseconds(50));
            if (qore_check_cancel(xsink, "compatibility test rendezvous")
                    || std::chrono::steady_clock::now() >= deadline) {
                failed = true;
                changed.notify_all();
            }
        }
        if (failed) {
            if (!*xsink) xsink->raiseException("COMPAT-RENDEZVOUS-ERROR", "both native callers did not arrive");
            return false;
        }
        return true;
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    unsigned arrivals = 0;
    unsigned phase = 0;
    bool failed = false;
};

static Rendezvous rendezvous;

static QoreValue rendezvous_bool(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    if (!rendezvous.wait(xsink)) return QoreValue();
    bool result = args->retrieveEntry(0).getAsBool();
    // Both reads finish while both native callers' contexts are still active. No Qore call changes either
    // context between the two rendezvous points.
    if (!rendezvous.wait(xsink)) return QoreValue();
    return result;
}

static QoreValue value_bool(const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    return args->retrieveEntry(0).getAsBool();
}

static QoreValue filtered_value(const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    return args->retrieveEntry(0).refSelf();
}

static QoreValue callback(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    const ResolvedCallReferenceNode* cb = args->retrieveEntry(0).get<const ResolvedCallReferenceNode>();
    ReferenceHolder<QoreListNode> cbargs(new QoreListNode(autoTypeInfo), xsink);
    cbargs->push(args->retrieveEntry(1).refSelf(), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return cb->invoke(*cbargs, xsink);
}

static QoreValue callback_state(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    const ResolvedCallReferenceNode* cb = args->retrieveEntry(0).get<const ResolvedCallReferenceNode>();
    ReferenceHolder<QoreListNode> cbargs(new QoreListNode(autoTypeInfo), xsink);
    cbargs->push(args->retrieveEntry(1).refSelf(), xsink);
    if (*xsink) {
        return QoreValue();
    }
    bool before = args->retrieveEntry(1).getAsBool();
    ExceptionSink child;
    ValueHolder result(cb->invoke(*cbargs, &child), xsink);
    bool caught = child.isException();
    if (caught) {
        child.clear();
    } else if (child) {
        xsink->assimilate(child);
        return QoreValue();
    }
    bool after = args->retrieveEntry(1).getAsBool();
    ReferenceHolder<QoreListNode> report(new QoreListNode(autoTypeInfo), xsink);
    for (bool value : {before, after, caught}) {
        report->push(value, xsink);
        if (*xsink) {
            return QoreValue();
        }
    }
    return report.release();
}

static QoreValue compare_values(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreValue left = args->retrieveEntry(0);
    QoreValue right = args->retrieveEntry(1);
    ReferenceHolder<QoreListNode> result(new QoreListNode(boolTypeInfo), xsink);
    bool comparisons[] = {left.isEqualSoft(right, xsink), !q_compare_soft(left, right, xsink),
        !compareSoft(left.getInternalNode(), right.getInternalNode(), xsink),
        left.getInternalNode()->is_equal_soft(right.getInternalNode(), xsink)};
    if (*xsink) return QoreValue();
    for (bool value : comparisons) {
        result->push(value, xsink);
        if (*xsink) return QoreValue();
    }
    return result.release();
}

// The public container APIs are also used inside native builtins entered from legacy AST code.
static QoreValue container_values(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreValue value = args->retrieveEntry(0);
    ReferenceHolder<QoreListNode> list(new QoreListNode(softBoolTypeInfo), xsink);
    ReferenceHolder<QoreListNode> input(new QoreListNode(autoTypeInfo), xsink);
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(softBoolTypeInfo), xsink);
    ReferenceHolder<QoreHashNode> source(new QoreHashNode(autoTypeInfo), xsink);
    if (list->push(value.refSelf(), xsink) || list->insert(value.refSelf(), xsink)
            || input->push(value.refSelf(), xsink) || list->merge(*input, xsink)
            || hash->setKeyValue("set", value.refSelf(), xsink)
            || source->setKeyValue("merge", value.refSelf(), xsink)) {
        return QoreValue();
    }
    hash->merge(*source, xsink);
    if (*xsink) return QoreValue();
    // Both public replacement overloads must filter under this native invocation's source policy.
    ValueHolder removed(list->splice(0, 1, value, xsink), xsink);
    if (*xsink) return QoreValue();
    removed = list->extract(1, 1, value, xsink);
    if (*xsink) return QoreValue();
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    if (result->push(list.release(), xsink) || result->push(hash.release(), xsink)) return QoreValue();
    return result.release();
}

// Assignment APIs are called from native module code with public headers only.
static QoreValue assignment_values(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreValue value = args->retrieveEntry(0);
    QoreObject* object = args->retrieveEntry(1).get<QoreObject>();
    const ReferenceNode* reference = args->retrieveEntry(2).get<const ReferenceNode>();
    const QoreHashNode* typed_input = args->retrieveEntry(3).get<const QoreHashNode>();
    ReferenceHolder<QoreHashNode> typed(typed_input->copy(), xsink);
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(softBoolTypeInfo), xsink);
    if (object->setMemberValue("value", object->getClass(), value.refSelf(), xsink)) return QoreValue();
    ValueHolder assigned(xsink);
    {
        QoreTypeSafeReferenceHelper target(reference, xsink);
        if (*xsink || target.assign(value.refSelf())) return QoreValue();
        assigned = target.getValue().refSelf();
    }
    HashAssignmentHelper target(**hash, "value");
    target.assign(value.refSelf(), xsink);
    if (*xsink || typed->setKeyValue("set", value.refSelf(), xsink)) return QoreValue();
    HashAssignmentHelper typed_target(**typed, "helper");
    typed_target.assign(value.refSelf(), xsink);
    if (*xsink) return QoreValue();
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    result->push(object->evalMember("value", xsink), xsink);
    result->push(assigned.release(), xsink);
    result->push(hash->getKeyValue("value").refSelf(), xsink);
    result->push(typed->getKeyValue("set").refSelf(), xsink);
    result->push(typed->getKeyValue("helper").refSelf(), xsink);
    return *xsink ? QoreValue() : QoreValue(result.release());
}

static QoreValue config_state(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    q_rt_flags_t original = rc.getRuntimeFlags();
    RuntimeConfig copy(rc);
    RuntimeConfig assigned;
    assigned = rc;
    bool before = args->retrieveEntry(0).getAsBool();
    rc.setRuntimeFlags(original ^ QCF_USES_EXTRA_ARGS);
    bool changed = rc.getRuntimeFlags() == (original ^ QCF_USES_EXTRA_ARGS);
    bool after = args->retrieveEntry(0).getAsBool();
    rc.setRuntimeFlags(original);
    // Detached snapshots are for inspection and never install their policy on this thread.
    copy.setRuntimeFlags(0);
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (bool value : {before, after, changed, !(original >> 48), assigned.getRuntimeFlags() == original,
            rc.getRuntimeFlags() == original}) {
        result->push(value, xsink);
        if (*xsink) return QoreValue();
    }
    return result.release();
}

static std::mutex native_results_mutex;
static std::vector<bool> native_results;

// A real external subclass: only installed public headers, and the pre-existing execValue source API.
class NativeBoolCallback : public ResolvedCallReferenceNode {
public:
    explicit NativeBoolCallback(bool record = false, bool fail = false) : record(record), fail(fail) {
    }

    QoreValue execValue(const QoreListNode* args, ExceptionSink* xsink) const override {
        ValueHolder value(new QoreStringNode("text"), xsink);
        bool result = value->getAsBool();
        if (record) {
            std::lock_guard<std::mutex> lock(native_results_mutex);
            native_results.push_back(result);
        }
        if (fail) {
            xsink->raiseException("NATIVE-CALLBACK-ERROR", "callback failure");
            return QoreValue();
        }
        return result;
    }

    QoreFunction* getFunction() const override {
        return nullptr;
    }

private:
    const bool record;
    const bool fail;
};

static QoreValue native_callback(const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    return new NativeBoolCallback(args && args->retrieveEntry(0).getAsBool(),
        args && args->retrieveEntry(1).getAsBool());
}

static QoreValue handler_results(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> result(new QoreListNode(boolTypeInfo), xsink);
    std::lock_guard<std::mutex> lock(native_results_mutex);
    for (bool value : native_results) {
        result->push(value, xsink);
        if (*xsink) {
            return QoreValue();
        }
    }
    native_results.clear();
    return result.release();
}

class NativeProbeData : public AbstractPrivateData {
public:
    NativeProbeData(bool body, bool filtered) : body(body), filtered(filtered) {}
    const bool body;
    const bool filtered;
};

static void probe_constructor(const QoreMethod& method, const void*, QoreObject* self,
        const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new NativeProbeData(args->retrieveEntry(0).getAsBool(),
        args->retrieveEntry(1).getAsBool()));
}

static void probe_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    data->deref(xsink);
}

static void probe_copy(const QoreClass& cls, const void*, QoreObject* self, QoreObject*,
        AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    if (static_cast<NativeProbeData*>(data)->filtered) {
        xsink->raiseException("NATIVE-COPY-ERROR", "native copy rejected before initializing its private data");
        return;
    }
    ValueHolder value(new QoreStringNode("text"), xsink);
    bool observed = value->getAsBool();
    self->setPrivate(cls.getID(), new NativeProbeData(observed, observed));
}

// A public native-originated copy has no Qore call-site policy, even inside a legacy builtin.
static QoreValue native_copy(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreObject* object = args->retrieveEntry(0).get<QoreObject>();
    return object->getClass()->execCopy(object, xsink);
}

static QoreValue native_copy_state(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreObject* object = args->retrieveEntry(0).get<QoreObject>();
    ValueHolder value(new QoreStringNode("text"), xsink);
    bool before = value->getAsBool();
    ExceptionSink child;
    ValueHolder copied(object->getClass()->execCopy(object, &child), xsink);
    bool caught = child.isException();
    if (caught) child.clear();
    else if (child) {
        xsink->assimilate(child);
        return QoreValue();
    }
    bool after = value->getAsBool();
    ReferenceHolder<QoreListNode> report(new QoreListNode(boolTypeInfo), xsink);
    for (bool item : {before, after, caught}) {
        if (report->push(item, xsink)) return QoreValue();
    }
    return report.release();
}

static QoreValue probe_observed(const QoreMethod&, const void*, QoreObject*, AbstractPrivateData* data,
        const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    auto* probe = static_cast<NativeProbeData*>(data);
    ReferenceHolder<QoreListNode> values(new QoreListNode(boolTypeInfo), xsink);
    values->push(probe->body, xsink);
    values->push(probe->filtered, xsink);
    return values.release();
}

static QoreValue probe_method(const QoreMethod&, const void*, QoreObject*, AbstractPrivateData*,
        const QoreListNode* args, RuntimeConfig&, ExceptionSink*) {
    return args->retrieveEntry(0).getAsBool();
}

static QoreValue probe_static(const QoreMethod&, const void*, const QoreListNode* args,
        RuntimeConfig&, ExceptionSink*) {
    return args->retrieveEntry(0).getAsBool();
}

static void init(QoreModuleInitContext&, ExceptionSink&) {
    // Registered below after creating the namespace.

    probe_ns = new QoreNamespace("CompatProbe");
    QoreClass* probe = new QoreClass("NativeProbe", "CompatProbe");
    probe->addConstructor(nullptr, probe_constructor, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        {autoTypeInfo, softBoolTypeInfo}, {}, {"value", "filtered"});
    probe->setDestructor(probe_destructor);
    probe->setCopy(nullptr, probe_copy);
    probe->addMethod(nullptr, "observed", probe_observed, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoListTypeInfo);
    probe->addMethod(nullptr, "value_bool", probe_method, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, {autoTypeInfo}, {}, {"value"});
    probe->addMethod(nullptr, "filter_bool", probe_method, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, {softBoolTypeInfo}, {}, {"value"});
    probe->addStaticMethod(nullptr, "static_bool", probe_static, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, {autoTypeInfo}, {}, {"value"});
    probe->addStaticMethod(nullptr, "static_filter", probe_static, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, {softBoolTypeInfo}, {}, {"value"});
    // overloads that take the argument by name, whose variant a named call with an argument of no type at parse time
    // resolves at run time by parameter name
    probe->addMethod(nullptr, "named_bool", probe_method, Public, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, {stringTypeInfo}, {}, {"value"});
    probe->addMethod(nullptr, "named_bool", probe_method, Public, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, {bigIntTypeInfo}, {}, {"value"});
    probe->addStaticMethod(nullptr, "named_static_bool", probe_static, Public, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, {stringTypeInfo}, {}, {"value"});
    probe->addStaticMethod(nullptr, "named_static_bool", probe_static, Public, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, {bigIntTypeInfo}, {}, {"value"});
    probe_ns->addSystemClass(probe);
    probe_ns->addBuiltinVariant("native_copy", native_copy, QCF_NO_FLAGS, QDOM_DEFAULT,
        objectTypeInfo, 1, objectTypeInfo, QORE_PARAM_NO_ARG, "object");
    probe_ns->addBuiltinVariant("native_copy_state", native_copy_state, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 1, objectTypeInfo, QORE_PARAM_NO_ARG, "object");
    probe_ns->addBuiltinVariant("compare_values", compare_values, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 2, autoTypeInfo, QORE_PARAM_NO_ARG, "left", autoTypeInfo, QORE_PARAM_NO_ARG, "right");
    probe_ns->addBuiltinVariant("container_values", container_values, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("assignment_values", assignment_values, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 4, autoTypeInfo, QORE_PARAM_NO_ARG, "value", objectTypeInfo, QORE_PARAM_NO_ARG, "object",
        referenceTypeInfo, QORE_PARAM_NO_ARG, "reference", autoTypeInfo, QORE_PARAM_NO_ARG, "typed_hash");
    probe_ns->addBuiltinVariant("config_state", config_state, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("native_callback", native_callback, QCF_NO_FLAGS, QDOM_DEFAULT,
        codeTypeInfo, 2, boolTypeInfo, false, "record", boolTypeInfo, false, "fail");
    probe_ns->addBuiltinVariant("handler_results", handler_results, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 0);
    probe_ns->addBuiltinVariant("value_bool", value_bool, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("named_bool", value_bool, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("named_bool", value_bool, QCF_NAMED_ARGS, QDOM_DEFAULT,
        boolTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("callback", callback, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoTypeInfo, 2, codeTypeInfo, QORE_PARAM_NO_ARG, "callback", autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("soft_bool", filtered_value, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, 1, softBoolTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("soft_int", filtered_value, QCF_NO_FLAGS, QDOM_DEFAULT,
        bigIntTypeInfo, 1, softBigIntTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("callback_state", callback_state, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoListTypeInfo, 2, codeTypeInfo, QORE_PARAM_NO_ARG, "callback", autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    probe_ns->addBuiltinVariant("rendezvous_bool", rendezvous_bool, QCF_NO_FLAGS, QDOM_DEFAULT,
        boolTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
}

static void ns_init(QoreNamespace*, QoreNamespace* qns, ExceptionSink&) {
    qns->addNamespace(probe_ns->copy());
}

static void cleanup() {
    delete probe_ns;
    probe_ns = nullptr;
}

extern "C" DLLEXPORT void compatprobe_qore_module_desc(QoreModuleInfo& info) {
    info.name = "compatprobe";
    info.version = "1.0";
    info.desc = "Lexical compatibility native callback probe";
    info.author = "Qore Technologies, s.r.o.";
    info.url = "https://qore.org";
    info.api_major = QORE_MODULE_API_MAJOR;
    info.api_minor = QORE_MODULE_API_MINOR;
    info.init = init;
    info.ns_init = ns_init;
    info.del = cleanup;
    info.license = QL_MIT;
    info.license_str = "MIT";
}
