/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file pluginorder-module.cpp test plugin module for the identity of plugin operations in AOT code (qore#6136)

    One source built twice, as the modules \c pluginordera and \c pluginorderb (see the CMakeLists.txt of the
    directory above); neither is installed.  Each registers the class <tt>PLUGINORDER_NS::Value</tt> as a plugin type
    and a subscript operation <tt>Value[int]</tt> that returns the key plus the module's offset (1000 for
    \c pluginordera, 2000 for \c pluginorderb), so that a call that dispatches to the operation of the other module
    gives another result; its plugin-local id is 1, after a unary operation <tt>~Value</tt> (local id 0) that returns
    the value.  Process-global plugin operation ids are assigned in the order in which plugin modules are
    loaded; plugin-order-identity.qtest loads the modules in different orders at compile time and at run time.

    For the tests of a mismatch, the environment variable \c PLUGINORDER_TEST_VARIANT can name the module, followed by
    a colon and a variant, when the module is loaded:
    - <tt>signature</tt>: the operation returns \c auto instead of \c int, which changes its signature
    - <tt>missing</tt>: the module registers only the unary operation, so the subscript operation that compiled code
      names (local id 1) is missing

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include <qore/QorePluginType.h>

#include <cstdlib>
#include <cstring>
#include <string>

#define PLUGINORDER_DESC_SYMBOL2(id) id ## _qore_module_desc
#define PLUGINORDER_DESC_SYMBOL(id) PLUGINORDER_DESC_SYMBOL2(id)

namespace {
//! the namespace of the module
QoreNamespace* module_ns = nullptr;
//! the class of the plugin type
QoreClass* value_class = nullptr;

QoreValue valueFromBits(uint64_t bits) {
    QoreSimpleValue value;
    value.setRawBits(bits);
    return QoreValue(value);
}

uint64_t bitsFromValue(const QoreValue& value) {
    return value.rawBits();
}

void valueIncref(uint64_t bits) noexcept {
    valueFromBits(bits).ref();
}

void valueDecref(uint64_t bits) noexcept {
    QoreValue value = valueFromBits(bits);
    value.discard(nullptr);
}

uint64_t valueClone(uint64_t bits, ExceptionSink*) {
    valueIncref(bits);
    return bits;
}

bool valueEqual(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink*) {
    return lhs_bits == rhs_bits;
}

int64_t valueHash(uint64_t bits, ExceptionSink*) {
    return static_cast<int64_t>(bits);
}

int valueSerialize(uint64_t, QorePluginByteWriteCallback, void*, ExceptionSink* xsink) {
    xsink->raiseException("PLUGINORDER-ERROR", "values cannot be serialized");
    return -1;
}

uint64_t valueDeserialize(QorePluginByteReadCallback, uint32_t, void*, ExceptionSink* xsink) {
    xsink->raiseException("PLUGINORDER-ERROR", "values cannot be deserialized");
    return 0;
}

//! Value[int]: returns the key plus the module's offset
uint64_t valueSubscript(uint64_t container_bits, uint64_t key_bits, ExceptionSink* xsink) {
    QoreValue container = valueFromBits(container_bits);
    if (container.getType() != NT_OBJECT || container.get<const QoreObject>()->getClass() != value_class) {
        xsink->raiseException("PLUGINORDER-ERROR", "%s::Value subscript called with %s", PLUGINORDER_NS,
            container.getTypeName());
        return bitsFromValue(QoreValue());
    }
    return bitsFromValue(QoreValue(valueFromBits(key_bits).getAsBigInt() + PLUGINORDER_OFFSET));
}

//! ~Value: returns the value
uint64_t valueNot(uint64_t value_bits, ExceptionSink*) {
    valueIncref(value_bits);
    return value_bits;
}

void value_constructor(const QoreMethod&, const void*, QoreObject*, const QoreListNode*, RuntimeConfig&,
        ExceptionSink*) {
}

//! returns the test variant of the module (see the file's description), or an empty string
std::string getVariant() {
    const char* env = getenv("PLUGINORDER_TEST_VARIANT");
    if (!env) {
        return std::string();
    }
    size_t len = strlen(PLUGINORDER_NAME);
    if (strncmp(env, PLUGINORDER_NAME, len) || env[len] != ':') {
        return std::string();
    }
    return std::string(env + len + 1);
}
}

static void pluginorder_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    module_ns = new QoreNamespace(PLUGINORDER_NS);
    // QoreClass has a protected destructor; the namespace takes ownership
    value_class = new QoreClass("Value", "Qore::" PLUGINORDER_NS "::Value");
    value_class->addConstructor(nullptr, value_constructor);
    module_ns->addSystemClass(value_class);

    std::string variant = getVariant();

    QorePluginTypeDescriptor type = {};
    type.local_type_id = 0;
    type.type_name = PLUGINORDER_NS "::Value";
    type.type_info = value_class->getTypeInfo();
    type.value_ops.incref = valueIncref;
    type.value_ops.decref = valueDecref;
    type.value_ops.clone = valueClone;
    type.value_ops.equal = valueEqual;
    type.value_ops.hash = valueHash;
    type.value_ops.cleanup_slot = valueDecref;
    type.serialize = valueSerialize;
    type.deserialize = valueDeserialize;
    type.serializer_format_version = 1;
    type.baseline_qdom_domains = QDOM_DEFAULT;

    QorePluginOperation ops[2] = {};
    ops[0].local_id = 0;
    ops[0].operation_name = "bit_not";
    ops[0].signature.arity = 1;
    ops[0].signature.primary_type = value_class->getTypeInfo();
    ops[0].signature.return_type = value_class->getTypeInfo();
    ops[0].signature.access = QorePluginValueAccess::ReadOnly;
    ops[0].signature.result_alias = QorePluginResultAlias::ReturnsLhs;
    ops[0].signature.helper_abi = QorePluginHelperAbi::UnaryValue;
    ops[0].info.is_pure_modulo_xsink = true;
    ops[0].info.type_promotion_kind = QorePluginOpcodeTypePromotion::Exact;
    ops[0].runtime_helper = reinterpret_cast<void (*)()>(valueNot);
    ops[0].qdom_domains = QDOM_DEFAULT;

    ops[1].local_id = 1;
    ops[1].operation_name = "subscript";
    ops[1].signature.arity = 2;
    ops[1].signature.primary_type = value_class->getTypeInfo();
    ops[1].signature.secondary_type = bigIntTypeInfo;
    ops[1].signature.return_type = variant == "signature" ? autoTypeInfo : bigIntTypeInfo;
    ops[1].signature.access = QorePluginValueAccess::ReadOnly;
    ops[1].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[1].signature.helper_abi = QorePluginHelperAbi::SubscriptValue;
    ops[1].info.may_throw_exception = true;
    ops[1].info.is_pure_modulo_xsink = true;
    ops[1].info.type_promotion_kind = QorePluginOpcodeTypePromotion::Exact;
    ops[1].info.cost_class = 1;
    ops[1].runtime_helper = reinterpret_cast<void (*)()>(valueSubscript);
    ops[1].qdom_domains = QDOM_DEFAULT;

    QorePluginTypeRegistration reg = {};
    reg.module_name = PLUGINORDER_NAME;
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = &type;
    reg.num_types = 1;
    reg.operations = ops;
    reg.num_operations = variant == "missing" ? 1 : 2;

    QorePluginRegistrationContextV1 plugin_ctx = {};
    plugin_ctx.struct_size = sizeof(plugin_ctx);
    plugin_ctx.module_path = ctx.path.c_str();
    plugin_ctx.module_handle = ctx.plugin_module_handle;
    qore_register_plugin_types_v1(&plugin_ctx, &reg, &xsink);
}

static void pluginorder_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(module_ns->copy());
}

static void pluginorder_module_delete() {
    delete module_ns;
    module_ns = nullptr;
    value_class = nullptr;
}

extern "C" DLLEXPORT void PLUGINORDER_DESC_SYMBOL(PLUGINORDER_ID)(QoreModuleInfo& mod_info) {
    mod_info.name = PLUGINORDER_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test plugin module for the identity of plugin operations in AOT code";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = pluginorder_module_init;
    mod_info.ns_init = pluginorder_module_ns_init;
    mod_info.del = pluginorder_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
