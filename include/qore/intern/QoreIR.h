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
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <qore/QoreValue.h>

class QoreProgramLocation;
class LocalVar;
class Var;
class OnBlockExitStatement;
class ContextStatement;
class SummarizeStatement;
class CaseNode;
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
class QoreEnumMember;

/** IR opcode identifiers.

    IMPORTANT: Opcode IDs are explicitly numbered for binary compatibility.
    When adding new opcodes:
    - Always assign the next available ID after the current maximum
    - Never reuse or reassign an existing ID
    - Never insert opcodes in the middle of the enum
    - Update QORE_IR_MAX_OPCODE after adding new opcodes
    When removing opcodes:
    - Leave the ID as a gap (comment it out but preserve the number)
    - The runtime should handle unknown opcodes gracefully
*/
enum class QoreIROpcode : uint16_t {
    // Constants (0-6)
    ConstInt            = 0,
    ConstFloat          = 1,
    ConstBool           = 2,
    ConstNothing        = 3,
    ConstNull           = 4,
    ConstString         = 5,
    ConstDate           = 6,

    // List/Hash construction (7-18)
    MakeList            = 7,
    MakeHash            = 8,
    CreateEmptyList     = 9,    // Create empty list for functional operator results
    CreateSizedList     = 10,   // Create list with pre-allocated capacity (for typed map loops)
    ListAppend          = 11,   // Append value to list (for map/select loops)
    ListSize            = 12,   // Get list element count (returns native i64)
    ListGetInt          = 13,   // Get int element from list by index (returns native i64)
    ListGetFloat        = 14,   // Get float element from list by index (returns native double)
    ListGetValue        = 15,   // Get any element from list by index (returns nanboxed QoreValue)
    ListSetInt          = 16,   // Set int element in list by index (for pre-sized typed map output)
    ListSetFloat        = 17,   // Set float element in list by index
    ListSetValue        = 18,   // Set any element in list by index

    // Arithmetic (19-44)
    AddInt              = 19,
    AddFloat            = 20,
    AddAny              = 21,
    AddString           = 22,   // Typed string concatenation (no runtime type checks)
    StringConcat        = 23,   // Multi-string concatenation (a + b + c + d in one pass)
    SubInt              = 24,
    SubFloat            = 25,
    SubAny              = 26,
    MulInt              = 27,
    MulFloat            = 28,
    MulAny              = 29,
    DivInt              = 30,
    DivFloat            = 31,
    DivAny              = 32,
    ModInt              = 33,
    ModAny              = 34,
    AndInt              = 35,
    AndAny              = 36,
    OrInt               = 37,
    OrAny               = 38,
    XorInt              = 39,
    XorAny              = 40,
    ShlInt              = 41,
    ShlAny              = 42,
    ShrInt              = 43,
    ShrAny              = 44,

    // Compound assignment (45-68)
    ShlAssignInt        = 45,
    ShlAssignAny        = 46,
    ShrAssignInt        = 47,
    ShrAssignAny        = 48,
    AddAssignInt        = 49,
    AddAssignFloat      = 50,
    AddAssignAny        = 51,
    SubAssignInt        = 52,
    SubAssignFloat      = 53,
    SubAssignAny        = 54,
    MulAssignInt        = 55,
    MulAssignFloat      = 56,
    MulAssignAny        = 57,
    DivAssignInt        = 58,
    DivAssignFloat      = 59,
    DivAssignAny        = 60,
    ModAssignInt        = 61,
    ModAssignAny        = 62,
    AndAssignInt        = 63,
    AndAssignAny        = 64,
    OrAssignInt         = 65,
    OrAssignAny         = 66,
    XorAssignInt        = 67,
    XorAssignAny        = 68,

    // LValue operations (69-89)
    LoadLValue          = 69,
    StoreLValue         = 70,
    PreIncLValue        = 71,
    PreDecLValue        = 72,
    PostIncLValue       = 73,
    PostDecLValue       = 74,
    AddAssignLValue     = 75,
    SubAssignLValue     = 76,
    MulAssignLValue     = 77,
    DivAssignLValue     = 78,
    ModAssignLValue     = 79,
    AndAssignLValue     = 80,
    OrAssignLValue      = 81,
    XorAssignLValue     = 82,
    ShlAssignLValue     = 83,
    ShrAssignLValue     = 84,
    ShiftLValue         = 85,
    UnshiftLValue       = 86,
    PopAny              = 87,
    PushAny             = 88,
    SpliceLValue        = 89,

    // Extract/Remove (90-99)
    ExtractAny          = 90,
    ExtractList         = 91,
    ExtractString       = 92,
    ExtractBinary       = 93,
    RemoveAny           = 94,
    RemoveList          = 95,
    RemoveHash          = 96,
    RemoveObject        = 97,
    RemoveString        = 98,
    RemoveBinary        = 99,

    // Collection utilities (100-122)
    KeysAny             = 100,
    KeysList            = 101,
    KeysHash            = 102,
    RegexMatchAny       = 103,
    RegexMatchBool      = 104,
    RegexNMatchBool     = 105,
    RegexExtractAny     = 106,
    RegexExtractList    = 107,
    RegexSubstAny       = 108,
    RegexSubstString    = 109,
    InstanceOfBool      = 110,
    TrimAny             = 111,
    TrimString          = 112,
    ChompAny            = 113,
    ChompString         = 114,
    TransliterateAny    = 115,
    TransliterateString = 116,
    BackgroundInt        = 117,
    ListAssignAny       = 118,
    ExistsAny           = 119,
    ExistsBool          = 120,
    ElementsAny         = 121,
    ElementsInt         = 122,

    // Dot-eval (123-130)
    DotEvalAny          = 123,
    DotEvalInt          = 124,
    DotEvalFloat        = 125,
    DotEvalString       = 126,
    DotEvalDate         = 127,
    DotEvalList         = 128,
    DotEvalHash         = 129,
    DotEvalObject       = 130,

    // Iterators and control (131-144)
    MapSelectList       = 131,
    HashMap             = 132,
    HashMapSelect       = 133,
    // Foreach = 134,     // REMOVED: fully lowered to iterators
    IteratorCreate      = 135,  // Create iterator from list/iterable
    IteratorNext        = 136,  // Advance iterator, branch if done, store value
    OnBlockExit         = 137,
    ScopeEnter          = 138,
    ScopeExit           = 139,
    ThreadExit          = 140,
    // Debug = 141,       // REMOVED: fully lowered inline
    // Assert = 142,      // REMOVED: fully lowered inline
    Context             = 143,
    Summarize           = 144,

    // Comparisons (145-174)
    EqInt               = 145,
    EqFloat             = 146,
    EqString            = 147,
    EqAny               = 148,
    NeInt               = 149,
    NeFloat             = 150,
    NeString            = 151,
    NeAny               = 152,
    EqHard              = 153,
    NeHard              = 154,
    LtInt               = 155,
    LtFloat             = 156,
    LtString            = 157,
    LtAny               = 158,
    LeInt               = 159,
    LeFloat             = 160,
    LeString            = 161,
    LeAny               = 162,
    GtInt               = 163,
    GtFloat             = 164,
    GtString            = 165,
    GtAny               = 166,
    GeInt               = 167,
    GeFloat             = 168,
    GeString            = 169,
    GeAny               = 170,
    CmpInt              = 171,
    CmpFloat            = 172,
    CmpString           = 173,
    CmpAny              = 174,

    // Type coercion and logic (175-182)
    ToBool              = 175,
    Not                 = 176,
    IsNullOrNothing     = 177,
    Phi                 = 178,
    UnaryPlusAny        = 179,
    UnaryMinusInt       = 180,
    UnaryMinusFloat     = 181,
    UnaryMinusAny       = 182,

    // Fold operations (183-208)
    FoldlAny            = 183,
    FoldlInt            = 184,
    FoldlFloat          = 185,
    FoldrAny            = 186,
    FoldrInt            = 187,
    FoldrFloat          = 188,
    // Optimized fold operations (native LLVM loops)
    FoldlSumInt         = 189,  // foldl $1 + $2, list, init
    FoldlSumFloat       = 190,
    FoldlProdInt        = 191,  // foldl $1 * $2, list, init
    FoldlProdFloat      = 192,
    FoldlDiffInt        = 193,  // foldl $1 - $2, list, init
    FoldlDiffFloat      = 194,
    FoldlMinInt         = 195,  // foldl min($1, $2), list, init
    FoldlMinFloat       = 196,
    FoldlMaxInt         = 197,  // foldl max($1, $2), list, init
    FoldlMaxFloat       = 198,
    // Optimized foldr operations (native LLVM loops)
    FoldrSumInt         = 199,  // foldr $1 + $2, list
    FoldrSumFloat       = 200,
    FoldrProdInt        = 201,  // foldr $1 * $2, list
    FoldrProdFloat      = 202,
    FoldrDiffInt        = 203,  // foldr $1 - $2, list (reverse iteration)
    FoldrDiffFloat      = 204,
    FoldrMinInt         = 205,  // foldr min($1, $2), list
    FoldrMinFloat       = 206,
    FoldrMaxInt         = 207,  // foldr max($1, $2), list
    FoldrMaxFloat       = 208,

    // Map operations (209-222)
    MapAny              = 209,
    MapInt              = 210,
    MapFloat            = 211,
    // Optimized map operations (native LLVM loops)
    MapScaleInt         = 212,  // map $1 * const, list
    MapScaleFloat       = 213,
    MapOffsetInt        = 214,  // map $1 + const, list
    MapOffsetFloat      = 215,
    MapSquareInt        = 216,  // map $1 * $1, list
    MapSquareFloat      = 217,
    // Fully specialized hash-key map operations (single C++ runtime call)
    MapHashKeyValue     = 218,  // map $1.key, list<hash> -> list<auto>
    MapHashKeyInt       = 219,  // map $1.key, list<hash> -> list<int> (int values)
    MapHashKeyOffsetInt = 220,  // map ($1.key + N), list<hash> -> list<int>
    MapHashKeyScaleInt  = 221,  // map ($1.key * N), list<hash> -> list<int>
    HashMapTwoKeys      = 222,  // map {$1.k1: $1.k2}, list<hash> -> hash<auto>

    // Select operations (223-241)
    SelectAny           = 223,
    SelectInt           = 224,
    SelectFloat         = 225,
    // Optimized select operations (native LLVM loops)
    SelectPositiveInt   = 226,  // select $1 > 0, list
    SelectPositiveFloat = 227,
    SelectNonZeroInt    = 228,  // select $1 != 0, list
    SelectNonZeroFloat  = 229,
    // Fused map+select operations (single-pass, no intermediate list)
    FusedMapSelectScalePositiveInt      = 230,  // (map $1 * c, (select list, $1 > 0))
    FusedMapSelectScalePositiveFloat    = 231,
    FusedMapSelectOffsetPositiveInt     = 232,  // (map $1 + c, (select list, $1 > 0))
    FusedMapSelectOffsetPositiveFloat   = 233,
    FusedMapSelectSquarePositiveInt     = 234,  // (map $1 * $1, (select list, $1 > 0))
    FusedMapSelectSquarePositiveFloat   = 235,
    // Fused map+foldl operations (single-pass, no intermediate list)
    FusedMapFoldlSumScaleInt            = 236,  // foldl $1 + $2, (map $1 * c, list)
    FusedMapFoldlSumScaleFloat          = 237,
    FusedMapFoldlSumSquareInt           = 238,  // foldl $1 + $2, (map $1 * $1, list)
    FusedMapFoldlSumSquareFloat         = 239,
    FusedMapFoldlProdScaleInt           = 240,  // foldl $1 * $2, (map $1 * c, list)
    FusedMapFoldlProdScaleFloat         = 241,

    // AST-delegated functional operators (242-244)
    MapSelectAny        = 242,
    HashMapAny          = 243,
    HashMapSelectAny    = 244,

    // Range operations (245-251)
    RangeAny            = 245,
    RangeInt            = 246,
    RangeFloat          = 247,
    RangeDate           = 248,
    RangeSliceAny       = 249,
    RangeSliceInt       = 250,
    RangeSliceFloat     = 251,

    // Cast operations (252-257)
    CastAny             = 252,
    CastList            = 253,
    CastHash            = 254,  // Cast to bare hashdecl type or plain hash
    CastObject          = 255,
    CastEnum            = 256,
    CastComplexHash     = 257,  // Cast to complex hash type (hash<KeyType, ValueType>)

    // Control flow (258-263)
    Br                  = 258,
    BrIf                = 259,
    SwitchInt           = 260,  // Integer switch with LLVM switch instruction
    SwitchString        = 261,  // String switch with hash-table lookup
    Return              = 262,
    ReturnNothing       = 263,

    // Variable access (264-273)
    LoadLocal           = 264,
    StoreLocal          = 265,
    UninstantiateLocal  = 266,  // Uninstantiate a local variable at block scope exit
    LoadArg             = 267,
    LoadClosure         = 268,
    StoreClosure        = 269,
    LoadGlobal          = 270,
    StoreGlobal         = 271,
    LoadThreadLocal     = 272,
    StoreThreadLocal    = 273,

    // Implicit argument opcodes (274-281)
    LoadImplicitArg     = 274,  // Load $1, $2, etc. by offset (0 for $1, 1 for $2, etc.)
    LoadImplicitArgv    = 275,  // Load entire $argv list
    LoadImplicitElement = 276,  // Load $# (current element index in map/select)
    // Context setup/teardown for functional operators (map, select, foldl, etc.)
    PushImplicitArg     = 277,  // Push value as $1, result = old context for restoration
    SetImplicitArgv     = 278,  // Set list directly as implicit args (for foldl $1/$2)
    PopImplicitArg      = 279,  // Restore previous $1 context (operand = old context)
    PushImplicitElement = 280,  // Push index as $#, result = old element for restoration
    PopImplicitElement  = 281,  // Restore previous $# context (operand = old element)

    // Direct access opcodes (282-285)
    HashKeyAccess       = 282,  // Load hash.key - direct hash member access
    HashKeyAccessInt    = 283,  // Load hash.key as native int64 (known int value type)
    LoadSelfMember      = 284,  // Load self.member_name - accesses current object's member
    LoadStaticVar       = 285,  // Load static class variable - accesses QoreVarInfo directly

    // Object instantiation (286)
    NewObject           = 286,  // Create new object - calls QoreClass::execConstructor directly

    // Constant and closure creation (287-291)
    LoadConstant        = 287,  // Load runtime constant - accesses ConstantEntry::saved_val
    CreateClosure       = 288,  // Create closure/lambda
    CreateCallRef       = 289,  // Create call reference - function/static method references
    CreateMethodRef     = 290,  // Create method reference - object method references
    CreateParseRef      = 291,  // Create parse reference - \var lvalue references

    // Typed container construction (292-295)
    NewHashDecl         = 292,  // Create new hashdecl instance
    NewComplexHash      = 293,  // Create new typed hash
    NewComplexList      = 294,  // Create new typed list
    VrnConstruct        = 295,  // Construct value for VarRefNewObjectNode (non-object types)

    // Hash building (296)
    HashSetKeyValue     = 296,  // Set key-value pair in hash (for hash map loops)

    // Reverse iteration (297)
    IteratorCreateReverse = 297, // Create reverse iterator from list/iterable (for foldr)

    // Function/method calls (298-309)
    Call                = 298,
    CallDirect          = 299,  // Direct function call - resolved at parse time
    CallIndirect        = 300,
    CallMethod          = 301,
    CallMethodDirect    = 302,  // Direct method call - no dispatch needed
    InvokeMethodDirect  = 303,  // Direct method call with exception handling
    CallStatic          = 304,
    CallStaticDirect    = 305,  // Direct static method call - pre-evaluated args
    DotEvalMethodDirect = 306,  // Direct dot-eval method call - pre-evaluated base+args
    InvokeDotEvalMethodDirect = 307, // Direct dot-eval with exception handling
    CallClosureDirect   = 308,  // Direct closure/callref call
    Invoke              = 309,

    // Type guards (310-314)
    GuardInt            = 310,
    GetObjectClass      = 311,  // Get runtime class pointer from object value
    GuardFloat          = 312,
    GuardType           = 313,
    GuardNotNothing     = 314,

    // Exception handling (315-320)
    LandingPad          = 315,
    CatchException      = 316,
    CatchCleanup        = 317,
    Rethrow             = 318,
    Throw               = 319,
    InvokeSimError      = 320,

    // Reference management (321-323)
    Incref              = 321,
    Decref              = 322,
    DecrefNoThrow       = 323,

    // Regex switch (324)
    SwitchRegexMatch    = 324,  // Test switch regex case: (switch_val, regex_case_ptr) -> bool

    // Native list push (325)
    ListPush            = 325,  // Native list push: (list, value) -> list (in-place push)

    // Reference foreach opcodes (326-331)
    RefForeachInit      = 326,  // Init ref foreach state from ParseReferenceNode expr -> state handle
    RefForeachSize      = 327,  // Get iteration count from state handle -> int64
    RefForeachGetEntry  = 328,  // Get element at index: (state, index) -> value
    RefForeachRecord    = 329,  // Record modified value: (state, value) -> void
    RefForeachFinalize  = 330,  // Write back to reference and cleanup: (state) -> void
    RefForeachCleanup   = 331,  // Cleanup without write-back: (state) -> void

    // Number arithmetic (332-335) — typed QoreNumberNode operations
    // Both operands guaranteed NT_NUMBER; no tag-check branches needed.
    // Compile to direct qore_rt_number_* helper calls.
    AddNumber           = 332,
    SubNumber           = 333,
    MulNumber           = 334,
    DivNumber           = 335,

    // Hash element store (336) — write value to hash{const_key} with COW support
    // operands[0] = hash container, operands[1] = value to store
    HashKeyStore        = 336,

    // List element access (337) — load list[index] directly (no AST delegation)
    // operands[0] = list container, operands[1] = index
    ListIndexAccess     = 337,

    // List element store (338) — write value to list[index] with COW support
    // operands[0] = list container, operands[1] = value to store, operands[2] = index
    ListIndexStore      = 338,

    // Fused local int operations (339-341) — reduce dispatch overhead for tight loops
    // These fuse multiple instructions (load+op+store) into single opcodes.
    AddAssignLocalInt   = 339,  // target_local += source_local (both typed int)
    IncrementLocalInt   = 340,  // local += delta (typed int local, constant delta)
    BranchIfLtLocalInt  = 341,  // if (lhs_local < rhs_local) goto true else goto false
    ConstEnum           = 342,  // TAG_ENUM constant from QoreEnumMember*

    // Read-only list element access (343) — borrowed reference, no refSelf
    // operands[0] = list container, operands[1] = index
    // Safe when the list outlives the use of the returned element (e.g., map/select loops)
    ListGetValueNoRef   = 343,

    // Switch case match with enum unwrapping (344)
    // operands[0] = switch expression value
    // Uses CaseNode::matches() which unwraps TAG_ENUM before hard comparison
    SwitchCaseMatch     = 344,

    // Runtime type check (345) — returns native bool (1 if value is NT_LIST or NT_OBJECT, 0 otherwise)
    // Used by select to determine if the result should be returned as a list or unwrapped to a scalar
    IsCollectionType    = 345,

    //! Make a hash from constant string keys and value operands (avoids key boxing/conversion)
    MakeHashConstKeys   = 346,

    //! Convert any value to string (replaces builtin string() function call)
    ToString            = 347,

    //! Format a list as sprintf(fmt, args...) — used by assert failure messages
    Sprintf             = 348,

    // NOTE: When adding new opcodes, assign the next sequential ID (348, 349, ...)
    // QORE_IR_MAX_OPCODE is derived automatically from the last enum value below.
};

//! Maximum opcode ID supported by this build (derived from the last enum value)
//! NOTE: Both QoreIRInterpreter.cpp and QoreIRToLLVM.cpp have matching
//! static_assert guards that will break when this value changes, forcing
//! review of their dispatch switches.
constexpr uint16_t QORE_IR_MAX_OPCODE = static_cast<uint16_t>(QoreIROpcode::Sprintf);
static_assert(QORE_IR_MAX_OPCODE == 348, "QORE_IR_MAX_OPCODE changed — update this assertion and "
    "verify binary format compatibility");

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
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::IsCollectionType:
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
        Enum,
    };

    Kind kind = Kind::Nothing;
    int64_t int_value = 0;
    double float_value = 0.0;
    bool bool_value = false;
    std::string string_value;
    int64_t date_microseconds = 0;
    bool date_is_relative = false;
    const QoreEnumMember* enum_member = nullptr;
};

class QoreIRBasicBlock;
struct QoreIRPhiIncoming;

class QoreIRInstruction {
public:
    explicit QoreIRInstruction(QoreIROpcode op) : opcode(op), cached_start_line(-1) {
    }

    virtual ~QoreIRInstruction() = default;

    QoreIROpcode opcode;
    int16_t cached_start_line;  // -1 = no loc, >=0 = loc->start_line (fills padding after opcode)
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
    //! True for synthetic rethrows (e.g. foreach ref cleanup) that just propagate the
    //! exception already on xsink without accessing td->catchException
    bool synthetic = false;
};

class QoreIRLocalInstruction : public QoreIRInstruction {
public:
    QoreIRLocalInstruction(QoreIROpcode op, LocalVar* n_local, bool n_auto_ref = true)
            : QoreIRInstruction(op), local(n_local), auto_ref(n_auto_ref) {
    }

    LocalVar* local = nullptr;
    bool auto_ref = true;  // For LoadLocal: if true, calls refSelf(); if false, loads without inflating refcount
    bool weak = false;  // For StoreLocal: if true, wraps object/hash/list in weak reference
    bool is_closure = false;       // Pre-computed from local->closureUse() during IR analysis
    bool is_ref = false;           // Pre-computed from local->isRef() during IR analysis
    uint32_t slot_id = UINT32_MAX; // Pre-computed slot index into locals_slot_cache
};


class QoreIRVarInstruction : public QoreIRInstruction {
public:
    QoreIRVarInstruction(QoreIROpcode op, Var* n_var) : QoreIRInstruction(op), var(n_var) {
    }

    Var* var = nullptr;
    bool weak = false;  // For StoreGlobal/StoreThreadLocal: if true, wraps object/hash/list in weak reference
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

//! Hash element store instruction — write hash{const_key} = value with COW support
class QoreIRHashKeyStoreInstruction : public QoreIRInstruction {
public:
    QoreIRHashKeyStoreInstruction(const VarRefNode* n_container, const char* n_key)
            : QoreIRInstruction(QoreIROpcode::HashKeyStore),
              container(n_container), key_name(n_key) {
    }

    const VarRefNode* container;  //!< Container variable (for COW: get LocalVar* from ref.id)
    std::string key_name;
    uint32_t container_slot_id = UINT32_MAX; // Pre-computed slot index for container variable
    // operands[0] = hash value (from LoadLocal on container)
    // operands[1] = new element value to store
};

//! List index access instruction - loads list[index] directly (no AST delegation)
class QoreIRListIndexAccessInstruction : public QoreIRInstruction {
public:
    explicit QoreIRListIndexAccessInstruction()
            : QoreIRInstruction(QoreIROpcode::ListIndexAccess) {
    }
    // operands[0] = list container
    // operands[1] = index expression
};

//! List index store instruction - write value to list[index] with COW support
class QoreIRListIndexStoreInstruction : public QoreIRInstruction {
public:
    explicit QoreIRListIndexStoreInstruction(const VarRefNode* n_container)
            : QoreIRInstruction(QoreIROpcode::ListIndexStore),
              container(n_container) {
    }

    const VarRefNode* container;  //!< Container variable (for COW: get LocalVar* from ref.id)
    uint32_t container_slot_id = UINT32_MAX; // Pre-computed slot index for container variable
    // operands[0] = list value (from LoadLocal on container)
    // operands[1] = new element value to store
    // operands[2] = index expression
};

//! Fused add-assign for two typed int locals: target += source
//! Replaces LoadLocal(target) + LoadLocal(source) + AddAssignInt + StoreLocal(target)
class QoreIRAddAssignLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRAddAssignLocalIntInstruction(LocalVar* n_target, LocalVar* n_source)
            : QoreIRInstruction(QoreIROpcode::AddAssignLocalInt),
              target(n_target), source(n_source) {
    }

    LocalVar* target;
    LocalVar* source;
    uint32_t target_slot_id = UINT32_MAX;  //!< Pre-computed slot index for target
    uint32_t source_slot_id = UINT32_MAX;  //!< Pre-computed slot index for source
    bool target_ir_only = false;  //!< True if target is IR-only (skip write-through)
};

//! Fused increment for typed int local: local += delta (constant int)
//! Replaces LoadLocal + ConstInt(delta) + AddAssignInt + StoreLocal
class QoreIRIncrementLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRIncrementLocalIntInstruction(LocalVar* n_local, int64_t n_delta)
            : QoreIRInstruction(QoreIROpcode::IncrementLocalInt),
              local(n_local), delta(n_delta) {
    }

    LocalVar* local;
    int64_t delta;
    uint32_t slot_id = UINT32_MAX;  //!< Pre-computed slot index for local
    bool ir_only = false;  //!< True if local is IR-only (skip write-through)
};

//! Fused compare-and-branch for two typed int locals: if (lhs < rhs) goto true else goto false
//! Replaces LoadLocal(lhs) + LoadLocal(rhs) + LtInt + BranchIf
class QoreIRBranchIfLtLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRBranchIfLtLocalIntInstruction(LocalVar* n_lhs, LocalVar* n_rhs,
            QoreIRBasicBlock* n_true_target, QoreIRBasicBlock* n_false_target)
            : QoreIRInstruction(QoreIROpcode::BranchIfLtLocalInt),
              lhs(n_lhs), rhs(n_rhs),
              true_target(n_true_target), false_target(n_false_target) {
    }

    LocalVar* lhs;
    LocalVar* rhs;
    uint32_t lhs_slot_id = UINT32_MAX;   //!< Pre-computed slot index for lhs
    uint32_t rhs_slot_id = UINT32_MAX;   //!< Pre-computed slot index for rhs
    QoreIRBasicBlock* true_target;
    QoreIRBasicBlock* false_target;
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

//! Make list instruction with optional parse-time type info
class QoreIRMakeListInstruction : public QoreIRInstruction {
public:
    explicit QoreIRMakeListInstruction() : QoreIRInstruction(QoreIROpcode::MakeList) {
    }

    const QoreTypeInfo* typeInfo = nullptr;  //!< parse-time type info (may be nullptr for auto)
};

//! Make hash instruction with optional parse-time type info
class QoreIRMakeHashInstruction : public QoreIRInstruction {
public:
    explicit QoreIRMakeHashInstruction() : QoreIRInstruction(QoreIROpcode::MakeHash) {
    }

    const QoreTypeInfo* typeInfo = nullptr;  //!< parse-time type info (may be nullptr for auto)
};

//! Make hash with constant string keys - operands are values only, keys are pre-stored
class QoreIRMakeHashConstKeysInstruction : public QoreIRInstruction {
public:
    QoreIRMakeHashConstKeysInstruction(std::vector<std::string>&& n_keys)
            : QoreIRInstruction(QoreIROpcode::MakeHashConstKeys), keys(std::move(n_keys)) {
    }

    std::vector<std::string> keys;  //!< constant key names (one per value operand)
    const QoreTypeInfo* typeInfo = nullptr;  //!< parse-time type info (may be nullptr for auto)
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
    QoreIRLValueInstruction(QoreIROpcode op, const QoreValue& n_lvalue, bool n_weak = false)
            : QoreIRInstruction(op), lvalue(n_lvalue), weak(n_weak) {
        lvalue.ref();
    }

    ~QoreIRLValueInstruction() override {
        lvalue.discard(nullptr);
    }

    //! Sentinel value indicating the lvalue target is a known non-local variable
    //! (member via SelfVarrefNode, static class var, global). No local cache
    //! invalidation is needed for these targets.
    static constexpr uint32_t LVALUE_NON_LOCAL = UINT32_MAX - 1;

    //! Returns true if the lvalue target is a known local variable with a valid slot_id.
    bool hasLocalTarget() const { return lvalue_slot_id < LVALUE_NON_LOCAL; }

    QoreValue lvalue;
    bool weak = false;  //!< true for weak (:=) assignment
    //! Pre-computed slot_id for the lvalue target variable.
    //! Valid slot_id (< LVALUE_NON_LOCAL): target is a local variable at this slot index.
    //! LVALUE_NON_LOCAL: target is a known non-local (member, static, global) — skip local cache invalidation.
    //! UINT32_MAX: unresolved target — full local cache wipe for safety.
    uint32_t lvalue_slot_id = UINT32_MAX;
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
    bool has_ref_args = false;  //!< True if any operand is a reference type (may be modified by callee)
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
    bool has_ref_args = false;              //!< True if any operand is a reference type (may be modified by callee)
    bool is_self_recursive = false;         //!< True if this is a self-recursive call (same function name)
    //!< operands[0..n-1] are the function arguments

    // Cached inline IR call state (computed on first execution, avoids repeated lookups)
    mutable std::atomic<int8_t> inline_ir_state{0};  //!< 0=unchecked, 1=eligible, -1=ineligible
    mutable const QoreIRFunction* cached_callee_ir = nullptr;
    mutable const UserVariantBase* cached_uvb = nullptr;
    mutable const QoreTypeInfo* cached_return_type = nullptr;
};

//! Direct method call instruction - bypasses virtual dispatch for final classes/methods
//! The method pointer is resolved at compile time and stored directly in the instruction
class QoreIRCallMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRCallMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant = nullptr, const QoreValue& n_expr = QoreValue())
            : QoreIRInstruction(QoreIROpcode::CallMethodDirect),
              method(n_method),
              qc(n_qc),
              variant(n_variant),
              expr(n_expr) {
        if (expr) expr.ref();
    }

    ~QoreIRCallMethodDirectInstruction() override {
        if (expr) {
            ExceptionSink xsink;
            expr.discard(&xsink);
        }
    }

    const QoreMethod* method = nullptr;     //!< The resolved method pointer
    const QoreClass* qc = nullptr;          //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant (for fast call path)
    QoreValue expr;                         //!< Original AST expression (for AOT)
    bool has_ref_args = false;              //!< True if any operand is a reference type (may be modified by callee)
    //!< operands[0..n-1] are the method arguments (self is obtained from runtime)
};

//! Direct method call with exception handling - bypasses virtual dispatch for final classes
//! in try/catch blocks. Combines the devirtualization of CallMethodDirect with the exception
//! routing of Invoke, avoiding AST evaluation overhead.
class QoreIRInvokeMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRInvokeMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            const AbstractQoreFunctionVariant* n_variant,
            QoreIRBasicBlock* n_normal, QoreIRBasicBlock* n_exception, const QoreValue& n_expr = QoreValue())
            : QoreIRInstruction(QoreIROpcode::InvokeMethodDirect),
              method(n_method), qc(n_qc), variant(n_variant),
              normal_target(n_normal), exception_target(n_exception),
              expr(n_expr) {
        if (expr) expr.ref();
    }

    ~QoreIRInvokeMethodDirectInstruction() override {
        if (expr) {
            ExceptionSink xsink;
            expr.discard(&xsink);
        }
    }

    const QoreMethod* method = nullptr;         //!< The resolved method pointer
    const QoreClass* qc = nullptr;              //!< The class containing the method
    const AbstractQoreFunctionVariant* variant = nullptr; //!< The resolved variant (for fast call path)
    QoreIRBasicBlock* normal_target = nullptr;  //!< Target block on success
    QoreIRBasicBlock* exception_target = nullptr; //!< Target block on exception
    QoreValue expr;                             //!< Original AST expression (for AOT)
    bool has_ref_args = false;                  //!< True if any operand is a reference type (may be modified by callee)
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
    bool has_ref_args = false;              //!< True if any operand is a reference type (may be modified by callee)
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
    bool has_ref_args = false;              //!< True if any operand is a reference type (may be modified by callee)
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
    bool has_ref_args = false;                  //!< True if any operand is a reference type (may be modified by callee)
    QoreIRBasicBlock* normal_target = nullptr;  //!< Target block on success
    QoreIRBasicBlock* exception_target = nullptr; //!< Target block on exception
    //!< operands[0] is the base expression, operands[1..n-1] are arguments
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

    //! Constructor for deserialized case (no AST statement available)
    QoreIROnBlockExitInstruction(obe_type_e n_obe_type, std::unique_ptr<QoreIRFunction> n_handler_ir)
            : QoreIRInstruction(QoreIROpcode::OnBlockExit), obe_type(n_obe_type),
              handler_ir(std::move(n_handler_ir)) {
    }

    const OnBlockExitStatement* stmt = nullptr;
    //! Handler type for deserialized case (when stmt is null)
    obe_type_e obe_type = OBE_Unconditional;
    //! Compiled handler body (nullptr = AST fallback).
    //! When set, the handler body can be executed via the IR interpreter
    //! instead of delegating to StatementBlock::exec().
    std::unique_ptr<QoreIRFunction> handler_ir;
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
    bool weak = false;            //!< true for weak (:=) assignment in StoreLValue invoke path
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

    ~QoreIRSwitchRegexMatchInstruction() override {
        if (owns_regex_case) {
            delete const_cast<CaseNodeRegex*>(regex_case);
        }
    }

    const CaseNodeRegex* regex_case = nullptr;  //!< The regex case node containing the regex
    bool owns_regex_case = false;  //!< true when deserialized (we own the CaseNodeRegex)
};

//! Switch case match instruction - tests if switch value matches a case value with enum unwrapping
//! Uses CaseNode::matches() which unwraps TAG_ENUM from both sides before isEqualHard()
//! operands[0] is the switch value to test
//! result is a bool indicating match
class QoreIRSwitchCaseMatchInstruction : public QoreIRInstruction {
public:
    explicit QoreIRSwitchCaseMatchInstruction(const CaseNode* n_case_node)
            : QoreIRInstruction(QoreIROpcode::SwitchCaseMatch), case_node(n_case_node) {
    }

    ~QoreIRSwitchCaseMatchInstruction() override {
        if (owns_case_node) {
            delete case_node;
        }
    }

    const CaseNode* case_node = nullptr;  //!< The case node for match evaluation
    bool owns_case_node = false;  //!< True when case_node was created during deserialization
};

//! Reference foreach init instruction — stores the ParseReferenceNode expression
//! for AOT serialization (expr slot). Result is an opaque state handle.
class QoreIRRefForeachInitInstruction : public QoreIRInstruction {
public:
    explicit QoreIRRefForeachInitInstruction(const QoreValue& parse_ref_expr)
            : QoreIRInstruction(QoreIROpcode::RefForeachInit), expr(parse_ref_expr) {
        expr.ref();
    }

    ~QoreIRRefForeachInitInstruction() override {
        expr.discard(nullptr);
    }

    QoreValue expr;  //!< ParseReferenceNode expression
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

    //! True if this block contains phi nodes at the beginning.
    //! Set during computeSlotIdsAndEmbed() to optimize phi loop skipping.
    bool has_phi_nodes = false;
};

class QoreIRFunction {
public:
    explicit QoreIRFunction(std::string n_name) : name(std::move(n_name)) {
    }

    ~QoreIRFunction() {
        delete cached_pre_instantiated;
    }

    QoreIRBasicBlock* createBlock(const std::string& block_name) {
        auto block = std::make_unique<QoreIRBasicBlock>(block_name);
        QoreIRBasicBlock* ptr = block.get();
        blocks.push_back(std::move(block));
        return ptr;
    }

    //! Move an existing block to the end of the block list.
    //! This ensures it is processed after all subsequently-created blocks
    //! during LLVM lowering (which processes blocks in list order).
    void moveBlockToEnd(QoreIRBasicBlock* block) {
        auto it = std::find_if(blocks.begin(), blocks.end(),
                [block](const std::unique_ptr<QoreIRBasicBlock>& p) { return p.get() == block; });
        if (it != blocks.end()) {
            auto ptr = std::move(*it);
            blocks.erase(it);
            blocks.push_back(std::move(ptr));
        }
    }

    QoreIRValue createValue() {
        return QoreIRValue(next_value_id++);
    }

    std::string name;
    std::vector<std::unique_ptr<QoreIRBasicBlock>> blocks;

    // Maximum value ID assigned in this function (used to right-size value vector in interpreter)
    uint32_t max_value_id = 0;

    // Maximum local variable slot ID (used to right-size locals array in interpreter)
    // Maps LocalVar* pointers to their slot IDs for flat array access instead of hash maps
    uint32_t max_local_slot_id = 0;

    // Mapping of LocalVar* pointers to their slot IDs (computed during IR analysis)
    // Used by the IR interpreter to convert unordered_map lookups to direct array access
    std::unordered_map<const LocalVar*, uint32_t> local_var_slots;

    // Mapping of param index → slot_id (built during IR lowering).
    // Used by IRDirectParams to pre-populate the slot cache from caller-provided
    // values, bypassing TLS instantiate/eval/uninstantiate round-trip.
    std::unordered_map<int, uint32_t> param_slot_ids;

    // Mapping of param index → LocalVar* (built during IR lowering).
    // Used to access parameter type information during direct param processing.
    std::unordered_map<int, const LocalVar*> param_local_vars;

    //! True when all params are IR-only (not accessed by AST expression trees,
    //! not closure-captured, not reference-typed) AND no argvid is needed.
    //! When true, callers can use IRDirectParams to skip TLS entirely.
    bool direct_params_eligible = false;

    // Set of LocalVar* pointers that are already instantiated by the caller
    // (tiered compilation: params from setupCall(), argvid/selfid from evalTiered(),
    // all body locals from the statement tree).  The JIT must not
    // re-instantiate/uninstantiate these.
    std::unordered_set<const void*> pre_instantiated_locals;

    // Cached LocalVar* set for fast iteration in evalTiered() without per-call allocation.
    // Populated in attemptIRLowering() with the same sources as pre_instantiated_locals.
    std::unordered_set<const LocalVar*> pre_instantiated_cache;

    // Cached pre-instantiated set (params + argvid + ast_visible_body_locals)
    // This is built once during IR lowering and reused on every call.
    // selfid is handled separately since it's conditional on (self && selfid).
    mutable std::unordered_set<const LocalVar*>* cached_pre_instantiated = nullptr;

    // Return type info for the function (populated in attemptIRLowering()).
    // Used by Return opcode lowering in QoreIRToLLVM to apply type coercion.
    const QoreTypeInfo* return_type_info = nullptr;

    // All body locals from the statement tree (top-level + all nested blocks
    // from fully-lowered statements: if/for/while/try/switch).
    // Used by evalTiered() to instantiate/uninstantiate all locals before/after
    // IR or JIT execution, so that AST Invoke callbacks can find them on the
    // thread-local variable stack.
    std::vector<LocalVar*> all_body_locals;

    // Subset of all_body_locals that are NOT IR-only: must be pre-instantiated by
    // evalTiered() so AST Invoke callbacks can find them on the thread-local stack.
    // Populated by computeIROnlyLocals() after ir_only_locals is determined.
    std::vector<LocalVar*> ast_visible_body_locals;

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

    // Map from call instruction pointer → set of LocalVar* live immediately after
    // Used by liveness analysis to determine which locals need reloading after each call
    std::unordered_map<const QoreIRInstruction*, std::unordered_set<const LocalVar*>>
        live_locals_after_call;

    //! Compute slot IDs for local variables and embed them into instructions.
    //! Also computes max_value_id for right-sizing the interpreter value vector.
    //! Must be called after IR lowering and verification, before execution.
    void computeSlotIdsAndEmbed();

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
        case QoreIROpcode::BranchIfLtLocalInt:
            return true;
        default:
            return false;
    }
}

#endif
