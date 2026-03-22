/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTInstRegistry.cpp

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

#include "qore/intern/QoreAOTInstRegistry.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/QoreValue.h"

// ============================================================================
// Base Group (0) - No extra fields
// ============================================================================

static bool writeBase(AOTInstWriteCtx& ctx) {
    // Base group has no extra fields beyond opcode + group tag + base fields
    return true;
}

static std::unique_ptr<QoreIRInstruction> readBase(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto inst = std::make_unique<QoreIRInstruction>(static_cast<QoreIROpcode>(opcode_raw));
    return inst;
}

// ============================================================================
// Const Group (1) - Constant value with kind dispatch
// ============================================================================

static bool writeConst(AOTInstWriteCtx& ctx) {
    // Note: This is a placeholder for now. The actual write code is in
    // serializeIRInstruction() and will be extracted in full Phase 2.
    // For validation, we just return true - the write side still uses the old switch.
    return true;
}

static std::unique_ptr<QoreIRInstruction> readConst(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    // Placeholder: actual implementation will be extracted from deserializeIRInstruction
    return nullptr;
}

// ============================================================================
// Branch Group (2) - Single block target
// ============================================================================

static bool writeBranch(AOTInstWriteCtx& ctx) {
    // Placeholder - write side still uses old switch during validation phase
    return true;
}

static std::unique_ptr<QoreIRInstruction> readBranch(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    // Placeholder - read side still uses old switch during validation phase
    return nullptr;
}

// ============================================================================
// Return Group (4) - Optional value return
// ============================================================================

static bool writeReturn(AOTInstWriteCtx& ctx) {
    // Placeholder - write side still uses old switch during validation phase
    return true;
}

static std::unique_ptr<QoreIRInstruction> readReturn(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    // Placeholder - read side still uses old switch during validation phase
    return nullptr;
}

// ============================================================================
// Local Group (6) - Local variable with slot info
// ============================================================================

static bool writeLocal(AOTInstWriteCtx& ctx) {
    // Placeholder - write side still uses old switch during validation phase
    return true;
}

static std::unique_ptr<QoreIRInstruction> readLocal(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    // Placeholder - read side still uses old switch during validation phase
    return nullptr;
}

// ============================================================================
// OnBlockExit Group (45) - Handler with nested IR
// ============================================================================

static bool writeOnBlockExit(AOTInstWriteCtx& ctx) {
    // Placeholder - write side still uses old switch during validation phase
    return true;
}

static std::unique_ptr<QoreIRInstruction> readOnBlockExit(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    // Placeholder - read side still uses old switch during validation phase
    return nullptr;
}

// ============================================================================
// Instruction Group Registry Table
// ============================================================================

const QoreIRInstGroupInfo AOT_INST_GROUP_REGISTRY[AOT_INST_GROUP_TABLE_SIZE] = {
    // Index 0: Base
    { "Base", 0, true, false, writeBase, readBase, "Base instruction with no group-specific fields" },

    // Index 1: Const
    { "Const", 1, true, false, writeConst, readConst, "Constant value (int/float/bool/string/date/enum)" },

    // Index 2: Branch
    { "Branch", 2, true, false, writeBranch, readBranch, "Unconditional branch to target block" },

    // Indices 3: (Unused - BranchIf handled by old switch for now)
    { nullptr, 3, false, false, nullptr, nullptr, nullptr },

    // Index 4: Return
    { "Return", 4, true, false, writeReturn, readReturn, "Function return with optional value" },

    // Indices 5 onwards (Unused for now - handled by old switch during validation)
    { nullptr, 5, false, false, nullptr, nullptr, nullptr },
    { "Local", 6, true, false, writeLocal, readLocal, "Local variable slot definition" },

    // Placeholder entries for all other indices (will be filled in during full Phase 2)
    #define UNUSED_ENTRY(idx) { nullptr, idx, false, false, nullptr, nullptr, nullptr }

    UNUSED_ENTRY(7), UNUSED_ENTRY(8), UNUSED_ENTRY(9), UNUSED_ENTRY(10),
    UNUSED_ENTRY(11), UNUSED_ENTRY(12), UNUSED_ENTRY(13), UNUSED_ENTRY(14),
    UNUSED_ENTRY(15), UNUSED_ENTRY(16), UNUSED_ENTRY(17), UNUSED_ENTRY(18),
    UNUSED_ENTRY(19), UNUSED_ENTRY(20), UNUSED_ENTRY(21), UNUSED_ENTRY(22),
    UNUSED_ENTRY(23), UNUSED_ENTRY(24), UNUSED_ENTRY(25), UNUSED_ENTRY(26),
    UNUSED_ENTRY(27), UNUSED_ENTRY(28), UNUSED_ENTRY(29), UNUSED_ENTRY(30),
    UNUSED_ENTRY(31), UNUSED_ENTRY(32), UNUSED_ENTRY(33), UNUSED_ENTRY(34),
    UNUSED_ENTRY(35), UNUSED_ENTRY(36), UNUSED_ENTRY(37), UNUSED_ENTRY(38),
    UNUSED_ENTRY(39), UNUSED_ENTRY(40), UNUSED_ENTRY(41), UNUSED_ENTRY(42),
    UNUSED_ENTRY(43), UNUSED_ENTRY(44),

    // Index 45: OnBlockExit (has nested IR)
    { "OnBlockExit", 45, true, true, writeOnBlockExit, readOnBlockExit, "Exception handler with nested IR function" },

    UNUSED_ENTRY(46), UNUSED_ENTRY(47), UNUSED_ENTRY(48), UNUSED_ENTRY(49),
    UNUSED_ENTRY(50), UNUSED_ENTRY(51), UNUSED_ENTRY(52), UNUSED_ENTRY(53),
    UNUSED_ENTRY(54), UNUSED_ENTRY(55), UNUSED_ENTRY(56),

    // Remainder of table (207 unused entries for indices 57-255)
    #undef UNUSED_ENTRY
    #define UNUSED_ENTRY(idx) { nullptr, idx, false, false, nullptr, nullptr, nullptr }
    UNUSED_ENTRY(57), UNUSED_ENTRY(58), UNUSED_ENTRY(59), UNUSED_ENTRY(60),
    UNUSED_ENTRY(61), UNUSED_ENTRY(62), UNUSED_ENTRY(63), UNUSED_ENTRY(64),
    UNUSED_ENTRY(65), UNUSED_ENTRY(66), UNUSED_ENTRY(67), UNUSED_ENTRY(68),
    UNUSED_ENTRY(69), UNUSED_ENTRY(70), UNUSED_ENTRY(71), UNUSED_ENTRY(72),
    UNUSED_ENTRY(73), UNUSED_ENTRY(74), UNUSED_ENTRY(75), UNUSED_ENTRY(76),
    UNUSED_ENTRY(77), UNUSED_ENTRY(78), UNUSED_ENTRY(79), UNUSED_ENTRY(80),
    UNUSED_ENTRY(81), UNUSED_ENTRY(82), UNUSED_ENTRY(83), UNUSED_ENTRY(84),
    UNUSED_ENTRY(85), UNUSED_ENTRY(86), UNUSED_ENTRY(87), UNUSED_ENTRY(88),
    UNUSED_ENTRY(89), UNUSED_ENTRY(90), UNUSED_ENTRY(91), UNUSED_ENTRY(92),
    UNUSED_ENTRY(93), UNUSED_ENTRY(94), UNUSED_ENTRY(95), UNUSED_ENTRY(96),
    UNUSED_ENTRY(97), UNUSED_ENTRY(98), UNUSED_ENTRY(99), UNUSED_ENTRY(100),
    UNUSED_ENTRY(101), UNUSED_ENTRY(102), UNUSED_ENTRY(103), UNUSED_ENTRY(104),
    UNUSED_ENTRY(105), UNUSED_ENTRY(106), UNUSED_ENTRY(107), UNUSED_ENTRY(108),
    UNUSED_ENTRY(109), UNUSED_ENTRY(110), UNUSED_ENTRY(111), UNUSED_ENTRY(112),
    UNUSED_ENTRY(113), UNUSED_ENTRY(114), UNUSED_ENTRY(115), UNUSED_ENTRY(116),
    UNUSED_ENTRY(117), UNUSED_ENTRY(118), UNUSED_ENTRY(119), UNUSED_ENTRY(120),
    UNUSED_ENTRY(121), UNUSED_ENTRY(122), UNUSED_ENTRY(123), UNUSED_ENTRY(124),
    UNUSED_ENTRY(125), UNUSED_ENTRY(126), UNUSED_ENTRY(127), UNUSED_ENTRY(128),
    UNUSED_ENTRY(129), UNUSED_ENTRY(130), UNUSED_ENTRY(131), UNUSED_ENTRY(132),
    UNUSED_ENTRY(133), UNUSED_ENTRY(134), UNUSED_ENTRY(135), UNUSED_ENTRY(136),
    UNUSED_ENTRY(137), UNUSED_ENTRY(138), UNUSED_ENTRY(139), UNUSED_ENTRY(140),
    UNUSED_ENTRY(141), UNUSED_ENTRY(142), UNUSED_ENTRY(143), UNUSED_ENTRY(144),
    UNUSED_ENTRY(145), UNUSED_ENTRY(146), UNUSED_ENTRY(147), UNUSED_ENTRY(148),
    UNUSED_ENTRY(149), UNUSED_ENTRY(150), UNUSED_ENTRY(151), UNUSED_ENTRY(152),
    UNUSED_ENTRY(153), UNUSED_ENTRY(154), UNUSED_ENTRY(155), UNUSED_ENTRY(156),
    UNUSED_ENTRY(157), UNUSED_ENTRY(158), UNUSED_ENTRY(159), UNUSED_ENTRY(160),
    UNUSED_ENTRY(161), UNUSED_ENTRY(162), UNUSED_ENTRY(163), UNUSED_ENTRY(164),
    UNUSED_ENTRY(165), UNUSED_ENTRY(166), UNUSED_ENTRY(167), UNUSED_ENTRY(168),
    UNUSED_ENTRY(169), UNUSED_ENTRY(170), UNUSED_ENTRY(171), UNUSED_ENTRY(172),
    UNUSED_ENTRY(173), UNUSED_ENTRY(174), UNUSED_ENTRY(175), UNUSED_ENTRY(176),
    UNUSED_ENTRY(177), UNUSED_ENTRY(178), UNUSED_ENTRY(179), UNUSED_ENTRY(180),
    UNUSED_ENTRY(181), UNUSED_ENTRY(182), UNUSED_ENTRY(183), UNUSED_ENTRY(184),
    UNUSED_ENTRY(185), UNUSED_ENTRY(186), UNUSED_ENTRY(187), UNUSED_ENTRY(188),
    UNUSED_ENTRY(189), UNUSED_ENTRY(190), UNUSED_ENTRY(191), UNUSED_ENTRY(192),
    UNUSED_ENTRY(193), UNUSED_ENTRY(194), UNUSED_ENTRY(195), UNUSED_ENTRY(196),
    UNUSED_ENTRY(197), UNUSED_ENTRY(198), UNUSED_ENTRY(199), UNUSED_ENTRY(200),
    UNUSED_ENTRY(201), UNUSED_ENTRY(202), UNUSED_ENTRY(203), UNUSED_ENTRY(204),
    UNUSED_ENTRY(205), UNUSED_ENTRY(206), UNUSED_ENTRY(207), UNUSED_ENTRY(208),
    UNUSED_ENTRY(209), UNUSED_ENTRY(210), UNUSED_ENTRY(211), UNUSED_ENTRY(212),
    UNUSED_ENTRY(213), UNUSED_ENTRY(214), UNUSED_ENTRY(215), UNUSED_ENTRY(216),
    UNUSED_ENTRY(217), UNUSED_ENTRY(218), UNUSED_ENTRY(219), UNUSED_ENTRY(220),
    UNUSED_ENTRY(221), UNUSED_ENTRY(222), UNUSED_ENTRY(223), UNUSED_ENTRY(224),
    UNUSED_ENTRY(225), UNUSED_ENTRY(226), UNUSED_ENTRY(227), UNUSED_ENTRY(228),
    UNUSED_ENTRY(229), UNUSED_ENTRY(230), UNUSED_ENTRY(231), UNUSED_ENTRY(232),
    UNUSED_ENTRY(233), UNUSED_ENTRY(234), UNUSED_ENTRY(235), UNUSED_ENTRY(236),
    UNUSED_ENTRY(237), UNUSED_ENTRY(238), UNUSED_ENTRY(239), UNUSED_ENTRY(240),
    UNUSED_ENTRY(241), UNUSED_ENTRY(242), UNUSED_ENTRY(243), UNUSED_ENTRY(244),
    UNUSED_ENTRY(245), UNUSED_ENTRY(246), UNUSED_ENTRY(247), UNUSED_ENTRY(248),
    UNUSED_ENTRY(249), UNUSED_ENTRY(250), UNUSED_ENTRY(251), UNUSED_ENTRY(252),
    UNUSED_ENTRY(253), UNUSED_ENTRY(254), UNUSED_ENTRY(255),
    #undef UNUSED_ENTRY
};
