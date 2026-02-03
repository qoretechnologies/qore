/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIR.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREIR_H
#define _QORE_INTERN_QOREIR_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <qore/QoreValue.h>

class QoreProgramLocation;
class LocalVar;
class Var;
class ForEachStatement;
class OnBlockExitStatement;
class DebugStatement;
class AssertStatement;
class ContextStatement;
class SummarizeStatement;
class QoreTypeInfo;

enum class QoreIROpcode : uint16_t {
    ConstInt,
    ConstFloat,
    ConstBool,
    ConstNothing,
    ConstNull,
    ConstString,
    ConstDate,
    MakeList,
    MakeHash,

    AddInt,
    AddFloat,
    AddAny,
    SubInt,
    SubFloat,
    SubAny,
    MulInt,
    MulFloat,
    MulAny,
    DivInt,
    DivFloat,
    DivAny,
    ModInt,
    ModAny,
    AndInt,
    AndAny,
    OrInt,
    OrAny,
    XorInt,
    XorAny,
    ShlInt,
    ShlAny,
    ShrInt,
    ShrAny,
    ShlAssignInt,
    ShlAssignAny,
    ShrAssignInt,
    ShrAssignAny,
    AddAssignInt,
    AddAssignFloat,
    AddAssignAny,
    SubAssignInt,
    SubAssignFloat,
    SubAssignAny,
    MulAssignInt,
    MulAssignFloat,
    MulAssignAny,
    DivAssignInt,
    DivAssignFloat,
    DivAssignAny,
    ModAssignInt,
    ModAssignAny,
    AndAssignInt,
    AndAssignAny,
    OrAssignInt,
    OrAssignAny,
    XorAssignInt,
    XorAssignAny,
    LoadLValue,
    StoreLValue,
    PreIncLValue,
    PreDecLValue,
    PostIncLValue,
    PostDecLValue,
    AddAssignLValue,
    SubAssignLValue,
    MulAssignLValue,
    DivAssignLValue,
    ModAssignLValue,
    AndAssignLValue,
    OrAssignLValue,
    XorAssignLValue,
    ShlAssignLValue,
    ShrAssignLValue,
    ShiftLValue,
    UnshiftLValue,
    PopAny,
    PushAny,
    SpliceLValue,
    ExtractAny,
    ExtractList,
    ExtractString,
    ExtractBinary,
    RemoveAny,
    RemoveList,
    RemoveHash,
    RemoveObject,
    RemoveString,
    RemoveBinary,
    KeysAny,
    KeysList,
    KeysHash,
    RegexMatchAny,
    RegexMatchBool,
    RegexNMatchBool,
    RegexExtractAny,
    RegexExtractList,
    RegexSubstAny,
    RegexSubstString,
    InstanceOfBool,
    TrimAny,
    TrimString,
    ChompAny,
    ChompString,
    TransliterateAny,
    TransliterateString,
    BackgroundInt,
    ListAssignAny,
    ExistsAny,
    ExistsBool,
    ElementsAny,
    ElementsInt,
    DotEvalAny,
    DotEvalInt,
    DotEvalFloat,
    DotEvalString,
    DotEvalDate,
    DotEvalList,
    DotEvalHash,
    DotEvalObject,
    MapSelectList,
    HashMap,
    HashMapSelect,
    Foreach,
    OnBlockExit,
    ThreadExit,
    Debug,
    Assert,
    Context,
    Summarize,

    EqInt,
    EqFloat,
    EqAny,
    NeInt,
    NeFloat,
    NeAny,
    EqHard,
    NeHard,
    LtInt,
    LtFloat,
    LtAny,
    LeInt,
    LeFloat,
    LeAny,
    GtInt,
    GtFloat,
    GtAny,
    GeInt,
    GeFloat,
    GeAny,
    CmpInt,
    CmpFloat,
    CmpAny,

    ToBool,
    Not,
    IsNullOrNothing,
    Phi,
    UnaryPlusAny,
    UnaryMinusInt,
    UnaryMinusFloat,
    UnaryMinusAny,
    FoldlAny,
    FoldlInt,
    FoldlFloat,
    FoldrAny,
    FoldrInt,
    FoldrFloat,
    MapAny,
    MapInt,
    MapFloat,
    SelectAny,
    SelectInt,
    SelectFloat,
    MapSelectAny,
    HashMapAny,
    HashMapSelectAny,
    RangeAny,
    RangeInt,
    RangeFloat,
    RangeDate,
    RangeSliceAny,
    RangeSliceInt,
    RangeSliceFloat,
    CastAny,
    CastList,
    CastHash,
    CastObject,
    CastEnum,

    Br,
    BrIf,
    Return,
    ReturnNothing,

    LoadLocal,
    StoreLocal,
    LoadArg,
    LoadClosure,
    StoreClosure,
    LoadGlobal,
    StoreGlobal,
    LoadThreadLocal,
    StoreThreadLocal,

    Call,
    CallIndirect,
    CallMethod,
    CallStatic,
    Invoke,

    GuardInt,
    GuardFloat,
    GuardType,
    GuardNotNothing,

    LandingPad,
    CatchException,
    Rethrow,
    Throw,
    InvokeSimError,

    Incref,
    Decref,
    DecrefNoThrow,
};

//! Returns true if the opcode is a unary computation op (used by Invoke dispatch)
inline bool isUnaryInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a binary computation op (used by Invoke dispatch)
inline bool isBinaryInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignFloat:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate:
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a call-type op (used by Invoke dispatch)
inline bool isCallInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a non-subst regex op (used by Invoke dispatch)
inline bool isRegexInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a DotEval-type op (method call on expression result)
inline bool isDotEvalInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
            return true;
        default:
            return false;
    }
}

struct QoreIRValue {
    uint32_t id = 0;

    explicit QoreIRValue(uint32_t n_id = 0) : id(n_id) {
    }

    bool isValid() const {
        return id != 0;
    }
};

struct QoreIRConstant {
    enum class Kind {
        Int,
        Float,
        Bool,
    Nothing,
    Null,
        String,
        Date,
    };

    Kind kind = Kind::Nothing;
    int64_t int_value = 0;
    double float_value = 0.0;
    bool bool_value = false;
    std::string string_value;
    int64_t date_microseconds = 0;
    bool date_is_relative = false;
};

class QoreIRBasicBlock;
struct QoreIRPhiIncoming;

class QoreIRInstruction {
public:
    explicit QoreIRInstruction(QoreIROpcode op) : opcode(op) {
    }

    virtual ~QoreIRInstruction() = default;

    QoreIROpcode opcode;
    const QoreProgramLocation* loc = nullptr;
    QoreIRValue result{};
    std::vector<QoreIRValue> operands;
    QoreIRBasicBlock* exception_target = nullptr;
};

class QoreIRConstInstruction : public QoreIRInstruction {
public:
    QoreIRConstInstruction() : QoreIRInstruction(QoreIROpcode::ConstNothing) {
    }

    QoreIRConstant constant;
};

class QoreIRBranchInstruction : public QoreIRInstruction {
public:
    QoreIRBranchInstruction() : QoreIRInstruction(QoreIROpcode::Br) {
    }

    QoreIRBasicBlock* target = nullptr;
};

class QoreIRBranchIfInstruction : public QoreIRInstruction {
public:
    QoreIRBranchIfInstruction() : QoreIRInstruction(QoreIROpcode::BrIf) {
    }

    QoreIRValue condition{};
    QoreIRBasicBlock* true_target = nullptr;
    QoreIRBasicBlock* false_target = nullptr;
};

struct QoreIRPhiIncoming {
    QoreIRValue value{};
    QoreIRBasicBlock* block = nullptr;
};

class QoreIRPhiInstruction : public QoreIRInstruction {
public:
    QoreIRPhiInstruction() : QoreIRInstruction(QoreIROpcode::Phi) {
    }

    std::vector<QoreIRPhiIncoming> incoming;
};

class QoreIRGuardInstruction : public QoreIRInstruction {
public:
    explicit QoreIRGuardInstruction(QoreIROpcode op) : QoreIRInstruction(op) {
    }

    QoreIRBasicBlock* deopt_target = nullptr;
    const QoreTypeInfo* type_info = nullptr;
};

class QoreIRReturnInstruction : public QoreIRInstruction {
public:
    QoreIRReturnInstruction() : QoreIRInstruction(QoreIROpcode::ReturnNothing) {
    }

    bool has_value = false;
    QoreIRValue value{};
};

class QoreIRThrowInstruction : public QoreIRInstruction {
public:
    QoreIRThrowInstruction() : QoreIRInstruction(QoreIROpcode::Throw) {
    }

    QoreIRBasicBlock* exception_target = nullptr;
};

class QoreIRLocalInstruction : public QoreIRInstruction {
public:
    QoreIRLocalInstruction(QoreIROpcode op, LocalVar* n_local) : QoreIRInstruction(op), local(n_local) {
    }

    LocalVar* local = nullptr;
};

class QoreIRVarInstruction : public QoreIRInstruction {
public:
    QoreIRVarInstruction(QoreIROpcode op, Var* n_var) : QoreIRInstruction(op), var(n_var) {
    }

    Var* var = nullptr;
};

class QoreIRLValueInstruction : public QoreIRInstruction {
public:
    QoreIRLValueInstruction(QoreIROpcode op, const QoreValue& n_lvalue)
            : QoreIRInstruction(op), lvalue(n_lvalue) {
        lvalue.ref();
    }

    ~QoreIRLValueInstruction() override {
        lvalue.discard(nullptr);
    }

    QoreValue lvalue;
};

class QoreIRExprInstruction : public QoreIRInstruction {
public:
    QoreIRExprInstruction(QoreIROpcode op, const QoreValue& n_expr) : QoreIRInstruction(op), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRExprInstruction() override {
        expr.discard(nullptr);
    }

    QoreValue expr;
};

class QoreIRForeachInstruction : public QoreIRInstruction {
public:
    explicit QoreIRForeachInstruction(const ForEachStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Foreach), stmt(n_stmt) {
    }

    const ForEachStatement* stmt = nullptr;
};

class QoreIROnBlockExitInstruction : public QoreIRInstruction {
public:
    explicit QoreIROnBlockExitInstruction(const OnBlockExitStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::OnBlockExit), stmt(n_stmt) {
    }

    const OnBlockExitStatement* stmt = nullptr;
};

class QoreIRDebugInstruction : public QoreIRInstruction {
public:
    explicit QoreIRDebugInstruction(const DebugStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Debug), stmt(n_stmt) {
    }

    const DebugStatement* stmt = nullptr;
};

class QoreIRAssertInstruction : public QoreIRInstruction {
public:
    explicit QoreIRAssertInstruction(const AssertStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Assert), stmt(n_stmt) {
    }

    const AssertStatement* stmt = nullptr;
};

class QoreIRContextInstruction : public QoreIRInstruction {
public:
    explicit QoreIRContextInstruction(const ContextStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Context), stmt(n_stmt) {
    }

    const ContextStatement* stmt = nullptr;
};

class QoreIRSummarizeInstruction : public QoreIRInstruction {
public:
    explicit QoreIRSummarizeInstruction(const SummarizeStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Summarize), stmt(n_stmt) {
    }

    const SummarizeStatement* stmt = nullptr;
};

class QoreIRInvokeInstruction : public QoreIRInstruction {
public:
    QoreIRInvokeInstruction(const QoreValue& n_expr, QoreIRBasicBlock* n_normal, QoreIRBasicBlock* n_exception)
            : QoreIRInstruction(QoreIROpcode::Invoke),
            expr(n_expr),
            normal_target(n_normal),
            exception_target(n_exception) {
        expr.ref();
    }

    ~QoreIRInvokeInstruction() override {
        expr.discard(nullptr);
    }

    QoreValue expr;
    QoreIROpcode invoke_opcode = QoreIROpcode::Invoke;
    QoreIRBasicBlock* normal_target = nullptr;
    QoreIRBasicBlock* exception_target = nullptr;
};

class QoreIRBasicBlock {
public:
    explicit QoreIRBasicBlock(std::string n_name) : name(std::move(n_name)) {
    }

    template <typename T, typename... Args>
    T* appendInstruction(Args&&... args) {
        auto inst = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = inst.get();
        instructions.push_back(std::move(inst));
        return ptr;
    }

    std::string name;
    std::vector<std::unique_ptr<QoreIRInstruction>> instructions;
};

class QoreIRFunction {
public:
    explicit QoreIRFunction(std::string n_name) : name(std::move(n_name)) {
    }

    QoreIRBasicBlock* createBlock(const std::string& block_name) {
        auto block = std::make_unique<QoreIRBasicBlock>(block_name);
        QoreIRBasicBlock* ptr = block.get();
        blocks.push_back(std::move(block));
        return ptr;
    }

    QoreIRValue createValue() {
        return QoreIRValue(next_value_id++);
    }

    std::string name;
    std::vector<std::unique_ptr<QoreIRBasicBlock>> blocks;

    // Set of LocalVar* pointers that are already instantiated by the caller
    // (tiered compilation: params from setupCall(), argvid/selfid from evalTiered(),
    // all body locals from the statement tree).  The JIT must not
    // re-instantiate/uninstantiate these.
    std::unordered_set<const void*> pre_instantiated_locals;

    // All body locals from the statement tree (top-level + all nested blocks
    // from fully-lowered statements: if/for/while/try/switch).
    // Used by evalTiered() to instantiate/uninstantiate all locals before/after
    // IR or JIT execution, so that AST Invoke callbacks can find them on the
    // thread-local variable stack.
    std::vector<LocalVar*> all_body_locals;

private:
    uint32_t next_value_id = 1;
};

#endif
