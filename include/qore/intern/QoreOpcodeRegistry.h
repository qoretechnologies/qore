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
struct OpcodeInfo {
    const char* name;
    bool can_return_nothing;
    bool never_returns_nothing;
    bool is_terminator;
    int expected_operands;
};

//! Registry of all 346 IR opcodes (in enum ID order, accounting for removed IDs 134, 141, 142)
//! PHASE 1 IMPLEMENTATION: Complete registry with verified properties from existing code
constexpr OpcodeInfo OPCODE_REGISTRY[349] = {
    { "ConstInt                            ", false, false, false, -1 }, // 0
    { "ConstFloat                          ", false, false, false, -1 }, // 1
    { "ConstBool                           ", false, false, false, -1 }, // 2
    { "ConstNothing                        ", false, false, false, -1 }, // 3
    { "ConstNull                           ", false, false, false, -1 }, // 4
    { "ConstString                         ", false, false, false, -1 }, // 5
    { "ConstDate                           ", false, false, false, -1 }, // 6
    { "MakeList                            ", false, false, false, -1 }, // 7
    { "MakeHash                            ", false, false, false, -1 }, // 8
    { "CreateEmptyList                     ", false, false, false, -1 }, // 9
    { "CreateSizedList                     ", false, false, false, -1 }, // 10
    { "ListAppend                          ", false, false, false, -1 }, // 11
    { "ListSize                            ", false, false, false, -1 }, // 12
    { "ListGetInt                          ", false, false, false, -1 }, // 13
    { "ListGetFloat                        ", false, false, false, -1 }, // 14
    { "ListGetValue                        ", false, false, false, -1 }, // 15
    { "ListSetInt                          ", false, false, false, -1 }, // 16
    { "ListSetFloat                        ", false, false, false, -1 }, // 17
    { "ListSetValue                        ", false, false, false, -1 }, // 18
    { "AddInt                              ", false, true , false, -1 }, // 19
    { "AddFloat                            ", false, true , false, -1 }, // 20
    { "AddAny                              ", false, false, false, -1 }, // 21
    { "AddString                           ", false, false, false, -1 }, // 22
    { "StringConcat                        ", false, false, false, -1 }, // 23
    { "SubInt                              ", false, true , false, -1 }, // 24
    { "SubFloat                            ", false, true , false, -1 }, // 25
    { "SubAny                              ", false, false, false, -1 }, // 26
    { "MulInt                              ", false, true , false, -1 }, // 27
    { "MulFloat                            ", false, true , false, -1 }, // 28
    { "MulAny                              ", false, false, false, -1 }, // 29
    { "DivInt                              ", false, true , false, -1 }, // 30
    { "DivFloat                            ", false, true , false, -1 }, // 31
    { "DivAny                              ", false, false, false, -1 }, // 32
    { "ModInt                              ", false, true , false, -1 }, // 33
    { "ModAny                              ", false, false, false, -1 }, // 34
    { "AndInt                              ", false, true , false, -1 }, // 35
    { "AndAny                              ", false, false, false, -1 }, // 36
    { "OrInt                               ", false, true , false, -1 }, // 37
    { "OrAny                               ", false, false, false, -1 }, // 38
    { "XorInt                              ", false, true , false, -1 }, // 39
    { "XorAny                              ", false, false, false, -1 }, // 40
    { "ShlInt                              ", false, true , false, -1 }, // 41
    { "ShlAny                              ", false, false, false, -1 }, // 42
    { "ShrInt                              ", false, true , false, -1 }, // 43
    { "ShrAny                              ", false, false, false, -1 }, // 44
    { "ShlAssignInt                        ", false, true , false, -1 }, // 45
    { "ShlAssignAny                        ", false, false, false, -1 }, // 46
    { "ShrAssignInt                        ", false, true , false, -1 }, // 47
    { "ShrAssignAny                        ", false, false, false, -1 }, // 48
    { "AddAssignInt                        ", false, true , false, -1 }, // 49
    { "AddAssignFloat                      ", false, true , false, -1 }, // 50
    { "AddAssignAny                        ", false, false, false, -1 }, // 51
    { "SubAssignInt                        ", false, true , false, -1 }, // 52
    { "SubAssignFloat                      ", false, true , false, -1 }, // 53
    { "SubAssignAny                        ", false, false, false, -1 }, // 54
    { "MulAssignInt                        ", false, true , false, -1 }, // 55
    { "MulAssignFloat                      ", false, true , false, -1 }, // 56
    { "MulAssignAny                        ", false, false, false, -1 }, // 57
    { "DivAssignInt                        ", false, true , false, -1 }, // 58
    { "DivAssignFloat                      ", false, true , false, -1 }, // 59
    { "DivAssignAny                        ", false, false, false, -1 }, // 60
    { "ModAssignInt                        ", false, true , false, -1 }, // 61
    { "ModAssignAny                        ", false, false, false, -1 }, // 62
    { "AndAssignInt                        ", false, true , false, -1 }, // 63
    { "AndAssignAny                        ", false, false, false, -1 }, // 64
    { "OrAssignInt                         ", false, true , false, -1 }, // 65
    { "OrAssignAny                         ", false, false, false, -1 }, // 66
    { "XorAssignInt                        ", false, true , false, -1 }, // 67
    { "XorAssignAny                        ", false, false, false, -1 }, // 68
    { "LoadLValue                          ", false, false, false, -1 }, // 69
    { "StoreLValue                         ", false, false, false, -1 }, // 70
    { "PreIncLValue                        ", false, false, false, -1 }, // 71
    { "PreDecLValue                        ", false, false, false, -1 }, // 72
    { "PostIncLValue                       ", false, false, false, -1 }, // 73
    { "PostDecLValue                       ", false, false, false, -1 }, // 74
    { "AddAssignLValue                     ", false, false, false, -1 }, // 75
    { "SubAssignLValue                     ", false, false, false, -1 }, // 76
    { "MulAssignLValue                     ", false, false, false, -1 }, // 77
    { "DivAssignLValue                     ", false, false, false, -1 }, // 78
    { "ModAssignLValue                     ", false, false, false, -1 }, // 79
    { "AndAssignLValue                     ", false, false, false, -1 }, // 80
    { "OrAssignLValue                      ", false, false, false, -1 }, // 81
    { "XorAssignLValue                     ", false, false, false, -1 }, // 82
    { "ShlAssignLValue                     ", false, false, false, -1 }, // 83
    { "ShrAssignLValue                     ", false, false, false, -1 }, // 84
    { "ShiftLValue                         ", false, false, false, -1 }, // 85
    { "UnshiftLValue                       ", false, false, false, -1 }, // 86
    { "PopAny                              ", false, false, false, -1 }, // 87
    { "PushAny                             ", false, false, false, -1 }, // 88
    { "SpliceLValue                        ", false, false, false, -1 }, // 89
    { "ExtractAny                          ", false, false, false, -1 }, // 90
    { "ExtractList                         ", false, false, false, -1 }, // 91
    { "ExtractString                       ", false, false, false, -1 }, // 92
    { "ExtractBinary                       ", false, false, false, -1 }, // 93
    { "RemoveAny                           ", false, false, false, -1 }, // 94
    { "RemoveList                          ", false, false, false, -1 }, // 95
    { "RemoveHash                          ", false, false, false, -1 }, // 96
    { "RemoveObject                        ", false, false, false, -1 }, // 97
    { "RemoveString                        ", false, false, false, -1 }, // 98
    { "RemoveBinary                        ", false, false, false, -1 }, // 99
    { "KeysAny                             ", false, false, false, -1 }, // 100
    { "KeysList                            ", false, false, false, -1 }, // 101
    { "KeysHash                            ", false, false, false, -1 }, // 102
    { "RegexMatchAny                       ", false, false, false, -1 }, // 103
    { "RegexMatchBool                      ", false, true , false, -1 }, // 104
    { "RegexNMatchBool                     ", false, true , false, -1 }, // 105
    { "RegexExtractAny                     ", false, false, false, -1 }, // 106
    { "RegexExtractList                    ", false, false, false, -1 }, // 107
    { "RegexSubstAny                       ", false, false, false, -1 }, // 108
    { "RegexSubstString                    ", false, true , false, -1 }, // 109
    { "InstanceOfBool                      ", false, true , false, -1 }, // 110
    { "TrimAny                             ", false, false, false, -1 }, // 111
    { "TrimString                          ", false, true , false, -1 }, // 112
    { "ChompAny                            ", false, false, false, -1 }, // 113
    { "ChompString                         ", false, true , false, -1 }, // 114
    { "TransliterateAny                    ", false, true , false, -1 }, // 115
    { "TransliterateString                 ", false, true , false, -1 }, // 116
    { "BackgroundInt                       ", false, true , false, -1 }, // 117
    { "ListAssignAny                       ", false, false, false, -1 }, // 118
    { "ExistsAny                           ", false, false, false, -1 }, // 119
    { "ExistsBool                          ", false, true , false, -1 }, // 120
    { "ElementsAny                         ", false, false, false, -1 }, // 121
    { "ElementsInt                         ", false, true , false, -1 }, // 122
    { "DotEvalAny                          ", false, false, false, -1 }, // 123
    { "DotEvalInt                          ", false, false, false, -1 }, // 124
    { "DotEvalFloat                        ", false, false, false, -1 }, // 125
    { "DotEvalString                       ", false, false, false, -1 }, // 126
    { "DotEvalDate                         ", false, false, false, -1 }, // 127
    { "DotEvalList                         ", false, false, false, -1 }, // 128
    { "DotEvalHash                         ", false, false, false, -1 }, // 129
    { "DotEvalObject                       ", false, false, false, -1 }, // 130
    { "MapSelectList                       ", false, false, false, -1 }, // 131
    { "HashMap                             ", false, false, false, -1 }, // 132
    { "HashMapSelect                       ", false, false, false, -1 }, // 133
    { nullptr, false, false, false, -1 }, // ID 134 REMOVED
    { "IteratorCreate                      ", false, false, false, -1 }, // 135
    { "IteratorNext                        ", false, false, true , -1 }, // 136
    { "OnBlockExit                         ", false, false, false, -1 }, // 137
    { "ScopeEnter                          ", false, false, false, -1 }, // 138
    { "ScopeExit                           ", false, false, false, -1 }, // 139
    { "ThreadExit                          ", false, false, true , -1 }, // 140
    { nullptr, false, false, false, -1 }, // ID 141 REMOVED
    { nullptr, false, false, false, -1 }, // ID 142 REMOVED
    { "Context                             ", false, false, false, -1 }, // 143
    { "Summarize                           ", false, false, false, -1 }, // 144
    { "EqInt                               ", false, true , false, -1 }, // 145
    { "EqFloat                             ", false, true , false, -1 }, // 146
    { "EqString                            ", false, false, false, -1 }, // 147
    { "EqAny                               ", false, true , false, -1 }, // 148
    { "NeInt                               ", false, true , false, -1 }, // 149
    { "NeFloat                             ", false, true , false, -1 }, // 150
    { "NeString                            ", false, false, false, -1 }, // 151
    { "NeAny                               ", false, true , false, -1 }, // 152
    { "EqHard                              ", false, true , false, -1 }, // 153
    { "NeHard                              ", false, true , false, -1 }, // 154
    { "LtInt                               ", false, true , false, -1 }, // 155
    { "LtFloat                             ", false, true , false, -1 }, // 156
    { "LtString                            ", false, false, false, -1 }, // 157
    { "LtAny                               ", false, true , false, -1 }, // 158
    { "LeInt                               ", false, true , false, -1 }, // 159
    { "LeFloat                             ", false, true , false, -1 }, // 160
    { "LeString                            ", false, false, false, -1 }, // 161
    { "LeAny                               ", false, true , false, -1 }, // 162
    { "GtInt                               ", false, true , false, -1 }, // 163
    { "GtFloat                             ", false, true , false, -1 }, // 164
    { "GtString                            ", false, false, false, -1 }, // 165
    { "GtAny                               ", false, true , false, -1 }, // 166
    { "GeInt                               ", false, true , false, -1 }, // 167
    { "GeFloat                             ", false, true , false, -1 }, // 168
    { "GeString                            ", false, false, false, -1 }, // 169
    { "GeAny                               ", false, true , false, -1 }, // 170
    { "CmpInt                              ", false, true , false, -1 }, // 171
    { "CmpFloat                            ", false, true , false, -1 }, // 172
    { "CmpString                           ", false, false, false, -1 }, // 173
    { "CmpAny                              ", false, true , false, -1 }, // 174
    { "ToBool                              ", false, true , false, -1 }, // 175
    { "Not                                 ", false, true , false, -1 }, // 176
    { "IsNullOrNothing                     ", false, true , false, -1 }, // 177
    { "Phi                                 ", false, false, false, -1 }, // 178
    { "UnaryPlusAny                        ", false, false, false, -1 }, // 179
    { "UnaryMinusInt                       ", false, true , false, -1 }, // 180
    { "UnaryMinusFloat                     ", false, true , false, -1 }, // 181
    { "UnaryMinusAny                       ", false, false, false, -1 }, // 182
    { "FoldlAny                            ", false, false, false, -1 }, // 183
    { "FoldlInt                            ", false, false, false, -1 }, // 184
    { "FoldlFloat                          ", false, false, false, -1 }, // 185
    { "FoldrAny                            ", false, false, false, -1 }, // 186
    { "FoldrInt                            ", false, false, false, -1 }, // 187
    { "FoldrFloat                          ", false, false, false, -1 }, // 188
    { "FoldlSumInt                         ", false, false, false, -1 }, // 189
    { "FoldlSumFloat                       ", false, false, false, -1 }, // 190
    { "FoldlProdInt                        ", false, false, false, -1 }, // 191
    { "FoldlProdFloat                      ", false, false, false, -1 }, // 192
    { "FoldlDiffInt                        ", false, false, false, -1 }, // 193
    { "FoldlDiffFloat                      ", false, false, false, -1 }, // 194
    { "FoldlMinInt                         ", false, false, false, -1 }, // 195
    { "FoldlMinFloat                       ", false, false, false, -1 }, // 196
    { "FoldlMaxInt                         ", false, false, false, -1 }, // 197
    { "FoldlMaxFloat                       ", false, false, false, -1 }, // 198
    { "FoldrSumInt                         ", false, false, false, -1 }, // 199
    { "FoldrSumFloat                       ", false, false, false, -1 }, // 200
    { "FoldrProdInt                        ", false, false, false, -1 }, // 201
    { "FoldrProdFloat                      ", false, false, false, -1 }, // 202
    { "FoldrDiffInt                        ", false, false, false, -1 }, // 203
    { "FoldrDiffFloat                      ", false, false, false, -1 }, // 204
    { "FoldrMinInt                         ", false, false, false, -1 }, // 205
    { "FoldrMinFloat                       ", false, false, false, -1 }, // 206
    { "FoldrMaxInt                         ", false, false, false, -1 }, // 207
    { "FoldrMaxFloat                       ", false, false, false, -1 }, // 208
    { "MapAny                              ", false, false, false, -1 }, // 209
    { "MapInt                              ", false, false, false, -1 }, // 210
    { "MapFloat                            ", false, false, false, -1 }, // 211
    { "MapScaleInt                         ", false, false, false, -1 }, // 212
    { "MapScaleFloat                       ", false, false, false, -1 }, // 213
    { "MapOffsetInt                        ", false, false, false, -1 }, // 214
    { "MapOffsetFloat                      ", false, false, false, -1 }, // 215
    { "MapSquareInt                        ", false, false, false, -1 }, // 216
    { "MapSquareFloat                      ", false, false, false, -1 }, // 217
    { "MapHashKeyValue                     ", false, false, false, -1 }, // 218
    { "MapHashKeyInt                       ", false, false, false, -1 }, // 219
    { "MapHashKeyOffsetInt                 ", false, false, false, -1 }, // 220
    { "MapHashKeyScaleInt                  ", false, false, false, -1 }, // 221
    { "HashMapTwoKeys                      ", false, false, false, -1 }, // 222
    { "SelectAny                           ", false, false, false, -1 }, // 223
    { "SelectInt                           ", false, false, false, -1 }, // 224
    { "SelectFloat                         ", false, false, false, -1 }, // 225
    { "SelectPositiveInt                   ", false, false, false, -1 }, // 226
    { "SelectPositiveFloat                 ", false, false, false, -1 }, // 227
    { "SelectNonZeroInt                    ", false, false, false, -1 }, // 228
    { "SelectNonZeroFloat                  ", false, false, false, -1 }, // 229
    { "FusedMapSelectScalePositiveInt      ", false, false, false, -1 }, // 230
    { "FusedMapSelectScalePositiveFloat    ", false, false, false, -1 }, // 231
    { "FusedMapSelectOffsetPositiveInt     ", false, false, false, -1 }, // 232
    { "FusedMapSelectOffsetPositiveFloat   ", false, false, false, -1 }, // 233
    { "FusedMapSelectSquarePositiveInt     ", false, false, false, -1 }, // 234
    { "FusedMapSelectSquarePositiveFloat   ", false, false, false, -1 }, // 235
    { "FusedMapFoldlSumScaleInt            ", false, false, false, -1 }, // 236
    { "FusedMapFoldlSumScaleFloat          ", false, false, false, -1 }, // 237
    { "FusedMapFoldlSumSquareInt           ", false, false, false, -1 }, // 238
    { "FusedMapFoldlSumSquareFloat         ", false, false, false, -1 }, // 239
    { "FusedMapFoldlProdScaleInt           ", false, false, false, -1 }, // 240
    { "FusedMapFoldlProdScaleFloat         ", false, false, false, -1 }, // 241
    { "MapSelectAny                        ", false, false, false, -1 }, // 242
    { "HashMapAny                          ", false, false, false, -1 }, // 243
    { "HashMapSelectAny                    ", false, false, false, -1 }, // 244
    { "RangeAny                            ", false, false, false, -1 }, // 245
    { "RangeInt                            ", false, false, false, -1 }, // 246
    { "RangeFloat                          ", false, false, false, -1 }, // 247
    { "RangeDate                           ", false, false, false, -1 }, // 248
    { "RangeSliceAny                       ", false, false, false, -1 }, // 249
    { "RangeSliceInt                       ", false, false, false, -1 }, // 250
    { "RangeSliceFloat                     ", false, false, false, -1 }, // 251
    { "CastAny                             ", false, false, false, -1 }, // 252
    { "CastList                            ", false, false, false, -1 }, // 253
    { "CastHash                            ", false, false, false, -1 }, // 254
    { "CastObject                          ", false, false, false, -1 }, // 255
    { "CastEnum                            ", false, false, false, -1 }, // 256
    { "CastComplexHash                     ", false, false, false, -1 }, // 257
    { "Br                                  ", false, false, true , -1 }, // 258
    { "BrIf                                ", false, false, true , -1 }, // 259
    { "SwitchInt                           ", false, false, true , -1 }, // 260
    { "SwitchString                        ", false, false, true , -1 }, // 261
    { "Return                              ", false, false, true , -1 }, // 262
    { "ReturnNothing                       ", false, false, true , -1 }, // 263
    { "LoadLocal                           ", true , false, false, -1 }, // 264
    { "StoreLocal                          ", false, false, false, -1 }, // 265
    { "UninstantiateLocal                  ", false, false, false, -1 }, // 266
    { "LoadArg                             ", false, false, false, -1 }, // 267
    { "LoadClosure                         ", true , false, false, -1 }, // 268
    { "StoreClosure                        ", false, false, false, -1 }, // 269
    { "LoadGlobal                          ", true , false, false, -1 }, // 270
    { "StoreGlobal                         ", false, false, false, -1 }, // 271
    { "LoadThreadLocal                     ", false, false, false, -1 }, // 272
    { "StoreThreadLocal                    ", false, false, false, -1 }, // 273
    { "LoadImplicitArg                     ", false, false, false, -1 }, // 274
    { "LoadImplicitArgv                    ", false, false, false, -1 }, // 275
    { "LoadImplicitElement                 ", false, false, false, -1 }, // 276
    { "PushImplicitArg                     ", false, false, false, -1 }, // 277
    { "SetImplicitArgv                     ", false, false, false, -1 }, // 278
    { "PopImplicitArg                      ", false, false, false, -1 }, // 279
    { "PushImplicitElement                 ", false, false, false, -1 }, // 280
    { "PopImplicitElement                  ", false, false, false, -1 }, // 281
    { "HashKeyAccess                       ", true , false, false, -1 }, // 282
    { "HashKeyAccessInt                    ", true , false, false, -1 }, // 283
    { "LoadSelfMember                      ", false, false, false, -1 }, // 284
    { "LoadStaticVar                       ", false, false, false, -1 }, // 285
    { "NewObject                           ", false, false, false, -1 }, // 286
    { "LoadConstant                        ", false, false, false, -1 }, // 287
    { "CreateClosure                       ", false, false, false, -1 }, // 288
    { "CreateCallRef                       ", false, false, false, -1 }, // 289
    { "CreateMethodRef                     ", false, false, false, -1 }, // 290
    { "CreateParseRef                      ", false, false, false, -1 }, // 291
    { "NewHashDecl                         ", false, false, false, -1 }, // 292
    { "NewComplexHash                      ", false, false, false, -1 }, // 293
    { "NewComplexList                      ", false, false, false, -1 }, // 294
    { "VrnConstruct                        ", false, false, false, -1 }, // 295
    { "HashSetKeyValue                     ", false, false, false, -1 }, // 296
    { "IteratorCreateReverse               ", false, false, false, -1 }, // 297
    { "Call                                ", true , false, false, -1 }, // 298
    { "CallDirect                          ", true , false, false, -1 }, // 299
    { "CallIndirect                        ", true , false, false, -1 }, // 300
    { "CallMethod                          ", true , false, false, -1 }, // 301
    { "CallMethodDirect                    ", true , false, false, -1 }, // 302
    { "InvokeMethodDirect                  ", true , false, true , -1 }, // 303
    { "CallStatic                          ", true , false, false, -1 }, // 304
    { "CallStaticDirect                    ", true , false, false, -1 }, // 305
    { "DotEvalMethodDirect                 ", true , false, false, -1 }, // 306
    { "InvokeDotEvalMethodDirect           ", true , false, true , -1 }, // 307
    { "CallClosureDirect                   ", true , false, false, -1 }, // 308
    { "Invoke                              ", true , false, true , -1 }, // 309
    { "GuardInt                            ", false, false, false, -1 }, // 310
    { "GetObjectClass                      ", false, false, false, -1 }, // 311
    { "GuardFloat                          ", false, false, false, -1 }, // 312
    { "GuardType                           ", false, false, false, -1 }, // 313
    { "GuardNotNothing                     ", false, false, false, -1 }, // 314
    { "LandingPad                          ", false, false, false, -1 }, // 315
    { "CatchException                      ", true , false, false, -1 }, // 316
    { "CatchCleanup                        ", false, false, false, -1 }, // 317
    { "Rethrow                             ", false, false, true , -1 }, // 318
    { "Throw                               ", false, false, true , -1 }, // 319
    { "InvokeSimError                      ", false, false, true , -1 }, // 320
    { "Incref                              ", false, false, false, -1 }, // 321
    { "Decref                              ", false, false, false, -1 }, // 322
    { "DecrefNoThrow                       ", false, false, false, -1 }, // 323
    { "SwitchRegexMatch                    ", false, false, false, -1 }, // 324
    { "ListPush                            ", false, false, false, -1 }, // 325
    { "RefForeachInit                      ", false, false, false, -1 }, // 326
    { "RefForeachSize                      ", false, false, false, -1 }, // 327
    { "RefForeachGetEntry                  ", false, false, false, -1 }, // 328
    { "RefForeachRecord                    ", false, false, false, -1 }, // 329
    { "RefForeachFinalize                  ", false, false, false, -1 }, // 330
    { "RefForeachCleanup                   ", false, false, false, -1 }, // 331
    { "AddNumber                           ", false, false, false, -1 }, // 332
    { "SubNumber                           ", false, false, false, -1 }, // 333
    { "MulNumber                           ", false, false, false, -1 }, // 334
    { "DivNumber                           ", false, false, false, -1 }, // 335
    { "HashKeyStore                        ", false, false, false, -1 }, // 336
    { "ListIndexAccess                     ", true , false, false, -1 }, // 337
    { "ListIndexStore                      ", false, false, false, -1 }, // 338
    { "AddAssignLocalInt                   ", false, false, false, -1 }, // 339
    { "IncrementLocalInt                   ", false, false, false, -1 }, // 340
    { "BranchIfLtLocalInt                  ", false, false, true , -1 }, // 341
    { "ConstEnum                           ", false, false, false, -1 }, // 342
    { "ListGetValueNoRef                   ", false, false, false, -1 }, // 343
    { "SwitchCaseMatch                     ", false, false, false, -1 }, // 344
    { "IsCollectionType                    ", false, false, false, -1 }, // 345
    { "MakeHashConstKeys                   ", false, false, false, -1 }, // 346
    { "ToString                            ", false, false, false, -1 }, // 347
    { "Sprintf                             ", false, false, false, -1 }, // 348
};

//! Static assertion to verify registry completeness
static_assert(
    sizeof(OPCODE_REGISTRY) / sizeof(OPCODE_REGISTRY[0]) == 349,
    "OPCODE_REGISTRY has incorrect entry count - should be exactly 349"
);

//! ============================================================================
//! PHASE 2: Lookup Functions - Query registry by opcode ID
//! ============================================================================

//! Get opcode info by opcode enum value
//! Returns pointer to registry entry, or nullptr for invalid opcode
inline const OpcodeInfo* getOpcodeInfo(int opcode_id) {
    if (opcode_id >= 0 && opcode_id < 349) {
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

#endif
