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
//! PHASE 4: Extended metadata for analysis and optimization
struct OpcodeInfo {
    const char* name;                   //! Opcode name (e.g., "LoadLocal")
    bool can_return_nothing;            //! Can legitimately return NOTHING
    bool never_returns_nothing;         //! Always returns non-NOTHING value
    bool is_terminator;                 //! Ends a basic block (control flow)
    int expected_operands;              //! Expected operand count (-1 = variable)

    // PHASE 4: Extended properties for optimization and analysis
    const char* description;            //! What the opcode does (for debugging/docs)
    bool may_have_side_effects;         //! Can modify global state (globals, statics, objects)
    bool may_throw_exception;           //! Can raise exceptions at runtime
    const char* corresponding_ast_node; //! AST node type this IR opcode comes from
};

//! Registry of all 346 IR opcodes (in enum ID order, accounting for removed IDs 134, 141, 142)
//! PHASE 1 IMPLEMENTATION: Complete registry with verified properties from existing code
constexpr OpcodeInfo OPCODE_REGISTRY[355] = {
    { "ConstInt                            ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 0
    { "ConstFloat                          ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 1
    { "ConstBool                           ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 2
    { "ConstNothing                        ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 3
    { "ConstNull                           ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 4
    { "ConstString                         ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 5
    { "ConstDate                           ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 6
    { "MakeList                            ", false, false, false, -1, "MakeList", false, true , "ParseNode" }, // 7
    { "MakeHash                            ", false, false, false, -1, "MakeHash", false, true , "ParseNode" }, // 8
    { "CreateEmptyList                     ", false, false, false, -1, "CreateEmptyList", false, true , "ParseNode" }, // 9
    { "CreateSizedList                     ", false, false, false, -1, "CreateSizedList", false, true , "ParseNode" }, // 10
    { "ListAppend                          ", false, false, false, -1, "ListAppend", false, true , "ParseNode" }, // 11
    { "ListSize                            ", false, false, false, -1, "ListSize", false, true , "ParseNode" }, // 12
    { "ListGetInt                          ", false, false, false, -1, "ListGetInt", false, true , "ParseNode" }, // 13
    { "ListGetFloat                        ", false, false, false, -1, "ListGetFloat", false, true , "ParseNode" }, // 14
    { "ListGetValue                        ", false, false, false, -1, "ListGetValue", false, true , "ParseNode" }, // 15
    { "ListSetInt                          ", false, false, false, -1, "ListSetInt", false, true , "ParseNode" }, // 16
    { "ListSetFloat                        ", false, false, false, -1, "ListSetFloat", false, true , "ParseNode" }, // 17
    { "ListSetValue                        ", false, false, false, -1, "ListSetValue", false, true , "ParseNode" }, // 18
    { "AddInt                              ", false, true , false, -1, "AddInt", false, true , "ParseNode" }, // 19
    { "AddFloat                            ", false, true , false, -1, "AddFloat", false, true , "ParseNode" }, // 20
    { "AddAny                              ", false, false, false, -1, "AddAny", false, true , "ParseNode" }, // 21
    { "AddString                           ", false, false, false, -1, "AddString", false, true , "ParseNode" }, // 22
    { "StringConcat                        ", false, false, false, -1, "StringConcat", false, true , "ParseNode" }, // 23
    { "SubInt                              ", false, true , false, -1, "SubInt", false, true , "ParseNode" }, // 24
    { "SubFloat                            ", false, true , false, -1, "SubFloat", false, true , "ParseNode" }, // 25
    { "SubAny                              ", false, false, false, -1, "SubAny", false, true , "ParseNode" }, // 26
    { "MulInt                              ", false, true , false, -1, "MulInt", false, true , "ParseNode" }, // 27
    { "MulFloat                            ", false, true , false, -1, "MulFloat", false, true , "ParseNode" }, // 28
    { "MulAny                              ", false, false, false, -1, "MulAny", false, true , "ParseNode" }, // 29
    { "DivInt                              ", false, true , false, -1, "DivInt", false, true , "ParseNode" }, // 30
    { "DivFloat                            ", false, true , false, -1, "DivFloat", false, true , "ParseNode" }, // 31
    { "DivAny                              ", false, false, false, -1, "DivAny", false, true , "ParseNode" }, // 32
    { "ModInt                              ", false, true , false, -1, "ModInt", false, true , "ParseNode" }, // 33
    { "ModAny                              ", false, false, false, -1, "ModAny", false, true , "ParseNode" }, // 34
    { "AndInt                              ", false, true , false, -1, "AndInt", false, true , "ParseNode" }, // 35
    { "AndAny                              ", false, false, false, -1, "AndAny", false, true , "ParseNode" }, // 36
    { "OrInt                               ", false, true , false, -1, "OrInt", false, true , "ParseNode" }, // 37
    { "OrAny                               ", false, false, false, -1, "OrAny", false, true , "ParseNode" }, // 38
    { "XorInt                              ", false, true , false, -1, "XorInt", false, true , "ParseNode" }, // 39
    { "XorAny                              ", false, false, false, -1, "XorAny", false, true , "ParseNode" }, // 40
    { "ShlInt                              ", false, true , false, -1, "ShlInt", false, true , "ParseNode" }, // 41
    { "ShlAny                              ", false, false, false, -1, "ShlAny", false, true , "ParseNode" }, // 42
    { "ShrInt                              ", false, true , false, -1, "ShrInt", false, true , "ParseNode" }, // 43
    { "ShrAny                              ", false, false, false, -1, "ShrAny", false, true , "ParseNode" }, // 44
    { "ShlAssignInt                        ", false, true , false, -1, "ShlAssignInt", false, true , "ParseNode" }, // 45
    { "ShlAssignAny                        ", false, false, false, -1, "ShlAssignAny", false, true , "ParseNode" }, // 46
    { "ShrAssignInt                        ", false, true , false, -1, "ShrAssignInt", false, true , "ParseNode" }, // 47
    { "ShrAssignAny                        ", false, false, false, -1, "ShrAssignAny", false, true , "ParseNode" }, // 48
    { "AddAssignInt                        ", false, true , false, -1, "AddAssignInt", false, true , "ParseNode" }, // 49
    { "AddAssignFloat                      ", false, true , false, -1, "AddAssignFloat", false, true , "ParseNode" }, // 50
    { "AddAssignAny                        ", false, false, false, -1, "AddAssignAny", false, true , "ParseNode" }, // 51
    { "SubAssignInt                        ", false, true , false, -1, "SubAssignInt", false, true , "ParseNode" }, // 52
    { "SubAssignFloat                      ", false, true , false, -1, "SubAssignFloat", false, true , "ParseNode" }, // 53
    { "SubAssignAny                        ", false, false, false, -1, "SubAssignAny", false, true , "ParseNode" }, // 54
    { "MulAssignInt                        ", false, true , false, -1, "MulAssignInt", false, true , "ParseNode" }, // 55
    { "MulAssignFloat                      ", false, true , false, -1, "MulAssignFloat", false, true , "ParseNode" }, // 56
    { "MulAssignAny                        ", false, false, false, -1, "MulAssignAny", false, true , "ParseNode" }, // 57
    { "DivAssignInt                        ", false, true , false, -1, "DivAssignInt", false, true , "ParseNode" }, // 58
    { "DivAssignFloat                      ", false, true , false, -1, "DivAssignFloat", false, true , "ParseNode" }, // 59
    { "DivAssignAny                        ", false, false, false, -1, "DivAssignAny", false, true , "ParseNode" }, // 60
    { "ModAssignInt                        ", false, true , false, -1, "ModAssignInt", false, true , "ParseNode" }, // 61
    { "ModAssignAny                        ", false, false, false, -1, "ModAssignAny", false, true , "ParseNode" }, // 62
    { "AndAssignInt                        ", false, true , false, -1, "AndAssignInt", false, true , "ParseNode" }, // 63
    { "AndAssignAny                        ", false, false, false, -1, "AndAssignAny", false, true , "ParseNode" }, // 64
    { "OrAssignInt                         ", false, true , false, -1, "OrAssignInt", false, true , "ParseNode" }, // 65
    { "OrAssignAny                         ", false, false, false, -1, "OrAssignAny", false, true , "ParseNode" }, // 66
    { "XorAssignInt                        ", false, true , false, -1, "XorAssignInt", false, true , "ParseNode" }, // 67
    { "XorAssignAny                        ", false, false, false, -1, "XorAssignAny", false, true , "ParseNode" }, // 68
    { "LoadLValue                          ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 69
    { "StoreLValue                         ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 70
    { "PreIncLValue                        ", false, false, false, -1, "PreIncLValue", false, true , "ParseNode" }, // 71
    { "PreDecLValue                        ", false, false, false, -1, "PreDecLValue", false, true , "ParseNode" }, // 72
    { "PostIncLValue                       ", false, false, false, -1, "PostIncLValue", false, true , "ParseNode" }, // 73
    { "PostDecLValue                       ", false, false, false, -1, "PostDecLValue", false, true , "ParseNode" }, // 74
    { "AddAssignLValue                     ", false, false, false, -1, "AddAssignLValue", false, true , "ParseNode" }, // 75
    { "SubAssignLValue                     ", false, false, false, -1, "SubAssignLValue", false, true , "ParseNode" }, // 76
    { "MulAssignLValue                     ", false, false, false, -1, "MulAssignLValue", false, true , "ParseNode" }, // 77
    { "DivAssignLValue                     ", false, false, false, -1, "DivAssignLValue", false, true , "ParseNode" }, // 78
    { "ModAssignLValue                     ", false, false, false, -1, "ModAssignLValue", false, true , "ParseNode" }, // 79
    { "AndAssignLValue                     ", false, false, false, -1, "AndAssignLValue", false, true , "ParseNode" }, // 80
    { "OrAssignLValue                      ", false, false, false, -1, "OrAssignLValue", false, true , "ParseNode" }, // 81
    { "XorAssignLValue                     ", false, false, false, -1, "XorAssignLValue", false, true , "ParseNode" }, // 82
    { "ShlAssignLValue                     ", false, false, false, -1, "ShlAssignLValue", false, true , "ParseNode" }, // 83
    { "ShrAssignLValue                     ", false, false, false, -1, "ShrAssignLValue", false, true , "ParseNode" }, // 84
    { "ShiftLValue                         ", false, false, false, -1, "ShiftLValue", false, true , "ParseNode" }, // 85
    { "UnshiftLValue                       ", false, false, false, -1, "UnshiftLValue", false, true , "ParseNode" }, // 86
    { "PopAny                              ", false, false, false, -1, "PopAny", false, true , "ParseNode" }, // 87
    { "PushAny                             ", false, false, false, -1, "PushAny", false, true , "ParseNode" }, // 88
    { "SpliceLValue                        ", false, false, false, -1, "SpliceLValue", false, true , "ParseNode" }, // 89
    { "ExtractAny                          ", false, false, false, -1, "ExtractAny", false, true , "ParseNode" }, // 90
    { "ExtractList                         ", false, false, false, -1, "ExtractList", false, true , "ParseNode" }, // 91
    { "ExtractString                       ", false, false, false, -1, "ExtractString", false, true , "ParseNode" }, // 92
    { "ExtractBinary                       ", false, false, false, -1, "ExtractBinary", false, true , "ParseNode" }, // 93
    { "RemoveAny                           ", false, false, false, -1, "RemoveAny", false, true , "ParseNode" }, // 94
    { "RemoveList                          ", false, false, false, -1, "RemoveList", false, true , "ParseNode" }, // 95
    { "RemoveHash                          ", false, false, false, -1, "RemoveHash", false, true , "ParseNode" }, // 96
    { "RemoveObject                        ", false, false, false, -1, "RemoveObject", false, true , "ParseNode" }, // 97
    { "RemoveString                        ", false, false, false, -1, "RemoveString", false, true , "ParseNode" }, // 98
    { "RemoveBinary                        ", false, false, false, -1, "RemoveBinary", false, true , "ParseNode" }, // 99
    { "KeysAny                             ", false, false, false, -1, "KeysAny", false, true , "ParseNode" }, // 100
    { "KeysList                            ", false, false, false, -1, "KeysList", false, true , "ParseNode" }, // 101
    { "KeysHash                            ", false, false, false, -1, "KeysHash", false, true , "ParseNode" }, // 102
    { "RegexMatchAny                       ", false, false, false, -1, "RegexMatchAny", false, true , "ParseNode" }, // 103
    { "RegexMatchBool                      ", false, true , false, -1, "RegexMatchBool", false, true , "ParseNode" }, // 104
    { "RegexNMatchBool                     ", false, true , false, -1, "RegexNMatchBool", false, true , "ParseNode" }, // 105
    { "RegexExtractAny                     ", false, false, false, -1, "RegexExtractAny", false, true , "ParseNode" }, // 106
    { "RegexExtractList                    ", false, false, false, -1, "RegexExtractList", false, true , "ParseNode" }, // 107
    { "RegexSubstAny                       ", false, false, false, -1, "RegexSubstAny", false, true , "ParseNode" }, // 108
    { "RegexSubstString                    ", false, true , false, -1, "RegexSubstString", false, true , "ParseNode" }, // 109
    { "InstanceOfBool                      ", false, true , false, -1, "InstanceOfBool", false, true , "ParseNode" }, // 110
    { "TrimAny                             ", false, false, false, -1, "TrimAny", false, true , "ParseNode" }, // 111
    { "TrimString                          ", false, true , false, -1, "TrimString", false, true , "ParseNode" }, // 112
    { "ChompAny                            ", false, false, false, -1, "ChompAny", false, true , "ParseNode" }, // 113
    { "ChompString                         ", false, true , false, -1, "ChompString", false, true , "ParseNode" }, // 114
    { "TransliterateAny                    ", false, true , false, -1, "TransliterateAny", false, true , "ParseNode" }, // 115
    { "TransliterateString                 ", false, true , false, -1, "TransliterateString", false, true , "ParseNode" }, // 116
    { "BackgroundInt                       ", false, true , false, -1, "BackgroundInt", false, true , "ParseNode" }, // 117
    { "ListAssignAny                       ", false, false, false, -1, "ListAssignAny", false, true , "ParseNode" }, // 118
    { "ExistsAny                           ", false, false, false, -1, "ExistsAny", false, true , "ParseNode" }, // 119
    { "ExistsBool                          ", false, true , false, -1, "ExistsBool", false, true , "ParseNode" }, // 120
    { "ElementsAny                         ", false, false, false, -1, "ElementsAny", false, true , "ParseNode" }, // 121
    { "ElementsInt                         ", false, true , false, -1, "ElementsInt", false, true , "ParseNode" }, // 122
    { "DotEvalAny                          ", false, false, false, -1, "DotEvalAny", false, true , "ParseNode" }, // 123
    { "DotEvalInt                          ", false, false, false, -1, "DotEvalInt", false, true , "ParseNode" }, // 124
    { "DotEvalFloat                        ", false, false, false, -1, "DotEvalFloat", false, true , "ParseNode" }, // 125
    { "DotEvalString                       ", false, false, false, -1, "DotEvalString", false, true , "ParseNode" }, // 126
    { "DotEvalDate                         ", false, false, false, -1, "DotEvalDate", false, true , "ParseNode" }, // 127
    { "DotEvalList                         ", false, false, false, -1, "DotEvalList", false, true , "ParseNode" }, // 128
    { "DotEvalHash                         ", false, false, false, -1, "DotEvalHash", false, true , "ParseNode" }, // 129
    { "DotEvalObject                       ", false, false, false, -1, "DotEvalObject", false, true , "ParseNode" }, // 130
    { "MapSelectList                       ", false, false, false, -1, "MapSelectList", false, true , "ParseNode" }, // 131
    { "HashMap                             ", false, false, false, -1, "HashMap", false, true , "ParseNode" }, // 132
    { "HashMapSelect                       ", false, false, false, -1, "HashMapSelect", false, true , "ParseNode" }, // 133
    { nullptr, false, false, false, -1, nullptr, false, false, nullptr }, // ID 134 REMOVED
    { "IteratorCreate                      ", false, false, false, -1, "IteratorCreate", false, true , "ParseNode" }, // 135
    { "IteratorNext                        ", false, false, true , -1, "IteratorNext", false, true , "ParseNode" }, // 136
    { "OnBlockExit                         ", false, false, false, -1, "OnBlockExit", false, true , "ParseNode" }, // 137
    { "ScopeEnter                          ", false, false, false, -1, "ScopeEnter", false, true , "ParseNode" }, // 138
    { "ScopeExit                           ", false, false, false, -1, "ScopeExit", false, true , "ParseNode" }, // 139
    { "ThreadExit                          ", false, false, true , -1, "ThreadExit", false, true , "ParseNode" }, // 140
    { nullptr, false, false, false, -1, nullptr, false, false, nullptr }, // ID 141 REMOVED
    { nullptr, false, false, false, -1, nullptr, false, false, nullptr }, // ID 142 REMOVED
    { "Context                             ", false, false, false, -1, "Context", false, true , "ParseNode" }, // 143
    { "Summarize                           ", false, false, false, -1, "Summarize", false, true , "ParseNode" }, // 144
    { "EqInt                               ", false, true , false, -1, "EqInt", false, true , "ParseNode" }, // 145
    { "EqFloat                             ", false, true , false, -1, "EqFloat", false, true , "ParseNode" }, // 146
    { "EqString                            ", false, false, false, -1, "EqString", false, true , "ParseNode" }, // 147
    { "EqAny                               ", false, true , false, -1, "EqAny", false, true , "ParseNode" }, // 148
    { "NeInt                               ", false, true , false, -1, "NeInt", false, true , "ParseNode" }, // 149
    { "NeFloat                             ", false, true , false, -1, "NeFloat", false, true , "ParseNode" }, // 150
    { "NeString                            ", false, false, false, -1, "NeString", false, true , "ParseNode" }, // 151
    { "NeAny                               ", false, true , false, -1, "NeAny", false, true , "ParseNode" }, // 152
    { "EqHard                              ", false, true , false, -1, "EqHard", false, true , "ParseNode" }, // 153
    { "NeHard                              ", false, true , false, -1, "NeHard", false, true , "ParseNode" }, // 154
    { "LtInt                               ", false, true , false, -1, "LtInt", false, true , "ParseNode" }, // 155
    { "LtFloat                             ", false, true , false, -1, "LtFloat", false, true , "ParseNode" }, // 156
    { "LtString                            ", false, false, false, -1, "LtString", false, true , "ParseNode" }, // 157
    { "LtAny                               ", false, true , false, -1, "LtAny", false, true , "ParseNode" }, // 158
    { "LeInt                               ", false, true , false, -1, "LeInt", false, true , "ParseNode" }, // 159
    { "LeFloat                             ", false, true , false, -1, "LeFloat", false, true , "ParseNode" }, // 160
    { "LeString                            ", false, false, false, -1, "LeString", false, true , "ParseNode" }, // 161
    { "LeAny                               ", false, true , false, -1, "LeAny", false, true , "ParseNode" }, // 162
    { "GtInt                               ", false, true , false, -1, "GtInt", false, true , "ParseNode" }, // 163
    { "GtFloat                             ", false, true , false, -1, "GtFloat", false, true , "ParseNode" }, // 164
    { "GtString                            ", false, false, false, -1, "GtString", false, true , "ParseNode" }, // 165
    { "GtAny                               ", false, true , false, -1, "GtAny", false, true , "ParseNode" }, // 166
    { "GeInt                               ", false, true , false, -1, "GeInt", false, true , "ParseNode" }, // 167
    { "GeFloat                             ", false, true , false, -1, "GeFloat", false, true , "ParseNode" }, // 168
    { "GeString                            ", false, false, false, -1, "GeString", false, true , "ParseNode" }, // 169
    { "GeAny                               ", false, true , false, -1, "GeAny", false, true , "ParseNode" }, // 170
    { "CmpInt                              ", false, true , false, -1, "CmpInt", false, true , "ParseNode" }, // 171
    { "CmpFloat                            ", false, true , false, -1, "CmpFloat", false, true , "ParseNode" }, // 172
    { "CmpString                           ", false, false, false, -1, "CmpString", false, true , "ParseNode" }, // 173
    { "CmpAny                              ", false, true , false, -1, "CmpAny", false, true , "ParseNode" }, // 174
    { "ToBool                              ", false, true , false, -1, "ToBool", false, true , "ParseNode" }, // 175
    { "Not                                 ", false, true , false, -1, "Not", false, true , "ParseNode" }, // 176
    { "IsNullOrNothing                     ", false, true , false, -1, "IsNullOrNothing", false, true , "ParseNode" }, // 177
    { "Phi                                 ", false, false, false, -1, "Phi", false, true , "ParseNode" }, // 178
    { "UnaryPlusAny                        ", false, false, false, -1, "UnaryPlusAny", false, true , "ParseNode" }, // 179
    { "UnaryMinusInt                       ", false, true , false, -1, "UnaryMinusInt", false, true , "ParseNode" }, // 180
    { "UnaryMinusFloat                     ", false, true , false, -1, "UnaryMinusFloat", false, true , "ParseNode" }, // 181
    { "UnaryMinusAny                       ", false, false, false, -1, "UnaryMinusAny", false, true , "ParseNode" }, // 182
    { "FoldlAny                            ", false, false, false, -1, "FoldlAny", false, true , "ParseNode" }, // 183
    { "FoldlInt                            ", false, false, false, -1, "FoldlInt", false, true , "ParseNode" }, // 184
    { "FoldlFloat                          ", false, false, false, -1, "FoldlFloat", false, true , "ParseNode" }, // 185
    { "FoldrAny                            ", false, false, false, -1, "FoldrAny", false, true , "ParseNode" }, // 186
    { "FoldrInt                            ", false, false, false, -1, "FoldrInt", false, true , "ParseNode" }, // 187
    { "FoldrFloat                          ", false, false, false, -1, "FoldrFloat", false, true , "ParseNode" }, // 188
    { "FoldlSumInt                         ", false, false, false, -1, "FoldlSumInt", false, true , "ParseNode" }, // 189
    { "FoldlSumFloat                       ", false, false, false, -1, "FoldlSumFloat", false, true , "ParseNode" }, // 190
    { "FoldlProdInt                        ", false, false, false, -1, "FoldlProdInt", false, true , "ParseNode" }, // 191
    { "FoldlProdFloat                      ", false, false, false, -1, "FoldlProdFloat", false, true , "ParseNode" }, // 192
    { "FoldlDiffInt                        ", false, false, false, -1, "FoldlDiffInt", false, true , "ParseNode" }, // 193
    { "FoldlDiffFloat                      ", false, false, false, -1, "FoldlDiffFloat", false, true , "ParseNode" }, // 194
    { "FoldlMinInt                         ", false, false, false, -1, "FoldlMinInt", false, true , "ParseNode" }, // 195
    { "FoldlMinFloat                       ", false, false, false, -1, "FoldlMinFloat", false, true , "ParseNode" }, // 196
    { "FoldlMaxInt                         ", false, false, false, -1, "FoldlMaxInt", false, true , "ParseNode" }, // 197
    { "FoldlMaxFloat                       ", false, false, false, -1, "FoldlMaxFloat", false, true , "ParseNode" }, // 198
    { "FoldrSumInt                         ", false, false, false, -1, "FoldrSumInt", false, true , "ParseNode" }, // 199
    { "FoldrSumFloat                       ", false, false, false, -1, "FoldrSumFloat", false, true , "ParseNode" }, // 200
    { "FoldrProdInt                        ", false, false, false, -1, "FoldrProdInt", false, true , "ParseNode" }, // 201
    { "FoldrProdFloat                      ", false, false, false, -1, "FoldrProdFloat", false, true , "ParseNode" }, // 202
    { "FoldrDiffInt                        ", false, false, false, -1, "FoldrDiffInt", false, true , "ParseNode" }, // 203
    { "FoldrDiffFloat                      ", false, false, false, -1, "FoldrDiffFloat", false, true , "ParseNode" }, // 204
    { "FoldrMinInt                         ", false, false, false, -1, "FoldrMinInt", false, true , "ParseNode" }, // 205
    { "FoldrMinFloat                       ", false, false, false, -1, "FoldrMinFloat", false, true , "ParseNode" }, // 206
    { "FoldrMaxInt                         ", false, false, false, -1, "FoldrMaxInt", false, true , "ParseNode" }, // 207
    { "FoldrMaxFloat                       ", false, false, false, -1, "FoldrMaxFloat", false, true , "ParseNode" }, // 208
    { "MapAny                              ", false, false, false, -1, "MapAny", false, true , "ParseNode" }, // 209
    { "MapInt                              ", false, false, false, -1, "MapInt", false, true , "ParseNode" }, // 210
    { "MapFloat                            ", false, false, false, -1, "MapFloat", false, true , "ParseNode" }, // 211
    { "MapScaleInt                         ", false, false, false, -1, "MapScaleInt", false, true , "ParseNode" }, // 212
    { "MapScaleFloat                       ", false, false, false, -1, "MapScaleFloat", false, true , "ParseNode" }, // 213
    { "MapOffsetInt                        ", false, false, false, -1, "MapOffsetInt", false, true , "ParseNode" }, // 214
    { "MapOffsetFloat                      ", false, false, false, -1, "MapOffsetFloat", false, true , "ParseNode" }, // 215
    { "MapSquareInt                        ", false, false, false, -1, "MapSquareInt", false, true , "ParseNode" }, // 216
    { "MapSquareFloat                      ", false, false, false, -1, "MapSquareFloat", false, true , "ParseNode" }, // 217
    { "MapHashKeyValue                     ", false, false, false, -1, "MapHashKeyValue", false, true , "ParseNode" }, // 218
    { "MapHashKeyInt                       ", false, false, false, -1, "MapHashKeyInt", false, true , "ParseNode" }, // 219
    { "MapHashKeyOffsetInt                 ", false, false, false, -1, "MapHashKeyOffsetInt", false, true , "ParseNode" }, // 220
    { "MapHashKeyScaleInt                  ", false, false, false, -1, "MapHashKeyScaleInt", false, true , "ParseNode" }, // 221
    { "HashMapTwoKeys                      ", false, false, false, -1, "HashMapTwoKeys", false, true , "ParseNode" }, // 222
    { "SelectAny                           ", false, false, false, -1, "SelectAny", false, true , "ParseNode" }, // 223
    { "SelectInt                           ", false, false, false, -1, "SelectInt", false, true , "ParseNode" }, // 224
    { "SelectFloat                         ", false, false, false, -1, "SelectFloat", false, true , "ParseNode" }, // 225
    { "SelectPositiveInt                   ", false, false, false, -1, "SelectPositiveInt", false, true , "ParseNode" }, // 226
    { "SelectPositiveFloat                 ", false, false, false, -1, "SelectPositiveFloat", false, true , "ParseNode" }, // 227
    { "SelectNonZeroInt                    ", false, false, false, -1, "SelectNonZeroInt", false, true , "ParseNode" }, // 228
    { "SelectNonZeroFloat                  ", false, false, false, -1, "SelectNonZeroFloat", false, true , "ParseNode" }, // 229
    { "FusedMapSelectScalePositiveInt      ", false, false, false, -1, "FusedMapSelectScalePositiveInt", false, true , "ParseNode" }, // 230
    { "FusedMapSelectScalePositiveFloat    ", false, false, false, -1, "FusedMapSelectScalePositiveFloat", false, true , "ParseNode" }, // 231
    { "FusedMapSelectOffsetPositiveInt     ", false, false, false, -1, "FusedMapSelectOffsetPositiveInt", false, true , "ParseNode" }, // 232
    { "FusedMapSelectOffsetPositiveFloat   ", false, false, false, -1, "FusedMapSelectOffsetPositiveFloat", false, true , "ParseNode" }, // 233
    { "FusedMapSelectSquarePositiveInt     ", false, false, false, -1, "FusedMapSelectSquarePositiveInt", false, true , "ParseNode" }, // 234
    { "FusedMapSelectSquarePositiveFloat   ", false, false, false, -1, "FusedMapSelectSquarePositiveFloat", false, true , "ParseNode" }, // 235
    { "FusedMapFoldlSumScaleInt            ", false, false, false, -1, "FusedMapFoldlSumScaleInt", false, true , "ParseNode" }, // 236
    { "FusedMapFoldlSumScaleFloat          ", false, false, false, -1, "FusedMapFoldlSumScaleFloat", false, true , "ParseNode" }, // 237
    { "FusedMapFoldlSumSquareInt           ", false, false, false, -1, "FusedMapFoldlSumSquareInt", false, true , "ParseNode" }, // 238
    { "FusedMapFoldlSumSquareFloat         ", false, false, false, -1, "FusedMapFoldlSumSquareFloat", false, true , "ParseNode" }, // 239
    { "FusedMapFoldlProdScaleInt           ", false, false, false, -1, "FusedMapFoldlProdScaleInt", false, true , "ParseNode" }, // 240
    { "FusedMapFoldlProdScaleFloat         ", false, false, false, -1, "FusedMapFoldlProdScaleFloat", false, true , "ParseNode" }, // 241
    { "MapSelectAny                        ", false, false, false, -1, "MapSelectAny", false, true , "ParseNode" }, // 242
    { "HashMapAny                          ", false, false, false, -1, "HashMapAny", false, true , "ParseNode" }, // 243
    { "HashMapSelectAny                    ", false, false, false, -1, "HashMapSelectAny", false, true , "ParseNode" }, // 244
    { "RangeAny                            ", false, false, false, -1, "RangeAny", false, true , "ParseNode" }, // 245
    { "RangeInt                            ", false, false, false, -1, "RangeInt", false, true , "ParseNode" }, // 246
    { "RangeFloat                          ", false, false, false, -1, "RangeFloat", false, true , "ParseNode" }, // 247
    { "RangeDate                           ", false, false, false, -1, "RangeDate", false, true , "ParseNode" }, // 248
    { "RangeSliceAny                       ", false, false, false, -1, "RangeSliceAny", false, true , "ParseNode" }, // 249
    { "RangeSliceInt                       ", false, false, false, -1, "RangeSliceInt", false, true , "ParseNode" }, // 250
    { "RangeSliceFloat                     ", false, false, false, -1, "RangeSliceFloat", false, true , "ParseNode" }, // 251
    { "CastAny                             ", false, false, false, -1, "CastAny", false, true , "ParseNode" }, // 252
    { "CastList                            ", false, false, false, -1, "CastList", false, true , "ParseNode" }, // 253
    { "CastHash                            ", false, false, false, -1, "CastHash", false, true , "ParseNode" }, // 254
    { "CastObject                          ", false, false, false, -1, "CastObject", false, true , "ParseNode" }, // 255
    { "CastEnum                            ", false, false, false, -1, "CastEnum", false, true , "ParseNode" }, // 256
    { "CastComplexHash                     ", false, false, false, -1, "CastComplexHash", false, true , "ParseNode" }, // 257
    { "Br                                  ", false, false, true , -1, "Control flow", false, false, "ControlFlowNode" }, // 258
    { "BrIf                                ", false, false, true , -1, "Control flow", false, false, "ControlFlowNode" }, // 259
    { "SwitchInt                           ", false, false, true , -1, "Control flow", false, false, "ControlFlowNode" }, // 260
    { "SwitchString                        ", false, false, true , -1, "Control flow", false, false, "ControlFlowNode" }, // 261
    { "Return                              ", false, false, true , -1, "Control transfer", false, true , "ControlFlowNode" }, // 262
    { "ReturnNothing                       ", false, false, true , -1, "Control transfer", false, true , "ControlFlowNode" }, // 263
    { "LoadLocal                           ", true , false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 264
    { "StoreLocal                          ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 265
    { "UninstantiateLocal                  ", false, false, false, -1, "UninstantiateLocal", false, true , "ParseNode" }, // 266
    { "LoadArg                             ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 267
    { "LoadClosure                         ", true , false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 268
    { "StoreClosure                        ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 269
    { "LoadGlobal                          ", true , false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 270
    { "StoreGlobal                         ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 271
    { "LoadThreadLocal                     ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 272
    { "StoreThreadLocal                    ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 273
    { "LoadImplicitArg                     ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 274
    { "LoadImplicitArgv                    ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 275
    { "LoadImplicitElement                 ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 276
    { "PushImplicitArg                     ", false, false, false, -1, "PushImplicitArg", false, true , "ParseNode" }, // 277
    { "SetImplicitArgv                     ", false, false, false, -1, "SetImplicitArgv", false, true , "ParseNode" }, // 278
    { "PopImplicitArg                      ", false, false, false, -1, "PopImplicitArg", false, true , "ParseNode" }, // 279
    { "PushImplicitElement                 ", false, false, false, -1, "PushImplicitElement", false, true , "ParseNode" }, // 280
    { "PopImplicitElement                  ", false, false, false, -1, "PopImplicitElement", false, true , "ParseNode" }, // 281
    { "HashKeyAccess                       ", true , false, false, -1, "HashKeyAccess", false, true , "ParseNode" }, // 282
    { "HashKeyAccessInt                    ", true , false, false, -1, "HashKeyAccessInt", false, true , "ParseNode" }, // 283
    { "LoadSelfMember                      ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 284
    { "LoadStaticVar                       ", false, false, false, -1, "Load variable value", false, false, "VarRefNode" }, // 285
    { "NewObject                           ", false, false, false, -1, "NewObject", false, true , "ParseNode" }, // 286
    { "LoadConstant                        ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 287
    { "CreateClosure                       ", false, false, false, -1, "CreateClosure", false, true , "ParseNode" }, // 288
    { "CreateCallRef                       ", false, false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 289
    { "CreateMethodRef                     ", false, false, false, -1, "CreateMethodRef", false, true , "ParseNode" }, // 290
    { "CreateParseRef                      ", false, false, false, -1, "CreateParseRef", false, true , "ParseNode" }, // 291
    { "NewHashDecl                         ", false, false, false, -1, "NewHashDecl", false, true , "ParseNode" }, // 292
    { "NewComplexHash                      ", false, false, false, -1, "NewComplexHash", false, true , "ParseNode" }, // 293
    { "NewComplexList                      ", false, false, false, -1, "NewComplexList", false, true , "ParseNode" }, // 294
    { "VrnConstruct                        ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 295
    { "HashSetKeyValue                     ", false, false, false, -1, "HashSetKeyValue", false, true , "ParseNode" }, // 296
    { "IteratorCreateReverse               ", false, false, false, -1, "IteratorCreateReverse", false, true , "ParseNode" }, // 297
    { "Call                                ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 298
    { "CallDirect                          ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 299
    { "CallIndirect                        ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 300
    { "CallMethod                          ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 301
    { "CallMethodDirect                    ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 302
    { "InvokeMethodDirect                  ", true , false, true , -1, "Call function or method", true , true , "FunctionCallNode" }, // 303
    { "CallStatic                          ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 304
    { "CallStaticDirect                    ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 305
    { "DotEvalMethodDirect                 ", true , false, false, -1, "DotEvalMethodDirect", false, true , "ParseNode" }, // 306
    { "InvokeDotEvalMethodDirect           ", true , false, true , -1, "Call function or method", true , true , "FunctionCallNode" }, // 307
    { "CallClosureDirect                   ", true , false, false, -1, "Call function or method", true , true , "FunctionCallNode" }, // 308
    { "Invoke                              ", true , false, true , -1, "Call function or method", true , true , "FunctionCallNode" }, // 309
    { "GuardInt                            ", false, false, false, -1, "GuardInt", false, true , "ParseNode" }, // 310
    { "GetObjectClass                      ", false, false, false, -1, "GetObjectClass", false, true , "ParseNode" }, // 311
    { "GuardFloat                          ", false, false, false, -1, "GuardFloat", false, true , "ParseNode" }, // 312
    { "GuardType                           ", false, false, false, -1, "GuardType", false, true , "ParseNode" }, // 313
    { "GuardNotNothing                     ", false, false, false, -1, "GuardNotNothing", false, true , "ParseNode" }, // 314
    { "LandingPad                          ", false, false, false, -1, "LandingPad", false, true , "ParseNode" }, // 315
    { "CatchException                      ", true , false, false, -1, "CatchException", false, true , "ParseNode" }, // 316
    { "CatchCleanup                        ", false, false, false, -1, "CatchCleanup", false, true , "ParseNode" }, // 317
    { "Rethrow                             ", false, false, true , -1, "Rethrow", false, true , "ParseNode" }, // 318
    { "Throw                               ", false, false, true , -1, "Control transfer", false, true , "ControlFlowNode" }, // 319
    { "InvokeSimError                      ", false, false, true , -1, "Call function or method", true , true , "FunctionCallNode" }, // 320
    { "Incref                              ", false, false, false, -1, "Incref", false, true , "ParseNode" }, // 321
    { "Decref                              ", false, false, false, -1, "Decref", false, true , "ParseNode" }, // 322
    { "DecrefNoThrow                       ", false, false, false, -1, "Control transfer", false, true , "ControlFlowNode" }, // 323
    { "SwitchRegexMatch                    ", false, false, false, -1, "Control flow", false, false, "ControlFlowNode" }, // 324
    { "ListPush                            ", false, false, false, -1, "ListPush", false, true , "ParseNode" }, // 325
    { "RefForeachInit                      ", false, false, false, -1, "RefForeachInit", false, true , "ParseNode" }, // 326
    { "RefForeachSize                      ", false, false, false, -1, "RefForeachSize", false, true , "ParseNode" }, // 327
    { "RefForeachGetEntry                  ", false, false, false, -1, "RefForeachGetEntry", false, true , "ParseNode" }, // 328
    { "RefForeachRecord                    ", false, false, false, -1, "RefForeachRecord", false, true , "ParseNode" }, // 329
    { "RefForeachFinalize                  ", false, false, false, -1, "RefForeachFinalize", false, true , "ParseNode" }, // 330
    { "RefForeachCleanup                   ", false, false, false, -1, "RefForeachCleanup", false, true , "ParseNode" }, // 331
    { "AddNumber                           ", false, false, false, -1, "AddNumber", false, true , "ParseNode" }, // 332
    { "SubNumber                           ", false, false, false, -1, "SubNumber", false, true , "ParseNode" }, // 333
    { "MulNumber                           ", false, false, false, -1, "MulNumber", false, true , "ParseNode" }, // 334
    { "DivNumber                           ", false, false, false, -1, "DivNumber", false, true , "ParseNode" }, // 335
    { "HashKeyStore                        ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 336
    { "ListIndexAccess                     ", true , false, false, -1, "ListIndexAccess", false, true , "ParseNode" }, // 337
    { "ListIndexStore                      ", false, false, false, -1, "Store to variable", true , true , "AssignmentNode" }, // 338
    { "AddAssignLocalInt                   ", false, false, false, -1, "AddAssignLocalInt", false, true , "ParseNode" }, // 339
    { "IncrementLocalInt                   ", false, false, false, -1, "IncrementLocalInt", false, true , "ParseNode" }, // 340
    { "BranchIfLtLocalInt                  ", false, false, true , -1, "Control flow", false, false, "ControlFlowNode" }, // 341
    { "ConstEnum                           ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 342
    { "ListGetValueNoRef                   ", false, false, false, -1, "ListGetValueNoRef", false, true , "ParseNode" }, // 343
    { "SwitchCaseMatch                     ", false, false, false, -1, "Control flow", false, false, "ControlFlowNode" }, // 344
    { "IsCollectionType                    ", false, false, false, -1, "IsCollectionType", false, true , "ParseNode" }, // 345
    { "MakeHashConstKeys                   ", false, false, false, -1, "Load constant value", false, false, "ConstantNode" }, // 346
    { "ToString                            ", false, false, false, -1, "ToString", false, true , "ParseNode" }, // 347
    { "Sprintf                             ", false, false, false, -1, "Sprintf", false, true , "ParseNode" }, // 348
    { "NewHashDeclFromHash                 ", false, false, false, -1, "NewHashDeclFromHash", false, true , "ParseNode" }, // 349
    { "InstantiateLocal                    ", false, false, false, -1, "InstantiateLocal", false, true , "ParseNode" }, // 350
    { "AddTimeout                          ", false, false, false, -1, "AddTimeout", false, true , "ParseNode" }, // 351
    { "SubTimeout                          ", false, false, false, -1, "SubTimeout", false, true , "ParseNode" }, // 352
    { "HashDerefDynamic                    ", false, false, false, -1, "HashDerefDynamic", false, true , "ParseNode" }, // 353
    { "ListIndexDynamic                    ", false, false, false, -1, "ListIndexDynamic", false, true , "ParseNode" }, // 354
};

//! Static assertion to verify registry completeness
static_assert(
    sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == 355,
    "OPCODE_REGISTRY has incorrect entry count - should be exactly 355"
);

//! ============================================================================
//! PHASE 2: Lookup Functions - Query registry by opcode ID
//! ============================================================================

//! Get opcode info by opcode enum value
//! Returns pointer to registry entry, or nullptr for invalid opcode
inline const OpcodeInfo* getOpcodeInfo(int opcode_id) {
    if (opcode_id >= 0 && opcode_id < 355) {
        const OpcodeInfo* info = &OPCODE_REGISTRY[opcode_id];
        // Check for null entry (removed opcode IDs: 134, 141, 142)
        if (info->name == nullptr) {
            return nullptr;
        }
        return info;
    }
    return nullptr;
}

//! Check if opcode can legitimately return NOTHING
inline bool getOpcodeCanReturnNothing(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->can_return_nothing : false;  // conservative: assume safe
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

//! Get expected operand count for opcode
inline int getOpcodeExpectedOperands(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->expected_operands : -1;  // -1 = variable/context-dependent
}

//! ============================================================================
//! PHASE 4: Extended Property Query Functions
//! ============================================================================

//! Get human-readable description of what the opcode does
inline const char* getOpcodeDescription(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->description : "<NO DESCRIPTION>";
}

//! Check if opcode can modify global/static state or object members
inline bool getOpcodeMayHaveSideEffects(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->may_have_side_effects : true;  // conservative: assume has side effects
}

//! Check if opcode can throw an exception at runtime
inline bool getOpcodeMayThrowException(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->may_throw_exception : true;  // conservative: assume may throw
}

//! Get the AST node type that this IR opcode comes from
inline const char* getOpcodeCorrespondingASTNode(int opcode_id) {
    const OpcodeInfo* info = getOpcodeInfo(opcode_id);
    return info ? info->corresponding_ast_node : "ParseNode";  // default to generic
}

#endif
