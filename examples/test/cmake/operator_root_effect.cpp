// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
#define _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/RuntimeConfig.h>
#include <cstdio>
#include <stdexcept>
#include <utility>

// The runner extracts the unchanged, hidden default implementation verbatim.
// SimpleQoreNode's vtable requires it when linking an internal-header consumer.
#include "operator-base-parse.inc"

namespace {
unsigned checks = 0;
unsigned live_nodes = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Base>
class Fixture : public Base {
public:
    template <typename... Args>
    explicit Fixture(Args&&... args) : Base(std::forward<Args>(args)...) {
        ++live_nodes;
    }
    ~Fixture() override {
        --live_nodes;
    }
    QoreString* getAsString(bool& del, int, ExceptionSink*) const override {
        del = true;
        return new QoreString("operator effect fixture");
    }
    int getAsString(QoreString& str, int, ExceptionSink*) const override {
        str.concat("operator effect fixture");
        return 0;
    }
    const char* getTypeName() const override {
        return "operator effect fixture";
    }
    QoreOperatorNode* copyBackground(ExceptionSink*) const override {
        throw std::logic_error("fixture does not support background copying");
    }
protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink*) const override {
        needs_deref = false;
        return QoreValue();
    }
    int parseInitImpl(QoreValue&, QoreParseContext&) override {
        throw std::logic_error("fixture does not support parsing");
    }
};

class IndirectLValue : public LValueOperatorNode {
public:
    using LValueOperatorNode::LValueOperatorNode;
    bool hasEffectAsRoot() const override {
        return false;  // The template must classify inheritance, not delegate to this override.
    }
};

// Exercise a real RTTI cross-cast: the template's T is QoreOperatorNode, but a
// distinct LValueOperatorNode subobject exists in the most-derived object.
template <typename Operator>
class CrossBase : public Operator, public LValueOperatorNode {
public:
    template <typename... Args>
    explicit CrossBase(Args&&... args) : Operator(std::forward<Args>(args)...), LValueOperatorNode(nullptr) {
    }
    bool hasEffect() const override {
        return Operator::hasEffect();
    }
    bool hasEffectAsRoot() const override {
        return Operator::hasEffectAsRoot();
    }
};

void check(const QoreOperatorNode& node, bool root_effect, bool child_effect) {
    require((dynamic_cast<const LValueOperatorNode*>(&node) != nullptr) == root_effect,
        "fixture hierarchy does not match the independent RTTI reference");
    require(node.hasEffect() == child_effect, "child effect classification changed");
    require(node.hasEffectAsRoot() == root_effect, "root effect differs from RTTI reference");
    require(node.hasEffectAsRoot() == root_effect, "repeated classification changed");
    require(node.needsReturnValue(), "effect query changed return-value ownership");
}

template <typename Op, typename Actual = Op, typename... Args>
void exercise(bool root_effect, bool child_effect, Args&&... args) {
    Fixture<Actual> value(nullptr, std::forward<Args>(args)...);
    Op& op = value;
    check(op, root_effect, child_effect);
    op.ignoreReturnValue();
    require(!op.needsReturnValue(), "return-value state did not change");
    require(op.hasEffectAsRoot() == root_effect, "ignored return value changed root classification");
}

QoreValue operand(unsigned kind) {
    switch (kind) {
        case 0: return QoreValue();
        case 1: return QoreValue(int64(-77));
        case 2: return QoreValue(new QoreStringNode("caf\xc3\xa9"));
        case 3: return QoreValue(new Fixture<LValueOperatorNode>(nullptr));
        default: throw std::logic_error("unknown operand kind");
    }
}

template <typename T>
void shapes(bool root_effect) {
    for (unsigned kind = 0; kind < 4; ++kind) {
        // A holder keeps ownership through constructor or assertion failures;
        // the node receives its own reference and releases it through its real destructor.
        ValueHolder input(operand(kind), nullptr);
        exercise<QoreSingleExpressionOperatorNode<T>>(root_effect, kind == 3, input->refSelf());
        exercise<QoreSingleValueExpressionOperatorNode<T>>(root_effect, kind == 3, input->refSelf());
        exercise<QoreBinaryOperatorNode<T>>(root_effect, kind == 3, input->refSelf(), QoreValue());
        exercise<QoreNOperatorNodeBase<3, T>>(root_effect, kind == 3, input->refSelf(),
            QoreSimpleValue(QoreValue()), QoreSimpleValue(QoreValue(int64(4))));
    }
}

void crossCasts() {
    for (unsigned kind = 0; kind < 4; ++kind) {
        ValueHolder input(operand(kind), nullptr);
        using Single = QoreSingleExpressionOperatorNode<>;
        using Value = QoreSingleValueExpressionOperatorNode<>;
        using Binary = QoreBinaryOperatorNode<>;
        using Nary = QoreNOperatorNodeBase<3>;
        exercise<Single, CrossBase<Single>>(true, kind == 3, input->refSelf());
        exercise<Value, CrossBase<Value>>(true, kind == 3, input->refSelf());
        exercise<Binary, CrossBase<Binary>>(true, kind == 3, input->refSelf(), QoreValue());
        exercise<Nary, CrossBase<Nary>>(true, kind == 3, input->refSelf(),
            QoreSimpleValue(QoreValue()), QoreSimpleValue(QoreValue(int64(4))));
    }
}
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        shapes<QoreOperatorNode>(false);
        shapes<LValueOperatorNode>(true);
        shapes<IndirectLValue>(true);
        crossCasts();
        require(live_nodes == 0, "fixture node lifetime leaked");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, e.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u operator effect checks\n", checks);
    }
    return status;
}
