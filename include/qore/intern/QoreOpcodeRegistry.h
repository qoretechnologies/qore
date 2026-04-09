/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreOpcodeRegistry.h - Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction.
*/

#ifndef _QORE_OPCODE_REGISTRY_H
#define _QORE_OPCODE_REGISTRY_H

//! Centralized metadata for IR opcodes - SINGLE SOURCE OF TRUTH
//!
//! All opcode properties are defined here. Query functions below provide
//! typed access. No manual switch statements should duplicate this data.
struct OpcodeInfo {
    const char* name;                   //! Opcode name (e.g., "LoadLocal")
    bool can_return_nothing;            //! Can legitimately return NOTHING
    bool never_returns_nothing;         //! Always returns non-NOTHING value
    bool is_terminator;                 //! Ends a basic block (control flow)
    int expected_operands;              //! Expected operand count (-1 = variable)

    const char* description;            //! What the opcode does (for debugging/docs)
    bool may_have_side_effects;         //! Can modify global state (globals, statics, objects)
    bool may_throw_exception;           //! Can raise exceptions at runtime
    const char* corresponding_ast_node; //! AST node type this IR opcode comes from

    bool produces_result;               //! Instruction produces a result value (for verifier)
    bool skip_aot_expr_slot;            //! Skip EXPR_TREE slot when operands are pre-evaluated (AOT)
    bool is_unary_invoke;               //! Unary computation op (Invoke dispatches via qore_rt_unary_op)
    bool is_binary_invoke;              //! Binary computation op (Invoke dispatches via qore_rt_binary_op)
};

//! Registry of all IR opcodes (in enum ID order)
constexpr OpcodeInfo OPCODE_REGISTRY[358] = {
    { "ConstInt"                      , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 0
    { "ConstFloat"                    , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 1
    { "ConstBool"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 2
    { "ConstNothing"                  , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 3
    { "ConstNull"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 4
    { "ConstString"                   , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 5
    { "ConstDate"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 6
    { "MakeList"                      , false, false, false, -1, "MakeList", false, true , "ParseNode", true , false, false, false }, // 7
    { "MakeHash"                      , false, false, false, -1, "MakeHash", false, true , "ParseNode", true , false, false, false }, // 8
    { "CreateEmptyList"               , false, false, false,  0, "CreateEmptyList", false, true , "ParseNode", true , false, false, false }, // 9
    { "CreateSizedList"               , false, false, false,  1, "CreateSizedList", false, true , "ParseNode", true , false, false, false }, // 10
    { "ListAppend"                    , false, false, false,  2, "ListAppend", false, true , "ParseNode", false, false, false, false }, // 11
    { "ListSize"                      , false, false, false,  1, "ListSize", false, true , "ParseNode", true , false, false, false }, // 12
    { "ListGetInt"                    , false, false, false,  2, "ListGetInt", false, true , "ParseNode", true , false, false, false }, // 13
    { "ListGetFloat"                  , false, false, false,  2, "ListGetFloat", false, true , "ParseNode", true , false, false, false }, // 14
    { "ListGetValue"                  , false, false, false,  2, "ListGetValue", false, true , "ParseNode", true , false, false, false }, // 15
    { "ListSetInt"                    , false, false, false,  3, "ListSetInt", false, true , "ParseNode", false, false, false, false }, // 16
    { "ListSetFloat"                  , false, false, false,  3, "ListSetFloat", false, true , "ParseNode", false, false, false, false }, // 17
    { "ListSetValue"                  , false, false, false,  3, "ListSetValue", false, true , "ParseNode", false, false, false, false }, // 18
    { "AddInt"                        , false, true , false,  2, "AddInt", false, true , "ParseNode", true , true , false, true  }, // 19
    { "AddFloat"                      , false, true , false,  2, "AddFloat", false, true , "ParseNode", true , true , false, true  }, // 20
    { "AddAny"                        , false, false, false,  2, "AddAny", false, true , "ParseNode", true , true , false, true  }, // 21
    { "AddString"                     , false, false, false,  2, "AddString", false, true , "ParseNode", true , false, false, false }, // 22
    { "StringConcat"                  , false, false, false, -1, "StringConcat", false, true , "ParseNode", true , false, false, false }, // 23
    { "SubInt"                        , false, true , false,  2, "SubInt", false, true , "ParseNode", true , true , false, true  }, // 24
    { "SubFloat"                      , false, true , false,  2, "SubFloat", false, true , "ParseNode", true , true , false, true  }, // 25
    { "SubAny"                        , false, false, false,  2, "SubAny", false, true , "ParseNode", true , true , false, true  }, // 26
    { "MulInt"                        , false, true , false,  2, "MulInt", false, true , "ParseNode", true , true , false, true  }, // 27
    { "MulFloat"                      , false, true , false,  2, "MulFloat", false, true , "ParseNode", true , true , false, true  }, // 28
    { "MulAny"                        , false, false, false,  2, "MulAny", false, true , "ParseNode", true , true , false, true  }, // 29
    { "DivInt"                        , false, true , false,  2, "DivInt", false, true , "ParseNode", true , true , false, true  }, // 30
    { "DivFloat"                      , false, true , false,  2, "DivFloat", false, true , "ParseNode", true , true , false, true  }, // 31
    { "DivAny"                        , false, false, false,  2, "DivAny", false, true , "ParseNode", true , true , false, true  }, // 32
    { "ModInt"                        , false, true , false,  2, "ModInt", false, true , "ParseNode", true , true , false, true  }, // 33
    { "ModAny"                        , false, false, false,  2, "ModAny", false, true , "ParseNode", true , true , false, true  }, // 34
    { "AndInt"                        , false, true , false,  2, "AndInt", false, true , "ParseNode", true , true , false, true  }, // 35
    { "AndAny"                        , false, false, false,  2, "AndAny", false, true , "ParseNode", true , true , false, true  }, // 36
    { "OrInt"                         , false, true , false,  2, "OrInt", false, true , "ParseNode", true , true , false, true  }, // 37
    { "OrAny"                         , false, false, false,  2, "OrAny", false, true , "ParseNode", true , true , false, true  }, // 38
    { "XorInt"                        , false, true , false,  2, "XorInt", false, true , "ParseNode", true , true , false, true  }, // 39
    { "XorAny"                        , false, false, false,  2, "XorAny", false, true , "ParseNode", true , true , false, true  }, // 40
    { "ShlInt"                        , false, true , false,  2, "ShlInt", false, true , "ParseNode", true , true , false, true  }, // 41
    { "ShlAny"                        , false, false, false,  2, "ShlAny", false, true , "ParseNode", true , true , false, true  }, // 42
    { "ShrInt"                        , false, true , false,  2, "ShrInt", false, true , "ParseNode", true , true , false, true  }, // 43
    { "ShrAny"                        , false, false, false,  2, "ShrAny", false, true , "ParseNode", true , true , false, true  }, // 44
    { "ShlAssignInt"                  , false, true , false,  2, "ShlAssignInt", false, true , "ParseNode", true , true , false, true  }, // 45
    { "ShlAssignAny"                  , false, false, false,  2, "ShlAssignAny", false, true , "ParseNode", true , true , false, true  }, // 46
    { "ShrAssignInt"                  , false, true , false,  2, "ShrAssignInt", false, true , "ParseNode", true , true , false, true  }, // 47
    { "ShrAssignAny"                  , false, false, false,  2, "ShrAssignAny", false, true , "ParseNode", true , true , false, true  }, // 48
    { "AddAssignInt"                  , false, true , false,  2, "AddAssignInt", false, true , "ParseNode", true , true , false, true  }, // 49
    { "AddAssignFloat"                , false, true , false,  2, "AddAssignFloat", false, true , "ParseNode", true , true , false, true  }, // 50
    { "AddAssignAny"                  , false, false, false,  2, "AddAssignAny", false, true , "ParseNode", true , true , false, true  }, // 51
    { "SubAssignInt"                  , false, true , false,  2, "SubAssignInt", false, true , "ParseNode", true , true , false, true  }, // 52
    { "SubAssignFloat"                , false, true , false,  2, "SubAssignFloat", false, true , "ParseNode", true , true , false, true  }, // 53
    { "SubAssignAny"                  , false, false, false,  2, "SubAssignAny", false, true , "ParseNode", true , true , false, true  }, // 54
    { "MulAssignInt"                  , false, true , false,  2, "MulAssignInt", false, true , "ParseNode", true , true , false, true  }, // 55
    { "MulAssignFloat"                , false, true , false,  2, "MulAssignFloat", false, true , "ParseNode", true , true , false, true  }, // 56
    { "MulAssignAny"                  , false, false, false,  2, "MulAssignAny", false, true , "ParseNode", true , true , false, true  }, // 57
    { "DivAssignInt"                  , false, true , false,  2, "DivAssignInt", false, true , "ParseNode", true , true , false, true  }, // 58
    { "DivAssignFloat"                , false, true , false,  2, "DivAssignFloat", false, true , "ParseNode", true , true , false, true  }, // 59
    { "DivAssignAny"                  , false, false, false,  2, "DivAssignAny", false, true , "ParseNode", true , true , false, true  }, // 60
    { "ModAssignInt"                  , false, true , false,  2, "ModAssignInt", false, true , "ParseNode", true , true , false, true  }, // 61
    { "ModAssignAny"                  , false, false, false,  2, "ModAssignAny", false, true , "ParseNode", true , true , false, true  }, // 62
    { "AndAssignInt"                  , false, true , false,  2, "AndAssignInt", false, true , "ParseNode", true , true , false, true  }, // 63
    { "AndAssignAny"                  , false, false, false,  2, "AndAssignAny", false, true , "ParseNode", true , true , false, true  }, // 64
    { "OrAssignInt"                   , false, true , false,  2, "OrAssignInt", false, true , "ParseNode", true , true , false, true  }, // 65
    { "OrAssignAny"                   , false, false, false,  2, "OrAssignAny", false, true , "ParseNode", true , true , false, true  }, // 66
    { "XorAssignInt"                  , false, true , false,  2, "XorAssignInt", false, true , "ParseNode", true , true , false, true  }, // 67
    { "XorAssignAny"                  , false, false, false,  2, "XorAssignAny", false, true , "ParseNode", true , true , false, true  }, // 68
    { "LoadLValue"                    , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 69
    { "StoreLValue"                   , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 70
    { "PreIncLValue"                  , false, false, false,  0, "PreIncLValue", false, true , "ParseNode", true , false, false, false }, // 71
    { "PreDecLValue"                  , false, false, false,  0, "PreDecLValue", false, true , "ParseNode", true , false, false, false }, // 72
    { "PostIncLValue"                 , false, false, false,  0, "PostIncLValue", false, true , "ParseNode", true , false, false, false }, // 73
    { "PostDecLValue"                 , false, false, false,  0, "PostDecLValue", false, true , "ParseNode", true , false, false, false }, // 74
    { "AddAssignLValue"               , false, false, false,  1, "AddAssignLValue", false, true , "ParseNode", true , false, false, false }, // 75
    { "SubAssignLValue"               , false, false, false,  1, "SubAssignLValue", false, true , "ParseNode", true , false, false, false }, // 76
    { "MulAssignLValue"               , false, false, false,  1, "MulAssignLValue", false, true , "ParseNode", true , false, false, false }, // 77
    { "DivAssignLValue"               , false, false, false,  1, "DivAssignLValue", false, true , "ParseNode", true , false, false, false }, // 78
    { "ModAssignLValue"               , false, false, false,  1, "ModAssignLValue", false, true , "ParseNode", true , false, false, false }, // 79
    { "AndAssignLValue"               , false, false, false,  1, "AndAssignLValue", false, true , "ParseNode", true , false, false, false }, // 80
    { "OrAssignLValue"                , false, false, false,  1, "OrAssignLValue", false, true , "ParseNode", true , false, false, false }, // 81
    { "XorAssignLValue"               , false, false, false,  1, "XorAssignLValue", false, true , "ParseNode", true , false, false, false }, // 82
    { "ShlAssignLValue"               , false, false, false,  1, "ShlAssignLValue", false, true , "ParseNode", true , false, false, false }, // 83
    { "ShrAssignLValue"               , false, false, false,  1, "ShrAssignLValue", false, true , "ParseNode", true , false, false, false }, // 84
    { "ShiftLValue"                   , false, false, false,  0, "ShiftLValue", false, true , "ParseNode", true , false, false, false }, // 85
    { "UnshiftLValue"                 , false, false, false,  1, "UnshiftLValue", false, true , "ParseNode", true , false, false, false }, // 86
    { "PopAny"                        , false, false, false,  0, "PopAny", false, true , "ParseNode", true , false, false, false }, // 87
    { "PushAny"                       , false, false, false,  0, "PushAny", false, true , "ParseNode", true , false, false, false }, // 88
    { "SpliceLValue"                  , false, false, false,  3, "SpliceLValue", false, true , "ParseNode", true , false, false, false }, // 89
    { "ExtractAny"                    , false, false, false,  4, "ExtractAny", false, true , "ParseNode", true , false, false, false }, // 90
    { "ExtractList"                   , false, false, false,  4, "ExtractList", false, true , "ParseNode", true , false, false, false }, // 91
    { "ExtractString"                 , false, false, false,  4, "ExtractString", false, true , "ParseNode", true , false, false, false }, // 92
    { "ExtractBinary"                 , false, false, false,  4, "ExtractBinary", false, true , "ParseNode", true , false, false, false }, // 93
    { "RemoveAny"                     , false, false, false,  1, "RemoveAny", false, true , "ParseNode", true , false, false, false }, // 94
    { "RemoveList"                    , false, false, false,  1, "RemoveList", false, true , "ParseNode", true , false, false, false }, // 95
    { "RemoveHash"                    , false, false, false,  1, "RemoveHash", false, true , "ParseNode", true , false, false, false }, // 96
    { "RemoveObject"                  , false, false, false,  1, "RemoveObject", false, true , "ParseNode", true , false, false, false }, // 97
    { "RemoveString"                  , false, false, false,  1, "RemoveString", false, true , "ParseNode", true , false, false, false }, // 98
    { "RemoveBinary"                  , false, false, false,  1, "RemoveBinary", false, true , "ParseNode", true , false, false, false }, // 99
    { "KeysAny"                       , false, false, false,  1, "KeysAny", false, true , "ParseNode", true , true , true , false }, // 100
    { "KeysList"                      , false, false, false,  1, "KeysList", false, true , "ParseNode", true , true , true , false }, // 101
    { "KeysHash"                      , false, false, false,  1, "KeysHash", false, true , "ParseNode", true , true , true , false }, // 102
    { "RegexMatchAny"                 , false, false, false,  1, "RegexMatchAny", false, true , "ParseNode", true , true , false, false }, // 103
    { "RegexMatchBool"                , false, true , false,  1, "RegexMatchBool", false, true , "ParseNode", true , true , false, false }, // 104
    { "RegexNMatchBool"               , false, true , false,  1, "RegexNMatchBool", false, true , "ParseNode", true , true , false, false }, // 105
    { "RegexExtractAny"               , false, false, false,  1, "RegexExtractAny", false, true , "ParseNode", true , true , false, false }, // 106
    { "RegexExtractList"              , false, false, false,  1, "RegexExtractList", false, true , "ParseNode", true , true , false, false }, // 107
    { "RegexSubstAny"                 , false, false, false,  1, "RegexSubstAny", false, true , "ParseNode", true , false, false, false }, // 108
    { "RegexSubstString"              , false, true , false,  1, "RegexSubstString", false, true , "ParseNode", true , false, false, false }, // 109
    { "InstanceOfBool"                , false, true , false,  1, "InstanceOfBool", false, true , "ParseNode", true , true , false, false }, // 110
    { "TrimAny"                       , false, false, false,  0, "TrimAny", false, true , "ParseNode", true , false, false, false }, // 111
    { "TrimString"                    , false, true , false,  0, "TrimString", false, true , "ParseNode", true , false, false, false }, // 112
    { "ChompAny"                      , false, false, false,  0, "ChompAny", false, true , "ParseNode", true , false, false, false }, // 113
    { "ChompString"                   , false, true , false,  0, "ChompString", false, true , "ParseNode", true , false, false, false }, // 114
    { "TransliterateAny"              , false, true , false,  0, "TransliterateAny", false, true , "ParseNode", true , false, false, false }, // 115
    { "TransliterateString"           , false, true , false,  0, "TransliterateString", false, true , "ParseNode", true , false, false, false }, // 116
    { "BackgroundInt"                 , false, true , false,  0, "BackgroundInt", false, true , "ParseNode", true , false, false, false }, // 117
    { "ListAssignAny"                 , false, false, false,  0, "ListAssignAny", false, true , "ParseNode", true , false, false, false }, // 118
    { "ExistsAny"                     , false, false, false,  1, "ExistsAny", false, true , "ParseNode", true , true , true , false }, // 119
    { "ExistsBool"                    , false, true , false,  1, "ExistsBool", false, true , "ParseNode", true , true , true , false }, // 120
    { "ElementsAny"                   , false, false, false,  1, "ElementsAny", false, true , "ParseNode", true , true , true , false }, // 121
    { "ElementsInt"                   , false, true , false,  1, "ElementsInt", false, true , "ParseNode", true , true , true , false }, // 122
    { "DotEvalAny"                    , false, false, false,  1, "DotEvalAny", false, true , "ParseNode", true , false, false, false }, // 123
    { "DotEvalInt"                    , false, false, false,  1, "DotEvalInt", false, true , "ParseNode", true , false, false, false }, // 124
    { "DotEvalFloat"                  , false, false, false,  1, "DotEvalFloat", false, true , "ParseNode", true , false, false, false }, // 125
    { "DotEvalString"                 , false, false, false,  1, "DotEvalString", false, true , "ParseNode", true , false, false, false }, // 126
    { "DotEvalDate"                   , false, false, false,  1, "DotEvalDate", false, true , "ParseNode", true , false, false, false }, // 127
    { "DotEvalList"                   , false, false, false,  1, "DotEvalList", false, true , "ParseNode", true , false, false, false }, // 128
    { "DotEvalHash"                   , false, false, false,  1, "DotEvalHash", false, true , "ParseNode", true , false, false, false }, // 129
    { "DotEvalObject"                 , false, false, false,  1, "DotEvalObject", false, true , "ParseNode", true , false, false, false }, // 130
    { "MapSelectList"                 , false, false, false,  3, "MapSelectList", false, true , "ParseNode", true , false, false, false }, // 131
    { "HashMap"                       , false, false, false,  3, "HashMap", false, true , "ParseNode", true , false, false, false }, // 132
    { "HashMapSelect"                 , false, false, false,  4, "HashMapSelect", false, true , "ParseNode", true , false, false, false }, // 133
    { "IteratorCreate"                , false, false, false,  0, "IteratorCreate", false, true , "ParseNode", true , false, false, false }, // 134
    { "IteratorNext"                  , false, false, true ,  0, "IteratorNext", false, true , "ParseNode", true , false, false, false }, // 135
    { "OnBlockExit"                   , false, false, false,  0, "OnBlockExit", false, true , "ParseNode", false, false, false, false }, // 136
    { "ScopeEnter"                    , false, false, false,  0, "ScopeEnter", false, true , "ParseNode", false, false, false, false }, // 137
    { "ScopeExit"                     , false, false, false,  0, "ScopeExit", false, true , "ParseNode", false, false, false, false }, // 138
    { "ThreadExit"                    , false, false, true ,  0, "ThreadExit", false, true , "ParseNode", false, false, false, false }, // 139
    { "Context"                       , false, false, false,  0, "Context", false, true , "ParseNode", false, false, false, false }, // 140
    { "Summarize"                     , false, false, false,  0, "Summarize", false, true , "ParseNode", false, false, false, false }, // 141
    { "EqInt"                         , false, true , false,  2, "EqInt", false, true , "ParseNode", true , true , false, true  }, // 142
    { "EqFloat"                       , false, true , false,  2, "EqFloat", false, true , "ParseNode", true , true , false, true  }, // 143
    { "EqString"                      , false, false, false,  2, "EqString", false, true , "ParseNode", true , false, false, false }, // 144
    { "EqAny"                         , false, true , false,  2, "EqAny", false, true , "ParseNode", true , true , false, true  }, // 145
    { "NeInt"                         , false, true , false,  2, "NeInt", false, true , "ParseNode", true , true , false, true  }, // 146
    { "NeFloat"                       , false, true , false,  2, "NeFloat", false, true , "ParseNode", true , true , false, true  }, // 147
    { "NeString"                      , false, false, false,  2, "NeString", false, true , "ParseNode", true , false, false, false }, // 148
    { "NeAny"                         , false, true , false,  2, "NeAny", false, true , "ParseNode", true , true , false, true  }, // 149
    { "EqHard"                        , false, true , false,  2, "EqHard", false, true , "ParseNode", true , true , false, true  }, // 150
    { "NeHard"                        , false, true , false,  2, "NeHard", false, true , "ParseNode", true , true , false, true  }, // 151
    { "LtInt"                         , false, true , false,  2, "LtInt", false, true , "ParseNode", true , true , false, true  }, // 152
    { "LtFloat"                       , false, true , false,  2, "LtFloat", false, true , "ParseNode", true , true , false, true  }, // 153
    { "LtString"                      , false, false, false,  2, "LtString", false, true , "ParseNode", true , false, false, false }, // 154
    { "LtAny"                         , false, true , false,  2, "LtAny", false, true , "ParseNode", true , true , false, true  }, // 155
    { "LeInt"                         , false, true , false,  2, "LeInt", false, true , "ParseNode", true , true , false, true  }, // 156
    { "LeFloat"                       , false, true , false,  2, "LeFloat", false, true , "ParseNode", true , true , false, true  }, // 157
    { "LeString"                      , false, false, false,  2, "LeString", false, true , "ParseNode", true , false, false, false }, // 158
    { "LeAny"                         , false, true , false,  2, "LeAny", false, true , "ParseNode", true , true , false, true  }, // 159
    { "GtInt"                         , false, true , false,  2, "GtInt", false, true , "ParseNode", true , true , false, true  }, // 160
    { "GtFloat"                       , false, true , false,  2, "GtFloat", false, true , "ParseNode", true , true , false, true  }, // 161
    { "GtString"                      , false, false, false,  2, "GtString", false, true , "ParseNode", true , false, false, false }, // 162
    { "GtAny"                         , false, true , false,  2, "GtAny", false, true , "ParseNode", true , true , false, true  }, // 163
    { "GeInt"                         , false, true , false,  2, "GeInt", false, true , "ParseNode", true , true , false, true  }, // 164
    { "GeFloat"                       , false, true , false,  2, "GeFloat", false, true , "ParseNode", true , true , false, true  }, // 165
    { "GeString"                      , false, false, false,  2, "GeString", false, true , "ParseNode", true , false, false, false }, // 166
    { "GeAny"                         , false, true , false,  2, "GeAny", false, true , "ParseNode", true , true , false, true  }, // 167
    { "CmpInt"                        , false, true , false,  2, "CmpInt", false, true , "ParseNode", true , true , false, true  }, // 168
    { "CmpFloat"                      , false, true , false,  2, "CmpFloat", false, true , "ParseNode", true , true , false, true  }, // 169
    { "CmpString"                     , false, false, false,  2, "CmpString", false, true , "ParseNode", true , false, false, false }, // 170
    { "CmpAny"                        , false, true , false,  2, "CmpAny", false, true , "ParseNode", true , true , false, true  }, // 171
    { "ToBool"                        , false, true , false,  1, "ToBool", false, true , "ParseNode", true , true , true , false }, // 172
    { "Not"                           , false, true , false,  1, "Not", false, true , "ParseNode", true , true , true , false }, // 173
    { "IsNullOrNothing"               , false, true , false,  1, "IsNullOrNothing", false, true , "ParseNode", true , true , true , false }, // 174
    { "Phi"                           , false, false, false, -1, "Phi", false, true , "ParseNode", true , false, false, false }, // 175
    { "UnaryPlusAny"                  , false, false, false,  1, "UnaryPlusAny", false, true , "ParseNode", true , true , true , false }, // 176
    { "UnaryMinusInt"                 , false, true , false,  1, "UnaryMinusInt", false, true , "ParseNode", true , true , true , false }, // 177
    { "UnaryMinusFloat"               , false, true , false,  1, "UnaryMinusFloat", false, true , "ParseNode", true , true , true , false }, // 178
    { "UnaryMinusAny"                 , false, false, false,  1, "UnaryMinusAny", false, true , "ParseNode", true , true , true , false }, // 179
    { "FoldlAny"                      , false, false, false,  2, "FoldlAny", false, true , "ParseNode", true , true , false, true  }, // 180
    { "FoldlInt"                      , false, false, false,  2, "FoldlInt", false, true , "ParseNode", true , true , false, true  }, // 181
    { "FoldlFloat"                    , false, false, false,  2, "FoldlFloat", false, true , "ParseNode", true , true , false, true  }, // 182
    { "FoldrAny"                      , false, false, false,  2, "FoldrAny", false, true , "ParseNode", true , true , false, true  }, // 183
    { "FoldrInt"                      , false, false, false,  2, "FoldrInt", false, true , "ParseNode", true , true , false, true  }, // 184
    { "FoldrFloat"                    , false, false, false,  2, "FoldrFloat", false, true , "ParseNode", true , true , false, true  }, // 185
    { "FoldlSumInt"                   , false, false, false,  2, "FoldlSumInt", false, true , "ParseNode", true , true , false, true  }, // 186
    { "FoldlSumFloat"                 , false, false, false,  2, "FoldlSumFloat", false, true , "ParseNode", true , true , false, true  }, // 187
    { "FoldlProdInt"                  , false, false, false,  2, "FoldlProdInt", false, true , "ParseNode", true , true , false, true  }, // 188
    { "FoldlProdFloat"                , false, false, false,  2, "FoldlProdFloat", false, true , "ParseNode", true , true , false, true  }, // 189
    { "FoldlDiffInt"                  , false, false, false,  2, "FoldlDiffInt", false, true , "ParseNode", true , true , false, true  }, // 190
    { "FoldlDiffFloat"                , false, false, false,  2, "FoldlDiffFloat", false, true , "ParseNode", true , true , false, true  }, // 191
    { "FoldlMinInt"                   , false, false, false,  2, "FoldlMinInt", false, true , "ParseNode", true , true , false, true  }, // 192
    { "FoldlMinFloat"                 , false, false, false,  2, "FoldlMinFloat", false, true , "ParseNode", true , true , false, true  }, // 193
    { "FoldlMaxInt"                   , false, false, false,  2, "FoldlMaxInt", false, true , "ParseNode", true , true , false, true  }, // 194
    { "FoldlMaxFloat"                 , false, false, false,  2, "FoldlMaxFloat", false, true , "ParseNode", true , true , false, true  }, // 195
    { "FoldrSumInt"                   , false, false, false,  2, "FoldrSumInt", false, true , "ParseNode", true , true , false, true  }, // 196
    { "FoldrSumFloat"                 , false, false, false,  2, "FoldrSumFloat", false, true , "ParseNode", true , true , false, true  }, // 197
    { "FoldrProdInt"                  , false, false, false,  2, "FoldrProdInt", false, true , "ParseNode", true , true , false, true  }, // 198
    { "FoldrProdFloat"                , false, false, false,  2, "FoldrProdFloat", false, true , "ParseNode", true , true , false, true  }, // 199
    { "FoldrDiffInt"                  , false, false, false,  2, "FoldrDiffInt", false, true , "ParseNode", true , true , false, true  }, // 200
    { "FoldrDiffFloat"                , false, false, false,  2, "FoldrDiffFloat", false, true , "ParseNode", true , true , false, true  }, // 201
    { "FoldrMinInt"                   , false, false, false,  2, "FoldrMinInt", false, true , "ParseNode", true , true , false, true  }, // 202
    { "FoldrMinFloat"                 , false, false, false,  2, "FoldrMinFloat", false, true , "ParseNode", true , true , false, true  }, // 203
    { "FoldrMaxInt"                   , false, false, false,  2, "FoldrMaxInt", false, true , "ParseNode", true , true , false, true  }, // 204
    { "FoldrMaxFloat"                 , false, false, false,  2, "FoldrMaxFloat", false, true , "ParseNode", true , true , false, true  }, // 205
    { "MapAny"                        , false, false, false,  2, "MapAny", false, true , "ParseNode", true , true , false, true  }, // 206
    { "MapInt"                        , false, false, false,  2, "MapInt", false, true , "ParseNode", true , true , false, true  }, // 207
    { "MapFloat"                      , false, false, false,  2, "MapFloat", false, true , "ParseNode", true , true , false, true  }, // 208
    { "MapScaleInt"                   , false, false, false,  2, "MapScaleInt", false, true , "ParseNode", true , false, false, false }, // 209
    { "MapScaleFloat"                 , false, false, false,  2, "MapScaleFloat", false, true , "ParseNode", true , false, false, false }, // 210
    { "MapOffsetInt"                  , false, false, false,  2, "MapOffsetInt", false, true , "ParseNode", true , false, false, false }, // 211
    { "MapOffsetFloat"                , false, false, false,  2, "MapOffsetFloat", false, true , "ParseNode", true , false, false, false }, // 212
    { "MapSquareInt"                  , false, false, false,  2, "MapSquareInt", false, true , "ParseNode", true , false, false, false }, // 213
    { "MapSquareFloat"                , false, false, false,  2, "MapSquareFloat", false, true , "ParseNode", true , false, false, false }, // 214
    { "MapHashKeyValue"               , false, false, false,  1, "MapHashKeyValue", false, true , "ParseNode", true , false, false, false }, // 215
    { "MapHashKeyInt"                 , false, false, false,  1, "MapHashKeyInt", false, true , "ParseNode", true , false, false, false }, // 216
    { "MapHashKeyOffsetInt"           , false, false, false,  2, "MapHashKeyOffsetInt", false, true , "ParseNode", true , false, false, false }, // 217
    { "MapHashKeyScaleInt"            , false, false, false,  2, "MapHashKeyScaleInt", false, true , "ParseNode", true , false, false, false }, // 218
    { "HashMapTwoKeys"                , false, false, false,  1, "HashMapTwoKeys", false, true , "ParseNode", true , false, false, false }, // 219
    { "SelectAny"                     , false, false, false,  2, "SelectAny", false, true , "ParseNode", true , true , false, true  }, // 220
    { "SelectInt"                     , false, false, false,  2, "SelectInt", false, true , "ParseNode", true , true , false, true  }, // 221
    { "SelectFloat"                   , false, false, false,  2, "SelectFloat", false, true , "ParseNode", true , true , false, true  }, // 222
    { "SelectPositiveInt"             , false, false, false,  2, "SelectPositiveInt", false, true , "ParseNode", true , false, false, false }, // 223
    { "SelectPositiveFloat"           , false, false, false,  2, "SelectPositiveFloat", false, true , "ParseNode", true , false, false, false }, // 224
    { "SelectNonZeroInt"              , false, false, false,  2, "SelectNonZeroInt", false, true , "ParseNode", true , false, false, false }, // 225
    { "SelectNonZeroFloat"            , false, false, false,  2, "SelectNonZeroFloat", false, true , "ParseNode", true , false, false, false }, // 226
    { "FusedMapSelectScalePositiveInt", false, false, false,  2, "FusedMapSelectScalePositiveInt", false, true , "ParseNode", true , false, false, false }, // 227
    { "FusedMapSelectScalePositiveFloat", false, false, false,  2, "FusedMapSelectScalePositiveFloat", false, true , "ParseNode", true , false, false, false }, // 228
    { "FusedMapSelectOffsetPositiveInt", false, false, false,  2, "FusedMapSelectOffsetPositiveInt", false, true , "ParseNode", true , false, false, false }, // 229
    { "FusedMapSelectOffsetPositiveFloat", false, false, false,  2, "FusedMapSelectOffsetPositiveFloat", false, true , "ParseNode", true , false, false, false }, // 230
    { "FusedMapSelectSquarePositiveInt", false, false, false,  2, "FusedMapSelectSquarePositiveInt", false, true , "ParseNode", true , false, false, false }, // 231
    { "FusedMapSelectSquarePositiveFloat", false, false, false,  2, "FusedMapSelectSquarePositiveFloat", false, true , "ParseNode", true , false, false, false }, // 232
    { "FusedMapFoldlSumScaleInt"      , false, false, false,  2, "FusedMapFoldlSumScaleInt", false, true , "ParseNode", true , true , false, true  }, // 233
    { "FusedMapFoldlSumScaleFloat"    , false, false, false,  2, "FusedMapFoldlSumScaleFloat", false, true , "ParseNode", true , true , false, true  }, // 234
    { "FusedMapFoldlSumSquareInt"     , false, false, false,  2, "FusedMapFoldlSumSquareInt", false, true , "ParseNode", true , true , false, true  }, // 235
    { "FusedMapFoldlSumSquareFloat"   , false, false, false,  2, "FusedMapFoldlSumSquareFloat", false, true , "ParseNode", true , true , false, true  }, // 236
    { "FusedMapFoldlProdScaleInt"     , false, false, false,  2, "FusedMapFoldlProdScaleInt", false, true , "ParseNode", true , true , false, true  }, // 237
    { "FusedMapFoldlProdScaleFloat"   , false, false, false,  2, "FusedMapFoldlProdScaleFloat", false, true , "ParseNode", true , true , false, true  }, // 238
    { "MapSelectAny"                  , false, false, false, -1, "MapSelectAny", false, true , "ParseNode", true , false, false, false }, // 239
    { "HashMapAny"                    , false, false, false, -1, "HashMapAny", false, true , "ParseNode", true , false, false, false }, // 240
    { "HashMapSelectAny"              , false, false, false, -1, "HashMapSelectAny", false, true , "ParseNode", true , false, false, false }, // 241
    { "RangeAny"                      , false, false, false,  2, "RangeAny", false, true , "ParseNode", true , true , false, true  }, // 242
    { "RangeInt"                      , false, false, false,  2, "RangeInt", false, true , "ParseNode", true , true , false, true  }, // 243
    { "RangeFloat"                    , false, false, false,  2, "RangeFloat", false, true , "ParseNode", true , true , false, true  }, // 244
    { "RangeDate"                     , false, false, false,  2, "RangeDate", false, true , "ParseNode", true , true , false, true  }, // 245
    { "RangeSliceAny"                 , false, false, false,  3, "RangeSliceAny", false, true , "ParseNode", true , false, false, false }, // 246
    { "RangeSliceInt"                 , false, false, false,  3, "RangeSliceInt", false, true , "ParseNode", true , false, false, false }, // 247
    { "RangeSliceFloat"               , false, false, false,  3, "RangeSliceFloat", false, true , "ParseNode", true , false, false, false }, // 248
    { "CastAny"                       , false, false, false,  1, "CastAny", false, true , "ParseNode", true , true , false, false }, // 249
    { "CastList"                      , false, false, false,  1, "CastList", false, true , "ParseNode", true , true , false, false }, // 250
    { "CastHash"                      , false, false, false,  1, "CastHash", false, true , "ParseNode", true , true , false, false }, // 251
    { "CastObject"                    , false, false, false,  1, "CastObject", false, true , "ParseNode", true , true , false, false }, // 252
    { "CastEnum"                      , false, false, false,  1, "CastEnum", false, true , "ParseNode", true , true , false, false }, // 253
    { "CastComplexHash"               , false, false, false,  1, "CastComplexHash", false, true , "ParseNode", true , false, false, false }, // 254
    { "Br"                            , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false }, // 255
    { "BrIf"                          , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false }, // 256
    { "SwitchInt"                     , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false }, // 257
    { "SwitchString"                  , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false }, // 258
    { "Return"                        , false, false, true ,  0, "Control transfer", false, true , "ControlFlowNode", false, false, false, false }, // 259
    { "ReturnNothing"                 , false, false, true ,  0, "Control transfer", false, true , "ControlFlowNode", false, false, false, false }, // 260
    { "LoadLocal"                     , true , false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 261
    { "StoreLocal"                    , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 262
    { "UninstantiateLocal"            , false, false, false,  0, "UninstantiateLocal", false, true , "ParseNode", false, false, false, false }, // 263
    { "LoadArg"                       , false, false, false, -1, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 264
    { "LoadClosure"                   , true , false, false, -1, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 265
    { "StoreClosure"                  , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 266
    { "LoadGlobal"                    , true , false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 267
    { "StoreGlobal"                   , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 268
    { "LoadThreadLocal"               , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 269
    { "StoreThreadLocal"              , false, false, false,  1, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 270
    { "LoadImplicitArg"               , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 271
    { "LoadImplicitArgv"              , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 272
    { "LoadImplicitElement"           , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 273
    { "PushImplicitArg"               , false, false, false,  1, "PushImplicitArg", false, true , "ParseNode", true , false, false, false }, // 274
    { "SetImplicitArgv"               , false, false, false,  1, "SetImplicitArgv", false, true , "ParseNode", true , false, false, false }, // 275
    { "PopImplicitArg"                , false, false, false,  1, "PopImplicitArg", false, true , "ParseNode", false, false, false, false }, // 276
    { "PushImplicitElement"           , false, false, false,  1, "PushImplicitElement", false, true , "ParseNode", true , false, false, false }, // 277
    { "PopImplicitElement"            , false, false, false,  1, "PopImplicitElement", false, true , "ParseNode", false, false, false, false }, // 278
    { "HashKeyAccess"                 , true , false, false,  1, "HashKeyAccess", false, true , "ParseNode", true , true , false, false }, // 279
    { "HashKeyAccessInt"              , true , false, false,  1, "HashKeyAccessInt", false, true , "ParseNode", true , true , false, false }, // 280
    { "LoadSelfMember"                , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 281
    { "LoadStaticVar"                 , false, false, false,  0, "Load variable value", false, false, "VarRefNode", true , false, false, false }, // 282
    { "NewObject"                     , false, false, false,  0, "NewObject", false, true , "ParseNode", true , false, false, false }, // 283
    { "LoadConstant"                  , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 284
    { "CreateClosure"                 , false, false, false,  0, "CreateClosure", false, true , "ParseNode", true , false, false, false }, // 285
    { "CreateCallRef"                 , false, false, false,  0, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 286
    { "CreateMethodRef"               , false, false, false,  0, "CreateMethodRef", false, true , "ParseNode", true , false, false, false }, // 287
    { "CreateParseRef"                , false, false, false,  0, "CreateParseRef", false, true , "ParseNode", true , false, false, false }, // 288
    { "NewHashDecl"                   , false, false, false,  0, "NewHashDecl", false, true , "ParseNode", true , false, false, false }, // 289
    { "NewComplexHash"                , false, false, false,  0, "NewComplexHash", false, true , "ParseNode", true , false, false, false }, // 290
    { "NewComplexList"                , false, false, false,  0, "NewComplexList", false, true , "ParseNode", true , false, false, false }, // 291
    { "VrnConstruct"                  , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 292
    { "HashSetKeyValue"               , false, false, false,  3, "HashSetKeyValue", false, true , "ParseNode", false, false, false, false }, // 293
    { "IteratorCreateReverse"         , false, false, false,  1, "IteratorCreateReverse", false, true , "ParseNode", true , false, false, false }, // 294
    { "Call"                          , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 295
    { "CallDirect"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 296
    { "CallIndirect"                  , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 297
    { "CallMethod"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 298
    { "CallMethodDirect"              , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 299
    { "InvokeMethodDirect"            , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 300
    { "CallStatic"                    , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 301
    { "CallStaticDirect"              , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 302
    { "DotEvalMethodDirect"           , true , false, false, -1, "DotEvalMethodDirect", false, true , "ParseNode", true , false, false, false }, // 303
    { "InvokeDotEvalMethodDirect"     , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 304
    { "CallClosureDirect"             , true , false, false, -1, "Call function or method", true , true , "FunctionCallNode", true , true , false, false }, // 305
    { "Invoke"                        , true , false, true , -1, "Call function or method", true , true , "FunctionCallNode", true , false, false, false }, // 306
    { "GuardInt"                      , false, false, false,  1, "GuardInt", false, true , "ParseNode", false, false, false, false }, // 307
    { "GetObjectClass"                , false, false, false,  1, "GetObjectClass", false, true , "ParseNode", true , false, false, false }, // 308
    { "GuardFloat"                    , false, false, false,  1, "GuardFloat", false, true , "ParseNode", false, false, false, false }, // 309
    { "GuardType"                     , false, false, false,  1, "GuardType", false, true , "ParseNode", false, false, false, false }, // 310
    { "GuardNotNothing"               , false, false, false,  1, "GuardNotNothing", false, true , "ParseNode", false, false, false, false }, // 311
    { "LandingPad"                    , false, false, false,  0, "LandingPad", false, true , "ParseNode", false, false, false, false }, // 312
    { "CatchException"                , true , false, false,  0, "CatchException", false, true , "ParseNode", true , false, false, false }, // 313
    { "CatchCleanup"                  , false, false, false,  0, "CatchCleanup", false, true , "ParseNode", false, false, false, false }, // 314
    { "Rethrow"                       , false, false, true , -1, "Rethrow", false, true , "ParseNode", false, false, false, false }, // 315
    { "Throw"                         , false, false, true ,  1, "Control transfer", false, true , "ControlFlowNode", false, false, false, false }, // 316
    { "InvokeSimError"                , false, false, true ,  0, "Call function or method", true , true , "FunctionCallNode", false, false, false, false }, // 317
    { "Incref"                        , false, false, false,  1, "Incref", false, true , "ParseNode", false, false, false, false }, // 318
    { "Decref"                        , false, false, false,  1, "Decref", false, true , "ParseNode", false, false, false, false }, // 319
    { "DecrefNoThrow"                 , false, false, false,  1, "Control transfer", false, true , "ControlFlowNode", false, false, false, false }, // 320
    { "SwitchRegexMatch"              , false, false, false,  1, "Control flow", false, false, "ControlFlowNode", true , false, false, false }, // 321
    { "ListPush"                      , false, false, false,  2, "ListPush", false, true , "ParseNode", true , false, false, false }, // 322
    { "RefForeachInit"                , false, false, false,  0, "RefForeachInit", false, true , "ParseNode", true , false, false, false }, // 323
    { "RefForeachSize"                , false, false, false,  1, "RefForeachSize", false, true , "ParseNode", true , false, false, false }, // 324
    { "RefForeachGetEntry"            , false, false, false,  2, "RefForeachGetEntry", false, true , "ParseNode", true , false, false, false }, // 325
    { "RefForeachRecord"              , false, false, false,  2, "RefForeachRecord", false, true , "ParseNode", false, false, false, false }, // 326
    { "RefForeachFinalize"            , false, false, false,  2, "RefForeachFinalize", false, true , "ParseNode", false, false, false, false }, // 327
    { "RefForeachCleanup"             , false, false, false,  1, "RefForeachCleanup", false, true , "ParseNode", false, false, false, false }, // 328
    { "AddNumber"                     , false, false, false,  2, "AddNumber", false, true , "ParseNode", true , true , false, true  }, // 329
    { "SubNumber"                     , false, false, false,  2, "SubNumber", false, true , "ParseNode", true , true , false, true  }, // 330
    { "MulNumber"                     , false, false, false,  2, "MulNumber", false, true , "ParseNode", true , true , false, true  }, // 331
    { "DivNumber"                     , false, false, false,  2, "DivNumber", false, true , "ParseNode", true , true , false, true  }, // 332
    { "HashKeyStore"                  , false, false, false,  2, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 333
    { "ListIndexAccess"               , true , false, false,  2, "ListIndexAccess", false, true , "ParseNode", true , false, false, false }, // 334
    { "ListIndexStore"                , false, false, false,  3, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 335
    { "AddAssignLocalInt"             , false, false, false,  0, "AddAssignLocalInt", false, true , "ParseNode", true , false, false, false }, // 336
    { "IncrementLocalInt"             , false, false, false,  0, "IncrementLocalInt", false, true , "ParseNode", true , false, false, false }, // 337
    { "BranchIfLtLocalInt"            , false, false, true ,  0, "Control flow", false, false, "ControlFlowNode", false, false, false, false }, // 338
    { "ConstEnum"                     , false, false, false,  0, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 339
    { "ListGetValueNoRef"             , false, false, false,  2, "ListGetValueNoRef", false, true , "ParseNode", true , false, false, false }, // 340
    { "SwitchCaseMatch"               , false, false, false,  1, "Control flow", false, false, "ControlFlowNode", true , false, false, false }, // 341
    { "IsCollectionType"              , false, false, false,  1, "IsCollectionType", false, true , "ParseNode", true , true , true , false }, // 342
    { "MakeHashConstKeys"             , false, false, false, -1, "Load constant value", false, false, "ConstantNode", true , false, false, false }, // 343
    { "ToString"                      , false, false, false, -1, "ToString", false, true , "ParseNode", true , false, false, false }, // 344
    { "Sprintf"                       , false, false, false, -1, "Sprintf", false, true , "ParseNode", true , false, false, false }, // 345
    { "NewHashDeclFromHash"           , false, false, false,  1, "NewHashDeclFromHash", false, true , "ParseNode", true , false, false, false }, // 346
    { "InstantiateLocal"              , false, false, false,  0, "InstantiateLocal", false, true , "ParseNode", false, false, false, false }, // 347
    { "AddTimeout"                    , false, false, false,  2, "AddTimeout", false, true , "ParseNode", true , true , false, false }, // 348
    { "SubTimeout"                    , false, false, false,  2, "SubTimeout", false, true , "ParseNode", true , true , false, false }, // 349
    { "HashDerefDynamic"              , false, false, false,  2, "HashDerefDynamic", true , true , "ParseNode", true , true , false, true  }, // 350
    { "ListIndexDynamic"              , false, false, false,  2, "ListIndexDynamic", false, true , "ParseNode", true , true , false, true  }, // 351
    { "HashKeyStoreDynamic"           , false, false, false,  3, "Store to variable", true , true , "AssignmentNode", false, false, false, false }, // 352
    { "LValuePathAssign"              , false, false, false,  1, "LValuePathAssign", true , true , "AssignmentNode", true , false, false, false }, // 353
    { "LValuePathCompound"            , false, false, false,  1, "LValuePathCompound", true , true , "ParseNode", true , false, false, false }, // 354
    { "LValuePathUnary"               , false, false, false,  0, "LValuePathUnary", true , true , "ParseNode", true , false, false, false }, // 355
    { "LValuePathBinaryMut"           , false, false, false,  1, "LValuePathBinaryMut", true , true , "ParseNode", true , false, false, false }, // 356
    { "LValuePathTernary"             , false, false, false,  2, "LValuePathTernary", true , true , "ParseNode", true , false, false, false }, // 357
};

//! Static assertion to verify registry completeness
static_assert(
    sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == 358,
    "OPCODE_REGISTRY has incorrect entry count - should be exactly 358"
);

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

#endif
