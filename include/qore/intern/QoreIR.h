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

#include <algorithm>
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
class CaseNodeRegex;
class QoreTypeInfo;
class QoreMethod;
class QoreClass;
class QoreVarInfo;
class AbstractQoreFunctionVariant;
class QoreClosureParseNode;
class RuntimeConstantRefNode;
class ParseReferenceNode;
class NewHashDeclNode;
class NewComplexHashNode;
class NewComplexListNode;
class VarRefNewObjectNode;

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
    CreateEmptyList,    // Create empty list for functional operator results
    CreateSizedList,    // Create list with pre-allocated capacity (for typed map loops)
    ListAppend,         // Append value to list (for map/select loops)
    ListSize,           // Get list element count (returns native i64)
    ListGetInt,         // Get int element from list by index (returns native i64)
    ListGetFloat,       // Get float element from list by index (returns native double)
    ListGetValue,       // Get any element from list by index (returns nanboxed QoreValue)
    ListSetInt,         // Set int element in list by index (for pre-sized typed map output)
    ListSetFloat,       // Set float element in list by index
    ListSetValue,       // Set any element in list by index

    AddInt,
    AddFloat,
    AddAny,
    AddString,                          // Typed string concatenation (no runtime type checks)
    StringConcat,                       // Multi-string concatenation (a + b + c + d in one pass)
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
    IteratorCreate,     // Create iterator from list/iterable
    IteratorNext,       // Advance iterator, branch if done, store value
    OnBlockExit,
    ScopeEnter,
    ScopeExit,
    ThreadExit,
    Debug,
    Assert,
    Context,
    Summarize,

    EqInt,
    EqFloat,
    EqString,
    EqAny,
    NeInt,
    NeFloat,
    NeString,
    NeAny,
    EqHard,
    NeHard,
    LtInt,
    LtFloat,
    LtString,
    LtAny,
    LeInt,
    LeFloat,
    LeString,
    LeAny,
    GtInt,
    GtFloat,
    GtString,
    GtAny,
    GeInt,
    GeFloat,
    GeString,
    GeAny,
    CmpInt,
    CmpFloat,
    CmpString,
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
    // Optimized fold operations (native LLVM loops)
    FoldlSumInt,        // foldl $1 + $2, list, init
    FoldlSumFloat,
    FoldlProdInt,       // foldl $1 * $2, list, init
    FoldlProdFloat,
    FoldlDiffInt,       // foldl $1 - $2, list, init
    FoldlDiffFloat,
    FoldlMinInt,        // foldl min($1, $2), list, init
    FoldlMinFloat,
    FoldlMaxInt,        // foldl max($1, $2), list, init
    FoldlMaxFloat,
    // Optimized foldr operations (native LLVM loops)
    FoldrSumInt,        // foldr $1 + $2, list
    FoldrSumFloat,
    FoldrProdInt,       // foldr $1 * $2, list
    FoldrProdFloat,
    FoldrDiffInt,       // foldr $1 - $2, list (reverse iteration)
    FoldrDiffFloat,
    FoldrMinInt,        // foldr min($1, $2), list
    FoldrMinFloat,
    FoldrMaxInt,        // foldr max($1, $2), list
    FoldrMaxFloat,
    MapAny,
    MapInt,
    MapFloat,
    // Optimized map operations (native LLVM loops)
    MapScaleInt,        // map $1 * const, list
    MapScaleFloat,
    MapOffsetInt,       // map $1 + const, list
    MapOffsetFloat,
    MapSquareInt,       // map $1 * $1, list
    MapSquareFloat,
    // Fully specialized hash-key map operations (single C++ runtime call)
    MapHashKeyValue,    // map $1.key, list<hash> → list<auto>
    MapHashKeyInt,      // map $1.key, list<hash> → list<int> (int values)
    MapHashKeyOffsetInt,// map ($1.key + N), list<hash> → list<int>
    MapHashKeyScaleInt, // map ($1.key * N), list<hash> → list<int>
    HashMapTwoKeys,     // map {$1.k1: $1.k2}, list<hash> → hash<auto>
    SelectAny,
    SelectInt,
    SelectFloat,
    // Optimized select operations (native LLVM loops)
    SelectPositiveInt,  // select $1 > 0, list
    SelectPositiveFloat,
    SelectNonZeroInt,   // select $1 != 0, list
    SelectNonZeroFloat,
    // Fused map+select operations (single-pass, no intermediate list)
    FusedMapSelectScalePositiveInt,     // (map $1 * c, (select list, $1 > 0))
    FusedMapSelectScalePositiveFloat,
    FusedMapSelectOffsetPositiveInt,    // (map $1 + c, (select list, $1 > 0))
    FusedMapSelectOffsetPositiveFloat,
    FusedMapSelectSquarePositiveInt,    // (map $1 * $1, (select list, $1 > 0))
    FusedMapSelectSquarePositiveFloat,
    // Fused map+foldl operations (single-pass, no intermediate list)
    FusedMapFoldlSumScaleInt,           // foldl $1 + $2, (map $1 * c, list) -> sum(list[i] * c)
    FusedMapFoldlSumScaleFloat,
    FusedMapFoldlSumSquareInt,          // foldl $1 + $2, (map $1 * $1, list) -> sum(list[i]^2)
    FusedMapFoldlSumSquareFloat,
    FusedMapFoldlProdScaleInt,          // foldl $1 * $2, (map $1 * c, list) -> prod(list[i] * c)
    FusedMapFoldlProdScaleFloat,
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
    SwitchInt,     // Integer switch with LLVM switch instruction
    SwitchString,  // String switch with hash-table lookup
    Return,
    ReturnNothing,

    LoadLocal,
    StoreLocal,
    UninstantiateLocal,  // Uninstantiate a local variable at block scope exit
    LoadArg,
    LoadClosure,
    StoreClosure,
    LoadGlobal,
    StoreGlobal,
    LoadThreadLocal,
    StoreThreadLocal,

    // Implicit argument opcodes for closures
    LoadImplicitArg,    // Load $1, $2, etc. by offset (0 for $1, 1 for $2, etc.)
    LoadImplicitArgv,   // Load entire $argv list
    LoadImplicitElement,// Load $# (current element index in map/select)
    // Context setup/teardown for functional operators (map, select, foldl, etc.)
    PushImplicitArg,    // Push value as $1, result = old context for restoration
    SetImplicitArgv,    // Set list directly as implicit args (for foldl $1/$2), result = old context
    PopImplicitArg,     // Restore previous $1 context (operand = old context)
    PushImplicitElement,// Push index as $#, result = old element for restoration
    PopImplicitElement, // Restore previous $# context (operand = old element)

    // Hash key access
    HashKeyAccess,      // Load hash.key - direct hash member access without AST delegation
    HashKeyAccessInt,   // Load hash.key as native int64 (known int value type)

    // Self member access
    LoadSelfMember,     // Load self.member_name - accesses current object's member

    // Static class variable access
    LoadStaticVar,      // Load static class variable - accesses QoreVarInfo directly

    // Object instantiation
    NewObject,          // Create new object - calls QoreClass::execConstructor directly

    // Constant and closure creation
    LoadConstant,       // Load runtime constant - accesses ConstantEntry::saved_val
    CreateClosure,      // Create closure/lambda - instantiates QoreClosureNode or QoreObjectClosureNode
    CreateCallRef,      // Create call reference - function/static method references
    CreateMethodRef,    // Create method reference - object method references
    CreateParseRef,     // Create parse reference - \var lvalue references

    // Typed container construction
    NewHashDecl,        // Create new hashdecl instance
    NewComplexHash,     // Create new typed hash
    NewComplexList,     // Create new typed list
    VrnConstruct,       // Construct value for VarRefNewObjectNode (non-object types)

    // Hash building
    HashSetKeyValue,    // Set key-value pair in hash (for hash map loops)

    // Reverse iteration
    IteratorCreateReverse, // Create reverse iterator from list/iterable (for foldr)

    Call,
    CallDirect,         // Direct function call - resolved at parse time (no AST round-trip)
    CallIndirect,
    CallMethod,
    CallMethodDirect,   // Direct method call - no dispatch needed (final class/method)
    InvokeMethodDirect, // Direct method call with exception handling (final class in try/catch)
    CallStatic,
    CallStaticDirect,       // Direct static method call - pre-evaluated args (no AST round-trip)
    DotEvalMethodDirect,    // Direct dot-eval method call - pre-evaluated base+args
    InvokeDotEvalMethodDirect, // Direct dot-eval method call with exception handling (try/catch)
    CallClosureDirect,      // Direct closure/callref call - operands[0]=callref, rest=args
    Invoke,

    GuardInt,
    GetObjectClass,         // Get runtime class pointer from object value (for devirtualization)
    GuardFloat,
    GuardType,
    GuardNotNothing,

    LandingPad,
    CatchException,
    CatchCleanup,
    Rethrow,
    Throw,
    InvokeSimError,

    Incref,
    Decref,
    DecrefNoThrow,

    SwitchRegexMatch,   // Test switch regex case: (switch_val, regex_case_ptr) -> bool
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
        case QoreIROpcode::FoldlSumInt:
        case QoreIROpcode::FoldlSumFloat:
        case QoreIROpcode::FoldlProdInt:
        case QoreIROpcode::FoldlProdFloat:
        case QoreIROpcode::FoldlDiffInt:
        case QoreIROpcode::FoldlDiffFloat:
        case QoreIROpcode::FoldlMinInt:
        case QoreIROpcode::FoldlMinFloat:
        case QoreIROpcode::FoldlMaxInt:
        case QoreIROpcode::FoldlMaxFloat:
        case QoreIROpcode::FoldrSumInt:
        case QoreIROpcode::FoldrSumFloat:
        case QoreIROpcode::FoldrProdInt:
        case QoreIROpcode::FoldrProdFloat:
        case QoreIROpcode::FoldrDiffInt:
        case QoreIROpcode::FoldrDiffFloat:
        case QoreIROpcode::FoldrMinInt:
        case QoreIROpcode::FoldrMinFloat:
        case QoreIROpcode::FoldrMaxInt:
        case QoreIROpcode::FoldrMaxFloat:
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
        case QoreIROpcode::FusedMapFoldlSumScaleInt:
        case QoreIROpcode::FusedMapFoldlSumScaleFloat:
        case QoreIROpcode::FusedMapFoldlSumSquareInt:
        case QoreIROpcode::FusedMapFoldlSumSquareFloat:
        case QoreIROpcode::FusedMapFoldlProdScaleInt:
        case QoreIROpcode::FusedMapFoldlProdScaleFloat:
            return true;
        default:
            return false;
    }
}

//! Returns true if the opcode is a call-type op (used by Invoke dispatch)
inline bool isCallInvokeOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Call:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::CallClosureDirect:
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

//! Integer switch case: value -> target block
struct QoreIRSwitchCase {
    int64_t value;
    QoreIRBasicBlock* target;
};

//! Integer switch instruction - maps to LLVM switch for efficient dispatch
class QoreIRSwitchIntInstruction : public QoreIRInstruction {
public:
    QoreIRSwitchIntInstruction() : QoreIRInstruction(QoreIROpcode::SwitchInt) {
    }

    QoreIRValue switch_val{};                       //!< Value being switched on
    QoreIRBasicBlock* default_target = nullptr;     //!< Default case target
    std::vector<QoreIRSwitchCase> cases;            //!< case value -> target block
};

//! String switch case: string value -> target block
struct QoreIRSwitchStringCase {
    std::string value;        //!< Case string value
    QoreIRBasicBlock* target; //!< Target block for this case
};

//! String switch instruction - uses hash-table lookup for efficient dispatch
class QoreIRSwitchStringInstruction : public QoreIRInstruction {
public:
    QoreIRSwitchStringInstruction() : QoreIRInstruction(QoreIROpcode::SwitchString) {
    }

    QoreIRValue switch_val{};                           //!< Value being switched on (string)
    QoreIRBasicBlock* default_target = nullptr;         //!< Default case target
    std::vector<QoreIRSwitchStringCase> cases;          //!< case string -> target block
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

//! Type profile for a single guard point: tracks observed runtime types
struct TypeProfile {
    std::atomic<uint32_t> int_count{0};
    std::atomic<uint32_t> float_count{0};
    std::atomic<uint32_t> string_count{0};
    std::atomic<uint32_t> bool_count{0};
    std::atomic<uint32_t> nothing_count{0};
    std::atomic<uint32_t> other_count{0};

    uint32_t total() const {
        return int_count.load(std::memory_order_relaxed)
            + float_count.load(std::memory_order_relaxed)
            + string_count.load(std::memory_order_relaxed)
            + bool_count.load(std::memory_order_relaxed)
            + nothing_count.load(std::memory_order_relaxed)
            + other_count.load(std::memory_order_relaxed);
    }

    //! Returns the dominant type if one type accounts for >= threshold of observations,
    //! or NT_ALL if no single type dominates
    qore_type_t dominantType(float threshold = 0.95f) const {
        uint32_t t = total();
        if (t == 0) {
            return NT_ALL;
        }
        float ft = (float)t;
        if ((float)int_count.load(std::memory_order_relaxed) / ft >= threshold) {
            return NT_INT;
        }
        if ((float)float_count.load(std::memory_order_relaxed) / ft >= threshold) {
            return NT_FLOAT;
        }
        if ((float)string_count.load(std::memory_order_relaxed) / ft >= threshold) {
            return NT_STRING;
        }
        if ((float)bool_count.load(std::memory_order_relaxed) / ft >= threshold) {
            return NT_BOOLEAN;
        }
        if ((float)nothing_count.load(std::memory_order_relaxed) / ft >= threshold) {
            return NT_NOTHING;
        }
        return NT_ALL;
    }

    void record(const QoreValue& v) {
        switch (v.getType()) {
            case NT_INT:
                int_count.fetch_add(1, std::memory_order_relaxed);
                break;
            case NT_FLOAT:
                float_count.fetch_add(1, std::memory_order_relaxed);
                break;
            case NT_STRING:
                string_count.fetch_add(1, std::memory_order_relaxed);
                break;
            case NT_BOOLEAN:
                bool_count.fetch_add(1, std::memory_order_relaxed);
                break;
            case NT_NOTHING:
                nothing_count.fetch_add(1, std::memory_order_relaxed);
                break;
            default:
                other_count.fetch_add(1, std::memory_order_relaxed);
                break;
        }
    }
};

class QoreIRGuardInstruction : public QoreIRInstruction {
public:
    explicit QoreIRGuardInstruction(QoreIROpcode op) : QoreIRInstruction(op) {
    }

    QoreIRBasicBlock* deopt_target = nullptr;
    const QoreTypeInfo* type_info = nullptr;
    uint32_t guard_id = 0;  //!< unique guard ID per function (for type profiling)
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
    QoreIRThrowInstruction(QoreIROpcode op = QoreIROpcode::Throw) : QoreIRInstruction(op) {
    }

    QoreIRBasicBlock* exception_target = nullptr;
    //! Number of active catch scopes to clean up (for Rethrow only)
    int catch_depth = 0;
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

//! Implicit argument reference instruction - loads $1, $2, etc.
class QoreIRImplicitArgInstruction : public QoreIRInstruction {
public:
    explicit QoreIRImplicitArgInstruction(int n_offset)
            : QoreIRInstruction(QoreIROpcode::LoadImplicitArg), offset(n_offset) {
    }

    int offset = 0;  // 0 for $1, 1 for $2, etc.
};

//! Hash key access instruction - loads hash.key directly (no AST delegation)
class QoreIRHashKeyAccessInstruction : public QoreIRInstruction {
public:
    explicit QoreIRHashKeyAccessInstruction(const char* n_key_name,
            QoreIROpcode op = QoreIROpcode::HashKeyAccess)
            : QoreIRInstruction(op), key_name(n_key_name) {
    }

    std::string key_name;
};

//! Map hash key instruction - fully specialized map over hash key access
class QoreIRMapHashKeyInstruction : public QoreIRInstruction {
public:
    QoreIRMapHashKeyInstruction(QoreIROpcode op, const char* n_key1, const char* n_key2 = nullptr)
            : QoreIRInstruction(op), key1(n_key1), key2(n_key2 ? n_key2 : "") {
    }

    std::string key1;  //!< primary key name
    std::string key2;  //!< secondary key (HashMapTwoKeys only)
};

//! Self member access instruction - loads self.member_name
class QoreIRSelfMemberInstruction : public QoreIRInstruction {
public:
    explicit QoreIRSelfMemberInstruction(const char* n_member_name)
            : QoreIRInstruction(QoreIROpcode::LoadSelfMember), member_name(n_member_name) {
    }

    std::string member_name;
};

//! Static class variable access instruction - loads a static class variable
class QoreIRStaticVarInstruction : public QoreIRInstruction {
public:
    QoreIRStaticVarInstruction(QoreVarInfo* n_vi, const char* n_var_name, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::LoadStaticVar), vi(n_vi), var_name(n_var_name),
              expr(n_expr) {
        expr.ref();
    }

    ~QoreIRStaticVarInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    QoreVarInfo* vi;
    std::string var_name;
    QoreValue expr;     //!< Original StaticClassVarRefNode AST expression (for AOT)
};

//! Object instantiation instruction - calls constructor directly
class QoreIRNewObjectInstruction : public QoreIRInstruction {
public:
    QoreIRNewObjectInstruction(const QoreClass* n_qc, const AbstractQoreFunctionVariant* n_variant,
            const QoreListNode* n_args, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::NewObject), qc(n_qc), variant(n_variant), args(n_args),
              expr(n_expr) {
        expr.ref();
    }

    ~QoreIRNewObjectInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreClass* qc;
    const AbstractQoreFunctionVariant* variant;
    const QoreListNode* args;       //!< Pre-evaluated constructor arguments
    QoreValue expr;                 //!< Original AST expression (for AOT)
};

//! Runtime constant load instruction - loads ConstantEntry::saved_val
class QoreIRLoadConstantInstruction : public QoreIRInstruction {
public:
    QoreIRLoadConstantInstruction(const RuntimeConstantRefNode* n_node, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::LoadConstant), node(n_node), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRLoadConstantInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const RuntimeConstantRefNode* node;  //!< The constant ref node (for JIT access to ConstantEntry)
    QoreValue expr;                      //!< Original AST expression (for AOT)
};

//! Closure creation instruction - creates QoreClosureNode or QoreObjectClosureNode
class QoreIRCreateClosureInstruction : public QoreIRInstruction {
public:
    QoreIRCreateClosureInstruction(const QoreClosureParseNode* n_closure_node,
            const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CreateClosure), closure_node(n_closure_node),
              expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCreateClosureInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreClosureParseNode* closure_node;
    QoreValue expr;  //!< Original AST expression (for AOT)
};

//! Call reference creation instruction - creates function/static method call references
class QoreIRCreateCallRefInstruction : public QoreIRInstruction {
public:
    QoreIRCreateCallRefInstruction(const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CreateCallRef), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCreateCallRefInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    QoreValue expr;  //!< Original AST expression (for AOT and JIT eval)
};

//! Method reference creation instruction - creates object method call references
class QoreIRCreateMethodRefInstruction : public QoreIRInstruction {
public:
    QoreIRCreateMethodRefInstruction(const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CreateMethodRef), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCreateMethodRefInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    QoreValue expr;  //!< Original AST expression (for AOT and JIT eval)
};

//! Parse reference creation instruction - creates \var lvalue references
class QoreIRCreateParseRefInstruction : public QoreIRInstruction {
public:
    QoreIRCreateParseRefInstruction(const ParseReferenceNode* n_node, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CreateParseRef), node(n_node), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCreateParseRefInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const ParseReferenceNode* node;
    QoreValue expr;  //!< Original AST expression (for AOT)
};

//! Hashdecl construction instruction
class QoreIRNewHashDeclInstruction : public QoreIRInstruction {
public:
    QoreIRNewHashDeclInstruction(const NewHashDeclNode* n_node, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::NewHashDecl), node(n_node), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRNewHashDeclInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const NewHashDeclNode* node;  //!< AST node (for eval)
    QoreValue expr;  //!< Original AST expression (for AOT)
};

//! Complex hash construction instruction
class QoreIRNewComplexHashInstruction : public QoreIRInstruction {
public:
    QoreIRNewComplexHashInstruction(const NewComplexHashNode* n_node, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::NewComplexHash), node(n_node), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRNewComplexHashInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const NewComplexHashNode* node;  //!< AST node (for eval)
    QoreValue expr;  //!< Original AST expression (for AOT)
};

//! Complex list construction instruction
class QoreIRNewComplexListInstruction : public QoreIRInstruction {
public:
    QoreIRNewComplexListInstruction(const NewComplexListNode* n_node, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::NewComplexList), node(n_node), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRNewComplexListInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const NewComplexListNode* node;  //!< AST node (for eval)
    QoreValue expr;  //!< Original AST expression (for AOT)
};

//! VarRefNewObjectNode construction instruction - constructs value for typed variable declarations
class QoreIRVrnConstructInstruction : public QoreIRInstruction {
public:
    QoreIRVrnConstructInstruction(const VarRefNewObjectNode* n_vrn, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::VrnConstruct), vrn(n_vrn), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRVrnConstructInstruction() {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const VarRefNewObjectNode* vrn;  //!< The VarRefNewObjectNode (for construction)
    QoreValue expr;  //!< Original AST expression (for AOT)
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

//! Direct function call instruction - bypasses AST round-trip for resolved function calls
//! Stores function/variant/program pointers resolved at parse time
class QoreIRCallDirectInstruction : public QoreIRInstruction {
public:
    QoreIRCallDirectInstruction(const QoreFunction* n_func, const AbstractQoreFunctionVariant* n_variant,
            QoreProgram* n_pgm, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CallDirect),
              func(n_func), variant(n_variant), pgm(n_pgm), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCallDirectInstruction() override {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreFunction* func = nullptr;     //!< The resolved function pointer
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant (may be null)
    QoreProgram* pgm = nullptr;             //!< The program context
    QoreValue expr;                         //!< Original AST expression (for AOT)
    //!< operands[0..n-1] are the function arguments
};

//! Direct method call instruction - bypasses virtual dispatch for final classes/methods
//! The method pointer is resolved at compile time and stored directly in the instruction
class QoreIRCallMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRCallMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant = nullptr)
            : QoreIRInstruction(QoreIROpcode::CallMethodDirect),
              method(n_method),
              qc(n_qc),
              variant(n_variant) {
    }

    const QoreMethod* method = nullptr;     //!< The resolved method pointer
    const QoreClass* qc = nullptr;          //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant (for fast call path)
    //!< operands[0..n-1] are the method arguments (self is obtained from runtime)
};

//! Direct method call with exception handling - bypasses virtual dispatch for final classes
//! in try/catch blocks. Combines the devirtualization of CallMethodDirect with the exception
//! routing of Invoke, avoiding AST evaluation overhead.
class QoreIRInvokeMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRInvokeMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant,
            QoreIRBasicBlock* n_normal, QoreIRBasicBlock* n_exception)
            : QoreIRInstruction(QoreIROpcode::InvokeMethodDirect),
              method(n_method), qc(n_qc), variant(n_variant),
              normal_target(n_normal), exception_target(n_exception) {
    }

    const QoreMethod* method = nullptr;         //!< The resolved method pointer
    const QoreClass* qc = nullptr;              //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant (for fast call path)
    QoreIRBasicBlock* normal_target = nullptr;  //!< Target block on success
    QoreIRBasicBlock* exception_target = nullptr; //!< Target block on exception
    //!< operands[0..n-1] are the method arguments (self is obtained from runtime)
};

//! Direct static method call instruction - pre-evaluates arguments to bypass AST round-trip.
//! Uses the Invoke + invoke_opcode pattern for try/catch (like CallDirect).
class QoreIRCallStaticDirectInstruction : public QoreIRInstruction {
public:
    QoreIRCallStaticDirectInstruction(const QoreMethod* n_method,
            const AbstractQoreFunctionVariant* n_variant, const QoreValue& n_expr)
            : QoreIRInstruction(QoreIROpcode::CallStaticDirect),
              method(n_method), variant(n_variant), expr(n_expr) {
        expr.ref();
    }

    ~QoreIRCallStaticDirectInstruction() override {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreMethod* method = nullptr;     //!< The resolved static method pointer
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant
    QoreValue expr;                         //!< Original AST expression (for AOT)
    //!< operands[0..n-1] are the pre-evaluated method arguments
};

//! Direct dot-eval method call instruction - pre-evaluates base object and arguments
//! for obj.method(args) calls where the method is resolved at parse time.
class QoreIRDotEvalMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRDotEvalMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant, const QoreValue& n_expr, bool n_pseudo)
            : QoreIRInstruction(QoreIROpcode::DotEvalMethodDirect),
              method(n_method), qc(n_qc), variant(n_variant), expr(n_expr), pseudo(n_pseudo) {
        expr.ref();
    }

    ~QoreIRDotEvalMethodDirectInstruction() override {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreMethod* method = nullptr;     //!< The resolved method pointer
    const QoreClass* qc = nullptr;          //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant
    QoreValue expr;                         //!< Original AST expression (for AOT)
    bool pseudo = false;                    //!< True if this is a pseudo-method call
    //!< operands[0] is the base expression, operands[1..n-1] are arguments
};

//! Direct dot-eval method call with exception handling - for try/catch blocks.
//! Pre-evaluates base object and arguments like DotEvalMethodDirect but routes
//! exceptions to the exception target block.
class QoreIRInvokeDotEvalMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRInvokeDotEvalMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant, const QoreValue& n_expr, bool n_pseudo,
            QoreIRBasicBlock* n_normal, QoreIRBasicBlock* n_exception)
            : QoreIRInstruction(QoreIROpcode::InvokeDotEvalMethodDirect),
              method(n_method), qc(n_qc), variant(n_variant), expr(n_expr), pseudo(n_pseudo),
              normal_target(n_normal), exception_target(n_exception) {
        expr.ref();
    }

    ~QoreIRInvokeDotEvalMethodDirectInstruction() override {
        ExceptionSink xsink;
        expr.discard(&xsink);
    }

    const QoreMethod* method = nullptr;         //!< The resolved method pointer
    const QoreClass* qc = nullptr;              //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant
    QoreValue expr;                             //!< Original AST expression (for AOT)
    bool pseudo = false;                        //!< True if this is a pseudo-method call
    QoreIRBasicBlock* normal_target = nullptr;  //!< Target block on success
    QoreIRBasicBlock* exception_target = nullptr; //!< Target block on exception
    //!< operands[0] is the base expression, operands[1..n-1] are arguments
};

class QoreIRForeachInstruction : public QoreIRInstruction {
public:
    explicit QoreIRForeachInstruction(const ForEachStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::Foreach), stmt(n_stmt) {
    }

    const ForEachStatement* stmt = nullptr;
};

//! Creates a FunctionalOperatorInterface from a list or iterable expression
//! result contains a pointer to the iterator (as int64_t)
class QoreIRIteratorCreateInstruction : public QoreIRInstruction {
public:
    QoreIRIteratorCreateInstruction(QoreIRValue n_iterable, FunctionalOperator* n_iterator_func = nullptr)
            : QoreIRInstruction(QoreIROpcode::IteratorCreate),
              iterable(n_iterable),
              iterator_func(n_iterator_func) {
    }

    QoreIRValue iterable;               //!< List value or iterable expression result
    FunctionalOperator* iterator_func;  //!< Optional functional operator for lazy evaluation
};

//! Advances iterator and branches based on done status
//! If not done, stores current value in result and branches to continue_target
//! If done, branches to done_target
class QoreIRIteratorNextInstruction : public QoreIRInstruction {
public:
    QoreIRIteratorNextInstruction(QoreIRValue n_iterator, QoreIRBasicBlock* n_done_target,
            QoreIRBasicBlock* n_continue_target)
            : QoreIRInstruction(QoreIROpcode::IteratorNext),
              iterator(n_iterator),
              done_target(n_done_target),
              continue_target(n_continue_target) {
    }

    QoreIRValue iterator;                       //!< Iterator handle from IteratorCreate
    QoreIRBasicBlock* done_target = nullptr;    //!< Target block when iterator is exhausted
    QoreIRBasicBlock* continue_target = nullptr; //!< Target block with next value
};

class QoreIROnBlockExitInstruction : public QoreIRInstruction {
public:
    explicit QoreIROnBlockExitInstruction(const OnBlockExitStatement* n_stmt)
            : QoreIRInstruction(QoreIROpcode::OnBlockExit), stmt(n_stmt) {
    }

    const OnBlockExitStatement* stmt = nullptr;
};

//! Marks the entry into a new scope that may have on_exit/on_success/on_error handlers
class QoreIRScopeEnterInstruction : public QoreIRInstruction {
public:
    explicit QoreIRScopeEnterInstruction(uint32_t n_scope_id)
            : QoreIRInstruction(QoreIROpcode::ScopeEnter), scope_id(n_scope_id) {
    }

    uint32_t scope_id = 0;  //!< unique scope ID within function
};

//! Marks the exit from a scope - triggers execution of on_exit handlers registered since matching ScopeEnter
class QoreIRScopeExitInstruction : public QoreIRInstruction {
public:
    explicit QoreIRScopeExitInstruction(uint32_t n_scope_id, bool n_is_error = false)
            : QoreIRInstruction(QoreIROpcode::ScopeExit), scope_id(n_scope_id), is_error(n_is_error) {
    }

    uint32_t scope_id = 0;  //!< scope ID from matching ScopeEnter
    bool is_error = false;  //!< true if exiting due to an exception
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
    std::string invoke_key_name;  //!< Key name for HashKeyAccess invoke path
};

//! LandingPad instruction - marks the entry point of an exception handler (catch block)
//! Stores the scope stack depth at the try-catch entry so the IR interpreter can
//! execute on_error/on_exit handlers for scopes entered within the try body.
class QoreIRLandingPadInstruction : public QoreIRInstruction {
public:
    explicit QoreIRLandingPadInstruction(size_t n_scope_depth, uint32_t n_try_scope_id = 0)
            : QoreIRInstruction(QoreIROpcode::LandingPad), scope_depth(n_scope_depth),
              try_scope_id(n_try_scope_id) {
    }

    //! Scope stack depth at the try-catch entry point
    size_t scope_depth = 0;

    //! Scope ID for the try-level ScopeEnter that saves the OBE count at try entry.
    //! Used by LLVM lowering to call qore_rt_exec_on_block_exit() with the saved count.
    uint32_t try_scope_id = 0;
};

//! Switch regex case match instruction - tests if switch value matches a regex case
//! operands[0] is the switch value to test
//! result is a bool indicating match
class QoreIRSwitchRegexMatchInstruction : public QoreIRInstruction {
public:
    explicit QoreIRSwitchRegexMatchInstruction(const CaseNodeRegex* n_regex_case)
            : QoreIRInstruction(QoreIROpcode::SwitchRegexMatch), regex_case(n_regex_case) {
    }

    const CaseNodeRegex* regex_case = nullptr;  //!< The regex case node containing the regex
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

    //! Insert instruction at the beginning of the block
    void prependInstruction(std::unique_ptr<QoreIRInstruction> inst) {
        instructions.insert(instructions.begin(), std::move(inst));
    }

    std::string name;
    std::vector<std::unique_ptr<QoreIRInstruction>> instructions;

    //! True if this block is a loop header (condition block for while/for/do-while).
    //! Used by the IR interpreter for loop-aware JIT promotion (OSR).
    bool is_loop_header = false;
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

    // Set of LocalVar* (as void*) that are only accessed by LoadLocal/StoreLocal
    // in fully-lowered IR code — never referenced by Invoke expression subtrees,
    // delegate-to-AST statements, or closure captures.  For these locals, the LLVM
    // lowering can skip qore_rt_assign_local (runtime sync) and reloadLocalFromRuntime
    // (reload after calls), since no AST callback will ever look them up.
    std::unordered_set<const void*> ir_only_locals;

    // Total number of unique locals referenced by LoadLocal/StoreLocal/UninstantiateLocal.
    // Used with ir_only_locals.size() to determine if ALL locals are IR-only
    // (enabling reloadAllLocalsFromRuntime to be skipped entirely).
    size_t total_local_count = 0;

    //! Analyze all instructions to classify locals as IR-only vs AST-visible.
    //! Must be called after IR lowering completes but before LLVM lowering/execution.
    void computeIROnlyLocals();

    //! Returns true if all body locals are IR-only (managed by LLVM allocas,
    //! never accessed via thread-local stack).  When true, callers can skip
    //! instantiating/uninstantiating body locals on the thread-local stack.
    bool areAllBodyLocalsIROnly() const {
        return !all_body_locals.empty() && !ir_only_locals.empty()
            && std::all_of(all_body_locals.begin(), all_body_locals.end(),
                [this](const LocalVar* lv) {
                    return ir_only_locals.count(reinterpret_cast<const void*>(lv));
                });
    }

    //! Type profiles for guards — indexed by guard_id
    //! Populated during IR interpretation; read during JIT compilation for specialization
    //! mutable: written during const IR execution (type recording is a side-channel)
    //! Uses unique_ptr<[]> because TypeProfile contains std::atomic members (non-movable)
    mutable std::unique_ptr<TypeProfile[]> guard_profiles;
    mutable uint32_t guard_profile_count = 0;

    //! Number of guards in this function
    uint32_t num_guards = 0;

    //! Allocate guard profiles array to match num_guards
    void initGuardProfiles() {
        if (guard_profile_count < num_guards) {
            guard_profiles.reset(new TypeProfile[num_guards]());
            guard_profile_count = num_guards;
        }
    }

    //! OSR flag: set by the IR interpreter when a hot loop is detected.
    //! Checked by evalTiered() after IR execution to trigger JIT compilation.
    //! mutable: written during const IR execution.
    mutable bool osr_jit_requested = false;

private:
    uint32_t next_value_id = 1;
};

//! Check if an opcode is a block terminator (transfers control flow)
inline bool isTerminator(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::SwitchInt:
        case QoreIROpcode::SwitchString:
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::ThreadExit:
            return true;
        default:
            return false;
    }
}

#endif
