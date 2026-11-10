/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreOpcodeRegistry.h - Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction.
*/

// the registry's rows name their QoreIROpcode: QoreIR.h defines the enum and then includes this file, so it is
// included before the guard, which makes the enum complete before the rows whichever of the two is included first
#include "qore/intern/QoreIR.h"

#ifndef _QORE_OPCODE_REGISTRY_H
#define _QORE_OPCODE_REGISTRY_H

#include <cstdint>
#include <string>

enum QoreOpcodePropertyFlag : uint32_t {
    QORE_OPCODE_PROPERTY_NONE = 0u,
    QORE_OPCODE_PROPERTY_CALL_INVOKE = 1u << 0,
    QORE_OPCODE_PROPERTY_REGEX_INVOKE = 1u << 1,
    QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE = 1u << 2,
    QORE_OPCODE_PROPERTY_RANGE_SLICE = 1u << 3,
};

//! Centralized metadata for IR opcodes - SINGLE SOURCE OF TRUTH
//!
//! All opcode properties are defined here. Query functions below provide
//! typed access. No manual switch statements should duplicate this data.
struct OpcodeInfo {
    QoreIROpcode opcode;                //! The opcode of the row, which must be its index; see OPCODE_REGISTRY
    const char* name;                   //! Opcode name (e.g., "LoadLocal")
    bool can_return_nothing;            //! Can legitimately return NOTHING
    bool never_returns_nothing;         //! Always returns non-NOTHING value
    bool is_terminator;                 //! Ends a basic block (control flow)
    int expected_operands;              //! Expected operand count (-1 = variable, <= -2 = minimum via OPCODE_MIN_OPERANDS)

    const char* description;            //! What the opcode does (for debugging/docs)
    bool may_have_side_effects;         //! Can modify global state (globals, statics, objects)
    bool may_throw_exception;           //! Can raise exceptions at runtime
    const char* corresponding_ast_node; //! AST node type this IR opcode comes from

    bool produces_result;               //! Instruction produces a result value (for verifier)
    bool optional_result;               //! Result is optional: verifier allows missing result even when produces_result is true
    bool skip_aot_expr_slot;            //! Skip EXPR_TREE slot when operands are pre-evaluated (AOT)
    bool is_unary_invoke;               //! Unary computation op (Invoke dispatches via qore_rt_unary_op)
    bool is_binary_invoke;              //! Binary computation op (Invoke dispatches via qore_rt_binary_op)
    uint32_t property_flags = QORE_OPCODE_PROPERTY_NONE; //! Additional conservative opcode-family flags

    constexpr bool hasProperty(QoreOpcodePropertyFlag flag) const {
        return (property_flags & flag) != 0;
    }
};

//! Encode a minimum operand count for variable-length instructions.
//! Value = -(min + 2): -2 means min=0, -3 means min=1, -5 means min=3.
//! The verifier checks operands.size() >= min instead of == exact.
#define OPCODE_MIN_OPERANDS(n) (-(static_cast<int>(n) + 2))

//! Registry of all IR opcodes (in enum ID order)
/** Row i is the metadata of the opcode with value i: lookups index the registry by the opcode's value.  Each row
    names its opcode, and a static_assert below checks that row i is opcode i, so that renumbering an opcode without
    moving its row (or adding an opcode without a row) fails to compile instead of giving an opcode the properties of
    another one.
*/
constexpr OpcodeInfo OPCODE_REGISTRY[QORE_IR_MAX_OPCODE + 1] = {
    { QoreIROpcode::ConstInt, "ConstInt"                      , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 0
    { QoreIROpcode::ConstFloat, "ConstFloat"                    , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 1
    { QoreIROpcode::ConstBool, "ConstBool"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 2
    { QoreIROpcode::ConstNothing, "ConstNothing"                  , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 3
    { QoreIROpcode::ConstNull, "ConstNull"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 4
    { QoreIROpcode::ConstString, "ConstString"                   , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 5
    { QoreIROpcode::ConstDate, "ConstDate"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 6
    { QoreIROpcode::MakeList, "MakeList"                      , false, false, false, -1, "MakeList", false, true , "ParseNode", true , false, false, false, false }, // 7
    { QoreIROpcode::MakeHash, "MakeHash"                      , false, false, false, -1, "MakeHash", false, true , "ParseNode", true , false, false, false, false }, // 8
    { QoreIROpcode::CreateEmptyList, "CreateEmptyList"               , false, false, false,  0, "CreateEmptyList", false, true , "ParseNode", true , false, false, false, false }, // 9
    { QoreIROpcode::CreateSizedList, "CreateSizedList"               , false, false, false,  1, "CreateSizedList", false, true , "ParseNode", true , false, false, false, false }, // 10
    { QoreIROpcode::ListAppend, "ListAppend"                    , false, false, false,  2, "ListAppend", false, true , "ParseNode", false, false, false, false, false }, // 11
    { QoreIROpcode::ListSize, "ListSize"                      , false, false, false,  1, "ListSize", false, true , "ParseNode", true , false, false, false, false }, // 12
    { QoreIROpcode::ListGetInt, "ListGetInt"                    , false, false, false,  2, "ListGetInt", false, true , "ParseNode", true , false, false, false, false }, // 13
    { QoreIROpcode::ListGetFloat, "ListGetFloat"                  , false, false, false,  2, "ListGetFloat", false, true , "ParseNode", true , false, false, false, false }, // 14
    { QoreIROpcode::ListGetValue, "ListGetValue"                  , false, false, false,  2, "ListGetValue", false, true , "ParseNode", true , false, false, false, false }, // 15
    { QoreIROpcode::ListSetInt, "ListSetInt"                    , false, false, false,  3, "ListSetInt", false, true , "ParseNode", false, false, false, false, false }, // 16
    { QoreIROpcode::ListSetFloat, "ListSetFloat"                  , false, false, false,  3, "ListSetFloat", false, true , "ParseNode", false, false, false, false, false }, // 17
    { QoreIROpcode::ListSetValue, "ListSetValue"                  , false, false, false,  3, "ListSetValue", false, true , "ParseNode", false, false, false, false, false }, // 18
    { QoreIROpcode::AddInt, "AddInt"                        , false, true , false,  2, "AddInt", false, false, "ParseNode", true , false, true , false, true  }, // 19
    { QoreIROpcode::AddFloat, "AddFloat"                      , false, true , false,  2, "AddFloat", false, false, "ParseNode", true , false, true , false, true  }, // 20
    { QoreIROpcode::AddAny, "AddAny"                        , false, false, false,  2, "AddAny", false, true , "ParseNode", true , false, true , false, true  }, // 21
    { QoreIROpcode::AddString, "AddString"                     , false, false, false,  2, "AddString", false, true , "ParseNode", true , false, false, false, false }, // 22
    { QoreIROpcode::StringConcat, "StringConcat"                  , false, false, false, -1, "StringConcat", false, true , "ParseNode", true , false, false, false, false }, // 23
    { QoreIROpcode::SubInt, "SubInt"                        , false, true , false,  2, "SubInt", false, false, "ParseNode", true , false, true , false, true  }, // 24
    { QoreIROpcode::SubFloat, "SubFloat"                      , false, true , false,  2, "SubFloat", false, false, "ParseNode", true , false, true , false, true  }, // 25
    { QoreIROpcode::SubAny, "SubAny"                        , false, false, false,  2, "SubAny", false, true , "ParseNode", true , false, true , false, true  }, // 26
    { QoreIROpcode::MulInt, "MulInt"                        , false, true , false,  2, "MulInt", false, false, "ParseNode", true , false, true , false, true  }, // 27
    { QoreIROpcode::MulFloat, "MulFloat"                      , false, true , false,  2, "MulFloat", false, false, "ParseNode", true , false, true , false, true  }, // 28
    { QoreIROpcode::MulAny, "MulAny"                        , false, false, false,  2, "MulAny", false, true , "ParseNode", true , false, true , false, true  }, // 29
    { QoreIROpcode::DivInt, "DivInt"                        , false, true , false,  2, "DivInt", false, true , "ParseNode", true , false, true , false, true  }, // 30
    { QoreIROpcode::DivFloat, "DivFloat"                      , false, true , false,  2, "DivFloat", false, true , "ParseNode", true , false, true , false, true  }, // 31
    { QoreIROpcode::DivAny, "DivAny"                        , false, false, false,  2, "DivAny", false, true , "ParseNode", true , false, true , false, true  }, // 32
    { QoreIROpcode::ModInt, "ModInt"                        , false, true , false,  2, "ModInt", false, true , "ParseNode", true , false, true , false, true  }, // 33
    { QoreIROpcode::ModAny, "ModAny"                        , false, false, false,  2, "ModAny", false, true , "ParseNode", true , false, true , false, true  }, // 34
    { QoreIROpcode::AndInt, "AndInt"                        , false, true , false,  2, "AndInt", false, false, "ParseNode", true , false, true , false, true  }, // 35
    { QoreIROpcode::AndAny, "AndAny"                        , false, false, false,  2, "AndAny", false, true , "ParseNode", true , false, true , false, true  }, // 36
    { QoreIROpcode::OrInt, "OrInt"                         , false, true , false,  2, "OrInt", false, false, "ParseNode", true , false, true , false, true  }, // 37
    { QoreIROpcode::OrAny, "OrAny"                         , false, false, false,  2, "OrAny", false, true , "ParseNode", true , false, true , false, true  }, // 38
    { QoreIROpcode::XorInt, "XorInt"                        , false, true , false,  2, "XorInt", false, false, "ParseNode", true , false, true , false, true  }, // 39
    { QoreIROpcode::XorAny, "XorAny"                        , false, false, false,  2, "XorAny", false, true , "ParseNode", true , false, true , false, true  }, // 40
    { QoreIROpcode::ShlInt, "ShlInt"                        , false, true , false,  2, "ShlInt", false, false, "ParseNode", true , false, true , false, true  }, // 41
    { QoreIROpcode::ShlAny, "ShlAny"                        , false, false, false,  2, "ShlAny", false, true , "ParseNode", true , false, true , false, true  }, // 42
    { QoreIROpcode::ShrInt, "ShrInt"                        , false, true , false,  2, "ShrInt", false, false, "ParseNode", true , false, true , false, true  }, // 43
    { QoreIROpcode::ShrAny, "ShrAny"                        , false, false, false,  2, "ShrAny", false, true , "ParseNode", true , false, true , false, true  }, // 44
    { QoreIROpcode::ShlAssignInt, "ShlAssignInt"                  , false, true , false,  2, "ShlAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 45
    { QoreIROpcode::ShlAssignAny, "ShlAssignAny"                  , false, false, false,  2, "ShlAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 46
    { QoreIROpcode::ShrAssignInt, "ShrAssignInt"                  , false, true , false,  2, "ShrAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 47
    { QoreIROpcode::ShrAssignAny, "ShrAssignAny"                  , false, false, false,  2, "ShrAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 48
    { QoreIROpcode::AddAssignInt, "AddAssignInt"                  , false, true , false,  2, "AddAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 49
    { QoreIROpcode::AddAssignFloat, "AddAssignFloat"                , false, true , false,  2, "AddAssignFloat", false, false, "ParseNode", true , false, true , false, true  }, // 50
    { QoreIROpcode::AddAssignAny, "AddAssignAny"                  , false, false, false,  2, "AddAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 51
    { QoreIROpcode::SubAssignInt, "SubAssignInt"                  , false, true , false,  2, "SubAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 52
    { QoreIROpcode::SubAssignFloat, "SubAssignFloat"                , false, true , false,  2, "SubAssignFloat", false, false, "ParseNode", true , false, true , false, true  }, // 53
    { QoreIROpcode::SubAssignAny, "SubAssignAny"                  , false, false, false,  2, "SubAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 54
    { QoreIROpcode::MulAssignInt, "MulAssignInt"                  , false, true , false,  2, "MulAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 55
    { QoreIROpcode::MulAssignFloat, "MulAssignFloat"                , false, true , false,  2, "MulAssignFloat", false, false, "ParseNode", true , false, true , false, true  }, // 56
    { QoreIROpcode::MulAssignAny, "MulAssignAny"                  , false, false, false,  2, "MulAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 57
    { QoreIROpcode::DivAssignInt, "DivAssignInt"                  , false, true , false,  2, "DivAssignInt", false, true , "ParseNode", true , false, true , false, true  }, // 58
    { QoreIROpcode::DivAssignFloat, "DivAssignFloat"                , false, true , false,  2, "DivAssignFloat", false, true , "ParseNode", true , false, true , false, true  }, // 59
    { QoreIROpcode::DivAssignAny, "DivAssignAny"                  , false, false, false,  2, "DivAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 60
    { QoreIROpcode::ModAssignInt, "ModAssignInt"                  , false, true , false,  2, "ModAssignInt", false, true , "ParseNode", true , false, true , false, true  }, // 61
    { QoreIROpcode::ModAssignAny, "ModAssignAny"                  , false, false, false,  2, "ModAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 62
    { QoreIROpcode::AndAssignInt, "AndAssignInt"                  , false, true , false,  2, "AndAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 63
    { QoreIROpcode::AndAssignAny, "AndAssignAny"                  , false, false, false,  2, "AndAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 64
    { QoreIROpcode::OrAssignInt, "OrAssignInt"                   , false, true , false,  2, "OrAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 65
    { QoreIROpcode::OrAssignAny, "OrAssignAny"                   , false, false, false,  2, "OrAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 66
    { QoreIROpcode::XorAssignInt, "XorAssignInt"                  , false, true , false,  2, "XorAssignInt", false, false, "ParseNode", true , false, true , false, true  }, // 67
    { QoreIROpcode::XorAssignAny, "XorAssignAny"                  , false, false, false,  2, "XorAssignAny", false, true , "ParseNode", true , false, true , false, true  }, // 68
    { QoreIROpcode::LoadLValue, "LoadLValue"                    , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 69
    { QoreIROpcode::StoreLValue, "StoreLValue"                   , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false, false }, // 70
    { QoreIROpcode::PreIncLValue, "PreIncLValue"                  , false, false, false,  0, "PreIncLValue", false, true , "ParseNode", true , false, false, false, false }, // 71
    { QoreIROpcode::PreDecLValue, "PreDecLValue"                  , false, false, false,  0, "PreDecLValue", false, true , "ParseNode", true , false, false, false, false }, // 72
    { QoreIROpcode::PostIncLValue, "PostIncLValue"                 , false, false, false,  0, "PostIncLValue", false, true , "ParseNode", true , false, false, false, false }, // 73
    { QoreIROpcode::PostDecLValue, "PostDecLValue"                 , false, false, false,  0, "PostDecLValue", false, true , "ParseNode", true , false, false, false, false }, // 74
    { QoreIROpcode::AddAssignLValue, "AddAssignLValue"               , false, false, false,  1, "AddAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 75
    { QoreIROpcode::SubAssignLValue, "SubAssignLValue"               , false, false, false,  1, "SubAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 76
    { QoreIROpcode::MulAssignLValue, "MulAssignLValue"               , false, false, false,  1, "MulAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 77
    { QoreIROpcode::DivAssignLValue, "DivAssignLValue"               , false, false, false,  1, "DivAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 78
    { QoreIROpcode::ModAssignLValue, "ModAssignLValue"               , false, false, false,  1, "ModAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 79
    { QoreIROpcode::AndAssignLValue, "AndAssignLValue"               , false, false, false,  1, "AndAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 80
    { QoreIROpcode::OrAssignLValue, "OrAssignLValue"                , false, false, false,  1, "OrAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 81
    { QoreIROpcode::XorAssignLValue, "XorAssignLValue"               , false, false, false,  1, "XorAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 82
    { QoreIROpcode::ShlAssignLValue, "ShlAssignLValue"               , false, false, false,  1, "ShlAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 83
    { QoreIROpcode::ShrAssignLValue, "ShrAssignLValue"               , false, false, false,  1, "ShrAssignLValue", false, true , "ParseNode", true , false, false, false, false }, // 84
    { QoreIROpcode::ShiftLValue, "ShiftLValue"                   , false, false, false,  0, "ShiftLValue", false, true , "ParseNode", true , false, false, false, false }, // 85
    { QoreIROpcode::UnshiftLValue, "UnshiftLValue"                 , false, false, false,  1, "UnshiftLValue", false, true , "ParseNode", true , false, false, false, false }, // 86
    { QoreIROpcode::PopAny, "PopAny"                        , false, false, false,  0, "PopAny", false, true , "ParseNode", true , false, false, false, false }, // 87
    { QoreIROpcode::PushAny, "PushAny"                       , false, false, false,  0, "PushAny", false, true , "ParseNode", true , false, false, false, false }, // 88
    { QoreIROpcode::SpliceLValue, "SpliceLValue"                  , false, false, false,  3, "SpliceLValue", false, true , "ParseNode", true , false, false, false, false }, // 89
    { QoreIROpcode::ExtractAny, "ExtractAny"                    , false, false, false,  4, "ExtractAny", false, true , "ParseNode", true , false, false, false, false }, // 90
    { QoreIROpcode::ExtractList, "ExtractList"                   , false, false, false,  4, "ExtractList", false, true , "ParseNode", true , false, false, false, false }, // 91
    { QoreIROpcode::ExtractString, "ExtractString"                 , false, false, false,  4, "ExtractString", false, true , "ParseNode", true , false, false, false, false }, // 92
    { QoreIROpcode::ExtractBinary, "ExtractBinary"                 , false, false, false,  4, "ExtractBinary", false, true , "ParseNode", true , false, false, false, false }, // 93
    { QoreIROpcode::RemoveAny, "RemoveAny"                     , false, false, false,  1, "RemoveAny", false, true , "ParseNode", true , false, false, false, false }, // 94
    { QoreIROpcode::RemoveList, "RemoveList"                    , false, false, false,  1, "RemoveList", false, true , "ParseNode", true , false, false, false, false }, // 95
    { QoreIROpcode::RemoveHash, "RemoveHash"                    , false, false, false,  1, "RemoveHash", false, true , "ParseNode", true , false, false, false, false }, // 96
    { QoreIROpcode::RemoveObject, "RemoveObject"                  , false, false, false,  1, "RemoveObject", false, true , "ParseNode", true , false, false, false, false }, // 97
    { QoreIROpcode::RemoveString, "RemoveString"                  , false, false, false,  1, "RemoveString", false, true , "ParseNode", true , false, false, false, false }, // 98
    { QoreIROpcode::RemoveBinary, "RemoveBinary"                  , false, false, false,  1, "RemoveBinary", false, true , "ParseNode", true , false, false, false, false }, // 99
    { QoreIROpcode::KeysAny, "KeysAny"                       , false, false, false,  1, "KeysAny", false, true , "ParseNode", true , false, true , true , false }, // 100
    { QoreIROpcode::KeysList, "KeysList"                      , false, false, false,  1, "KeysList", false, true , "ParseNode", true , false, true , true , false }, // 101
    { QoreIROpcode::KeysHash, "KeysHash"                      , false, false, false,  1, "KeysHash", false, true , "ParseNode", true , false, true , true , false }, // 102
    { QoreIROpcode::RegexMatchAny, "RegexMatchAny"                 , false, false, false,  1, "RegexMatchAny", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_REGEX_INVOKE }, // 103
    { QoreIROpcode::RegexMatchBool, "RegexMatchBool"                , false, true , false,  1, "RegexMatchBool", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_REGEX_INVOKE }, // 104
    { QoreIROpcode::RegexNMatchBool, "RegexNMatchBool"               , false, true , false,  1, "RegexNMatchBool", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_REGEX_INVOKE }, // 105
    { QoreIROpcode::RegexExtractAny, "RegexExtractAny"               , false, false, false,  1, "RegexExtractAny", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_REGEX_INVOKE }, // 106
    { QoreIROpcode::RegexExtractList, "RegexExtractList"              , false, false, false,  1, "RegexExtractList", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_REGEX_INVOKE }, // 107
    { QoreIROpcode::RegexSubstAny, "RegexSubstAny"                 , false, false, false,  1, "RegexSubstAny", false, true , "ParseNode", true , false, false, false, false }, // 108
    { QoreIROpcode::RegexSubstString, "RegexSubstString"              , false, true , false,  1, "RegexSubstString", false, true , "ParseNode", true , false, false, false, false }, // 109
    { QoreIROpcode::InstanceOfBool, "InstanceOfBool"                , false, true , false,  1, "InstanceOfBool", false, true , "ParseNode", true , false, true , false, false }, // 110
    { QoreIROpcode::TrimAny, "TrimAny"                       , false, false, false,  0, "TrimAny", false, true , "ParseNode", true , false, false, false, false }, // 111
    { QoreIROpcode::TrimString, "TrimString"                    , false, true , false,  0, "TrimString", false, true , "ParseNode", true , false, false, false, false }, // 112
    { QoreIROpcode::ChompAny, "ChompAny"                      , false, false, false,  0, "ChompAny", false, true , "ParseNode", true , false, false, false, false }, // 113
    { QoreIROpcode::ChompString, "ChompString"                   , false, true , false,  0, "ChompString", false, true , "ParseNode", true , false, false, false, false }, // 114
    { QoreIROpcode::TransliterateAny, "TransliterateAny"              , false, true , false,  0, "TransliterateAny", false, true , "ParseNode", true , false, false, false, false }, // 115
    { QoreIROpcode::TransliterateString, "TransliterateString"           , false, true , false,  0, "TransliterateString", false, true , "ParseNode", true , false, false, false, false }, // 116
    { QoreIROpcode::BackgroundInt, "BackgroundInt"                 , false, true , false, OPCODE_MIN_OPERANDS(0), "BackgroundInt", false, true , "ParseNode", true , false, true , false, false }, // 117
    { QoreIROpcode::ListAssignAny, "ListAssignAny"                 , false, false, false, OPCODE_MIN_OPERANDS(0), "ListAssignAny", false, true , "ParseNode", true , false, false, false, false }, // 118
    { QoreIROpcode::ExistsAny, "ExistsAny"                     , false, false, false,  1, "ExistsAny", false, true , "ParseNode", true , false, true , true , false }, // 119
    { QoreIROpcode::ExistsBool, "ExistsBool"                    , false, true , false,  1, "ExistsBool", false, true , "ParseNode", true , false, true , true , false }, // 120
    { QoreIROpcode::ElementsAny, "ElementsAny"                   , false, false, false,  1, "ElementsAny", false, true , "ParseNode", true , false, true , true , false }, // 121
    { QoreIROpcode::ElementsInt, "ElementsInt"                   , false, true , false,  1, "ElementsInt", false, true , "ParseNode", true , false, true , true , false }, // 122
    { QoreIROpcode::DotEvalAny, "DotEvalAny"                    , false, false, false,  1, "DotEvalAny", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 123
    { QoreIROpcode::DotEvalInt, "DotEvalInt"                    , false, false, false,  1, "DotEvalInt", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 124
    { QoreIROpcode::DotEvalFloat, "DotEvalFloat"                  , false, false, false,  1, "DotEvalFloat", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 125
    { QoreIROpcode::DotEvalString, "DotEvalString"                 , false, false, false,  1, "DotEvalString", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 126
    { QoreIROpcode::DotEvalDate, "DotEvalDate"                   , false, false, false,  1, "DotEvalDate", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 127
    { QoreIROpcode::DotEvalList, "DotEvalList"                   , false, false, false,  1, "DotEvalList", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 128
    { QoreIROpcode::DotEvalHash, "DotEvalHash"                   , false, false, false,  1, "DotEvalHash", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 129
    { QoreIROpcode::DotEvalObject, "DotEvalObject"                 , false, false, false,  1, "DotEvalObject", false, true , "ParseNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE }, // 130
    { QoreIROpcode::MapSelectList, "MapSelectList"                 , false, false, false,  3, "MapSelectList", false, true , "ParseNode", true , false, false, false, false }, // 131
    { QoreIROpcode::HashMap, "HashMap"                       , false, false, false,  3, "HashMap", false, true , "ParseNode", true , false, false, false, false }, // 132
    { QoreIROpcode::HashMapSelect, "HashMapSelect"                 , false, false, false,  4, "HashMapSelect", false, true , "ParseNode", true , false, false, false, false }, // 133
    { QoreIROpcode::IteratorCreate, "IteratorCreate"                , false, false, false,  0, "IteratorCreate", false, true , "ParseNode", true , false, false, false, false }, // 134
    { QoreIROpcode::IteratorNext, "IteratorNext"                  , false, false, true ,  0, "IteratorNext", false, true , "ParseNode", true , false, false, false, false }, // 135
    { QoreIROpcode::OnBlockExit, "OnBlockExit"                   , false, false, false,  0, "OnBlockExit", false, true , "ParseNode", false, false, false, false, false }, // 136
    { QoreIROpcode::ScopeEnter, "ScopeEnter"                    , false, false, false,  0, "ScopeEnter", false, true , "ParseNode", false, false, false, false, false }, // 137
    { QoreIROpcode::ScopeExit, "ScopeExit"                     , false, false, false,  0, "ScopeExit", false, true , "ParseNode", false, false, false, false, false }, // 138
    { QoreIROpcode::ThreadExit, "ThreadExit"                    , false, false, true ,  0, "ThreadExit", false, true , "ParseNode", false, false, false, false, false }, // 139
    { QoreIROpcode::Context, "Context"                       , false, false, false,  0, "ContextInit: create Context frame from data+where+sort", true , true , "ParseNode", true , false, false, false, false }, // 140
    { QoreIROpcode::Summarize, "Summarize"                     , false, false, false,  0, "Summarize", false, true , "ParseNode", false, false, false, false, false }, // 141
    { QoreIROpcode::EqInt, "EqInt"                         , false, true , false,  2, "EqInt", false, true , "ParseNode", true , false, true , false, true  }, // 142
    { QoreIROpcode::EqFloat, "EqFloat"                       , false, true , false,  2, "EqFloat", false, true , "ParseNode", true , false, true , false, true  }, // 143
    { QoreIROpcode::EqString, "EqString"                      , false, false, false,  2, "EqString", false, true , "ParseNode", true , false, false, false, false }, // 144
    { QoreIROpcode::EqAny, "EqAny"                         , false, true , false,  2, "EqAny", false, true , "ParseNode", true , false, true , false, true  }, // 145
    { QoreIROpcode::NeInt, "NeInt"                         , false, true , false,  2, "NeInt", false, true , "ParseNode", true , false, true , false, true  }, // 146
    { QoreIROpcode::NeFloat, "NeFloat"                       , false, true , false,  2, "NeFloat", false, true , "ParseNode", true , false, true , false, true  }, // 147
    { QoreIROpcode::NeString, "NeString"                      , false, false, false,  2, "NeString", false, true , "ParseNode", true , false, false, false, false }, // 148
    { QoreIROpcode::NeAny, "NeAny"                         , false, true , false,  2, "NeAny", false, true , "ParseNode", true , false, true , false, true  }, // 149
    { QoreIROpcode::EqHard, "EqHard"                        , false, true , false,  2, "EqHard", false, true , "ParseNode", true , false, true , false, true  }, // 150
    { QoreIROpcode::NeHard, "NeHard"                        , false, true , false,  2, "NeHard", false, true , "ParseNode", true , false, true , false, true  }, // 151
    { QoreIROpcode::LtInt, "LtInt"                         , false, true , false,  2, "LtInt", false, true , "ParseNode", true , false, true , false, true  }, // 152
    { QoreIROpcode::LtFloat, "LtFloat"                       , false, true , false,  2, "LtFloat", false, true , "ParseNode", true , false, true , false, true  }, // 153
    { QoreIROpcode::LtString, "LtString"                      , false, false, false,  2, "LtString", false, true , "ParseNode", true , false, false, false, false }, // 154
    { QoreIROpcode::LtAny, "LtAny"                         , false, true , false,  2, "LtAny", false, true , "ParseNode", true , false, true , false, true  }, // 155
    { QoreIROpcode::LeInt, "LeInt"                         , false, true , false,  2, "LeInt", false, true , "ParseNode", true , false, true , false, true  }, // 156
    { QoreIROpcode::LeFloat, "LeFloat"                       , false, true , false,  2, "LeFloat", false, true , "ParseNode", true , false, true , false, true  }, // 157
    { QoreIROpcode::LeString, "LeString"                      , false, false, false,  2, "LeString", false, true , "ParseNode", true , false, false, false, false }, // 158
    { QoreIROpcode::LeAny, "LeAny"                         , false, true , false,  2, "LeAny", false, true , "ParseNode", true , false, true , false, true  }, // 159
    { QoreIROpcode::GtInt, "GtInt"                         , false, true , false,  2, "GtInt", false, true , "ParseNode", true , false, true , false, true  }, // 160
    { QoreIROpcode::GtFloat, "GtFloat"                       , false, true , false,  2, "GtFloat", false, true , "ParseNode", true , false, true , false, true  }, // 161
    { QoreIROpcode::GtString, "GtString"                      , false, false, false,  2, "GtString", false, true , "ParseNode", true , false, false, false, false }, // 162
    { QoreIROpcode::GtAny, "GtAny"                         , false, true , false,  2, "GtAny", false, true , "ParseNode", true , false, true , false, true  }, // 163
    { QoreIROpcode::GeInt, "GeInt"                         , false, true , false,  2, "GeInt", false, true , "ParseNode", true , false, true , false, true  }, // 164
    { QoreIROpcode::GeFloat, "GeFloat"                       , false, true , false,  2, "GeFloat", false, true , "ParseNode", true , false, true , false, true  }, // 165
    { QoreIROpcode::GeString, "GeString"                      , false, false, false,  2, "GeString", false, true , "ParseNode", true , false, false, false, false }, // 166
    { QoreIROpcode::GeAny, "GeAny"                         , false, true , false,  2, "GeAny", false, true , "ParseNode", true , false, true , false, true  }, // 167
    { QoreIROpcode::CmpInt, "CmpInt"                        , false, true , false,  2, "CmpInt", false, true , "ParseNode", true , false, true , false, true  }, // 168
    { QoreIROpcode::CmpFloat, "CmpFloat"                      , false, true , false,  2, "CmpFloat", false, true , "ParseNode", true , false, true , false, true  }, // 169
    { QoreIROpcode::CmpString, "CmpString"                     , false, false, false,  2, "CmpString", false, true , "ParseNode", true , false, false, false, false }, // 170
    { QoreIROpcode::CmpAny, "CmpAny"                        , false, true , false,  2, "CmpAny", false, true , "ParseNode", true , false, true , false, true  }, // 171
    { QoreIROpcode::ToBool, "ToBool"                        , false, true , false,  1, "ToBool", false, true , "ParseNode", true , false, true , true , false }, // 172
    { QoreIROpcode::Not, "Not"                           , false, true , false,  1, "Not", false, true , "ParseNode", true , false, true , true , false }, // 173
    { QoreIROpcode::IsNullOrNothing, "IsNullOrNothing"               , false, true , false,  1, "IsNullOrNothing", false, true , "ParseNode", true , false, true , true , false }, // 174
    { QoreIROpcode::Phi, "Phi"                           , false, false, false, -1, "Phi", false, true , "ParseNode", true , false, false, false, false }, // 175
    { QoreIROpcode::UnaryPlusAny, "UnaryPlusAny"                  , false, false, false,  1, "UnaryPlusAny", false, true , "ParseNode", true , false, true , true , false }, // 176
    { QoreIROpcode::UnaryMinusInt, "UnaryMinusInt"                 , false, true , false,  1, "UnaryMinusInt", false, true , "ParseNode", true , false, true , true , false }, // 177
    { QoreIROpcode::UnaryMinusFloat, "UnaryMinusFloat"               , false, true , false,  1, "UnaryMinusFloat", false, true , "ParseNode", true , false, true , true , false }, // 178
    { QoreIROpcode::UnaryMinusAny, "UnaryMinusAny"                 , false, false, false,  1, "UnaryMinusAny", false, true , "ParseNode", true , false, true , true , false }, // 179
    { QoreIROpcode::FoldlAny, "FoldlAny"                      , false, false, false,  2, "FoldlAny", false, true , "ParseNode", true , false, true , false, true  }, // 180
    { QoreIROpcode::FoldlInt, "FoldlInt"                      , false, false, false,  2, "FoldlInt", false, true , "ParseNode", true , false, true , false, true  }, // 181
    { QoreIROpcode::FoldlFloat, "FoldlFloat"                    , false, false, false,  2, "FoldlFloat", false, true , "ParseNode", true , false, true , false, true  }, // 182
    { QoreIROpcode::FoldrAny, "FoldrAny"                      , false, false, false,  2, "FoldrAny", false, true , "ParseNode", true , false, true , false, true  }, // 183
    { QoreIROpcode::FoldrInt, "FoldrInt"                      , false, false, false,  2, "FoldrInt", false, true , "ParseNode", true , false, true , false, true  }, // 184
    { QoreIROpcode::FoldrFloat, "FoldrFloat"                    , false, false, false,  2, "FoldrFloat", false, true , "ParseNode", true , false, true , false, true  }, // 185
    { QoreIROpcode::FoldlSumInt, "FoldlSumInt"                   , false, false, false,  2, "FoldlSumInt", false, true , "ParseNode", true , false, true , false, true  }, // 186
    { QoreIROpcode::FoldlSumFloat, "FoldlSumFloat"                 , false, false, false,  2, "FoldlSumFloat", false, true , "ParseNode", true , false, true , false, true  }, // 187
    { QoreIROpcode::FoldlProdInt, "FoldlProdInt"                  , false, false, false,  2, "FoldlProdInt", false, true , "ParseNode", true , false, true , false, true  }, // 188
    { QoreIROpcode::FoldlProdFloat, "FoldlProdFloat"                , false, false, false,  2, "FoldlProdFloat", false, true , "ParseNode", true , false, true , false, true  }, // 189
    { QoreIROpcode::FoldlDiffInt, "FoldlDiffInt"                  , false, false, false,  2, "FoldlDiffInt", false, true , "ParseNode", true , false, true , false, true  }, // 190
    { QoreIROpcode::FoldlDiffFloat, "FoldlDiffFloat"                , false, false, false,  2, "FoldlDiffFloat", false, true , "ParseNode", true , false, true , false, true  }, // 191
    { QoreIROpcode::FoldlMinInt, "FoldlMinInt"                   , false, false, false,  2, "FoldlMinInt", false, true , "ParseNode", true , false, true , false, true  }, // 192
    { QoreIROpcode::FoldlMinFloat, "FoldlMinFloat"                 , false, false, false,  2, "FoldlMinFloat", false, true , "ParseNode", true , false, true , false, true  }, // 193
    { QoreIROpcode::FoldlMaxInt, "FoldlMaxInt"                   , false, false, false,  2, "FoldlMaxInt", false, true , "ParseNode", true , false, true , false, true  }, // 194
    { QoreIROpcode::FoldlMaxFloat, "FoldlMaxFloat"                 , false, false, false,  2, "FoldlMaxFloat", false, true , "ParseNode", true , false, true , false, true  }, // 195
    { QoreIROpcode::FoldrSumInt, "FoldrSumInt"                   , false, false, false,  2, "FoldrSumInt", false, true , "ParseNode", true , false, true , false, true  }, // 196
    { QoreIROpcode::FoldrSumFloat, "FoldrSumFloat"                 , false, false, false,  2, "FoldrSumFloat", false, true , "ParseNode", true , false, true , false, true  }, // 197
    { QoreIROpcode::FoldrProdInt, "FoldrProdInt"                  , false, false, false,  2, "FoldrProdInt", false, true , "ParseNode", true , false, true , false, true  }, // 198
    { QoreIROpcode::FoldrProdFloat, "FoldrProdFloat"                , false, false, false,  2, "FoldrProdFloat", false, true , "ParseNode", true , false, true , false, true  }, // 199
    { QoreIROpcode::FoldrDiffInt, "FoldrDiffInt"                  , false, false, false,  2, "FoldrDiffInt", false, true , "ParseNode", true , false, true , false, true  }, // 200
    { QoreIROpcode::FoldrDiffFloat, "FoldrDiffFloat"                , false, false, false,  2, "FoldrDiffFloat", false, true , "ParseNode", true , false, true , false, true  }, // 201
    { QoreIROpcode::FoldrMinInt, "FoldrMinInt"                   , false, false, false,  2, "FoldrMinInt", false, true , "ParseNode", true , false, true , false, true  }, // 202
    { QoreIROpcode::FoldrMinFloat, "FoldrMinFloat"                 , false, false, false,  2, "FoldrMinFloat", false, true , "ParseNode", true , false, true , false, true  }, // 203
    { QoreIROpcode::FoldrMaxInt, "FoldrMaxInt"                   , false, false, false,  2, "FoldrMaxInt", false, true , "ParseNode", true , false, true , false, true  }, // 204
    { QoreIROpcode::FoldrMaxFloat, "FoldrMaxFloat"                 , false, false, false,  2, "FoldrMaxFloat", false, true , "ParseNode", true , false, true , false, true  }, // 205
    { QoreIROpcode::MapAny, "MapAny"                        , false, false, false,  2, "MapAny", false, true , "ParseNode", true , false, true , false, true  }, // 206
    { QoreIROpcode::MapInt, "MapInt"                        , false, false, false,  2, "MapInt", false, true , "ParseNode", true , false, true , false, true  }, // 207
    { QoreIROpcode::MapFloat, "MapFloat"                      , false, false, false,  2, "MapFloat", false, true , "ParseNode", true , false, true , false, true  }, // 208
    { QoreIROpcode::MapScaleInt, "MapScaleInt"                   , false, false, false,  2, "MapScaleInt", false, true , "ParseNode", true , false, false, false, false }, // 209
    { QoreIROpcode::MapScaleFloat, "MapScaleFloat"                 , false, false, false,  2, "MapScaleFloat", false, true , "ParseNode", true , false, false, false, false }, // 210
    { QoreIROpcode::MapOffsetInt, "MapOffsetInt"                  , false, false, false,  2, "MapOffsetInt", false, true , "ParseNode", true , false, false, false, false }, // 211
    { QoreIROpcode::MapOffsetFloat, "MapOffsetFloat"                , false, false, false,  2, "MapOffsetFloat", false, true , "ParseNode", true , false, false, false, false }, // 212
    { QoreIROpcode::MapSquareInt, "MapSquareInt"                  , false, false, false,  2, "MapSquareInt", false, true , "ParseNode", true , false, false, false, false }, // 213
    { QoreIROpcode::MapSquareFloat, "MapSquareFloat"                , false, false, false,  2, "MapSquareFloat", false, true , "ParseNode", true , false, false, false, false }, // 214
    { QoreIROpcode::MapHashKeyValue, "MapHashKeyValue"               , false, false, false,  1, "MapHashKeyValue", false, true , "ParseNode", true , false, false, false, false }, // 215
    { QoreIROpcode::MapHashKeyInt, "MapHashKeyInt"                 , false, false, false,  1, "MapHashKeyInt", false, true , "ParseNode", true , false, false, false, false }, // 216
    { QoreIROpcode::MapHashKeyOffsetInt, "MapHashKeyOffsetInt"           , false, false, false,  2, "MapHashKeyOffsetInt", false, true , "ParseNode", true , false, false, false, false }, // 217
    { QoreIROpcode::MapHashKeyScaleInt, "MapHashKeyScaleInt"            , false, false, false,  2, "MapHashKeyScaleInt", false, true , "ParseNode", true , false, false, false, false }, // 218
    { QoreIROpcode::HashMapTwoKeys, "HashMapTwoKeys"                , false, false, false,  1, "HashMapTwoKeys", false, true , "ParseNode", true , false, false, false, false }, // 219
    { QoreIROpcode::SelectAny, "SelectAny"                     , false, false, false,  2, "SelectAny", false, true , "ParseNode", true , false, true , false, true  }, // 220
    { QoreIROpcode::SelectInt, "SelectInt"                     , false, false, false,  2, "SelectInt", false, true , "ParseNode", true , false, true , false, true  }, // 221
    { QoreIROpcode::SelectFloat, "SelectFloat"                   , false, false, false,  2, "SelectFloat", false, true , "ParseNode", true , false, true , false, true  }, // 222
    { QoreIROpcode::SelectPositiveInt, "SelectPositiveInt"             , false, false, false,  2, "SelectPositiveInt", false, true , "ParseNode", true , false, false, false, false }, // 223
    { QoreIROpcode::SelectPositiveFloat, "SelectPositiveFloat"           , false, false, false,  2, "SelectPositiveFloat", false, true , "ParseNode", true , false, false, false, false }, // 224
    { QoreIROpcode::SelectNonZeroInt, "SelectNonZeroInt"              , false, false, false,  2, "SelectNonZeroInt", false, true , "ParseNode", true , false, false, false, false }, // 225
    { QoreIROpcode::SelectNonZeroFloat, "SelectNonZeroFloat"            , false, false, false,  2, "SelectNonZeroFloat", false, true , "ParseNode", true , false, false, false, false }, // 226
    { QoreIROpcode::FusedMapSelectScalePositiveInt, "FusedMapSelectScalePositiveInt", false, false, false,  2, "FusedMapSelectScalePositiveInt", false, true , "ParseNode", true , false, false, false, false }, // 227
    { QoreIROpcode::FusedMapSelectScalePositiveFloat, "FusedMapSelectScalePositiveFloat", false, false, false,  2, "FusedMapSelectScalePositiveFloat", false, true , "ParseNode", true , false, false, false, false }, // 228
    { QoreIROpcode::FusedMapSelectOffsetPositiveInt, "FusedMapSelectOffsetPositiveInt", false, false, false,  2, "FusedMapSelectOffsetPositiveInt", false, true , "ParseNode", true , false, false, false, false }, // 229
    { QoreIROpcode::FusedMapSelectOffsetPositiveFloat, "FusedMapSelectOffsetPositiveFloat", false, false, false,  2, "FusedMapSelectOffsetPositiveFloat", false, true , "ParseNode", true , false, false, false, false }, // 230
    { QoreIROpcode::FusedMapSelectSquarePositiveInt, "FusedMapSelectSquarePositiveInt", false, false, false,  2, "FusedMapSelectSquarePositiveInt", false, true , "ParseNode", true , false, false, false, false }, // 231
    { QoreIROpcode::FusedMapSelectSquarePositiveFloat, "FusedMapSelectSquarePositiveFloat", false, false, false,  2, "FusedMapSelectSquarePositiveFloat", false, true , "ParseNode", true , false, false, false, false }, // 232
    { QoreIROpcode::FusedMapFoldlSumScaleInt, "FusedMapFoldlSumScaleInt"      , true , false, false,  2, "FusedMapFoldlSumScaleInt", false, true , "ParseNode", true , false, true , false, true  }, // 233
    { QoreIROpcode::FusedMapFoldlSumScaleFloat, "FusedMapFoldlSumScaleFloat"    , true , false, false,  2, "FusedMapFoldlSumScaleFloat", false, true , "ParseNode", true , false, true , false, true  }, // 234
    { QoreIROpcode::FusedMapFoldlSumSquareInt, "FusedMapFoldlSumSquareInt"     , true , false, false,  2, "FusedMapFoldlSumSquareInt", false, true , "ParseNode", true , false, true , false, true  }, // 235
    { QoreIROpcode::FusedMapFoldlSumSquareFloat, "FusedMapFoldlSumSquareFloat"   , true , false, false,  2, "FusedMapFoldlSumSquareFloat", false, true , "ParseNode", true , false, true , false, true  }, // 236
    { QoreIROpcode::FusedMapFoldlProdScaleInt, "FusedMapFoldlProdScaleInt"     , true , false, false,  2, "FusedMapFoldlProdScaleInt", false, true , "ParseNode", true , false, true , false, true  }, // 237
    { QoreIROpcode::FusedMapFoldlProdScaleFloat, "FusedMapFoldlProdScaleFloat"   , true , false, false,  2, "FusedMapFoldlProdScaleFloat", false, true , "ParseNode", true , false, true , false, true  }, // 238
    { QoreIROpcode::MapSelectAny, "MapSelectAny"                  , false, false, false, -1, "MapSelectAny", false, true , "ParseNode", true , false, false, false, false }, // 239
    { QoreIROpcode::HashMapAny, "HashMapAny"                    , false, false, false, -1, "HashMapAny", false, true , "ParseNode", true , false, false, false, false }, // 240
    { QoreIROpcode::HashMapSelectAny, "HashMapSelectAny"              , false, false, false, -1, "HashMapSelectAny", false, true , "ParseNode", true , false, false, false, false }, // 241
    { QoreIROpcode::RangeAny, "RangeAny"                      , false, false, false,  2, "RangeAny", false, true , "ParseNode", true , false, true , false, true  }, // 242
    { QoreIROpcode::RangeInt, "RangeInt"                      , false, false, false,  2, "RangeInt", false, true , "ParseNode", true , false, true , false, true  }, // 243
    { QoreIROpcode::RangeFloat, "RangeFloat"                    , false, false, false,  2, "RangeFloat", false, true , "ParseNode", true , false, true , false, true  }, // 244
    { QoreIROpcode::RangeDate, "RangeDate"                     , false, false, false,  2, "RangeDate", false, true , "ParseNode", true , false, true , false, true  }, // 245
    { QoreIROpcode::RangeSliceAny, "RangeSliceAny"                 , false, false, false,  3, "RangeSliceAny", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_RANGE_SLICE }, // 246
    { QoreIROpcode::RangeSliceInt, "RangeSliceInt"                 , false, false, false,  3, "RangeSliceInt", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_RANGE_SLICE }, // 247
    { QoreIROpcode::RangeSliceFloat, "RangeSliceFloat"               , false, false, false,  3, "RangeSliceFloat", false, true , "ParseNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_RANGE_SLICE }, // 248
    { QoreIROpcode::CastAny, "CastAny"                       , false, false, false,  1, "CastAny", false, true , "ParseNode", true , false, true , false, false }, // 249
    { QoreIROpcode::CastList, "CastList"                      , false, false, false,  1, "CastList", false, true , "ParseNode", true , false, true , false, false }, // 250
    { QoreIROpcode::CastHash, "CastHash"                      , false, false, false,  1, "CastHash", false, true , "ParseNode", true , false, true , false, false }, // 251
    { QoreIROpcode::CastObject, "CastObject"                    , false, false, false,  1, "CastObject", false, true , "ParseNode", true , false, true , false, false }, // 252
    { QoreIROpcode::CastEnum, "CastEnum"                      , false, false, false,  1, "CastEnum", false, true , "ParseNode", true , false, true , false, false }, // 253
    { QoreIROpcode::CastComplexHash, "CastComplexHash"               , false, false, false,  1, "CastComplexHash", false, true , "ParseNode", true , false, false, false, false }, // 254
    { QoreIROpcode::Br, "Br"                            , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false, false }, // 255
    { QoreIROpcode::BrIf, "BrIf"                          , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false, false }, // 256
    { QoreIROpcode::SwitchInt, "SwitchInt"                     , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false, false }, // 257
    { QoreIROpcode::SwitchString, "SwitchString"                  , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false, false }, // 258
    { QoreIROpcode::Return, "Return"                        , false, false, true ,  0, "Control transfer", false, true , "ControlFlowNode", false, false, false, false, false }, // 259
    { QoreIROpcode::ReturnNothing, "ReturnNothing"                 , false, false, true ,  0, "Control transfer", false, true , "ControlFlowNode", false, false, false, false, false }, // 260
    { QoreIROpcode::LoadLocal, "LoadLocal"                     , true , false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 261
    { QoreIROpcode::StoreLocal, "StoreLocal"                    , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", true , true , false, false, false }, // 262
    { QoreIROpcode::UninstantiateLocal, "UninstantiateLocal"            , false, false, false,  0, "UninstantiateLocal", false, true , "ParseNode", false, false, false, false, false }, // 263
    { QoreIROpcode::LoadArg, "LoadArg"                       , false, false, false, -1, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 264
    { QoreIROpcode::LoadClosure, "LoadClosure"                   , true , false, false, -1, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 265
    { QoreIROpcode::StoreClosure, "StoreClosure"                  , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", true , true , false, false, false }, // 266
    { QoreIROpcode::LoadGlobal, "LoadGlobal"                    , true , false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 267
    { QoreIROpcode::StoreGlobal, "StoreGlobal"                   , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", true , true , false, false, false }, // 268
    { QoreIROpcode::LoadThreadLocal, "LoadThreadLocal"               , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 269
    { QoreIROpcode::StoreThreadLocal, "StoreThreadLocal"              , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", true , true , false, false, false }, // 270
    { QoreIROpcode::LoadImplicitArg, "LoadImplicitArg"               , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 271
    { QoreIROpcode::LoadImplicitArgv, "LoadImplicitArgv"              , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 272
    { QoreIROpcode::LoadImplicitElement, "LoadImplicitElement"           , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 273
    { QoreIROpcode::PushImplicitArg, "PushImplicitArg"               , false, false, false,  1, "PushImplicitArg", false, true , "ParseNode", true , false, false, false, false }, // 274
    { QoreIROpcode::SetImplicitArgv, "SetImplicitArgv"               , false, false, false,  1, "SetImplicitArgv", false, true , "ParseNode", true , false, false, false, false }, // 275
    { QoreIROpcode::PopImplicitArg, "PopImplicitArg"                , false, false, false,  1, "PopImplicitArg", false, true , "ParseNode", false, false, false, false, false }, // 276
    { QoreIROpcode::PushImplicitElement, "PushImplicitElement"           , false, false, false,  1, "PushImplicitElement", false, true , "ParseNode", true , false, false, false, false }, // 277
    { QoreIROpcode::PopImplicitElement, "PopImplicitElement"            , false, false, false,  1, "PopImplicitElement", false, true , "ParseNode", false, false, false, false, false }, // 278
    { QoreIROpcode::HashKeyAccess, "HashKeyAccess"                 , true , false, false,  1, "HashKeyAccess", false, true , "ParseNode", true , false, true , false, false }, // 279
    { QoreIROpcode::HashKeyAccessInt, "HashKeyAccessInt"              , true , false, false,  1, "HashKeyAccessInt", false, true , "ParseNode", true , false, true , false, false }, // 280
    { QoreIROpcode::LoadSelfMember, "LoadSelfMember"                , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 281
    { QoreIROpcode::LoadStaticVar, "LoadStaticVar"                 , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false, false }, // 282
    { QoreIROpcode::NewObject, "NewObject"                     , false, false, false, -1, "NewObject", false, true , "ParseNode", true , false, false, false, false }, // 283
    { QoreIROpcode::LoadConstant, "LoadConstant"                  , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 284
    { QoreIROpcode::CreateClosure, "CreateClosure"                 , false, false, false,  0, "CreateClosure", false, true , "ParseNode", true , false, false, false, false }, // 285
    { QoreIROpcode::CreateCallRef, "CreateCallRef"                 , false, false, false,  0, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false }, // 286
    { QoreIROpcode::CreateMethodRef, "CreateMethodRef"               , false, false, false, OPCODE_MIN_OPERANDS(0), "CreateMethodRef", false, true , "ParseNode", true , false, false, false, false }, // 287
    { QoreIROpcode::CreateParseRef, "CreateParseRef"                , false, false, false, OPCODE_MIN_OPERANDS(0), "CreateParseRef", false, true , "ParseNode", true , false, false, false, false }, // 288
    { QoreIROpcode::NewHashDecl, "NewHashDecl"                   , false, false, false,  0, "NewHashDecl", false, true , "ParseNode", true , false, false, false, false }, // 289
    { QoreIROpcode::NewComplexHash, "NewComplexHash"                , false, false, false, OPCODE_MIN_OPERANDS(0), "NewComplexHash", false, true , "ParseNode", true , false, false, false, false }, // 290
    { QoreIROpcode::NewComplexList, "NewComplexList"                , false, false, false, OPCODE_MIN_OPERANDS(0), "NewComplexList", false, true , "ParseNode", true , false, false, false, false }, // 291
    { QoreIROpcode::VrnConstruct, "VrnConstruct"                  , false, false, false, OPCODE_MIN_OPERANDS(0), "VrnConstruct", false, true , "ParseNode", true , false, false, false, false }, // 292
    { QoreIROpcode::HashSetKeyValue, "HashSetKeyValue"               , false, false, false,  3, "HashSetKeyValue", false, true , "ParseNode", false, false, false, false, false }, // 293
    { QoreIROpcode::IteratorCreateReverse, "IteratorCreateReverse"         , false, false, false,  1, "IteratorCreateReverse", false, true , "ParseNode", true , false, false, false, false }, // 294
    { QoreIROpcode::Call, "Call"                          , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 295
    { QoreIROpcode::CallDirect, "CallDirect"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 296
    { QoreIROpcode::CallIndirect, "CallIndirect"                  , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 297
    { QoreIROpcode::CallMethod, "CallMethod"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 298
    { QoreIROpcode::CallMethodDirect, "CallMethodDirect"              , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 299
    { QoreIROpcode::InvokeMethodDirect, "InvokeMethodDirect"            , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false }, // 300
    { QoreIROpcode::CallStatic, "CallStatic"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 301
    { QoreIROpcode::CallStaticDirect, "CallStaticDirect"              , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 302
    { QoreIROpcode::DotEvalMethodDirect, "DotEvalMethodDirect"           , true , false, false, -1, "DotEvalMethodDirect", false, true , "ParseNode", true , false, false, false, false }, // 303
    { QoreIROpcode::InvokeDotEvalMethodDirect, "InvokeDotEvalMethodDirect"     , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false }, // 304
    { QoreIROpcode::CallClosureDirect, "CallClosureDirect"             , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, true , false, false, QORE_OPCODE_PROPERTY_CALL_INVOKE }, // 305
    { QoreIROpcode::Invoke, "Invoke"                        , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false, false }, // 306
    { QoreIROpcode::GuardInt, "GuardInt"                      , false, false, false,  1, "GuardInt", false, true , "ParseNode", false, false, false, false, false }, // 307
    { QoreIROpcode::GetObjectClass, "GetObjectClass"                , false, false, false,  1, "GetObjectClass", false, true , "ParseNode", true , false, false, false, false }, // 308
    { QoreIROpcode::GuardFloat, "GuardFloat"                    , false, false, false,  1, "GuardFloat", false, true , "ParseNode", false, false, false, false, false }, // 309
    { QoreIROpcode::GuardType, "GuardType"                     , false, false, false,  1, "GuardType", false, true , "ParseNode", false, false, false, false, false }, // 310
    { QoreIROpcode::GuardNotNothing, "GuardNotNothing"               , false, false, false,  1, "GuardNotNothing", false, true , "ParseNode", false, false, false, false, false }, // 311
    { QoreIROpcode::LandingPad, "LandingPad"                    , false, false, false,  0, "LandingPad", false, true , "ParseNode", false, false, false, false, false }, // 312
    { QoreIROpcode::CatchException, "CatchException"                , true , false, false,  0, "CatchException", false, true , "ParseNode", true , false, false, false, false }, // 313
    { QoreIROpcode::CatchCleanup, "CatchCleanup"                  , false, false, false,  0, "CatchCleanup", false, true , "ParseNode", false, false, false, false, false }, // 314
    { QoreIROpcode::Rethrow, "Rethrow"                       , false, false, true , -1, "Rethrow", false, true , "ParseNode", false, false, false, false, false }, // 315
    { QoreIROpcode::Throw, "Throw"                         , false, false, true ,  1, "Control transfer", false, true , "ControlFlowNode", false, false, false, false, false }, // 316
    { QoreIROpcode::InvokeSimError, "InvokeSimError"                , false, false, true ,  0, "Call function or method", true , true , "FunctionCallNode", false, false, false, false, false }, // 317
    { QoreIROpcode::Incref, "Incref"                        , false, false, false,  1, "Incref", false, true , "ParseNode", false, false, false, false, false }, // 318
    { QoreIROpcode::Decref, "Decref"                        , false, false, false,  1, "Decref", false, true , "ParseNode", false, false, false, false, false }, // 319
    { QoreIROpcode::DecrefNoThrow, "DecrefNoThrow"                 , false, false, false,  1, "Control transfer", false, true , "ControlFlowNode", false, false, false, false, false }, // 320
    { QoreIROpcode::SwitchRegexMatch, "SwitchRegexMatch"              , false, false, false,  1, "Control flow", false, false, "ControlFlowNode", true , false, false, false, false }, // 321
    { QoreIROpcode::ListPush, "ListPush"                      , false, false, false,  2, "ListPush", false, true , "ParseNode", true , false, false, false, false }, // 322
    { QoreIROpcode::RefForeachInit, "RefForeachInit"                , false, false, false,  0, "RefForeachInit", false, true , "ParseNode", true , false, false, false, false }, // 323
    { QoreIROpcode::RefForeachSize, "RefForeachSize"                , false, false, false,  1, "RefForeachSize", false, true , "ParseNode", true , false, false, false, false }, // 324
    { QoreIROpcode::RefForeachGetEntry, "RefForeachGetEntry"            , false, false, false,  2, "RefForeachGetEntry", false, true , "ParseNode", true , false, false, false, false }, // 325
    { QoreIROpcode::RefForeachRecord, "RefForeachRecord"              , false, false, false,  2, "RefForeachRecord", false, true , "ParseNode", false, false, false, false, false }, // 326
    { QoreIROpcode::RefForeachFinalize, "RefForeachFinalize"            , false, false, false,  2, "RefForeachFinalize", false, true , "ParseNode", false, false, false, false, false }, // 327
    { QoreIROpcode::RefForeachCleanup, "RefForeachCleanup"             , false, false, false,  1, "RefForeachCleanup", false, true , "ParseNode", false, false, false, false, false }, // 328
    { QoreIROpcode::AddNumber, "AddNumber"                     , false, false, false,  2, "AddNumber", false, true , "ParseNode", true , false, true , false, true  }, // 329
    { QoreIROpcode::SubNumber, "SubNumber"                     , false, false, false,  2, "SubNumber", false, true , "ParseNode", true , false, true , false, true  }, // 330
    { QoreIROpcode::MulNumber, "MulNumber"                     , false, false, false,  2, "MulNumber", false, true , "ParseNode", true , false, true , false, true  }, // 331
    { QoreIROpcode::DivNumber, "DivNumber"                     , false, false, false,  2, "DivNumber", false, true , "ParseNode", true , false, true , false, true  }, // 332
    { QoreIROpcode::HashKeyStore, "HashKeyStore"                  , false, false, false,  2, "Store to variable", true , true , "AssignmentNode", false, false, false, false, false }, // 333
    { QoreIROpcode::ListIndexAccess, "ListIndexAccess"               , true , false, false,  2, "ListIndexAccess", false, true , "ParseNode", true , false, false, false, false }, // 334
    { QoreIROpcode::ListIndexStore, "ListIndexStore"                , false, false, false,  3, "Store to variable", true , true , "AssignmentNode", false, false, false, false, false }, // 335
    { QoreIROpcode::AddAssignLocalInt, "AddAssignLocalInt"             , false, false, false,  0, "AddAssignLocalInt", false, true , "ParseNode", true , false, false, false, false }, // 336
    { QoreIROpcode::IncrementLocalInt, "IncrementLocalInt"             , false, false, false,  0, "IncrementLocalInt", false, true , "ParseNode", true , false, false, false, false }, // 337
    { QoreIROpcode::BranchIfLtLocalInt, "BranchIfLtLocalInt"            , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false, false }, // 338
    { QoreIROpcode::ConstEnum, "ConstEnum"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 339
    { QoreIROpcode::ListGetValueNoRef, "ListGetValueNoRef"             , false, false, false,  2, "ListGetValueNoRef", false, true , "ParseNode", true , false, false, false, false }, // 340
    { QoreIROpcode::SwitchCaseMatch, "SwitchCaseMatch"               , false, false, false,  1, "Control flow", false, false, "ControlFlowNode", true , false, false, false, false }, // 341
    { QoreIROpcode::IsCollectionType, "IsCollectionType"              , false, false, false,  1, "IsCollectionType", false, true , "ParseNode", true , false, true , true , false }, // 342
    { QoreIROpcode::MakeHashConstKeys, "MakeHashConstKeys"             , false, false, false, -1, "Load constant value", false, false, "ConstantNode", true , false, false, false, false }, // 343
    { QoreIROpcode::ToString, "ToString"                      , false, false, false, -1, "ToString", false, true , "ParseNode", true , false, false, false, false }, // 344
    { QoreIROpcode::Sprintf, "Sprintf"                       , false, false, false, -1, "Sprintf", false, true , "ParseNode", true , false, false, false, false }, // 345
    { QoreIROpcode::NewHashDeclFromHash, "NewHashDeclFromHash"           , false, false, false,  1, "NewHashDeclFromHash", false, true , "ParseNode", true , false, false, false, false }, // 346
    { QoreIROpcode::InstantiateLocal, "InstantiateLocal"              , false, false, false,  0, "InstantiateLocal", false, true , "ParseNode", false, false, false, false, false }, // 347
    { QoreIROpcode::AddTimeout, "AddTimeout"                    , false, false, false,  2, "AddTimeout", false, true , "ParseNode", true , false, true , false, true  }, // 348
    { QoreIROpcode::SubTimeout, "SubTimeout"                    , false, false, false,  2, "SubTimeout", false, true , "ParseNode", true , false, true , false, true  }, // 349
    { QoreIROpcode::HashDerefDynamic, "HashDerefDynamic"              , false, false, false,  2, "HashDerefDynamic", true , true , "ParseNode", true , false, true , false, true  }, // 350
    { QoreIROpcode::ListIndexDynamic, "ListIndexDynamic"              , false, false, false, OPCODE_MIN_OPERANDS(2), "ListIndexDynamic", false, true , "ParseNode", true , false, true , false, true  }, // 351
    { QoreIROpcode::HashKeyStoreDynamic, "HashKeyStoreDynamic"           , false, false, false,  3, "Store to variable", true , true , "AssignmentNode", false, false, false, false, false }, // 352
    { QoreIROpcode::LValuePathAssign, "LValuePathAssign"              , false, false, false, OPCODE_MIN_OPERANDS(1), "LValuePathAssign", true , true , "AssignmentNode", true , true , false, false, false }, // 353
    { QoreIROpcode::LValuePathCompound, "LValuePathCompound"            , false, false, false, OPCODE_MIN_OPERANDS(1), "LValuePathCompound", true , true , "ParseNode", true , false, false, false, false }, // 354
    { QoreIROpcode::LValuePathUnary, "LValuePathUnary"               , false, false, false, OPCODE_MIN_OPERANDS(0), "LValuePathUnary", true , true , "ParseNode", true , true , false, false, false }, // 355
    { QoreIROpcode::LValuePathBinaryMut, "LValuePathBinaryMut"           , false, false, false, OPCODE_MIN_OPERANDS(0), "LValuePathBinaryMut", true , true , "ParseNode", true , false, false, false, false }, // 356
    { QoreIROpcode::LValuePathTernary, "LValuePathTernary"             , false, false, false, OPCODE_MIN_OPERANDS(3), "LValuePathTernary", true , true , "ParseNode", true , false, false, false, false }, // 357
    { QoreIROpcode::DiscardTemps, "DiscardTemps"                  , false, false, false,  0, "Drain cleanup back to nearest PushTempMark", false, false, "ParseNode", false, false, false, false, false }, // 358
    { QoreIROpcode::PushTempMark, "PushTempMark"                  , false, false, false,  0, "Push sentinel marking start of statement-scoped cleanup region", false, false, "ParseNode", false, false, false, false, false }, // 359
    { QoreIROpcode::CallAOTHelper, "CallAOTHelper"                 , false, false, false,  0, "Call AOT-emitted helper function by LLVM symbol name", true , true , "ParseNode", true , false, false, false, false }, // 360
    { QoreIROpcode::ContextMaxPos, "ContextMaxPos"                 , false, true , false,  1, "Get max_pos (iteration count) from Context state handle", false, false, "ParseNode", true , false, false, false, false }, // 361
    { QoreIROpcode::ContextSetPos, "ContextSetPos"                 , false, false, false,  2, "Set current row position on Context state handle", true , false, "ParseNode", false, false, false, false, false }, // 362
    { QoreIROpcode::ContextDestroy, "ContextDestroy"                , false, false, false,  1, "Pop thread-local context stack + free Context frame", true , false, "ParseNode", false, false, false, false, false }, // 363
    { QoreIROpcode::Backquote, "Backquote"                     , false, false, false,  0, "Execute backquote command", true , true , "BackquoteNode", true , false, false, false, false }, // 364
    { QoreIROpcode::Find, "Find"                          , true , false, false,  0, "Execute find expression", true , true , "FindNode", true , false, false, false, false }, // 365
    { QoreIROpcode::ContextRef, "ContextRef"                    , true , false, false,  0, "Evaluate context field reference", false, true , "ContextrefNode", true , false, true , false, false }, // 366
    { QoreIROpcode::ContextRow, "ContextRow"                    , true , false, false,  0, "Evaluate current context row", false, true , "ContextRowNode", true , false, true , false, false }, // 367
    { QoreIROpcode::RefSelf, "RefSelf"                       , false, false, false,  1, "Create owned reference to value", false, false, "ParseNode", true , false, false, false, false }, // 368
    { QoreIROpcode::DebugBlock, "DebugBlock"                    , false, false, false,  0, "Debug-only StatementBlock entry marker", false, false, "ParseNode", false, false, false, false, false }, // 369
    { QoreIROpcode::CheckException, "CheckException"                , false, false, false,  0, "Route pending cleanup exception to the active handler", false, true , "ParseNode", false, false, false, false, false }, // 370
    { QoreIROpcode::NewComplexBuffer, "NewComplexBuffer"              , false, false, false, OPCODE_MIN_OPERANDS(0), "NewComplexBuffer", false, true , "ParseNode", true , false, false, false, false }, // 371
    { QoreIROpcode::PluginUnary, "PluginUnary"                   , false, false, false,  1, "Dispatch module-registered unary plugin operation", true , true , "PluginOperation", true , false, false, false, false }, // 372
    { QoreIROpcode::PluginBinary, "PluginBinary"                  , false, false, false,  2, "Dispatch module-registered binary plugin operation", true , true , "PluginOperation", true , false, false, false, false }, // 373
    { QoreIROpcode::PluginCall, "PluginCall"                    , false, false, false, OPCODE_MIN_OPERANDS(1), "Dispatch module-registered receiver call with value-list arguments", true , true , "PluginOperation", true , false, false, false, false }, // 374
    { QoreIROpcode::PluginSubscript, "PluginSubscript"               , false, false, false,  2, "Dispatch module-registered subscript operation", true , true , "PluginOperation", true , false, false, false, false }, // 375
    { QoreIROpcode::PluginConstruct, "PluginConstruct"               , false, false, false, OPCODE_MIN_OPERANDS(0), "Dispatch module-registered construct/static factory operation", true , true , "PluginOperation", true , false, false, false, false }, // 376
    { QoreIROpcode::PluginDenseBufferUnary, "PluginDenseBufferUnary"         , false, false, false,  2, "Dispatch module-registered dense-buffer unary plugin operation", true , true , "PluginOperation", true , false, false, false, false }, // 377
    { QoreIROpcode::PluginDenseBufferBinary, "PluginDenseBufferBinary"        , false, false, false,  3, "Dispatch module-registered dense-buffer binary plugin operation", true , true , "PluginOperation", true , false, false, false, false }, // 378
    { QoreIROpcode::IteratorCreateIterate, "IteratorCreateIterate"          , false, false, false,  0, "Iterator creation with iterate source semantics", false, true , "ParseNode", true , false, false, false, false }, // 379
    { QoreIROpcode::IterateValue, "IterateValue"                   , false, false, false,  1, "Evaluate iterate source as AbstractIterator object", true , true , "QoreIterateOperatorNode", true , false, false, false, false }, // 380
    { QoreIROpcode::ConstChar, "ConstChar"                      , false, false, false,  0, "Load char constant value", false, false, "ConstantNode", true , false, false, false, false }, // 381
    { QoreIROpcode::ToInt, "ToInt"                          , false, true , false,  1, "ToInt", false, true , "ParseNode", true , false, true , true , false }, // 382
    { QoreIROpcode::ToFloat, "ToFloat"                        , false, true , false,  1, "ToFloat", false, true , "ParseNode", true , false, true , true , false }, // 383
    { QoreIROpcode::SelectHashKeyPositiveInt, "SelectHashKeyPositiveInt"       , false, false, false,  1, "SelectHashKeyPositiveInt", false, true , "ParseNode", true , false, false, false, false }, // 384
    { QoreIROpcode::HashKeyAccessHash, "HashKeyAccessHash"              , true , false, false,  1, "HashKeyAccessHash", false, true , "ParseNode", true , false, true , false, false }, // 385
    { QoreIROpcode::HashKeyAccessHashGuarded, "HashKeyAccessHashGuarded"       , true , false, false,  1, "HashKeyAccessHashGuarded", false, true , "ParseNode", true , false, true , false, false }, // 386
    { QoreIROpcode::TypedForeachNextInt, "TypedForeachNextInt"            , false, true , true ,  3, "Advance exact list<int> foreach", false, true , "ForEachStatement", true , false, true , false, false }, // 387
    { QoreIROpcode::TypedForeachNextFloat, "TypedForeachNextFloat"          , false, true , true ,  3, "Advance exact list<float> foreach", false, true , "ForEachStatement", true , false, true , false, false }, // 388
    { QoreIROpcode::TypedForeachNextBool, "TypedForeachNextBool"           , false, true , true ,  3, "Advance exact list<bool> foreach", false, true , "ForEachStatement", true , false, true , false, false }, // 389
    { QoreIROpcode::TypedForeachNextString, "TypedForeachNextString"         , false, true , true ,  3, "Advance exact list<string> foreach", false, true , "ForEachStatement", true , false, true , false, false }, // 390
    { QoreIROpcode::FusedMapFoldlSumOffsetInt, "FusedMapFoldlSumOffsetInt"      , true , false, false,  2, "FusedMapFoldlSumOffsetInt", false, true , "ParseNode", true , false, true , false, true  }, // 391
    { QoreIROpcode::FusedMapFoldlSumOffsetFloat, "FusedMapFoldlSumOffsetFloat"    , true , false, false,  2, "FusedMapFoldlSumOffsetFloat", false, true , "ParseNode", true , false, true , false, true  }, // 392
    { QoreIROpcode::FoldlStringJoin, "FoldlStringJoin"                , true , false, false,  2, "Join exact list<string> with a constant separator", false, true , "FoldlOperatorNode", true , false, true , false, true  }, // 393
    { QoreIROpcode::StringJoinStart, "StringJoinStart"                , false, true , false,  3, "Start owned fused string join accumulator", false, true , "FoldlOperatorNode", true , false, true , false, false }, // 394
    { QoreIROpcode::StringJoinAppend, "StringJoinAppend"               , false, true , false,  3, "Append to owned fused string join accumulator", true , true , "FoldlOperatorNode", true , false, true , false, false }, // 395
    { QoreIROpcode::SprintfIntFixed, "SprintfIntFixed"                , false, true , false,  3, "Format int with fixed literal %d conversion", false, false, "FunctionCallNode", true , false, true , false, false }, // 396
    { QoreIROpcode::AppendStringCow, "AppendStringCow"                , false, true , false,  2, "Append typed string with copy-on-write", true , true , "QorePlusEqualsOperatorNode", true , false, true , false, false }, // 397
    { QoreIROpcode::MapHashKeyOffsetAny, "MapHashKeyOffsetAny"            , true , false, false,  2, "Map constant hash key plus integer with dynamic semantics", false, true , "QoreMapOperatorNode", true , false, true , false, false }, // 398
    { QoreIROpcode::ListSetLength, "ListSetLength"                  , false, false, false,  2, "Finalize fresh scalar list length", false, false, "ParseNode", false, false, false, false, false }, // 399
    { QoreIROpcode::ConstBoolBoxed, "ConstBoolBoxed"                 , false, false, false,  0, "Load boxed boolean constant", false, false, "ConstantNode", true , false, false, false, false }, // 400
    { QoreIROpcode::ListGetValueNoRefUnchecked, "ListGetValueNoRefUnchecked"      , false, false, false,  2, "Load proven in-bounds borrowed boxed list element", false, true , "ParseNode", true , false, false, false, false }, // 401
    { QoreIROpcode::ListStringJoin, "ListStringJoin"                 , false, true , false,  2, "Join assigned exact list<string>", false, true , "FunctionCallNode", true , false, true , false, true  }, // 402
    { QoreIROpcode::FoldrStringJoin, "FoldrStringJoin"                , true , false, false,  2, "Reverse-join exact list<string> with a constant separator", false, true , "FoldrOperatorNode", true , false, true , false, true  }, // 403
    { QoreIROpcode::ListStringJoinIdentityMap, "ListStringJoinIdentityMap"      , false, true , false,  2, "Join dense identity-map result over exact list<string>", false, true , "FunctionCallNode", true , false, true , false, true  }, // 404
    { QoreIROpcode::StringMethodJoinStart, "StringMethodJoinStart"          , false, true , false,  3, "Start owned method-join accumulator in separator encoding", false, true , "QoreDotEvalOperatorNode", true , false, true , false, false }, // 405
    { QoreIROpcode::ListIntSprintfJoin, "ListIntSprintfJoin"             , false, true , false,  4, "Format exact list<int> and join in one pass", false, true , "QoreDotEvalOperatorNode", true , false, true , false, false }, // 406
    { QoreIROpcode::DiscardTempsKeep, "DiscardTempsKeep"               , true , false, false,  1, "Drain cleanup back to the matching PushTempMark, keeping one value", true , true , "ParseNode", true , false, false, false, false }, // 407
    { QoreIROpcode::UnwindTemps, "UnwindTemps"                    , false, false, false,  0, "Release the temps of a scope that a break or continue leaves", false, false, "ParseNode", false, false, false, false, false }, // 408
    { QoreIROpcode::CheckReturnValue, "CheckReturnValue"               , false, false, false,  1, "Check the value of a return statement against the declared return type", false, true , "ReturnStatement", false, false, false, false, false }, // 409
    { QoreIROpcode::IteratorRelease, "IteratorRelease"                , false, false, false,  1, "Release the iterator of a foreach statement that a return or break leaves", true , true , "ForEachStatement", false, false, false, false, false }, // 410
    { QoreIROpcode::ReleaseOwnedTemp, "ReleaseOwnedTemp"               , false, false, false,  1, "Release the temporary of an enclosing statement that a return leaves", true , true , "ParseNode", false, false, false, false, false }, // 411
    { QoreIROpcode::DebugStatement, "DebugStatement"                , false, false, false,  0, "Debugger step event for the statement that follows", true , true , "AbstractStatement", false, false, false, false, false }, // 412
    { QoreIROpcode::ListSetCommonType, "ListSetCommonType"              , false, false, false,  2, "Set the type of a map result list from the common value type", false, true , "QoreMapOperatorNode", false, false, false, false, false }, // 413
    { QoreIROpcode::HashSetKeyValueFold, "HashSetKeyValueFold"            , false, true , false,  5, "Set a hash map key and fold the value type into the common value type", false, true , "QoreHashMapOperatorNode", true , false, false, false, false }, // 414
    { QoreIROpcode::HashSetCommonType, "HashSetCommonType"              , false, false, false,  2, "Set the type of a hash map result from the common value type", false, false, "QoreHashMapOperatorNode", false, false, false, false, false }, // 415
    { QoreIROpcode::ListSetScannedType, "ListSetScannedType"             , false, false, false, -1, "Set the type of a select result list from its values", false, true , "QoreSelectOperatorNode", false, false, false, false, false }, // 416
    { QoreIROpcode::ListAppendElement, "ListAppendElement"              , false, true , false,  3, "Append a value to a map result list as the AST interpreter pushes it and fold its type", false, true , "QoreMapOperatorNode", true , false, false, false, false }, // 417
    { QoreIROpcode::CheckCancelEvery, "CheckCancelEvery"               , false, false, false,  3, "Check for cancellation every 100 elements of an expression, as the AST interpreter does", true , true , "QoreFoldlOperatorNode", false, false, false, false, false }, // 418
    { QoreIROpcode::CheckRangeBound, "CheckRangeBound"                , false, false, false,  2, "Raise RANGE-ERROR for the start of a range that evaluated to NOTHING, before the end is evaluated", true , true , "QoreRangeOperatorNode", false, false, false, false, false }, // 419
    { QoreIROpcode::ListSelectorStep, "ListSelectorStep"               , true , false, false, OPCODE_MIN_OPERANDS(2), "Apply selectors of a list of selectors with ranges, as the AST interpreter does", true , true , "QoreSquareBracketsOperatorNode", true , true , false, false, false }, // 420
    { QoreIROpcode::IteratorCreateSelectors, "IteratorCreateSelectors"        , false, false, false,  2, "Create the iterator of a list of selectors with ranges, as the AST interpreter iterates it", false, true , "QoreSquareBracketsOperatorNode", true , false, false, false, false }, // 421
    { QoreIROpcode::SelectorIteratorPending, "SelectorIteratorPending"        , false, false, false,  1, "Return the selector that the iterator of a list of selectors needs next, or -1", true , false, "QoreSquareBracketsOperatorNode", true , false, false, false, false }, // 422
    { QoreIROpcode::SelectorIteratorFeed, "SelectorIteratorFeed"           , false, false, false, OPCODE_MIN_OPERANDS(3), "Give the iterator of a list of selectors the value of its next selector", true , true , "QoreSquareBracketsOperatorNode", false, false, false, false, false }, // 423
    { QoreIROpcode::ExceptionSuspend, "ExceptionSuspend"               , false, false, false,  0, "Set the exceptions raised aside for code that runs while they are raised", true , false, "QoreRemoveOperatorNode", true , false, false, false, false }, // 424
    { QoreIROpcode::ExceptionResume, "ExceptionResume"                , false, false, false,  1, "Raise again the exceptions set aside, before any raised since", true , false, "QoreRemoveOperatorNode", false, false, false, false, false }, // 425
    { QoreIROpcode::IteratorNextSelectors, "IteratorNextSelectors"          , false, false, true ,  0, "Give the next value of the iterator of a list of selectors, or branch if it has none now", true , true , "QoreSquareBracketsOperatorNode", true , false, false, false, false }, // 426
};

//! Returns true if row i of OPCODE_REGISTRY is the opcode with value i, for every row
constexpr bool qore_opcode_registry_is_in_opcode_order() {
    for (unsigned i = 0; i < sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]); ++i) {
        if (static_cast<unsigned>(OPCODE_REGISTRY[i].opcode) != i) {
            return false;
        }
    }
    return true;
}

//! Static assertions to verify registry completeness and order
static_assert(sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == QORE_IR_MAX_OPCODE + 1,
    "OPCODE_REGISTRY must have one row for each opcode");
// a missing row leaves a row at the end value-initialized (opcode 0), which this also reports
static_assert(qore_opcode_registry_is_in_opcode_order(),
    "OPCODE_REGISTRY row i must be the opcode with value i: move the row of a renumbered opcode to its new value, "
    "and add a row for each new opcode");

//! ============================================================================
//! Lookup Functions - Query registry by opcode ID
//! ============================================================================

//! Get opcode info by opcode enum value
//! Returns pointer to registry entry, or nullptr for invalid opcode
inline const OpcodeInfo* getOpcodeInfo(int opcode_id) {
    constexpr int REGISTRY_SIZE = sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]);
    if (opcode_id >= 0 && opcode_id < REGISTRY_SIZE) {
        const OpcodeInfo* info = &OPCODE_REGISTRY[opcode_id];
        return info;
    }
    return nullptr;
}

//! Check if opcode can legitimately return NOTHING
inline bool getOpcodeCanReturnNothing(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->can_return_nothing : false;
}

//! Check if opcode is guaranteed to never return NOTHING
inline bool getOpcodeNeverReturnsNothing(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->never_returns_nothing : false;
}

//! Check if opcode is a block terminator (control flow)
inline bool getOpcodeIsTerminator(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->is_terminator : false;
}

//! Get human-readable name of opcode
inline const char* getOpcodeName(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->name : "<UNKNOWN>";
}

//! Get expected operand count for opcode (-1 = variable/context-dependent)
inline int getOpcodeExpectedOperands(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->expected_operands : -1;
}

//! Get human-readable description of what the opcode does
inline const char* getOpcodeDescription(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->description : "<NO DESCRIPTION>";
}

//! Check if opcode can modify global/static state or object members
inline bool getOpcodeMayHaveSideEffects(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->may_have_side_effects : true;
}

//! Check if opcode can throw an exception at runtime
inline bool getOpcodeMayThrowException(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->may_throw_exception : true;
}

//! Get the AST node type that this IR opcode comes from
inline const char* getOpcodeCorrespondingASTNode(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->corresponding_ast_node : "ParseNode";
}

//! Check if opcode produces a result value
inline bool getOpcodeProducesResult(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->produces_result : false;
}

//! Check if result is optional (verifier allows missing result even when produces_result is true)
inline bool getOpcodeOptionalResult(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->optional_result : false;
}

//! Check if opcode can skip AOT expr slot when operands are pre-evaluated
inline bool getOpcodeSkipAotExprSlot(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->skip_aot_expr_slot : false;
}

//! Check if opcode is a unary computation op (Invoke dispatches via qore_rt_unary_op)
inline bool getOpcodeIsUnaryInvoke(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->is_unary_invoke : false;
}

//! Check if opcode is a binary computation op (Invoke dispatches via qore_rt_binary_op)
inline bool getOpcodeIsBinaryInvoke(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->is_binary_invoke : false;
}

//! Check if opcode is a call-type op dispatched through Invoke
inline bool getOpcodeIsCallInvoke(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->hasProperty(QORE_OPCODE_PROPERTY_CALL_INVOKE) : false;
}

//! Check if opcode is a non-substitution regex op dispatched through Invoke
inline bool getOpcodeIsRegexInvoke(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->hasProperty(QORE_OPCODE_PROPERTY_REGEX_INVOKE) : false;
}

//! Check if opcode is a dot-eval op dispatched through Invoke
inline bool getOpcodeIsDotEvalInvoke(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->hasProperty(QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE) : false;
}

//! Check if opcode is a range-slice expression op
inline bool getOpcodeIsRangeSlice(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->hasProperty(QORE_OPCODE_PROPERTY_RANGE_SLICE) : false;
}

//! Validate opcode registry invariants that are not expressible as static_asserts.
inline bool qore_ir_validate_opcode_registry(std::string& error) {
    constexpr int REGISTRY_SIZE = sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]);

    for (int i = 0; i < REGISTRY_SIZE; ++i) {
        const OpcodeInfo& info = OPCODE_REGISTRY[i];

        if (!info.name || !*info.name) {
            error = "opcode registry entry " + std::to_string(i) + " has no name";
            return false;
        }
        if (!info.description || !*info.description) {
            error = "opcode registry entry '" + std::string(info.name) + "' has no description";
            return false;
        }
        if (!info.corresponding_ast_node || !*info.corresponding_ast_node) {
            error = "opcode registry entry '" + std::string(info.name) + "' has no AST/source classification";
            return false;
        }
        if (info.can_return_nothing && info.never_returns_nothing) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' is both can_return_nothing and never_returns_nothing";
            return false;
        }
        if (info.optional_result && !info.produces_result) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' has optional_result without produces_result";
            return false;
        }
        if (info.is_unary_invoke && info.is_binary_invoke) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' is both unary-invoke and binary-invoke";
            return false;
        }
        if ((info.property_flags & ~(QORE_OPCODE_PROPERTY_CALL_INVOKE
                    | QORE_OPCODE_PROPERTY_REGEX_INVOKE
                    | QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE
                    | QORE_OPCODE_PROPERTY_RANGE_SLICE)) != 0) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' has unknown property flags";
            return false;
        }
        if (info.hasProperty(QORE_OPCODE_PROPERTY_CALL_INVOKE)
                && (info.hasProperty(QORE_OPCODE_PROPERTY_REGEX_INVOKE)
                    || info.hasProperty(QORE_OPCODE_PROPERTY_DOT_EVAL_INVOKE)
                    || info.hasProperty(QORE_OPCODE_PROPERTY_RANGE_SLICE))) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' mixes incompatible invoke/range property flags";
            return false;
        }
        if (info.hasProperty(QORE_OPCODE_PROPERTY_RANGE_SLICE) && info.expected_operands != 3) {
            error = "opcode registry entry '" + std::string(info.name)
                + "' is a range-slice opcode without three operands";
            return false;
        }

        if (info.expected_operands < -1) {
            int min_operands = -info.expected_operands - 2;
            if (min_operands < 0) {
                error = "opcode registry entry '" + std::string(info.name)
                    + "' has invalid minimum operand encoding";
                return false;
            }
        }
    }

    return true;
}

#endif
