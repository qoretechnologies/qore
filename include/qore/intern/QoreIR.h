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
#include <utility>
#include <vector>

#include <qore/QoreValue.h>

class QoreProgramLocation;
class LocalVar;
class Var;
class ForEachStatement;
class OnBlockExitStatement;
class DebugStatement;
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
    AddAssignAny,
    SubAssignInt,
    SubAssignAny,
    MulAssignInt,
    MulAssignAny,
    DivAssignInt,
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
    SpliceLValue,
    ExtractAny,
    RemoveAny,
    KeysAny,
    RegexMatchAny,
    RegexMatchBool,
    RegexExtractAny,
    RegexSubstAny,
    ExistsAny,
    ElementsAny,
    DotEvalAny,
    Foreach,
    OnBlockExit,
    ThreadExit,
    Debug,

    EqInt,
    EqAny,
    NeInt,
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

private:
    uint32_t next_value_id = 1;
};

#endif
