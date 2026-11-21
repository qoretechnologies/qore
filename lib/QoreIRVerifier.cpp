/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRVerifier.cpp

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

#include "qore/intern/QoreJITIncludes.h"
#include <qore/intern/QoreIRVerifier.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/QoreIRAnalysis.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/QoreAssignmentOperatorNode.h>
#include <qore/intern/QoreOperatorNode.h>
#include <qore/intern/QoreExtractOperatorNode.h>
#include <qore/intern/QoreSpliceOperatorNode.h>
#include <qore/intern/CaseNodeWithOperator.h>
#include <qore/intern/FindNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>
#include <qore/intern/SelfVarrefNode.h>
#include <qore/intern/ObjectMethodReferenceNode.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/QoreParseHashNode.h>
#include <qore/intern/QoreClosureParseNode.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/IfStatement.h>
#include <qore/intern/WhileStatement.h>
#include <qore/intern/ForStatement.h>
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/SwitchStatement.h>
#include <qore/intern/TryStatement.h>
#include <qore/intern/ReturnStatement.h>
#include <qore/intern/ThrowStatement.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/DebugStatement.h>
#include <qore/intern/AssertStatement.h>
#include <qore/intern/ExpressionStatement.h>
#include <qore/intern/QoreOpcodeRegistry.h>

// QoreIRTakeOver.cpp compiles this file with QORE_IR_TAKE_OVER_UNIT defined, outside the single compilation unit of
// libqore: that unit compiles the analysis of the parameters that take their arguments over (see
// qore_ir_compute_param_take_over() at the end of this file) and the walkers of expressions and statement blocks that
// it uses; "#ifndef QORE_IR_TAKE_OVER_UNIT" encloses the rest of the file
#ifndef QORE_IR_TAKE_OVER_UNIT
// isTerminator() is now defined in QoreIR.h

// Check if opcode produces a result value — delegates to registry
static bool requiresResult(QoreIROpcode op) {
    return getOpcodeProducesResult(static_cast<int>(op));
}

// Get expected operand count for opcode — delegates to registry
static int expectedOperands(QoreIROpcode op) {
    return getOpcodeExpectedOperands(static_cast<int>(op));
}

static const std::string& getOpcodeRegistryValidationError() {
    static const std::string registry_error = []() {
        std::string error;
        if (!qore_ir_validate_opcode_registry(error)) {
            return "IR opcode registry validation failed: " + error;
        }
        return std::string();
    }();
    return registry_error;
}

// Defined in QoreIRInterpreter.cpp
extern const VarRefNode* extractLValueBaseVarRef(const QoreValue&);

static const LocalVar* getLocalVarFromVarRef(const VarRefNode* var) {
    if (!var) {
        return nullptr;
    }
    qore_var_t vtype = var->getType();
    return (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS)
        ? reinterpret_cast<const LocalVar*>(var->ref.id) : nullptr;
}

static bool rejectReadOnlyLocalWrite(const LocalVar* lv, const char* op, std::string& error) {
    if (lv && lv->isReadOnly()) {
        error = std::string("readonly local '") + lv->getName() + "' used as write target by " + op;
        return true;
    }
    return false;
}

static bool rejectReadOnlyVarRefWrite(const VarRefNode* var, const char* op, std::string& error) {
    return rejectReadOnlyLocalWrite(getLocalVarFromVarRef(var), op, error);
}

// Collect all branch targets (blocks that can be reached)
static std::unordered_set<const QoreIRBasicBlock*> collectBranchTargets(const QoreIRFunction& func) {
    std::unordered_set<const QoreIRBasicBlock*> targets;
    // Entry block is always reachable
    if (!func.blocks.empty()) {
        targets.insert(func.blocks[0].get());
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            // Collect branch targets
            if (auto* br = qore_ir_inst_cast<const QoreIRBranchInstruction*>(inst.get())) {
                if (br->target) {
                    targets.insert(br->target);
                }
            } else if (auto* brif = qore_ir_inst_cast<const QoreIRBranchIfInstruction*>(inst.get())) {
                if (brif->true_target) {
                    targets.insert(brif->true_target);
                }
                if (brif->false_target) {
                    targets.insert(brif->false_target);
                }
            } else if (auto* invoke = qore_ir_inst_cast<const QoreIRInvokeInstruction*>(inst.get())) {
                if (invoke->normal_target) {
                    targets.insert(invoke->normal_target);
                }
                if (invoke->exception_target) {
                    targets.insert(invoke->exception_target);
                }
            } else if (auto* sw_int = qore_ir_inst_cast<const QoreIRSwitchIntInstruction*>(inst.get())) {
                for (const auto& case_info : sw_int->cases) {
                    if (case_info.target) {
                        targets.insert(case_info.target);
                    }
                }
                if (sw_int->default_target) {
                    targets.insert(sw_int->default_target);
                }
            } else if (auto* sw_str = qore_ir_inst_cast<const QoreIRSwitchStringInstruction*>(inst.get())) {
                for (const auto& case_info : sw_str->cases) {
                    if (case_info.target) {
                        targets.insert(case_info.target);
                    }
                }
                if (sw_str->default_target) {
                    targets.insert(sw_str->default_target);
                }
            } else if (auto* thr = qore_ir_inst_cast<const QoreIRThrowInstruction*>(inst.get())) {
                if (thr->exception_target) {
                    targets.insert(thr->exception_target);
                }
            } else if (auto* inv_md = qore_ir_inst_cast<const QoreIRInvokeMethodDirectInstruction*>(inst.get())) {
                if (inv_md->normal_target) {
                    targets.insert(inv_md->normal_target);
                }
                if (inv_md->exception_target) {
                    targets.insert(inv_md->exception_target);
                }
            } else if (auto* inv_de = qore_ir_inst_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get())) {
                if (inv_de->normal_target) {
                    targets.insert(inv_de->normal_target);
                }
                if (inv_de->exception_target) {
                    targets.insert(inv_de->exception_target);
                }
            } else if (auto* cah = qore_ir_inst_cast<const QoreIRCallAOTHelperInstruction*>(inst.get())) {
                // transient outline return-token target (AOT outliner only)
                if (cah->return_target) {
                    targets.insert(cah->return_target);
                }
            } else if (auto* fused = qore_ir_inst_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst.get())) {
                if (fused->true_target) {
                    targets.insert(fused->true_target);
                }
                if (fused->false_target) {
                    targets.insert(fused->false_target);
                }
            } else if (auto* iter_next = qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(inst.get())) {
                if (iter_next->done_target) {
                    targets.insert(iter_next->done_target);
                }
                if (iter_next->continue_target) {
                    targets.insert(iter_next->continue_target);
                }
            } else if (auto* guard = qore_ir_inst_cast<const QoreIRGuardInstruction*>(inst.get())) {
                if (guard->deopt_target) {
                    targets.insert(guard->deopt_target);
                }
            }
            // Also check base class exception_target for other opcodes
            if (inst->exception_target) {
                targets.insert(inst->exception_target);
            }
            // the paths of the debugger's flow requests at a step event (#6201)
            if (const QoreIRDebugFlowTargets* flow = qore_ir_debug_flow_targets(inst.get())) {
                for (QoreIRBasicBlock* target : flow->targets()) {
                    if (target) {
                        targets.insert(target);
                    }
                }
            }
        }
    }
    return targets;
}

namespace {
//! A node of the graph that verifyModuleIR() computes the dominators of: the instructions of a block from its start or
//! from an instruction that can raise an exception to the next such instruction (see QoreIRVerifier::verifyModuleIR())
struct QoreIRVerifyNode {
    //! the successors of the node
    std::vector<uint32_t> succ;
    //! the predecessors of the node
    std::vector<uint32_t> pred;
};

//! Returns the exception target of the instruction, if any: the target of the instruction, or that of a throw
QoreIRBasicBlock* qore_ir_verify_exception_target(const QoreIRInstruction& inst) {
    if (inst.exception_target) {
        return inst.exception_target;
    }
    if (const auto* throw_inst = qore_ir_inst_cast<const QoreIRThrowInstruction*>(&inst)) {
        return throw_inst->exception_target;
    }
    return nullptr;
}

//! Returns true if the instruction is a step of an iterator, whose result (the next element) is defined only on the
//! edge to its continue target: its done edge leaves from the node before it (see QoreIRVerifier::verifyModuleIR())
bool qore_ir_verify_is_iterator_step(const QoreIRInstruction& inst) {
    switch (inst.opcode) {
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::IteratorNextSelectors:
        case QoreIROpcode::TypedForeachNextInt:
        case QoreIROpcode::TypedForeachNextFloat:
        case QoreIROpcode::TypedForeachNextBool:
        case QoreIROpcode::TypedForeachNextString:
            return true;
        default:
            return false;
    }
}

//! Returns the name of the opcode of the instruction
std::string qore_ir_verify_opcode_name(const QoreIRInstruction& inst) {
    const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(inst.opcode));
    return oi && oi->name ? oi->name : "<unknown>";
}

//! The graph of nodes that verifyModuleIR() builds, which the verification of the ownership and of the stacks uses
struct QoreIRVerifyGraph {
    const QoreIRFunction& func;
    const std::unordered_map<const QoreIRBasicBlock*, uint32_t>& block_index;
    //! whether each block can run
    const std::vector<uint8_t>& reachable;
    //! the position of the terminator of each block that can run
    const std::vector<uint32_t>& block_end;
    //! the node at which each block that can run is entered
    const std::vector<uint32_t>& block_entry;
    //! the node of each instruction, by block
    const std::vector<std::vector<uint32_t>>& inst_node;
    uint32_t num_nodes;
};

//! How an instruction releases a value that it takes (see qore_ir_verify_released_operand())
enum class QoreIRVerifyRelease : uint8_t {
    //! no release
    None,
    //! a release of what the slot of the value holds, which clears the slot: another such release of the same value
    //! finds nothing to release, a use of it is refused
    Slot,
    //! a release of the object of the value, which the JIT makes through the value that it holds (the interpreter
    //! clears the slot, qore#6451): no other release or use of the value or of its copies may follow
    Object,
};

//! The state of a value at a point of a function, for the ownership checks of verifyModuleIR(): a value that is not
//! released holds what its definition gave it (0)
enum QoreIRVerifyValueState : uint8_t {
    //! the value was released through its own slot (QoreIRVerifyRelease::Slot)
    QORE_IR_VERIFY_CLEARED = 1,
    //! the object of the value was released through another value or by a release of the object
    QORE_IR_VERIFY_FREED = 2,
};

//! Returns the index of the operand that the instruction releases (the first operand), and how, or -1
/** Each opcode that releases a value that it takes, or a value that a value of the function holds a reference to, is
    listed here: the verifier refuses a later use of the value on any path.  An opcode not listed takes its operands
    as uses only: a new opcode that releases or transfers a value, or that pushes on or pops from a stack of the frame
    or of the thread (see qore_ir_verify_stack_effect()), must be added to these functions.
*/
int qore_ir_verify_released_operand(const QoreIRInstruction& inst, QoreIRVerifyRelease& kind) {
    switch (inst.opcode) {
        // the interpreter releases the value only if its frame holds a cleanup entry for it, and clears its slot; the
        // JIT releases only the reference in the value's cleanup slot, and clears the slot
        case QoreIROpcode::ReleaseOwnedTemp:
        // the interpreter and the JIT release the iterator in the value's slot and clear the slot; a copy of the
        // handle that a phi made still points to the iterator
        case QoreIROpcode::IteratorRelease:
            kind = QoreIRVerifyRelease::Slot;
            return 0;
        // the JIT releases the object that the value holds in its register (the interpreter clears the slot, see
        // qore#6451)
        case QoreIROpcode::ContextDestroy:
        case QoreIROpcode::RefForeachCleanup:
        case QoreIROpcode::RefForeachFinalize:
        case QoreIROpcode::ExceptionResume:
        // PopImplicitArg is not a release of its operand: it releases the arguments that the thread holds (those that the
        // push set) and makes the saved ones (its operand) current again without taking the reference of the operand's
        // slot, which SetImplicitArgv gives the frame and a ReleaseOwnedTemp releases after the pop; a second restore
        // from the same value, which would release the arguments that the first one restored, is refused by the stack
        // of the implicit arguments (qore_ir_verify_stack_effect())
        case QoreIROpcode::Decref:
        case QoreIROpcode::DecrefNoThrow:
            kind = QoreIRVerifyRelease::Object;
            return 0;
        default:
            kind = QoreIRVerifyRelease::None;
            return -1;
    }
}

//! Returns the opcodes that may define the container that the instruction changes (its first operand), if the
//! interpreter or the JIT take the value as a container of that type without a check of its type; nullptr otherwise
/** The lowering creates the list that it appends to, stores to or sets the length of, the list that it sets as the
    implicit arguments, and the hash that it sets keys of, with the instruction that creates it, in the same function
    (QoreIRLowering: map, select, hash map and the implicit arguments of a fold or a call reference): the value is
    the result of that instruction, never a phi or a value of another kind.
*/
const std::vector<QoreIROpcode>* qore_ir_verify_container_definers(QoreIROpcode opcode) {
    static const std::vector<QoreIROpcode> lists = {QoreIROpcode::CreateEmptyList, QoreIROpcode::CreateSizedList,
        QoreIROpcode::MakeList};
    static const std::vector<QoreIROpcode> hashes = {QoreIROpcode::MakeHash};
    switch (opcode) {
        // QoreValue::get<QoreListNode>() without a check (the interpreter, qore_rt_list_append(),
        // qore_rt_set_implicit_argv(), qore_rt_list_set_length_unchecked()), and the stores of the JIT to the entries
        // of a typed list that it created
        case QoreIROpcode::ListAppend:
        case QoreIROpcode::SetImplicitArgv:
        case QoreIROpcode::ListSetInt:
        case QoreIROpcode::ListSetFloat:
        case QoreIROpcode::ListSetValue:
        case QoreIROpcode::ListSetLength:
            return &lists;
        // QoreValue::get<QoreHashNode>() without a check (the interpreter, qore_rt_hash_set_key_value())
        case QoreIROpcode::HashSetKeyValue:
        case QoreIROpcode::HashSetKeyValueFold:
            return &hashes;
        default:
            return nullptr;
    }
}

//! The stacks that verifyModuleIR() follows: their depths are the same on every path into a point of the function
enum QoreIRVerifyStack : uint8_t {
    //! the temp scopes of the frame (PushTempMark), by temp scope id
    QORE_IR_VERIFY_TEMPS = 0,
    //! the scopes of the frame (ScopeEnter), by scope id
    QORE_IR_VERIFY_SCOPES,
    //! the implicit arguments of the thread (PushImplicitArg, SetImplicitArgv), by the value that saves the previous
    QORE_IR_VERIFY_ARGS,
    //! the implicit elements of the thread (PushImplicitElement), by the value that saves the previous
    QORE_IR_VERIFY_ELEMENTS,
    //! the contexts of the thread (Context), by the value that holds the context
    QORE_IR_VERIFY_CONTEXTS,
    //! the catch blocks of the frame (CatchException), by the number of scopes open when the catch block began: an
    //! exception that leaves scopes closes the catch blocks begun in them (qore_ir_close_catch_scopes(), qore#6658)
    QORE_IR_VERIFY_CATCHES,
    QORE_IR_VERIFY_NUM_STACKS,
};

//! The names of the stacks, for the errors
const char* const qore_ir_verify_stack_names[QORE_IR_VERIFY_NUM_STACKS] = {
    "temp scopes", "scopes", "implicit arguments", "implicit elements", "contexts", "catch blocks",
};

//! The effect of an instruction on a stack (see qore_ir_verify_stack_effect())
struct QoreIRVerifyStackEffect {
    //! the stack, or QORE_IR_VERIFY_NUM_STACKS for none
    QoreIRVerifyStack stack = QORE_IR_VERIFY_NUM_STACKS;
    //! true for a push, false for a pop
    bool push = false;
    //! the id that is pushed, or that the innermost entry must have for a pop
    uint32_t id = 0;
    //! a pop that leaves the entry on the stack (a trim of the handlers of a scope)
    bool keep = false;
};

//! Returns the effect of the instruction on the stacks that verifyModuleIR() follows
/** The interpreter and the JIT pair each pop with the innermost entry of its stack: a context and an implicit
    argument with the thread's innermost one (a context destroyed out of order leaves the thread's stack on a freed
    context), a temp scope and a scope with the frame's innermost one (the JIT releases the temporaries registered
    after the mark of a temp scope each time that its end runs, those held in registers without clearing them).  A
    pop that is not of the innermost entry is refused, and so is a pop that can run twice for one push.
*/
QoreIRVerifyStackEffect qore_ir_verify_stack_effect(const QoreIRInstruction& inst) {
    QoreIRVerifyStackEffect e;
    switch (inst.opcode) {
        case QoreIROpcode::PushTempMark:
            e.stack = QORE_IR_VERIFY_TEMPS;
            e.push = true;
            e.id = inst.temp_scope_id;
            break;
        case QoreIROpcode::DiscardTemps:
        case QoreIROpcode::DiscardTempsKeep:
        case QoreIROpcode::UnwindTemps:
            e.stack = QORE_IR_VERIFY_TEMPS;
            e.id = inst.temp_scope_id;
            break;
        case QoreIROpcode::ScopeEnter:
            if (const auto* enter = qore_ir_inst_cast<const QoreIRScopeEnterInstruction*>(&inst)) {
                e.stack = QORE_IR_VERIFY_SCOPES;
                e.push = true;
                e.id = enter->scope_id;
            }
            break;
        case QoreIROpcode::ScopeExit:
            if (const auto* exit = qore_ir_inst_cast<const QoreIRScopeExitInstruction*>(&inst)) {
                e.stack = QORE_IR_VERIFY_SCOPES;
                e.id = exit->scope_id;
                e.keep = exit->trim;
            }
            break;
        case QoreIROpcode::PushImplicitArg:
        case QoreIROpcode::SetImplicitArgv:
            e.stack = QORE_IR_VERIFY_ARGS;
            e.push = true;
            e.id = inst.result.id;
            break;
        case QoreIROpcode::PopImplicitArg:
            e.stack = QORE_IR_VERIFY_ARGS;
            e.id = inst.operands.empty() ? 0 : inst.operands[0].id;
            break;
        case QoreIROpcode::PushImplicitElement:
            e.stack = QORE_IR_VERIFY_ELEMENTS;
            e.push = true;
            e.id = inst.result.id;
            break;
        case QoreIROpcode::PopImplicitElement:
            e.stack = QORE_IR_VERIFY_ELEMENTS;
            e.id = inst.operands.empty() ? 0 : inst.operands[0].id;
            break;
        case QoreIROpcode::Context:
            e.stack = QORE_IR_VERIFY_CONTEXTS;
            e.push = true;
            e.id = inst.result.id;
            break;
        case QoreIROpcode::ContextDestroy:
            e.stack = QORE_IR_VERIFY_CONTEXTS;
            e.id = inst.operands.empty() ? 0 : inst.operands[0].id;
            break;
        default:
            break;
    }
    return e;
}

//! Returns true if the instruction pops a stack of the thread on its exception edge, which the verifier does not model:
//! the lowering gives none of them an exception target (see QoreIRVerifyOwnership::apply())
bool qore_ir_verify_pop_without_exception_edge(const QoreIRInstruction& inst) {
    switch (inst.opcode) {
        case QoreIROpcode::PopImplicitArg:
        case QoreIROpcode::PopImplicitElement:
        case QoreIROpcode::ContextDestroy:
            return true;
        default:
            return false;
    }
}

//! The state of the values and of the stacks at a point of a function (see qore_ir_verify_ownership())
struct QoreIRVerifyState {
    //! whether a path reaches the point
    bool reached = false;
    //! the values that are released (QoreIRVerifyValueState), sorted by value id; the others are not
    std::vector<std::pair<uint32_t, uint8_t>> released;
    //! the entries of each stack, innermost last
    std::vector<uint32_t> stacks[QORE_IR_VERIFY_NUM_STACKS];
    //! whether the entries of each stack are known: not on a path from an exception edge until it meets a path of
    //! the normal control flow (see QoreIRVerifyOwnership::verify())
    bool known[QORE_IR_VERIFY_NUM_STACKS] = {true, true, true, true, true, true};
    //! true on the path of an exception (QoreIRVerifyOwnership::verifyExceptionPaths()): the end of a temp scope
    //! closes the temp scopes opened after it as well, as the IR interpreter's DiscardTemps does: the branch to the
    //! handler leaves the mark of the first temp scope that the exception left, which the end of the enclosing
    //! statement removes (see design/ir-exception-branch-temp-scope.md, qore#6742)
    bool temps_relaxed = false;

    //! closes the catch blocks begun in the scopes that an exception left, with \a scopes scopes left open (the
    //! interpreter's LandingPad and ScopeExit, qore_ir_close_catch_scopes())
    void closeCatches(size_t scopes) {
        if (!known[QORE_IR_VERIFY_CATCHES]) {
            return;
        }
        std::vector<uint32_t>& catches = stacks[QORE_IR_VERIFY_CATCHES];
        while (!catches.empty() && catches.back() > scopes) {
            catches.pop_back();
        }
    }

    uint8_t get(uint32_t id) const {
        auto i = std::lower_bound(released.begin(), released.end(), std::make_pair(id, uint8_t(0)));
        return i != released.end() && i->first == id ? i->second : 0;
    }

    //! raises the state of the value to the state given
    void raise(uint32_t id, uint8_t state) {
        auto i = std::lower_bound(released.begin(), released.end(), std::make_pair(id, uint8_t(0)));
        if (i != released.end() && i->first == id) {
            i->second = std::max(i->second, state);
        } else {
            released.insert(i, {id, state});
        }
    }

    //! the value is defined: it holds what its definition gave it
    void define(uint32_t id) {
        auto i = std::lower_bound(released.begin(), released.end(), std::make_pair(id, uint8_t(0)));
        if (i != released.end() && i->first == id) {
            released.erase(i);
        }
    }
};

//! Verifies the ownership of the values and the stacks of the frame and of the thread (see
//! QoreIRVerifier::verifyModuleIR()), as a forward dataflow over the graph of nodes of the function
class QoreIRVerifyOwnership {
public:
    QoreIRVerifyOwnership(const QoreIRVerifyGraph& g, std::string& error) : g(g), error(error) {
    }

    bool verify();

private:
    //! how an edge between blocks leaves its node
    enum EdgeKind : uint8_t {
        //! from the end of the last node of the block: a successor of its terminator
        EdgeNormal,
        //! from the node before an instruction that raises an exception, to its handler
        EdgeException,
        //! from the node before a step of an iterator, to its done target
        EdgeDone,
    };

    struct Edge {
        uint32_t to_block;
        EdgeKind kind;
        //! the instruction that raises the exception or the step (EdgeException, EdgeDone)
        const QoreIRInstruction* inst;
        uint32_t pos;
    };

    const QoreIRVerifyGraph& g;
    std::string& error;
    size_t check_count = 0;
    //! the block, and the range of instructions, of each node
    std::vector<uint32_t> node_block, node_begin, node_end;
    //! the edges between blocks that leave each node
    std::vector<std::vector<Edge>> edges;
    //! the handles of objects and their copies by phis: the members of the group of each handle in a group of more
    //! than one value (see groupOf())
    std::unordered_map<uint32_t, uint32_t> group_root;
    std::unordered_map<uint32_t, std::vector<uint32_t>> group_members;

    bool cancelled() {
        if (!(++check_count % 1000) && qore_check_cancel(nullptr, "AOT IR verification")) {
            error = "AOT IR verification cancelled";
            return true;
        }
        return false;
    }

    std::string where(uint32_t b, size_t pos) const {
        return " (block " + std::to_string(b) + ", instruction " + std::to_string(pos) + ")";
    }

    void buildNodes();
    void buildGroups();

    //! releases the value: its own slot (QoreIRVerifyRelease::Slot) or its object, and the object of the other values
    //! of its group
    void release(QoreIRVerifyState& s, uint32_t id, QoreIRVerifyRelease kind) const {
        s.raise(id, kind == QoreIRVerifyRelease::Slot ? QORE_IR_VERIFY_CLEARED : QORE_IR_VERIFY_FREED);
        auto r = group_root.find(id);
        if (r == group_root.end()) {
            return;
        }
        for (uint32_t m : group_members.at(r->second)) {
            if (m != id) {
                s.raise(m, QORE_IR_VERIFY_FREED);
            }
        }
    }

    //! checks a use of a value (or a release of the given kind) in the state
    bool checkUse(const QoreIRVerifyState& s, uint32_t id, const QoreIRInstruction& inst, uint32_t b, size_t pos,
            QoreIRVerifyRelease kind = QoreIRVerifyRelease::None);

    //! applies an instruction to the state on the path that continues after it, or on its exception or done edge
    bool apply(QoreIRVerifyState& s, const QoreIRInstruction& inst, uint32_t b, size_t pos, EdgeKind mode);

    //! returns the position of the first instruction of the block that is not a phi
    uint32_t firstNonPhi(uint32_t b) const {
        const auto& insts = g.func.blocks[b]->instructions;
        uint32_t pos = 0;
        while (pos < g.block_end[b] && insts[pos]->opcode == QoreIROpcode::Phi) {
            ++pos;
        }
        return pos;
    }

    //! applies the stack effect of the instruction
    bool applyStack(QoreIRVerifyState& s, const QoreIRInstruction& inst, uint32_t b, size_t pos);

    //! merges the state of an edge into the state of the entry of the block
    bool join(QoreIRVerifyState& to, const QoreIRVerifyState& from, uint32_t from_block, uint32_t to_block,
            bool& changed);
    //! follows the paths of the exceptions with the stacks that each raise point has (qore#6658)
    bool verifyExceptionPaths(const std::vector<QoreIRVerifyState>& in);
    //! drains the temp scopes of the state of an exception edge as the IR interpreter does before it branches to the
    //! handler (qore_ir_exception_branch_temp_scope(), qore#6742)
    bool exceptionTemps(QoreIRVerifyState& es, const QoreIRInstruction& inst, uint32_t b, size_t pos);
};

void QoreIRVerifyOwnership::buildNodes() {
    node_block.assign(g.num_nodes, 0);
    node_begin.assign(g.num_nodes, 0);
    node_end.assign(g.num_nodes, 0);
    edges.assign(g.num_nodes, {});
    const size_t num_blocks = g.func.blocks.size();
    for (uint32_t b = 0; b < num_blocks; ++b) {
        if (!g.reachable[b]) {
            continue;
        }
        node_block[g.block_entry[b]] = b;
        const auto& insts = g.func.blocks[b]->instructions;
        for (uint32_t pos = 0; pos <= g.block_end[b]; ++pos) {
            uint32_t n = g.inst_node[b][pos];
            if (!node_end[n]) {
                node_block[n] = b;
                node_begin[n] = pos;
            }
            node_end[n] = pos + 1;
            if (QoreIRBasicBlock* target = qore_ir_verify_exception_target(*insts[pos])) {
                edges[n - 1].push_back({g.block_index.at(target), EdgeException, insts[pos].get(), pos});
            }
        }
        const QoreIRInstruction& last = *insts[g.block_end[b]];
        uint32_t last_node = g.inst_node[b][g.block_end[b]];
        if (const auto* next = qore_ir_verify_is_iterator_step(last)
                ? qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(&last) : nullptr) {
            edges[last_node].push_back({g.block_index.at(next->continue_target), EdgeNormal, &last, g.block_end[b]});
            edges[last_node - 1].push_back({g.block_index.at(next->done_target), EdgeDone, &last, g.block_end[b]});
        } else {
            qore_ir_visit_successors(last, [&](QoreIRBasicBlock* target) {
                edges[last_node].push_back({g.block_index.at(target), EdgeNormal, &last, g.block_end[b]});
            });
        }
    }
}

void QoreIRVerifyOwnership::buildGroups() {
    // the handles of the objects that the interpreter and the JIT release through a value that holds their address
    // (see qore_aot_ir_handle_definers() in the reader), and the phis that copy them: a group of values that hold the
    // address of the same object
    std::unordered_set<uint32_t> handles;
    std::vector<const QoreIRPhiInstruction*> phis;
    for (uint32_t b = 0; b < g.func.blocks.size(); ++b) {
        if (!g.reachable[b]) {
            continue;
        }
        const auto& insts = g.func.blocks[b]->instructions;
        for (uint32_t pos = 0; pos <= g.block_end[b]; ++pos) {
            const QoreIRInstruction& inst = *insts[pos];
            switch (inst.opcode) {
                case QoreIROpcode::IteratorCreate:
                case QoreIROpcode::IteratorCreateIterate:
                case QoreIROpcode::IteratorCreateSelectors:
                case QoreIROpcode::IteratorCreateSource:
                case QoreIROpcode::IteratorCreateReverse:
                case QoreIROpcode::Context:
                case QoreIROpcode::RefForeachInit:
                case QoreIROpcode::ExceptionSuspend:
                    if (inst.result.isValid()) {
                        handles.insert(inst.result.id);
                    }
                    break;
                case QoreIROpcode::Phi:
                    if (inst.result.isValid()) {
                        phis.push_back(static_cast<const QoreIRPhiInstruction*>(&inst));
                    }
                    break;
                default:
                    break;
            }
        }
    }
    if (handles.empty() || phis.empty()) {
        return;
    }
    // union-find of the phis with their incoming values that are phis or handles, in one pass: a group holds the
    // address of an object if it has a handle; a value of another kind (a constant merged with a handle) is not a
    // member, and does not join two groups
    std::unordered_set<uint32_t> phi_ids;
    for (const QoreIRPhiInstruction* phi : phis) {
        phi_ids.insert(phi->result.id);
    }
    std::unordered_map<uint32_t, uint32_t> parent;
    std::function<uint32_t(uint32_t)> find = [&](uint32_t id) -> uint32_t {
        auto i = parent.find(id);
        if (i == parent.end()) {
            return id;
        }
        uint32_t root = id;
        while (true) {
            auto j = parent.find(root);
            if (j == parent.end() || j->second == root) {
                break;
            }
            root = j->second;
        }
        // path compression
        while (id != root) {
            uint32_t next = parent[id];
            parent[id] = root;
            id = next;
        }
        return root;
    };
    for (const QoreIRPhiInstruction* phi : phis) {
        if (cancelled()) {
            return;
        }
        parent.emplace(phi->result.id, phi->result.id);
        for (const QoreIRPhiIncoming& in : phi->incoming) {
            if (!in.value.isValid() || (!phi_ids.count(in.value.id) && !handles.count(in.value.id))) {
                continue;
            }
            parent.emplace(in.value.id, in.value.id);
            uint32_t a = find(phi->result.id), c = find(in.value.id);
            if (a != c) {
                parent[a] = c;
            }
        }
    }
    // the groups with a handle
    std::unordered_set<uint32_t> handle_roots;
    for (const auto& i : parent) {
        if (handles.count(i.first)) {
            handle_roots.insert(find(i.first));
        }
    }
    for (const auto& i : parent) {
        uint32_t root = find(i.first);
        if (handle_roots.count(root)) {
            group_root[i.first] = root;
            group_members[root].push_back(i.first);
        }
    }
}

bool QoreIRVerifyOwnership::checkUse(const QoreIRVerifyState& s, uint32_t id, const QoreIRInstruction& inst,
        uint32_t b, size_t pos, QoreIRVerifyRelease kind) {
    uint8_t state = s.get(id);
    // a release of a slot that a release of the same slot cleared finds nothing to release
    if (!state || (kind == QoreIRVerifyRelease::Slot && state == QORE_IR_VERIFY_CLEARED)) {
        return true;
    }
    error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + (kind == QoreIRVerifyRelease::None
        ? " uses value " : " releases value ") + std::to_string(id)
        + (state == QORE_IR_VERIFY_CLEARED ? ", which is released" : ", whose object is released")
        + " on a path to it" + where(b, pos);
    return false;
}

bool QoreIRVerifyOwnership::applyStack(QoreIRVerifyState& s, const QoreIRInstruction& inst, uint32_t b, size_t pos) {
    // the two nested temp scopes of a PushTempMarkPair are opened as by two PushTempMarks, the outer one first (the
    // AOT debug IR stores it as these two instructions; the IR that qcc verifies before it writes it has the pair)
    if (inst.opcode == QoreIROpcode::PushTempMarkPair) {
        if (s.known[QORE_IR_VERIFY_TEMPS]) {
            s.stacks[QORE_IR_VERIFY_TEMPS].push_back(inst.temp_scope_id);
            s.stacks[QORE_IR_VERIFY_TEMPS].push_back(inst.unwind_inner_temp_scope_id);
        }
        return true;
    }
    // a catch block begins with its CatchException, which records the scopes open, and ends with a CatchCleanup, which
    // closes the catch blocks begun since its try statement began (its catch-exit depth, qore#6733) or the innermost one
    if (inst.opcode == QoreIROpcode::CatchException || inst.opcode == QoreIROpcode::CatchCleanup) {
        if (!s.known[QORE_IR_VERIFY_CATCHES]) {
            return true;
        }
        std::vector<uint32_t>& catches = s.stacks[QORE_IR_VERIFY_CATCHES];
        if (inst.opcode == QoreIROpcode::CatchException) {
            if (!s.known[QORE_IR_VERIFY_SCOPES]) {
                // the scopes are not known on this path, so the catch blocks are not either
                catches.clear();
                s.known[QORE_IR_VERIFY_CATCHES] = false;
                return true;
            }
            catches.push_back(static_cast<uint32_t>(s.stacks[QORE_IR_VERIFY_SCOPES].size()));
            return true;
        }
        auto depth = g.func.catch_exit_depths.find(&inst);
        if (depth != g.func.catch_exit_depths.end()) {
            if (depth->second > catches.size()) {
                error = "malformed IR: a CatchCleanup closes the catch blocks down to " + std::to_string(depth->second)
                    + ", but fewer are open on a path to it" + where(b, pos);
                return false;
            }
            catches.resize(depth->second);
            return true;
        }
        if (catches.empty()) {
            error = "malformed IR: a CatchCleanup closes a catch block, but none is open on a path to it"
                + where(b, pos);
            return false;
        }
        catches.pop_back();
        return true;
    }
    QoreIRVerifyStackEffect e = qore_ir_verify_stack_effect(inst);
    if (e.stack == QORE_IR_VERIFY_NUM_STACKS) {
        return true;
    }
    if (!s.known[e.stack]) {
        return true;
    }
    std::vector<uint32_t>& st = s.stacks[e.stack];
    if (e.push) {
        st.push_back(e.id);
        return true;
    }
    if (e.stack == QORE_IR_VERIFY_TEMPS && s.temps_relaxed) {
        auto i = std::find(st.rbegin(), st.rend(), e.id);
        if (i == st.rend()) {
            error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " closes a temp scope that is not open on "
                "the path of an exception to it" + where(b, pos);
            return false;
        }
        size_t idx = st.size() - (i - st.rbegin()) - 1;
        st.resize(e.keep ? idx + 1 : idx);
        return true;
    }
    // an implicit element is a value of the thread that a push saves and a pop restores: a pop restores the value from
    // before the push that it names, so it also closes the elements pushed after that one, as a return out of nested
    // loops that restores only the element of the outer one does; a pop of an element that is not open (closed
    // already, as by the handler of a loop on the path of a return out of it, qore#6643) restores an older value
    if (e.stack == QORE_IR_VERIFY_ELEMENTS && !e.keep) {
        auto i = std::find(st.rbegin(), st.rend(), e.id);
        if (i == st.rend()) {
            error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " restores an implicit element that is not "
                "open on every path to it" + where(b, pos);
            return false;
        }
        st.resize(st.size() - (i - st.rbegin()) - 1);
        return true;
    }
    if (st.empty() || st.back() != e.id) {
        error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " closes an entry of the "
            + qore_ir_verify_stack_names[e.stack] + " that is not the innermost one open on every path to it"
            + where(b, pos);
        return false;
    }
    if (!e.keep) {
        st.pop_back();
    }
    return true;
}

bool QoreIRVerifyOwnership::apply(QoreIRVerifyState& s, const QoreIRInstruction& inst, uint32_t b, size_t pos,
        EdgeKind mode) {
    if (inst.opcode == QoreIROpcode::Phi) {
        // the incoming values are checked on the edges into the block (see join())
        s.define(inst.result.id);
        return true;
    }
    QoreIRVerifyRelease kind;
    int released = qore_ir_verify_released_operand(inst, kind);
    const auto* next = qore_ir_verify_is_iterator_step(inst)
        ? qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(&inst) : nullptr;
    if (mode == EdgeException && qore_ir_verify_pop_without_exception_edge(inst)) {
        error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " has an exception target" + where(b, pos);
        return false;
    }
    if (inst.opcode == QoreIROpcode::LandingPad && pos != firstNonPhi(b)) {
        // a landing pad unwinds the scopes where the paths into its block meet (see verify())
        error = "malformed IR: a landing pad is not the first instruction of its block" + where(b, pos);
        return false;
    }
    if (mode == EdgeException && !next) {
        // the effects of the instruction that it has when it raises the exception: a release before the exception,
        // the end of a temp scope whose temporary's destructor raises it, the end of a scope whose handler raises
        // it (the interpreter and the JIT close the scope before its handlers run); the uses were checked on its
        // node, which starts with the state of the edge
        if (released >= 0 && static_cast<size_t>(released) < inst.operands.size()) {
            release(s, inst.operands[released].id, kind);
        }
        if (inst.opcode == QoreIROpcode::DiscardTemps || inst.opcode == QoreIROpcode::DiscardTempsKeep
                || inst.opcode == QoreIROpcode::ScopeExit) {
            if (!applyStack(s, inst, b, pos)) {
                return false;
            }
            // the exception that the handlers of the scope raise closes the catch blocks begun in it
            if (inst.opcode == QoreIROpcode::ScopeExit && s.known[QORE_IR_VERIFY_SCOPES]) {
                s.closeCatches(s.stacks[QORE_IR_VERIFY_SCOPES].size());
            }
        }
        return true;
    }
    // the uses: the operands and the values in fields of the instruction (qore_ir_visit_value_operands() visits the
    // operands first), but the released operand, and the ids of an lvalue path
    size_t index = 0;
    bool ok = true;
    qore_ir_visit_value_operands(inst, [&](QoreIRValue v) {
        size_t i = index++;
        if (ok && v.isValid() && static_cast<int>(i) != released) {
            ok = checkUse(s, v.id, inst, b, pos);
        }
    });
    if (!ok) {
        return false;
    }
    if (const auto* path = qore_ir_inst_cast<const QoreIRLValuePathInstruction*>(&inst)) {
        for (const LVPathStep& step : path->path) {
            if (step.operand_idx && step.operand_idx != UINT32_MAX && !checkUse(s, step.operand_idx, inst, b, pos)) {
                return false;
            }
            for (uint32_t id : step.slice_operand_ids) {
                if (id && id != UINT32_MAX && !checkUse(s, id, inst, b, pos)) {
                    return false;
                }
            }
        }
    }
    if (released >= 0 && static_cast<size_t>(released) < inst.operands.size()) {
        uint32_t id = inst.operands[released].id;
        if (!checkUse(s, id, inst, b, pos, kind)) {
            return false;
        }
        release(s, id, kind);
    }
    if (next) {
        // a step releases the values that the loop derived from the previous element, which only clears their slots
        for (const QoreIRValue& v : next->release_values) {
            if (v.isValid()) {
                if (!checkUse(s, v.id, inst, b, pos, QoreIRVerifyRelease::Slot)) {
                    return false;
                }
                release(s, v.id, QoreIRVerifyRelease::Slot);
            }
        }
        if (mode != EdgeNormal) {
            // the done edge, or an exception: a step of an iterator (other than of a list of selectors, which keeps
            // its iterator) releases the iterator and clears its slot, which a copy of the handle still points to
            if (inst.opcode == QoreIROpcode::IteratorNext && next->iterator.isValid()) {
                release(s, next->iterator.id, QoreIRVerifyRelease::Slot);
            }
            return true;
        }
    }
    if (!applyStack(s, inst, b, pos)) {
        return false;
    }
    if (inst.result.isValid()) {
        s.define(inst.result.id);
    }
    return true;
}

bool QoreIRVerifyOwnership::join(QoreIRVerifyState& to, const QoreIRVerifyState& from, uint32_t from_block,
        uint32_t to_block, bool& changed) {
    // the incoming values of the phis of the block for the edge
    const QoreIRBasicBlock* pred = g.func.blocks[from_block].get();
    const auto& insts = g.func.blocks[to_block]->instructions;
    for (size_t pos = 0; pos <= g.block_end[to_block]; ++pos) {
        const auto* phi = qore_ir_inst_cast<const QoreIRPhiInstruction*>(insts[pos].get());
        if (!phi) {
            break;
        }
        for (const QoreIRPhiIncoming& in : phi->incoming) {
            if (in.block == pred && in.value.isValid() && !checkUse(from, in.value.id, *phi, to_block, pos)) {
                return false;
            }
        }
    }
    changed = false;
    if (!to.reached) {
        to = from;
        changed = true;
        return true;
    }
    for (unsigned i = 0; i < QORE_IR_VERIFY_NUM_STACKS; ++i) {
        if (!from.known[i]) {
            continue;
        }
        if (!to.known[i]) {
            to.stacks[i] = from.stacks[i];
            to.known[i] = true;
            changed = true;
            continue;
        }
        if (to.stacks[i] != from.stacks[i]) {
            error = "malformed IR: the " + std::string(qore_ir_verify_stack_names[i]) + " open differ on the paths "
                "into block " + std::to_string(to_block) + " (from block " + std::to_string(from_block) + ")";
            return false;
        }
    }
    for (const auto& r : from.released) {
        uint8_t old = to.get(r.first);
        if (r.second > old) {
            to.raise(r.first, r.second);
            changed = true;
        }
    }
    return true;
}

bool QoreIRVerifyOwnership::verify() {
    buildNodes();
    buildGroups();
    const QoreIRFunction& func = g.func;
    std::vector<QoreIRVerifyState> in(g.num_nodes);
    std::vector<uint8_t> queued(g.num_nodes, 0);
    std::vector<uint32_t> work;
    uint32_t root = g.block_entry[0];
    in[root].reached = true;
    work.push_back(root);
    queued[root] = 1;
    // the landing pad at the start of a block unwinds the scopes of the frame to its depth (as the interpreter's
    // LandingPad does) before the paths into the block meet
    auto landingPadDepth = [&](uint32_t b) -> size_t {
        for (const auto& inst : func.blocks[b]->instructions) {
            if (inst->opcode == QoreIROpcode::Phi) {
                continue;
            }
            if (const auto* lp = qore_ir_inst_cast<const QoreIRLandingPadInstruction*>(inst.get())) {
                return lp->scope_depth;
            }
            break;
        }
        return SIZE_MAX;
    };
    while (!work.empty()) {
        if (cancelled()) {
            return false;
        }
        uint32_t n = work.back();
        work.pop_back();
        queued[n] = 0;
        uint32_t b = node_block[n];
        const auto& insts = func.blocks[b]->instructions;
        QoreIRVerifyState s = in[n];
        // the nodes of the block after its entry have the previous node as their only predecessor: the block is
        // walked from its entry, and only the entries of the blocks keep a state
        // the edges that leave the node before an instruction that raises an exception or before a step take the
        // state at the end of the node
        auto follow = [&](const QoreIRVerifyState& state, uint32_t to_block) -> bool {
            QoreIRVerifyState es = state;
            size_t depth = landingPadDepth(to_block);
            if (depth != SIZE_MAX && es.known[QORE_IR_VERIFY_SCOPES]) {
                std::vector<uint32_t>& scopes = es.stacks[QORE_IR_VERIFY_SCOPES];
                if (scopes.size() < depth) {
                    error = "malformed IR: the landing pad of block " + std::to_string(to_block) + " unwinds to "
                        + std::to_string(depth) + " scopes, but fewer are open on a path into it (from block "
                        + std::to_string(b) + ")";
                    return false;
                }
                scopes.resize(depth);
                es.closeCatches(depth);
            }
            uint32_t to = g.block_entry[to_block];
            bool changed;
            if (!join(in[to], es, b, to_block, changed)) {
                return false;
            }
            if (changed && !queued[to]) {
                queued[to] = 1;
                work.push_back(to);
            }
            return true;
        };
        for (;; ++n) {
            for (uint32_t pos = node_begin[n]; pos < node_end[n]; ++pos) {
                if (!apply(s, *insts[pos], b, pos, EdgeNormal)) {
                    return false;
                }
            }
            for (const Edge& e : edges[n]) {
                if (e.kind == EdgeNormal) {
                    if (!follow(s, e.to_block)) {
                        return false;
                    }
                    continue;
                }
                QoreIRVerifyState es = s;
                es.reached = true;
                if (!apply(es, *e.inst, b, e.pos, e.kind)) {
                    return false;
                }
                if (e.kind == EdgeException) {
                    // the entries that an exception leaves open are not known from IR read from a module: the
                    // temp scopes that it leaves are those opened after the temp scope of its handler, which the IR
                    // does not give (the temp scope ids are not written), and the lowering leaves the instructions
                    // of a return out of a loop with the handler of the loop's element, which closes again the
                    // scope and the implicit element that the return closed (qore#6643); the stacks are checked on
                    // the paths of the normal control flow, and on the paths from an exception edge once they meet
                    // one.  A landing pad unwinds the scopes to its depth: they are known after it (see follow())
                    for (unsigned i = 0; i < QORE_IR_VERIFY_NUM_STACKS; ++i) {
                        if (i != QORE_IR_VERIFY_SCOPES || landingPadDepth(e.to_block) == SIZE_MAX) {
                            es.stacks[i].clear();
                            es.known[i] = false;
                        }
                    }
                }
                if (!follow(es, e.to_block)) {
                    return false;
                }
            }
            // the next node of the block
            if (!(n + 1 < g.num_nodes && node_end[n] <= g.block_end[b] && node_block[n + 1] == b
                    && n + 1 != g.block_entry[b])) {
                break;
            }
        }
    }
    return verifyExceptionPaths(in);
}

//! Follows the paths of the exceptions with the stacks that each raise point has (qore#6658)
/** The dataflow of verify() leaves the stacks of a path from an exception edge unknown until it meets a path of the
    normal control flow, as the handler blocks are entered from raise points with different stacks (deeper scopes, an
    implicit element pushed or not): a state per block cannot hold them.  Each raise point in a block that a normal
    path reaches (whose stacks are known) is followed here with its own stacks through the blocks that only exceptions
    reach (their landing pads, their pops and scope exits, the branches between them), up to a block that a normal
    path reaches, whose stacks must then be those of the path: a handler that closes again a scope or an implicit
    element that the path has closed (qore#6643) is refused at its instruction, and so is a path that meets the normal
    control flow with other stacks.  The temp scopes are not followed: an exception drains them to the temp scope of
    its handler, which IR read from a module does not give.  Each block is walked once for each set of stacks that
    reaches it, which are few in lowered code (one per nesting of the raise points that share a handler).
*/
bool QoreIRVerifyOwnership::exceptionTemps(QoreIRVerifyState& es, const QoreIRInstruction& inst, uint32_t b,
        size_t pos) {
    std::vector<uint32_t>& temps = es.stacks[QORE_IR_VERIFY_TEMPS];
    // the temp scope that the exception drains to: that of the raising instruction, or, if the temp scope of its
    // handler is outside it, the first one opened after the handler's, as the IR interpreter computes it
    // (qore_ir_exception_branch_temp_scope()); the end of a temp scope that raises has closed its own already
    bool scope_closed = inst.opcode == QoreIROpcode::DiscardTemps || inst.opcode == QoreIROpcode::DiscardTempsKeep;
    uint32_t scope_id = scope_closed ? 0 : inst.temp_scope_id;
    uint32_t drain_to = scope_id;
    QoreIRBasicBlock* target = qore_ir_verify_exception_target(inst);
    if (target && (scope_id || scope_closed)) {
        uint32_t handler_scope_id = target->getHandlerTempScopeId();
        if (handler_scope_id && handler_scope_id != scope_id) {
            size_t i = temps.size();
            if (!scope_closed) {
                while (i > 0 && temps[i - 1] != scope_id) {
                    --i;
                }
            }
            for (; i > 1; --i) {
                if (temps[i - 2] == handler_scope_id) {
                    drain_to = temps[i - 1];
                    break;
                }
            }
        }
    }
    if (!drain_to) {
        return true;
    }
    // the temporaries after the mark of that scope are released, and its mark is kept (see temps_relaxed); for a
    // scope that is not open the branch releases nothing, as the interpreter's (the scope of the element of an
    // iterator that raises before the element's scope is opened): the end of the enclosing statement releases them
    auto i = std::find(temps.rbegin(), temps.rend(), drain_to);
    if (i != temps.rend()) {
        temps.resize(temps.size() - (i - temps.rbegin()));
    }
    (void)b;
    (void)pos;
    return true;
}

bool QoreIRVerifyOwnership::verifyExceptionPaths(const std::vector<QoreIRVerifyState>& in) {
    const QoreIRFunction& func = g.func;
    struct Item {
        uint32_t block;
        QoreIRVerifyState state;
        //! the raise point that the path comes from
        uint32_t from_block;
        uint32_t from_pos;
    };
    std::vector<Item> work;
    std::unordered_set<std::string> seen;
    auto key = [](uint32_t b, const QoreIRVerifyState& st) -> std::string {
        std::string k = std::to_string(b);
        for (unsigned i = 0; i < QORE_IR_VERIFY_NUM_STACKS; ++i) {
            k += st.known[i] ? '|' : '?';
            for (uint32_t id : st.stacks[i]) {
                k += std::to_string(id);
                k += ',';
            }
        }
        return k;
    };
    auto normalBlock = [&](uint32_t b) -> bool {
        const QoreIRVerifyState& st = in[g.block_entry[b]];
        return st.reached && st.known[QORE_IR_VERIFY_SCOPES];
    };
    // the state of an exception edge: the stacks are those of the raise point, but the temp scopes, which the
    // exception drains to its handler's
    auto exceptionState = [&](const QoreIRVerifyState& at, const QoreIRInstruction& inst, uint32_t b,
            uint32_t pos, QoreIRVerifyState& es) -> bool {
        es = at;
        es.reached = true;
        if (!apply(es, inst, b, pos, EdgeException)) {
            return false;
        }
        es.temps_relaxed = true;
        if (func.temp_scope_ids_unknown || !es.known[QORE_IR_VERIFY_TEMPS]) {
            // IR read from an image that does not carry the temp scopes of its instructions
            es.stacks[QORE_IR_VERIFY_TEMPS].clear();
            es.known[QORE_IR_VERIFY_TEMPS] = false;
            return true;
        }
        return exceptionTemps(es, inst, b, pos);
    };
    auto prefix = [&](uint32_t from_block, uint32_t from_pos) {
        error = "on the path of the exception of block " + std::to_string(from_block) + ", instruction "
            + std::to_string(from_pos) + ": " + error;
    };
    // walks the nodes of a block from its entry with the state, calling raise() for each exception edge and edge()
    // for each other edge
    auto walk = [&](uint32_t b, QoreIRVerifyState s, auto&& raise, auto&& edge) -> bool {
        for (uint32_t n = g.block_entry[b];; ++n) {
            const auto& insts = func.blocks[b]->instructions;
            for (uint32_t pos = node_begin[n]; pos < node_end[n]; ++pos) {
                if (!apply(s, *insts[pos], b, pos, EdgeNormal)) {
                    return false;
                }
            }
            for (const Edge& e : edges[n]) {
                if (e.kind == EdgeException) {
                    if (!raise(s, e)) {
                        return false;
                    }
                    continue;
                }
                QoreIRVerifyState es = s;
                if (e.kind == EdgeDone && !apply(es, *e.inst, b, e.pos, EdgeDone)) {
                    return false;
                }
                if (!edge(es, e)) {
                    return false;
                }
            }
            if (!(n + 1 < g.num_nodes && node_end[n] <= g.block_end[b] && node_block[n + 1] == b
                    && n + 1 != g.block_entry[b])) {
                break;
            }
        }
        return true;
    };
    // the raise points of the blocks that a normal path reaches
    for (uint32_t b = 0; b < func.blocks.size(); ++b) {
        if (!g.reachable[b] || !normalBlock(b)) {
            continue;
        }
        bool ok = walk(b, in[g.block_entry[b]], [&](const QoreIRVerifyState& s, const Edge& e) -> bool {
            QoreIRVerifyState es;
            if (!exceptionState(s, *e.inst, b, e.pos, es)) {
                return false;
            }
            work.push_back({e.to_block, std::move(es), b, e.pos});
            return true;
        }, [](const QoreIRVerifyState&, const Edge&) -> bool {
            return true;
        });
        if (!ok) {
            return false;
        }
    }
    while (!work.empty()) {
        if (cancelled()) {
            return false;
        }
        Item item = std::move(work.back());
        work.pop_back();
        uint32_t b = item.block;
        QoreIRVerifyState s = std::move(item.state);
        // the landing pad at the start of the block unwinds the scopes to its depth
        for (const auto& inst : func.blocks[b]->instructions) {
            if (inst->opcode == QoreIROpcode::Phi) {
                continue;
            }
            if (const auto* lp = qore_ir_inst_cast<const QoreIRLandingPadInstruction*>(inst.get())) {
                std::vector<uint32_t>& scopes = s.stacks[QORE_IR_VERIFY_SCOPES];
                if (s.known[QORE_IR_VERIFY_SCOPES] && scopes.size() < lp->scope_depth) {
                    error = "malformed IR: the landing pad of block " + std::to_string(b) + " unwinds to "
                        + std::to_string(lp->scope_depth) + " scopes, but fewer are open on the path";
                    prefix(item.from_block, item.from_pos);
                    return false;
                }
                if (scopes.size() > lp->scope_depth) {
                    scopes.resize(lp->scope_depth);
                }
                s.closeCatches(lp->scope_depth);
            }
            break;
        }
        if (normalBlock(b)) {
            // the path meets the normal control flow: its stacks must be those of the block
            const QoreIRVerifyState& ns = in[g.block_entry[b]];
            for (unsigned i = 0; i < QORE_IR_VERIFY_NUM_STACKS; ++i) {
                if (!s.known[i] || !ns.known[i]) {
                    continue;
                }
                // the temp scopes may be followed by marks that the exceptions left, which the end of the enclosing
                // statement removes (see temps_relaxed)
                if (i == QORE_IR_VERIFY_TEMPS
                        ? (s.stacks[i].size() < ns.stacks[i].size()
                            || !std::equal(ns.stacks[i].begin(), ns.stacks[i].end(), s.stacks[i].begin()))
                        : s.stacks[i] != ns.stacks[i]) {
                    error = "malformed IR: the " + std::string(qore_ir_verify_stack_names[i]) + " open differ from "
                        "those of the normal paths into block " + std::to_string(b);
                    prefix(item.from_block, item.from_pos);
                    return false;
                }
            }
            continue;
        }
        if (!seen.insert(key(b, s)).second) {
            continue;
        }
        uint32_t from_block = item.from_block, from_pos = item.from_pos;
        bool ok = walk(b, std::move(s), [&](const QoreIRVerifyState& at, const Edge& e) -> bool {
            QoreIRVerifyState es;
            if (!exceptionState(at, *e.inst, b, e.pos, es)) {
                prefix(from_block, from_pos);
                return false;
            }
            // a raise point of a handler block: its exception continues the path
            work.push_back({e.to_block, std::move(es), from_block, from_pos});
            return true;
        }, [&](const QoreIRVerifyState& at, const Edge& e) -> bool {
            work.push_back({e.to_block, at, from_block, from_pos});
            return true;
        });
        if (!ok) {
            if (error.find("on the path of the exception") == std::string::npos) {
                prefix(from_block, from_pos);
            }
            return false;
        }
    }
    return true;
}
}

bool QoreIRVerifier::verifyModuleIR(const QoreIRFunction& func, std::string& error) {
    const size_t num_blocks = func.blocks.size();
    if (!num_blocks || !func.blocks[0]) {
        error = "malformed IR: the function has no entry block";
        return false;
    }
    size_t check_count = 0;
    auto cancelled = [&]() -> bool {
        if (!(++check_count % 1000) && qore_check_cancel(nullptr, "AOT IR verification")) {
            error = "AOT IR verification cancelled";
            return true;
        }
        return false;
    };
    std::unordered_map<const QoreIRBasicBlock*, uint32_t> block_index;
    block_index.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        if (!func.blocks[i] || !block_index.emplace(func.blocks[i].get(), static_cast<uint32_t>(i)).second) {
            error = "malformed IR: block " + std::to_string(i) + " is missing or appears more than once";
            return false;
        }
    }
    auto where = [&](uint32_t b, size_t pos) -> std::string {
        return " (block " + std::to_string(b) + ", instruction " + std::to_string(pos) + ")";
    };
    // the index of a block that an instruction refers to; the reader refuses an index beyond the blocks, a block of
    // another function is refused here
    auto targetIndex = [&](const QoreIRBasicBlock* target, uint32_t b, size_t pos, uint32_t& idx) -> bool {
        auto it = block_index.find(target);
        if (it == block_index.end()) {
            error = "malformed IR: a branch target is not a block of the function" + where(b, pos);
            return false;
        }
        idx = it->second;
        return true;
    };

    // the blocks that can run, and their structure: not empty, the phis first, the only terminator at the end
    std::vector<uint8_t> reachable(num_blocks, 0);
    std::vector<uint32_t> work = {0};
    reachable[0] = 1;
    // the position of the terminator of each block that can run, which ends it
    std::vector<uint32_t> block_end(num_blocks, 0);
    while (!work.empty()) {
        if (cancelled()) {
            return false;
        }
        uint32_t b = work.back();
        work.pop_back();
        const QoreIRBasicBlock& block = *func.blocks[b];
        const auto& insts = block.instructions;
        if (insts.empty()) {
            error = "malformed IR: block " + std::to_string(b) + " has no instructions";
            return false;
        }
        // the block runs to its first terminator; the instructions after it never run, as the interpreter and the
        // JIT leave the block at the terminator (the lowering leaves code after a throw or a return in its block),
        // and are not checked: their values are not defined for the instructions that run
        size_t end_pos = insts.size();
        for (size_t pos = 0; pos < insts.size(); ++pos) {
            const OpcodeInfo* oi = getOpcodeInfo(static_cast<uint16_t>(insts[pos]->opcode));
            if (oi && oi->is_terminator) {
                end_pos = pos;
                break;
            }
        }
        if (end_pos == insts.size()) {
            error = "malformed IR: block " + std::to_string(b) + " has no terminator";
            return false;
        }
        block_end[b] = static_cast<uint32_t>(end_pos);
        bool phis = true;
        for (size_t pos = 0; pos <= end_pos; ++pos) {
            const QoreIRInstruction& inst = *insts[pos];
            bool terminator = pos == end_pos;
            // a simulated error, which the lowering does not give and which continues in its block as no other
            // terminator does
            if (inst.opcode == QoreIROpcode::InvokeSimError) {
                error = "malformed IR: InvokeSimError is not valid in IR read from a module" + where(b, pos);
                return false;
            }
            if (inst.opcode == QoreIROpcode::Phi) {
                if (!phis) {
                    error = "malformed IR: a phi follows another instruction" + where(b, pos);
                    return false;
                }
                const auto* phi = qore_ir_inst_cast<const QoreIRPhiInstruction*>(&inst);
                if (!phi || static_cast<uint8_t>(phi->value_kind) > static_cast<uint8_t>(QoreIRPhiValueKind::NativeBool)) {
                    error = "malformed IR: a phi has an invalid value kind" + where(b, pos);
                    return false;
                }
            } else {
                phis = false;
            }
            bool ok = true;
            auto add = [&](QoreIRBasicBlock* target) {
                uint32_t idx;
                if (!ok || !target) {
                    return;
                }
                if (!targetIndex(target, b, pos, idx)) {
                    ok = false;
                    return;
                }
                if (!reachable[idx]) {
                    reachable[idx] = 1;
                    work.push_back(idx);
                }
            };
            if (QoreIRBasicBlock* target = qore_ir_verify_exception_target(inst)) {
                add(target);
            }
            if (terminator) {
                qore_ir_visit_successors(inst, add);
            }
            if (!ok) {
                return false;
            }
        }
    }

    // the nodes: each block that can run starts with an empty node, the point at which it is entered; each instruction
    // that can raise an exception starts a node, and its exception edge leaves from the node before it, at which the
    // values defined by it and after it are not defined yet; a step of an iterator starts a node too, for its done
    // edge, on which its element is not defined
    std::vector<uint32_t> block_entry(num_blocks, UINT32_MAX);
    // the node of each instruction, by block
    std::vector<std::vector<uint32_t>> inst_node(num_blocks);
    std::vector<QoreIRVerifyNode> nodes;
    for (uint32_t b = 0; b < num_blocks; ++b) {
        if (!reachable[b]) {
            continue;
        }
        const auto& insts = func.blocks[b]->instructions;
        block_entry[b] = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();
        uint32_t current = block_entry[b];
        inst_node[b].resize(block_end[b] + 1);
        for (size_t pos = 0; pos <= block_end[b]; ++pos) {
            if (pos == 0 || qore_ir_verify_exception_target(*insts[pos]) || qore_ir_verify_is_iterator_step(*insts[pos])) {
                uint32_t next = static_cast<uint32_t>(nodes.size());
                nodes.emplace_back();
                nodes[current].succ.push_back(next);
                current = next;
            }
            inst_node[b][pos] = current;
        }
    }
    // the edges between blocks: from the last node of a block to its successors (from the node before a step of an
    // iterator to its done target), and from the node before an instruction that can raise an exception to its
    // exception target; the nodes from which each block reaches each of its successors are kept for the phis
    std::unordered_map<uint64_t, std::vector<uint32_t>> edge_nodes;
    std::vector<std::vector<uint32_t>> block_preds(num_blocks);
    auto edge = [&](uint32_t from, uint32_t from_block, uint32_t to_block) {
        nodes[from].succ.push_back(block_entry[to_block]);
        std::vector<uint32_t>& en = edge_nodes[(static_cast<uint64_t>(from_block) << 32) | to_block];
        if (en.empty()) {
            block_preds[to_block].push_back(from_block);
        }
        en.push_back(from);
    };
    for (uint32_t b = 0; b < num_blocks; ++b) {
        if (!reachable[b]) {
            continue;
        }
        const auto& insts = func.blocks[b]->instructions;
        for (size_t pos = 0; pos <= block_end[b]; ++pos) {
            if (QoreIRBasicBlock* target = qore_ir_verify_exception_target(*insts[pos])) {
                uint32_t node = inst_node[b][pos];
                // the node before the instruction: the entry node of the block for the first instruction
                edge(node - 1, b, block_index[target]);
            }
        }
        const QoreIRInstruction& last = *insts[block_end[b]];
        if (const auto* next = qore_ir_verify_is_iterator_step(last)
                ? qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(&last) : nullptr) {
            edge(inst_node[b].back(), b, block_index[next->continue_target]);
            edge(inst_node[b].back() - 1, b, block_index[next->done_target]);
        } else {
            qore_ir_visit_successors(last, [&](QoreIRBasicBlock* target) {
                edge(inst_node[b].back(), b, block_index[target]);
            });
        }
    }
    const uint32_t num_nodes = static_cast<uint32_t>(nodes.size());
    for (uint32_t n = 0; n < num_nodes; ++n) {
        for (uint32_t s : nodes[n].succ) {
            nodes[s].pred.push_back(n);
        }
    }

    // the dominators (Cooper, Harvey and Kennedy, "A Simple, Fast Dominance Algorithm"), in reverse postorder from the
    // entry node of the entry block
    std::vector<uint32_t> rpo_number(num_nodes, UINT32_MAX);
    std::vector<uint32_t> order;
    order.reserve(num_nodes);
    {
        std::vector<uint8_t> visited(num_nodes, 0);
        std::vector<std::pair<uint32_t, size_t>> stack = {{block_entry[0], 0}};
        visited[block_entry[0]] = 1;
        while (!stack.empty()) {
            auto& top = stack.back();
            if (top.second < nodes[top.first].succ.size()) {
                uint32_t s = nodes[top.first].succ[top.second++];
                if (!visited[s]) {
                    visited[s] = 1;
                    stack.push_back({s, 0});
                }
            } else {
                order.push_back(top.first);
                stack.pop_back();
            }
        }
        std::reverse(order.begin(), order.end());
        for (uint32_t i = 0; i < order.size(); ++i) {
            rpo_number[order[i]] = i;
        }
    }
    std::vector<uint32_t> idom(num_nodes, UINT32_MAX);
    const uint32_t root = block_entry[0];
    idom[root] = root;
    auto intersect = [&](uint32_t a, uint32_t b) {
        while (a != b) {
            while (rpo_number[a] > rpo_number[b]) {
                a = idom[a];
            }
            while (rpo_number[b] > rpo_number[a]) {
                b = idom[b];
            }
        }
        return a;
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (uint32_t i = 1; i < order.size(); ++i) {
            if (cancelled()) {
                return false;
            }
            uint32_t n = order[i];
            uint32_t new_idom = UINT32_MAX;
            for (uint32_t p : nodes[n].pred) {
                if (idom[p] == UINT32_MAX) {
                    continue;
                }
                new_idom = new_idom == UINT32_MAX ? p : intersect(p, new_idom);
            }
            if (new_idom != idom[n]) {
                idom[n] = new_idom;
                changed = true;
            }
        }
    }
    // the preorder and postorder numbers of the dominator tree, for constant-time dominance queries
    std::vector<uint32_t> dom_pre(num_nodes, 0), dom_post(num_nodes, 0);
    {
        std::vector<std::vector<uint32_t>> children(num_nodes);
        for (uint32_t n : order) {
            if (n != root) {
                children[idom[n]].push_back(n);
            }
        }
        uint32_t counter = 0;
        std::vector<std::pair<uint32_t, size_t>> stack = {{root, 0}};
        dom_pre[root] = counter++;
        while (!stack.empty()) {
            auto& top = stack.back();
            if (top.second < children[top.first].size()) {
                uint32_t c = children[top.first][top.second++];
                dom_pre[c] = counter++;
                stack.push_back({c, 0});
            } else {
                dom_post[top.first] = counter++;
                stack.pop_back();
            }
        }
    }
    auto dominates = [&](uint32_t a, uint32_t b) {
        return dom_pre[a] <= dom_pre[b] && dom_post[b] <= dom_post[a];
    };

    // the definitions of the values: their block and position; the reader has checked that each is defined once
    struct Def {
        uint32_t block;
        uint32_t pos;
    };
    std::unordered_map<uint32_t, Def> defs;
    // the containers that the function changes (see qore_ir_verify_container_definers())
    std::unordered_set<uint32_t> changed_containers;
    for (uint32_t b = 0; b < num_blocks; ++b) {
        if (!reachable[b]) {
            continue;
        }
        const auto& insts = func.blocks[b]->instructions;
        for (size_t pos = 0; pos <= block_end[b]; ++pos) {
            if (insts[pos]->result.isValid()) {
                defs[insts[pos]->result.id] = {b, static_cast<uint32_t>(pos)};
            }
            if (qore_ir_verify_container_definers(insts[pos]->opcode) && !insts[pos]->operands.empty()) {
                changed_containers.insert(insts[pos]->operands[0].id);
            }
        }
    }
    // a value is available at an instruction (pos < SIZE_MAX) or at the end of a node of a block if its definition
    // dominates it
    auto definedBefore = [&](uint32_t id, uint32_t node, uint32_t b, size_t pos) -> bool {
        auto it = defs.find(id);
        if (it == defs.end()) {
            return false;
        }
        const Def& d = it->second;
        uint32_t def_node = inst_node[d.block][d.pos];
        if (def_node == node) {
            return d.block == b && d.pos < pos;
        }
        return dominates(def_node, node);
    };
    auto useError = [&](uint32_t id, const QoreIRInstruction& inst, uint32_t b, size_t pos) {
        error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " uses value " + std::to_string(id)
            + (defs.count(id) ? ", which is not defined on every path to it" : ", which no instruction that can run "
                "defines") + where(b, pos);
    };

    for (uint32_t b = 0; b < num_blocks; ++b) {
        if (!reachable[b]) {
            continue;
        }
        const auto& insts = func.blocks[b]->instructions;
        for (size_t pos = 0; pos <= block_end[b]; ++pos) {
            if (cancelled()) {
                return false;
            }
            const QoreIRInstruction& inst = *insts[pos];
            uint32_t node = inst_node[b][pos];
            if (const auto* phi = qore_ir_inst_cast<const QoreIRPhiInstruction*>(&inst)) {
                // each predecessor that can run has an incoming value
                for (uint32_t p : block_preds[b]) {
                    bool found = false;
                    for (const QoreIRPhiIncoming& in : phi->incoming) {
                        if (in.block == func.blocks[p].get()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        error = "malformed IR: a phi has no incoming value from its predecessor block "
                            + std::to_string(p) + where(b, pos);
                        return false;
                    }
                }
                // the incoming value of a predecessor is available at the end of each edge from it to this block
                for (const QoreIRPhiIncoming& in : phi->incoming) {
                    auto pit = block_index.find(in.block);
                    if (pit == block_index.end()) {
                        error = "malformed IR: a phi has an incoming value from a block of no function" + where(b, pos);
                        return false;
                    }
                    uint32_t p = pit->second;
                    auto eit = edge_nodes.find((static_cast<uint64_t>(p) << 32) | b);
                    if (eit == edge_nodes.end()) {
                        // an incoming value of a block that is not a predecessor is never taken, but the JIT refuses
                        // it: it is refused here too, unless the block cannot run
                        if (reachable[p]) {
                            error = "malformed IR: a phi has an incoming value from block " + std::to_string(p)
                                + ", which is not a predecessor of its block" + where(b, pos);
                            return false;
                        }
                        continue;
                    }
                    for (uint32_t en : eit->second) {
                        if (!in.value.isValid() || !definedBefore(in.value.id, en, p, SIZE_MAX)) {
                            error = "malformed IR: a phi has incoming value " + std::to_string(in.value.id)
                                + " from block " + std::to_string(p) + ", which is not defined at the end of that "
                                "block" + where(b, pos);
                            return false;
                        }
                    }
                }
                continue;
            }
            bool ok = true;
            auto use = [&](uint32_t id) {
                if (ok && id && id != UINT32_MAX && !definedBefore(id, node, b, pos)) {
                    useError(id, inst, b, pos);
                    ok = false;
                }
            };
            // the temporary that ReleaseOwnedTemp releases (its first operand) is lowered on the paths that leave
            // its statement, also on those on which it has not been created: on such a path the interpreter finds no
            // cleanup entry for it and the JIT finds no reference in its slot (cleared at the entry of the function
            // and by each release), so the release does nothing.  It must still be defined by an instruction that
            // can run; its other operand, and the operands of every other instruction, must be defined on every path
            // to their use.  That a released value is not used again is verified with the ownership of the values
            // a value that is released if the frame holds it, on a path on which it may not have been defined
            auto releasedMaybe = [&](uint32_t id) -> bool {
                auto it = defs.find(id);
                if (it == defs.end()) {
                    error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " releases value "
                        + std::to_string(id) + ", which no instruction that can run defines" + where(b, pos);
                    return false;
                }
                return true;
            };
            size_t first_operand = 0;
            if (inst.opcode == QoreIROpcode::ReleaseOwnedTemp && !inst.operands.empty()) {
                if (!releasedMaybe(inst.operands[0].id)) {
                    return false;
                }
                first_operand = 1;
            }
            for (size_t i = first_operand; i < inst.operands.size(); ++i) {
                if (inst.operands[i].isValid()) {
                    use(inst.operands[i].id);
                }
            }
            // the values that the instruction holds in fields of its own (qore_ir_visit_value_operands() visits the
            // operands first)
            size_t index = 0;
            qore_ir_visit_value_operands(inst, [&](QoreIRValue v) {
                if (index++ >= inst.operands.size() && v.isValid()) {
                    use(v.id);
                }
            });
            if (const auto* next = qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(&inst);
                    next && inst.opcode != QoreIROpcode::IteratorNext && inst.opcode != QoreIROpcode::IteratorNextSelectors
                    && (inst.operands.size() != 3 || next->iterator.id != inst.operands[0].id
                        || next->index.id != inst.operands[1].id || next->limit.id != inst.operands[2].id)) {
                // the interpreter and the JIT read the list, the index and the limit of a step of a typed foreach from
                // fields of its own, which are its operands
                error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " has a list, index or limit that is not "
                    "its operand" + where(b, pos);
                return false;
            }
            if (const auto* next = qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(&inst)) {
                // the values that a step of an iterator releases are those that the loop derived from the previous
                // element, which the first step has not defined yet: they are released as ReleaseOwnedTemp releases
                // its temporary (above), by the interpreter and by the JIT alike
                for (const QoreIRValue& v : next->release_values) {
                    if (v.isValid() && !releasedMaybe(v.id)) {
                        return false;
                    }
                }
            } else if (const auto* path = qore_ir_inst_cast<const QoreIRLValuePathInstruction*>(&inst)) {
                for (const LVPathStep& step : path->path) {
                    use(step.operand_idx);
                    for (uint32_t id : step.slice_operand_ids) {
                        use(id);
                    }
                }
            }
            if (!ok) {
                return false;
            }
            // the container that the instruction changes is one that the function creates, of the type that the
            // interpreter and the JIT take it as without a check
            if (const std::vector<QoreIROpcode>* definers = qore_ir_verify_container_definers(inst.opcode)) {
                auto it = inst.operands.empty() ? defs.end() : defs.find(inst.operands[0].id);
                const QoreIRInstruction* def = it == defs.end() ? nullptr
                    : func.blocks[it->second.block]->instructions[it->second.pos].get();
                if (!def || std::find(definers->begin(), definers->end(), def->opcode) == definers->end()) {
                    error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " changes a container that is not "
                        "created by an instruction that creates its type" + where(b, pos);
                    return false;
                }
            }
            // the JIT reads the entries of the list of these instructions through the address of its entries, which
            // it takes once (with the list's size, see QoreIRToLLVM::getDirectListReadBound()): the list is one that
            // the function does not change, which could move its entries (the lowering reads the source of a map, a
            // select or a foreach, and changes only the list that it creates for the result)
            if (!changed_containers.empty()) {
                QoreIRValue read_list;
                switch (inst.opcode) {
                    case QoreIROpcode::ListGetInt:
                    case QoreIROpcode::ListGetFloat:
                    case QoreIROpcode::ListGetValueNoRefUnchecked:
                        read_list = inst.operands.empty() ? QoreIRValue() : inst.operands[0];
                        break;
                    case QoreIROpcode::TypedForeachNextInt:
                    case QoreIROpcode::TypedForeachNextFloat:
                    case QoreIROpcode::TypedForeachNextBool:
                    case QoreIROpcode::TypedForeachNextString:
                        read_list = static_cast<const QoreIRIteratorNextInstruction&>(inst).iterator;
                        break;
                    default:
                        break;
                }
                if (read_list.isValid() && changed_containers.count(read_list.id)) {
                    error = "malformed IR: " + qore_ir_verify_opcode_name(inst) + " reads the entries of a list that "
                        "the function changes" + where(b, pos);
                    return false;
                }
            }
        }
    }

    // the ownership of the values and the stacks of the frame and of the thread
    QoreIRVerifyGraph graph{func, block_index, reachable, block_end, block_entry, inst_node, num_nodes};
    return QoreIRVerifyOwnership(graph, error).verify();
}

//! Checks the paths of the debugger's flow requests at the step events of a function (QoreIRDebugFlowTargets)
/** Each path is entered from its step event only: a block of a path of its own (QoreIRBasicBlock::debug_flow) that
    no other edge enters and that has no Phi, so that the state of the function at the event is its only input
*/
static bool verifyDebugFlowTargets(const QoreIRFunction& func,
        const std::unordered_set<const QoreIRBasicBlock*>& block_set, std::string& error) {
    std::unordered_map<const QoreIRBasicBlock*, const QoreIRBasicBlock*> entries;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            const QoreIRDebugFlowTargets* flow = qore_ir_debug_flow_targets(inst.get());
            if (!flow) {
                continue;
            }
            for (const QoreIRBasicBlock* target : flow->targets()) {
                if (!target) {
                    continue;
                }
                if (!block_set.count(target) || !target->debug_flow || target->instructions.empty()
                        || target->instructions.front()->opcode == QoreIROpcode::Phi) {
                    error = "invalid debugger flow path '" + target->name + "' in block '" + block->name + "'";
                    return false;
                }
                if (!entries.emplace(target, block.get()).second) {
                    error = "debugger flow path '" + target->name + "' entered by more than one step event";
                    return false;
                }
            }
        }
    }
    if (entries.empty()) {
        return true;
    }
    // no other edge enters an entry block
    for (const auto& block : func.blocks) {
        if (block->instructions.empty()) {
            continue;
        }
        bool bad = false;
        qore_ir_visit_successors(*block->instructions.back(), [&](QoreIRBasicBlock* successor) {
            if (entries.count(successor)) {
                bad = true;
            }
        });
        for (const auto& inst : block->instructions) {
            if (inst->exception_target && entries.count(inst->exception_target)) {
                bad = true;
            }
        }
        if (bad) {
            error = "debugger flow path in block '" + block->name + "' entered by another edge";
            return false;
        }
    }
    return true;
}

bool QoreIRVerifier::verify(const QoreIRFunction& func, std::string& error) {
    const std::string& registry_error = getOpcodeRegistryValidationError();
    if (!registry_error.empty()) {
        error = registry_error;
        return false;
    }

    if (func.blocks.empty()) {
        error = "function has no basic blocks";
        return false;
    }

    // Collect branch targets to identify reachable blocks
    std::unordered_set<const QoreIRBasicBlock*> reachable = collectBranchTargets(func);

    std::unordered_set<const QoreIRBasicBlock*> block_set;
    for (const auto& block : func.blocks) {
        if (!block_set.insert(block.get()).second) {
            error = "duplicate basic block pointer";
            return false;
        }
        // Skip unreachable blocks - they may be empty/unterminated dead code
        if (reachable.find(block.get()) == reachable.end()) {
            continue;
        }
        if (block->instructions.empty()) {
            error = "basic block '" + block->name + "' has no instructions";
            return false;
        }
        const QoreIRInstruction* last = block->instructions.back().get();
        if (!isTerminator(last->opcode)) {
            error = "basic block '" + block->name + "' missing terminator";
            if (QORE_ENV_IS_SET("QORE_AOT_DEBUG")) {
                fprintf(stderr, "VERIFIER: block '%s' has %zu instructions, last opcode=%d\n",
                    block->name.c_str(), block->instructions.size(), static_cast<int>(last->opcode));
                for (size_t i = 0; i < block->instructions.size(); ++i) {
                    fprintf(stderr, "  inst[%zu]: opcode=%d\n", i,
                        static_cast<int>(block->instructions[i]->opcode));
                }
                // Also dump all blocks
                for (const auto& b : func.blocks) {
                    fprintf(stderr, "BLOCK '%s' (%zu instructions):", b->name.c_str(), b->instructions.size());
                    for (const auto& inst : b->instructions) {
                        fprintf(stderr, " %d", static_cast<int>(inst->opcode));
                    }
                    fprintf(stderr, "\n");
                }
            }
            return false;
        }
    }
    if (!verifyDebugFlowTargets(func, block_set, error)) {
        return false;
    }
    std::unordered_set<uint32_t> value_ids;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (requiresResult(inst->opcode)) {
                if (!inst->result.isValid()) {
                    // Allow missing result when the opcode has optional_result set
                    // (e.g., LValuePathUnary for void remove/delete statements)
                    if (!getOpcodeOptionalResult(static_cast<int>(inst->opcode))) {
                        error = "instruction missing result value: opcode=" + std::to_string(static_cast<int>(inst->opcode))
                            + " in block '" + block->name + "'";
                        return false;
                    }
                } else {
                    if (!value_ids.insert(inst->result.id).second) {
                        error = "duplicate result value id";
                        return false;
                    }
                }
            } else if (inst->result.isValid()) {
                error = "unexpected result value: opcode="
                    + std::to_string(static_cast<int>(inst->opcode))
                    + " in block '" + block->name + "'";
                return false;
            }
        }
    }
    for (const auto& block : func.blocks) {
        bool saw_non_phi = false;
        for (const auto& inst : block->instructions) {
            if (inst->opcode == QoreIROpcode::Phi) {
                if (saw_non_phi) {
                    error = "phi instruction not grouped at beginning of block '" + block->name + "'";
                    return false;
                }
            } else {
                saw_non_phi = true;
            }
            if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = qore_ir_inst_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke missing valid targets";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::InvokeMethodDirect) {
                auto* invoke_inst = qore_ir_inst_cast<const QoreIRInvokeMethodDirectInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke.method.direct missing valid targets";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
                auto* invoke_inst = qore_ir_inst_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke.dot_eval_method.direct missing valid targets";
                    return false;
                }
            }
            int expected = expectedOperands(inst->opcode);
            if (expected >= 0) {
                // Exact operand count check
                if (expected != static_cast<int>(inst->operands.size())) {
                    error = "unexpected operand count for opcode " + std::to_string(static_cast<int>(inst->opcode))
                        + " (expected " + std::to_string(expected) + ", got "
                        + std::to_string(inst->operands.size()) + ")";
                    return false;
                }
            } else if (expected <= -2) {
                // Minimum operand count check: encoded as -(min + 2)
                int min_operands = -(expected + 2);
                if (static_cast<int>(inst->operands.size()) < min_operands) {
                    error = "too few operands for opcode " + std::to_string(static_cast<int>(inst->opcode))
                        + " (minimum " + std::to_string(min_operands) + ", got "
                        + std::to_string(inst->operands.size()) + ")";
                    return false;
                }
            }
            // expected == -1: skip validation entirely (fully variable)
            if ((inst->opcode == QoreIROpcode::LoadArg || inst->opcode == QoreIROpcode::LoadClosure)
                    && inst->operands.size() > 1) {
                error = "load.arg/load.closure only support zero or one operand";
                return false;
            }
            for (const auto& op : inst->operands) {
                if (!op.isValid()) {
                    error = "invalid operand value id";
                    return false;
                }
                if (value_ids.find(op.id) == value_ids.end()) {
                    error = "operand references undefined value";
                    return false;
                }
            }
            // only an unconditional branch can enter a block that starts an iteration of a loop statement, as the
            // cancellation check that such a block makes is made on the branch, and the native code makes it only for
            // unconditional branches (see QoreIRBasicBlock::loop_cancel)
            if (inst->opcode != QoreIROpcode::Br) {
                bool enters_loop_cancel = false;
                qore_ir_visit_successors(*inst, [&enters_loop_cancel](QoreIRBasicBlock* target) {
                    if (target && target->loop_cancel != QoreIRLoopCancel::None) {
                        enters_loop_cancel = true;
                    }
                });
                if (enters_loop_cancel) {
                    error = "a conditional branch enters a block that checks for cancellation (opcode "
                        + std::to_string(static_cast<int>(inst->opcode)) + ")";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::Br) {
                auto* br = qore_ir_inst_cast<const QoreIRBranchInstruction*>(inst.get());
                if (!br || !br->target) {
                    error = "branch missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::BrIf) {
                auto* br = qore_ir_inst_cast<const QoreIRBranchIfInstruction*>(inst.get());
                if (!br || !br->condition.isValid()) {
                    error = "branch-if missing condition";
                    return false;
                }
                if (!br->true_target || !br->false_target) {
                    error = "branch-if missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::SwitchInt) {
                auto* sw = qore_ir_inst_cast<const QoreIRSwitchIntInstruction*>(inst.get());
                if (!sw || !sw->switch_val.isValid()) {
                    error = "switch.int missing switch value";
                    return false;
                }
                if (!sw->default_target) {
                    error = "switch.int missing default target";
                    return false;
                }
                for (const auto& c : sw->cases) {
                    if (!c.target) {
                        error = "switch.int case missing target";
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::IteratorCreate
                    || inst->opcode == QoreIROpcode::IteratorCreateIterate
                    || inst->opcode == QoreIROpcode::IteratorCreateSelectors
                    || inst->opcode == QoreIROpcode::IteratorCreateSource) {
                auto* iter = qore_ir_inst_cast<const QoreIRIteratorCreateInstruction*>(inst.get());
                if (!iter || !iter->iterable.isValid()) {
                    error = "iterator.create missing iterable";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::IteratorNext
                    || inst->opcode == QoreIROpcode::IteratorNextSelectors
                    || inst->opcode == QoreIROpcode::TypedForeachNextInt
                    || inst->opcode == QoreIROpcode::TypedForeachNextFloat
                    || inst->opcode == QoreIROpcode::TypedForeachNextBool
                    || inst->opcode == QoreIROpcode::TypedForeachNextString) {
                auto* iter = qore_ir_inst_cast<const QoreIRIteratorNextInstruction*>(inst.get());
                if (!iter || !iter->iterator.isValid()) {
                    error = "iterator.next missing iterator";
                    return false;
                }
                if (inst->opcode != QoreIROpcode::IteratorNext && inst->opcode != QoreIROpcode::IteratorNextSelectors
                        && (!iter->index.isValid() || !iter->limit.isValid()
                            || inst->operands.size() != 3
                            || inst->operands[0].id != iter->iterator.id
                            || inst->operands[1].id != iter->index.id
                            || inst->operands[2].id != iter->limit.id)) {
                    error = "typed foreach.next missing list/index/limit operands";
                    return false;
                }
                if (!iter->done_target || !iter->continue_target) {
                    error = "iterator.next missing targets";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::RefForeachInit) {
                auto* rfi = qore_ir_inst_cast<const QoreIRRefForeachInitInstruction*>(inst.get());
                if (!rfi) {
                    error = "ref.foreach.init instruction malformed";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Return
                    || inst->opcode == QoreIROpcode::ReturnNothing) {
                auto* ret = qore_ir_inst_cast<const QoreIRReturnInstruction*>(inst.get());
                if (!ret) {
                    error = "return instruction malformed";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::Return && !ret->has_value) {
                    error = "return missing value";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::ReturnNothing && ret->has_value) {
                    error = "return.nothing has value";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Phi) {
                auto* phi = qore_ir_inst_cast<const QoreIRPhiInstruction*>(inst.get());
                if (!phi) {
                    error = "phi instruction malformed";
                    return false;
                }
                if (phi->incoming.empty()) {
                    error = "phi missing incoming values";
                    return false;
                }
                if (phi->incoming.size() != inst->operands.size()) {
                    error = "phi operands do not match incoming values";
                    return false;
                }
                for (const auto& incoming : phi->incoming) {
                    if (!incoming.block || block_set.find(incoming.block) == block_set.end()) {
                        error = "phi references unknown block";
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::LoadClosure
                    || inst->opcode == QoreIROpcode::StoreClosure) {
                auto* local_inst = qore_ir_inst_cast<const QoreIRLocalInstruction*>(inst.get());
                if (!local_inst || !local_inst->local) {
                    error = "local instruction missing local";
                    return false;
                }
                if ((inst->opcode == QoreIROpcode::StoreLocal || inst->opcode == QoreIROpcode::StoreClosure)
                        && !local_inst->initial_assignment
                        && rejectReadOnlyLocalWrite(local_inst->local,
                            inst->opcode == QoreIROpcode::StoreLocal ? "StoreLocal" : "StoreClosure", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadGlobal
                    || inst->opcode == QoreIROpcode::StoreGlobal
                    || inst->opcode == QoreIROpcode::LoadThreadLocal
                    || inst->opcode == QoreIROpcode::StoreThreadLocal) {
                auto* var_inst = qore_ir_inst_cast<const QoreIRVarInstruction*>(inst.get());
                if (!var_inst || !var_inst->var) {
                    error = "variable instruction missing var";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadLValue
                    || inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue
                    || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue
                    || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue
                    || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue
                    || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue
                    || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue
                    || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue) {
                auto* lv_inst = qore_ir_inst_cast<const QoreIRLValueInstruction*>(inst.get());
                if (!lv_inst || !lv_inst->lvalue.hasNode()) {
                    error = "lvalue instruction missing lvalue";
                    return false;
                }
                if (inst->opcode != QoreIROpcode::LoadLValue
                        && rejectReadOnlyVarRefWrite(extractLValueBaseVarRef(lv_inst->lvalue), "lvalue op", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* fused = qore_ir_inst_cast<const QoreIRAddAssignLocalIntInstruction*>(inst.get());
                if (!fused || !fused->target || !fused->source) {
                    error = "AddAssignLocalInt instruction malformed";
                    return false;
                }
                if (rejectReadOnlyLocalWrite(fused->target, "AddAssignLocalInt", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                auto* fused = qore_ir_inst_cast<const QoreIRIncrementLocalIntInstruction*>(inst.get());
                if (!fused || !fused->local) {
                    error = "IncrementLocalInt instruction malformed";
                    return false;
                }
                if (rejectReadOnlyLocalWrite(fused->local, "IncrementLocalInt", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStore) {
                auto* hks = qore_ir_inst_cast<const QoreIRHashKeyStoreInstruction*>(inst.get());
                if (!hks) {
                    error = "HashKeyStore instruction malformed";
                    return false;
                }
                const LocalVar* lv = hks->container_lv ? hks->container_lv : getLocalVarFromVarRef(hks->container);
                if (rejectReadOnlyLocalWrite(lv, "HashKeyStore", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStoreDynamic) {
                auto* hksd = qore_ir_inst_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst.get());
                if (!hksd) {
                    error = "HashKeyStoreDynamic instruction malformed";
                    return false;
                }
                const LocalVar* lv = hksd->container_lv ? hksd->container_lv : getLocalVarFromVarRef(hksd->container);
                if (rejectReadOnlyLocalWrite(lv, "HashKeyStoreDynamic", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::ListIndexStore) {
                auto* lis = qore_ir_inst_cast<const QoreIRListIndexStoreInstruction*>(inst.get());
                if (!lis) {
                    error = "ListIndexStore instruction malformed";
                    return false;
                }
                if (rejectReadOnlyVarRefWrite(lis->container, "ListIndexStore", error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                auto* path_inst = qore_ir_inst_cast<const QoreIRLValuePathInstruction*>(inst.get());
                if (!path_inst) {
                    error = "LValuePath instruction malformed";
                    return false;
                }
                if (!path_inst->path.empty()) {
                    const LVPathStep& root = path_inst->path[0];
                    if ((root.kind == LVPathStepKind::LocalVar || root.kind == LVPathStepKind::ClosureVar)
                            && rejectReadOnlyLocalWrite(reinterpret_cast<const LocalVar*>(root.ref_ptr),
                                "LValuePath op", error)) {
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::CreateParseRef) {
                auto* pr = qore_ir_inst_cast<const QoreIRCreateParseRefInstruction*>(inst.get());
                if (!pr) {
                    error = "CreateParseRef instruction malformed";
                    return false;
                }
                const ParseReferenceNode* prn = pr->node
                    ? pr->node
                    : dynamic_cast<const ParseReferenceNode*>(pr->expr.getInternalNode());
                if (prn && rejectReadOnlyVarRefWrite(extractLValueBaseVarRef(prn->getLVExp()), "CreateParseRef",
                        error)) {
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallIndirect
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic
                    || inst->opcode == QoreIROpcode::CastAny
                    || inst->opcode == QoreIROpcode::CastList
                    || inst->opcode == QoreIROpcode::CastHash
                    || inst->opcode == QoreIROpcode::CastObject
                    || inst->opcode == QoreIROpcode::CastEnum
                    || inst->opcode == QoreIROpcode::ExtractAny
                    || inst->opcode == QoreIROpcode::ExtractList
                    || inst->opcode == QoreIROpcode::ExtractString
                    || inst->opcode == QoreIROpcode::ExtractBinary
                    || inst->opcode == QoreIROpcode::RemoveAny
                    || inst->opcode == QoreIROpcode::RemoveList
                    || inst->opcode == QoreIROpcode::RemoveHash
                    || inst->opcode == QoreIROpcode::RemoveObject
                    || inst->opcode == QoreIROpcode::RemoveString
                    || inst->opcode == QoreIROpcode::RemoveBinary
                    || inst->opcode == QoreIROpcode::KeysAny
                    || inst->opcode == QoreIROpcode::KeysList
                    || inst->opcode == QoreIROpcode::KeysHash
                    || inst->opcode == QoreIROpcode::RegexMatchAny
                    || inst->opcode == QoreIROpcode::RegexMatchBool
                    || inst->opcode == QoreIROpcode::RegexNMatchBool
                    || inst->opcode == QoreIROpcode::RegexExtractAny
                    || inst->opcode == QoreIROpcode::RegexExtractList
                    || inst->opcode == QoreIROpcode::RegexSubstAny
                    || inst->opcode == QoreIROpcode::RegexSubstString
                    || inst->opcode == QoreIROpcode::InstanceOfBool
                    || inst->opcode == QoreIROpcode::TrimAny
                    || inst->opcode == QoreIROpcode::TrimString
                    || inst->opcode == QoreIROpcode::ChompAny
                    || inst->opcode == QoreIROpcode::ChompString
                    || inst->opcode == QoreIROpcode::TransliterateAny
                    || inst->opcode == QoreIROpcode::TransliterateString
                    || inst->opcode == QoreIROpcode::BackgroundInt
                    || inst->opcode == QoreIROpcode::ListAssignAny
                    || inst->opcode == QoreIROpcode::PopAny
                    || inst->opcode == QoreIROpcode::PushAny
                    || inst->opcode == QoreIROpcode::ExistsAny
                    || inst->opcode == QoreIROpcode::ExistsBool
                    || inst->opcode == QoreIROpcode::ElementsAny
                    || inst->opcode == QoreIROpcode::ElementsInt
                    || inst->opcode == QoreIROpcode::DotEvalAny
                    || inst->opcode == QoreIROpcode::DotEvalInt
                    || inst->opcode == QoreIROpcode::DotEvalFloat
                    || inst->opcode == QoreIROpcode::DotEvalString
                    || inst->opcode == QoreIROpcode::DotEvalDate
                    || inst->opcode == QoreIROpcode::DotEvalList
                    || inst->opcode == QoreIROpcode::DotEvalHash
                    || inst->opcode == QoreIROpcode::DotEvalObject) {
                auto* expr_inst = qore_ir_inst_cast<const QoreIRExprInstruction*>(inst.get());
                auto* bg_inst = qore_ir_inst_cast<const QoreIRBackgroundInstruction*>(inst.get());
                if (bg_inst && bg_inst->kind == QoreIRBackgroundKind::Helper) {
                    if (!bg_inst->helper_ir || bg_inst->helper_params.size() != bg_inst->operands.size()) {
                        error = "background helper instruction missing its helper or with a parameter count "
                            "different from its operand count";
                        return false;
                    }
                } else if (bg_inst) {
                    if ((bg_inst->kind != QoreIRBackgroundKind::DotEval
                                && bg_inst->kind != QoreIRBackgroundKind::StaticMethod)
                            || bg_inst->name.empty()
                            || (bg_inst->kind == QoreIRBackgroundKind::DotEval
                                && bg_inst->operands.empty())) {
                        error = "background instruction missing native metadata";
                        return false;
                    }
                } else if (!expr_inst || !expr_inst->expr.hasNode()) {
                    error = "expr instruction missing expr";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::SwitchRegexMatch) {
                auto* regex_inst = qore_ir_inst_cast<const QoreIRSwitchRegexMatchInstruction*>(inst.get());
                if (!regex_inst || !regex_inst->regex_case) {
                    error = "SwitchRegexMatch instruction missing regex_case";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::SwitchCaseMatch) {
                auto* case_inst = qore_ir_inst_cast<const QoreIRSwitchCaseMatchInstruction*>(inst.get());
                if (!case_inst || !case_inst->case_node) {
                    error = "SwitchCaseMatch instruction missing case_node";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = qore_ir_inst_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->expr.hasNode()) {
                    error = "invoke instruction missing expr";
                    return false;
                }
            } else if (isPluginDispatchOpcode(inst->opcode)) {
                auto* plugin_inst = qore_ir_inst_cast<const QoreIRPluginInstruction*>(inst.get());
                if (!plugin_inst || !plugin_inst->operation.isValid()) {
                    error = "plugin dispatch instruction missing operation reference";
                    return false;
                }
            }
        }
    }
    return true;
}

// --- IR-only local classification ---

//! Returns true if the opcode is a delegate-to-AST statement that executes through
//! StatementBlock::exec() and could access any local on the thread-local variable stack.
static bool isDelegateToASTStatement(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Summarize:     // Summarize statement delegates to AST
        case QoreIROpcode::MapSelectAny:  // Delegate-to-AST functional operator
        case QoreIROpcode::HashMapAny:    // Delegate-to-AST functional operator
        case QoreIROpcode::HashMapSelectAny: // Delegate-to-AST functional operator
            return true;
        default:
            return false;
    }
}

#endif // QORE_IR_TAKE_OVER_UNIT

//! Returns true if the opcode is a lvalue operation that modifies variables through
//! the runtime stack (not through StoreLocal).
static bool isLValueOp(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::LoadLValue:
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::SpliceLValue:
            return true;
        default:
            return false;
    }
}

//! Recursively walk an AST expression tree and collect all local variable keys
//! (LocalVar* as void*) referenced by VarRefNode leaves.
//!
//! IMPORTANT: This walker is conservative — for any unrecognized node type,
//! it sets `unknown_node_found` to true, which causes the caller to treat
//! ALL locals as AST-visible.  This ensures correctness even if new AST node
//! types are added to Qore.
static void collectLocalsFromExpr(const QoreValue& expr,
        std::unordered_set<const void*>& ast_locals, bool& unknown_node_found,
        bool* ref_created = nullptr) {
    if (!expr.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }

    qore_type_t ntype = expr.getType();

    // VarRefNode — leaf node, check if it's a local variable
    if (ntype == NT_VARREF) {
        auto* var_ref = reinterpret_cast<const VarRefNode*>(node);
        qore_var_t type = var_ref->getType();
        if ((type == VT_LOCAL || type == VT_LOCAL_TS || type == VT_CLOSURE) && var_ref->ref.id) {
            ast_locals.insert(reinterpret_cast<const void*>(var_ref->ref.id));
        }
        // VarRefNewObjectNode inherits from both VarRefNode and FunctionCallBase;
        // its constructor args may reference locals that are evaluated through AST
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
            if (const QoreParseListNode* pargs = vrn->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalsFromExpr(pargs->get(i), ast_locals, unknown_node_found, ref_created);
                }
            }
            if (const QoreListNode* eargs = vrn->getArgs()) {
                ConstListIterator li(eargs);
                while (li.next()) {
                    collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found, ref_created);
                }
            }
        }
        return;
    }

    // Self variable reference — leaf node (not a local, no children)
    if (ntype == NT_SELF_VARREF) {
        return;
    }

    // Constants and literals — leaf nodes.  An object in an expression tree is the value of a resolved constant,
    // such as the system constants stdout and stderr in "stderr.printf(...)": a value, which references no local
    // variable.  Treating it as unknown kept every local of any function that calls a method on such a constant
    // on the runtime local stack.
    if (ntype == NT_STRING || ntype == NT_INT || ntype == NT_FLOAT || ntype == NT_BOOLEAN || ntype == NT_CHAR
            || ntype == NT_NOTHING || ntype == NT_NULL || ntype == NT_NUMBER
            || ntype == NT_DATE || ntype == NT_BINARY || ntype == NT_HASH
            || ntype == NT_LIST || ntype == NT_BACKQUOTE || ntype == NT_OBJECT) {
        return;
    }

    // Binary operators: recurse left and right
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        collectLocalsFromExpr(binop->getLeft(), ast_locals, unknown_node_found, ref_created);
        collectLocalsFromExpr(binop->getRight(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // Binary lvalue operators (=, +=, ... in an expression: f(x = g())); a reference that the right side creates is
    // found when it is walked
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalsFromExpr(binop->getLeft(), ast_locals, unknown_node_found, ref_created);
        collectLocalsFromExpr(binop->getRight(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // Binary int-specific operators (inherit separately from QoreBinaryOperatorNode<>)
    if (auto* binop = dynamic_cast<const QoreBinaryIntLValueOperatorNode*>(node)) {
        collectLocalsFromExpr(binop->getLeft(), ast_locals, unknown_node_found, ref_created);
        collectLocalsFromExpr(binop->getRight(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // Unary/single expression operators: recurse expression
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        collectLocalsFromExpr(unop->getExp(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // LValue single expression operators
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalsFromExpr(unop->getExp(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // Single-value expression operators (e.g., exists, which inherits from
    // QoreSingleValueExpressionOperatorNode<QoreOperatorNode>)
    if (auto* unop = dynamic_cast<const QoreSingleValueExpressionOperatorNode<>*>(node)) {
        collectLocalsFromExpr(unop->getExp(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // the value of the switch statement in the comparison of a switch case reads no local (qore#6252)
    if (dynamic_cast<const QoreSwitchValueOperatorNode*>(node)) {
        return;
    }

    // Dot eval operator (method call on object): left.method()
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        collectLocalsFromExpr(dot->getExpression(), ast_locals, unknown_node_found, ref_created);
        // Method call node has args that might reference locals
        if (const MethodCallNode* m = dot->getMethodCall()) {
            if (const QoreParseListNode* pargs = m->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalsFromExpr(pargs->get(i), ast_locals, unknown_node_found, ref_created);
                }
            }
            // Also check resolved args — after parse resolution, locals may only
            // be in the resolved args list
            if (const QoreListNode* rargs = m->getArgs()) {
                ConstListIterator li(rargs);
                while (li.next()) {
                    collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found, ref_created);
                }
            }
        }
        return;
    }

    // Function calls (FunctionCallNode, SelfFunctionCallNode, StaticMethodCallNode,
    // MethodCallNode — all inherit from FunctionCallBase)
    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        if (const QoreParseListNode* args = call->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalsFromExpr(args->get(i), ast_locals, unknown_node_found, ref_created);
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found, ref_created);
            }
        }
        return;
    }

    // Call reference calls: adder(32)
    if (auto* crc = dynamic_cast<const CallReferenceCallNode*>(node)) {
        collectLocalsFromExpr(crc->getExp(), ast_locals, unknown_node_found, ref_created);
        if (const QoreParseListNode* args = crc->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalsFromExpr(args->get(i), ast_locals, unknown_node_found, ref_created);
            }
        }
        if (const QoreListNode* args = crc->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalsFromExpr(li.getValue(), ast_locals, unknown_node_found, ref_created);
            }
        }
        return;
    }

    // A closure body does not execute in the enclosing function's AST frame.
    // Only its explicit capture list can reference enclosing locals.
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        if (const LVarSet* vlist = closure->getVList()) {
            size_t capture_i = 0;
            for (LocalVar* lv : *vlist) {
                if (capture_i++ && !(capture_i % 100)
                        && qore_check_cancel(nullptr,
                            "IR closure capture local analysis")) {
                    unknown_node_found = true;
                    return;
                }
                if (lv) {
                    ast_locals.insert(reinterpret_cast<const void*>(lv));
                }
            }
        }
        return;
    }

    // Object method reference: \obj.method() — recurse into the object expression
    if (ntype == NT_OBJMETHREF) {
        if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
            collectLocalsFromExpr(omr->getExp(), ast_locals, unknown_node_found, ref_created);
        }
        // ParseSelfMethodReferenceNode and ParseScopedSelfMethodReferenceNode have no local variable
        // references (self-based)
        return;
    }

    // Parse reference: \var — recurse into the lvalue expression
    if (ntype == NT_PARSEREFERENCE) {
        auto* pref = reinterpret_cast<const ParseReferenceNode*>(node);
        if (ref_created) {
            *ref_created = true;
        }
        collectLocalsFromExpr(pref->getLVExp(), ast_locals, unknown_node_found, ref_created);
        return;
    }

    // Function and class/self method call-reference literals have no local
    // expression children. Object method references are handled above through
    // their receiver expression.
    if (dynamic_cast<const LocalFunctionCallReferenceNode*>(node)
            || dynamic_cast<const DeferredFunctionCallReferenceNode*>(node)
            || dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node)
            || dynamic_cast<const DeferredStaticMethodCallReferenceNode*>(node)) {
        return;
    }

    // Parse list: recurse all elements
    if (ntype == NT_PARSE_LIST) {
        auto* plist = expr.get<const QoreParseListNode>();
        for (size_t i = 0; i < plist->size(); ++i) {
            collectLocalsFromExpr(plist->get(i), ast_locals, unknown_node_found, ref_created);
        }
        return;
    }

    // Unknown node type — conservatively mark for fallback
    unknown_node_found = true;
}

static void collectLocalsFromFindInstruction(const QoreIRFindInstruction* find_inst,
        std::unordered_set<const void*>& ast_locals, bool& unknown_node_found) {
    collectLocalsFromExpr(find_inst->exp, ast_locals, unknown_node_found);
    collectLocalsFromExpr(find_inst->find_exp, ast_locals, unknown_node_found);
    collectLocalsFromExpr(find_inst->where, ast_locals, unknown_node_found);
}

//! Recursively walk an expression in source order and collect LocalVar* slots
//! needed to serialize embedded AST expression trees.
/** The walk must reach every local variable of the expression: an instruction that evaluates an embedded AST
    expression reads its locals from the thread's local variable stack, and a background helper takes exactly the
    locals that have slots in it as its parameters (QoreIRLowering::lowerBackgroundHelper()), so a local the walk
    misses is not instantiated in the new thread (LVALUE-ERROR).  Every operator class therefore matches one of the
    operator branches below; in a debug build an operator that matches none fails an assertion, and
    examples/test/ir/IRSlotWalkOperatorCoverage.qtest checks that every operator class in include/qore/intern
    derives from one of the classes the walk handles.
*/
static void collectLocalSlotsFromExpr(const QoreValue& expr,
        std::vector<const LocalVar*>& locals, std::unordered_set<const void*>& seen) {
    if (!expr.hasNode()) {
        return;
    }
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }

    auto addVarRef = [&locals, &seen](const VarRefNode* var_ref) {
        qore_var_t type = var_ref->getType();
        if ((type == VT_LOCAL || type == VT_LOCAL_TS || type == VT_CLOSURE) && var_ref->ref.id) {
            const void* key = reinterpret_cast<const void*>(var_ref->ref.id);
            if (seen.insert(key).second) {
                locals.push_back(reinterpret_cast<const LocalVar*>(var_ref->ref.id));
            }
        }
    };
    auto addLocalVar = [&locals, &seen](const LocalVar* lv) {
        if (!lv) {
            return;
        }
        const void* key = reinterpret_cast<const void*>(lv);
        if (seen.insert(key).second) {
            locals.push_back(lv);
        }
    };

    qore_type_t ntype = expr.getType();
    if (ntype == NT_VARREF) {
        auto* var_ref = reinterpret_cast<const VarRefNode*>(node);
        addVarRef(var_ref);
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
            if (const QoreParseListNode* pargs = vrn->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalSlotsFromExpr(pargs->get(i), locals, seen);
                }
            }
            if (const QoreListNode* eargs = vrn->getArgs()) {
                ConstListIterator li(eargs);
                while (li.next()) {
                    collectLocalSlotsFromExpr(li.getValue(), locals, seen);
                }
            }
        }
        return;
    }

    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
        collectLocalSlotsFromExpr(binop->getLeft(), locals, seen);
        collectLocalSlotsFromExpr(binop->getRight(), locals, seen);
        return;
    }
    // the binary lvalue operators (assignments, +=, push, ..., and QoreBinaryIntLValueOperatorNode)
    if (auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalSlotsFromExpr(binop->getLeft(), locals, seen);
        collectLocalSlotsFromExpr(binop->getRight(), locals, seen);
        return;
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<>*>(node)) {
        collectLocalSlotsFromExpr(unop->getExp(), locals, seen);
        return;
    }
    if (auto* unop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(node)) {
        collectLocalSlotsFromExpr(unop->getExp(), locals, seen);
        return;
    }
    if (auto* unop = dynamic_cast<const QoreSingleValueExpressionOperatorNode<>*>(node)) {
        collectLocalSlotsFromExpr(unop->getExp(), locals, seen);
        return;
    }
    // the operators with three operands (?:, map with a select expression, the hash form of map, a range subscript)
    if (auto* nop = dynamic_cast<const QoreNOperatorNodeBase<3>*>(node)) {
        for (unsigned i = 0; i < 3; ++i) {
            collectLocalSlotsFromExpr(nop->get(i), locals, seen);
        }
        return;
    }
    // the operator with four operands (the hash form of map with a select expression)
    if (auto* nop = dynamic_cast<const QoreNOperatorNodeBase<4>*>(node)) {
        for (unsigned i = 0; i < 4; ++i) {
            collectLocalSlotsFromExpr(nop->get(i), locals, seen);
        }
        return;
    }
    if (auto* ext = dynamic_cast<const QoreExtractOperatorNode*>(node)) {
        collectLocalSlotsFromExpr(ext->getLValue(), locals, seen);
        collectLocalSlotsFromExpr(ext->getOffset(), locals, seen);
        collectLocalSlotsFromExpr(ext->getLength(), locals, seen);
        collectLocalSlotsFromExpr(ext->getNewValue(), locals, seen);
        return;
    }
    // the value of the switch statement in the comparison of a switch case has no operands (qore#6252)
    if (dynamic_cast<const QoreSwitchValueOperatorNode*>(node)) {
        return;
    }
    if (auto* spl = dynamic_cast<const QoreSpliceOperatorNode*>(node)) {
        collectLocalSlotsFromExpr(spl->getLValue(), locals, seen);
        collectLocalSlotsFromExpr(spl->getOffset(), locals, seen);
        collectLocalSlotsFromExpr(spl->getLength(), locals, seen);
        collectLocalSlotsFromExpr(spl->getNewValue(), locals, seen);
        return;
    }
    if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
        collectLocalSlotsFromExpr(dot->getExpression(), locals, seen);
        if (const MethodCallNode* m = dot->getMethodCall()) {
            if (const QoreParseListNode* pargs = m->getParseArgs()) {
                for (size_t i = 0; i < pargs->size(); ++i) {
                    collectLocalSlotsFromExpr(pargs->get(i), locals, seen);
                }
            }
            if (const QoreListNode* rargs = m->getArgs()) {
                ConstListIterator li(rargs);
                while (li.next()) {
                    collectLocalSlotsFromExpr(li.getValue(), locals, seen);
                }
            }
        }
        return;
    }
    if (auto* call = dynamic_cast<const FunctionCallBase*>(node)) {
        if (const QoreParseListNode* args = call->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalSlotsFromExpr(args->get(i), locals, seen);
            }
        }
        if (const QoreListNode* args = call->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalSlotsFromExpr(li.getValue(), locals, seen);
            }
        }
        return;
    }
    if (auto* crc = dynamic_cast<const CallReferenceCallNode*>(node)) {
        collectLocalSlotsFromExpr(crc->getExp(), locals, seen);
        if (const QoreParseListNode* args = crc->getParseArgs()) {
            for (size_t i = 0; i < args->size(); ++i) {
                collectLocalSlotsFromExpr(args->get(i), locals, seen);
            }
        }
        if (const QoreListNode* args = crc->getArgs()) {
            ConstListIterator li(args);
            while (li.next()) {
                collectLocalSlotsFromExpr(li.getValue(), locals, seen);
            }
        }
        return;
    }
    if (auto* closure = dynamic_cast<const QoreClosureParseNode*>(node)) {
        if (const LVarSet* vlist = closure->getVList()) {
            for (LocalVar* lv : *vlist) {
                addLocalVar(lv);
            }
        }
        return;
    }
    if (ntype == NT_OBJMETHREF) {
        if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
            collectLocalSlotsFromExpr(omr->getExp(), locals, seen);
        }
        return;
    }
    if (ntype == NT_PARSEREFERENCE) {
        auto* pref = reinterpret_cast<const ParseReferenceNode*>(node);
        collectLocalSlotsFromExpr(pref->getLVExp(), locals, seen);
        return;
    }
    // the arguments of a new expression of a hashdecl or a complex type, which the instruction evaluates with the
    // node: a local that only they use needs a slot as well, since a background helper takes exactly the locals that
    // have slots as its parameters (QoreIRLowering::lowerBackgroundHelper()); without one, the new thread evaluated
    // the arguments with the local not instantiated (LVALUE-ERROR).  A new object expression is a call, above.
    if (ntype == NT_SCOPE_REF) {
        const QoreParseListNode* plist = nullptr;
        if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
            plist = nhd->args;
        } else if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
            plist = nch->args;
        } else if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
            collectLocalSlotsFromExpr(ncl->args, locals, seen);
        } else if (auto* ncb = dynamic_cast<const NewComplexBufferNode*>(node)) {
            collectLocalSlotsFromExpr(ncb->args, locals, seen);
        } else {
            // every new expression is a call (ScopedObjectCallNode, above) or one of the classes here
            assert(false);
        }
        if (plist) {
            for (size_t i = 0; i < plist->size(); ++i) {
                collectLocalSlotsFromExpr(plist->get(i), locals, seen);
            }
        }
        return;
    }
    if (ntype == NT_PARSE_LIST) {
        auto* plist = expr.get<const QoreParseListNode>();
        for (size_t i = 0; i < plist->size(); ++i) {
            collectLocalSlotsFromExpr(plist->get(i), locals, seen);
        }
        return;
    }
    // a hash expression, such as the initializer of new hash<K, V>({"a": x})
    if (ntype == NT_PARSE_HASH) {
        auto* phash = expr.get<const QoreParseHashNode>();
        for (const QoreValue& key : phash->getKeys()) {
            collectLocalSlotsFromExpr(key, locals, seen);
        }
        for (const QoreValue& value : phash->getValues()) {
            collectLocalSlotsFromExpr(value, locals, seen);
        }
        return;
    }
    if (ntype == NT_FIND) {
        auto* find = reinterpret_cast<const FindNode*>(node);
        collectLocalSlotsFromExpr(find->exp, locals, seen);
        collectLocalSlotsFromExpr(find->find_exp, locals, seen);
        collectLocalSlotsFromExpr(find->where, locals, seen);
        return;
    }
    // an operator that matches none of the operator branches above would have its locals missed
    assert(ntype != NT_OPERATOR);
}

//! Recursively walks a StatementBlock AST tree to find all local variable references.
//! This is used to determine which locals are referenced by on_block_exit handler bodies
//! so they can be excluded from IR-only classification.
static void collectLocalsFromStatementBlock(const StatementBlock* block,
        std::unordered_set<const void*>& ast_locals, bool& unknown_node_found) {
    if (!block) {
        return;
    }
    for (const auto* stmt : block->getStatements()) {
        if (!stmt) {
            continue;
        }
        // ExpressionStatement: walk expression
        if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
            collectLocalsFromExpr(expr_stmt->getExpression(), ast_locals, unknown_node_found);
            continue;
        }
        // IfStatement: walk condition, then-block, else-block
        if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
            collectLocalsFromExpr(if_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(if_stmt->getIfCode(), ast_locals,
                    unknown_node_found);
            collectLocalsFromStatementBlock(if_stmt->getElseCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // WhileStatement / DoWhileStatement: walk condition, body
        if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
            collectLocalsFromExpr(while_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(while_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ForStatement: walk init, condition, increment, body
        if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
            collectLocalsFromExpr(for_stmt->getAssignment(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(for_stmt->getCond(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(for_stmt->getIterator(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(for_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ForEachStatement: walk variable, list, body
        if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
            collectLocalsFromExpr(foreach_stmt->getVar(), ast_locals, unknown_node_found);
            collectLocalsFromExpr(foreach_stmt->getList(), ast_locals, unknown_node_found);
            collectLocalsFromStatementBlock(foreach_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // SwitchStatement: walk switch expression, case expressions and blocks
        if (auto* switch_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
            collectLocalsFromExpr(switch_stmt->getSwitchExp(), ast_locals,
                    unknown_node_found);
            for (const CaseNode* cn = switch_stmt->getCases(); cn; cn = cn->next) {
                collectLocalsFromExpr(cn->val, ast_locals, unknown_node_found);
                collectLocalsFromStatementBlock(cn->code, ast_locals, unknown_node_found);
            }
            continue;
        }
        // TryStatement: walk try block, catch block
        if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
            collectLocalsFromStatementBlock(try_stmt->getTryBlock(), ast_locals,
                    unknown_node_found);
            collectLocalsFromStatementBlock(try_stmt->getCatchBlock(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // ReturnStatement: walk return expression
        if (auto* ret_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
            collectLocalsFromExpr(ret_stmt->getExpression(), ast_locals, unknown_node_found);
            continue;
        }
        // ThrowStatement: walk throw args
        if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(stmt)) {
            collectLocalsFromExpr(throw_stmt->getArgs(), ast_locals, unknown_node_found);
            continue;
        }
        // OnBlockExitStatement: walk handler body
        if (auto* obe_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
            collectLocalsFromStatementBlock(obe_stmt->getCode(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // DebugStatement: walk expression or block
        if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
            if (debug_stmt->getBlock()) {
                collectLocalsFromStatementBlock(debug_stmt->getBlock(), ast_locals,
                        unknown_node_found);
            }
            if (debug_stmt->getExpression()) {
                collectLocalsFromExpr(debug_stmt->getExpression(), ast_locals,
                        unknown_node_found);
            }
            continue;
        }
        // AssertStatement: walk condition expression
        if (auto* assert_stmt = dynamic_cast<const AssertStatement*>(stmt)) {
            collectLocalsFromExpr(assert_stmt->getCondition(), ast_locals,
                    unknown_node_found);
            continue;
        }
        // Nested StatementBlock: recurse
        if (auto* sub_block = dynamic_cast<const StatementBlock*>(stmt)) {
            collectLocalsFromStatementBlock(sub_block, ast_locals, unknown_node_found);
            continue;
        }
        // Unknown statement type — conservatively mark for fallback
        unknown_node_found = true;
    }
}

//! Extracts the AST expression from an instruction that stores one.
//! Returns nullptr if the instruction type doesn't have an expr field.
static const QoreValue* getInstructionExpr(const QoreIRInstruction* inst) {
    switch (inst->opcode) {
        // QoreIRExprInstruction (Call, CallIndirect, CallMethod, CallStatic, CallClosureDirect, and other expr-based ops)
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
            return &static_cast<const QoreIRExprInstruction*>(inst)->expr;

        // CallDirect, CallStaticDirect, DotEvalMethodDirect, and InvokeDotEvalMethodDirect have expr fields,
        // but they pre-evaluate all arguments into IR operands. The expr args are only used by AOT serialization,
        // not re-evaluated at JIT runtime. Returning nullptr prevents incorrect classification of call args as
        // AST-visible locals, allowing parameters to be properly classified as IR-only.
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
            return nullptr;

        // Invoke has its own instruction class.  When operands are present for
        // natively lowered invoke opcodes, the expression is metadata only
        // (target identity, regex/type information, etc.) and its argument/base
        // subtrees are not re-evaluated at runtime.  Walking those trees here
        // marks operand locals AST-visible and forces unnecessary runtime-stack
        // reloads after every call in hot AOT loops.
        case QoreIROpcode::Invoke: {
            const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst);
            switch (inv->invoke_opcode) {
                case QoreIROpcode::Call:
                case QoreIROpcode::CallDirect:
                case QoreIROpcode::CallIndirect:
                case QoreIROpcode::CallMethod:
                case QoreIROpcode::CallMethodDirect:
                case QoreIROpcode::CallStatic:
                case QoreIROpcode::CallStaticDirect:
                    return nullptr;

                case QoreIROpcode::RegexMatchBool:
                case QoreIROpcode::RegexNMatchBool:
                case QoreIROpcode::RegexExtractAny:
                case QoreIROpcode::RegexExtractList:
                case QoreIROpcode::ExistsAny:
                case QoreIROpcode::ExistsBool:
                case QoreIROpcode::InstanceOfBool:
                case QoreIROpcode::KeysAny:
                case QoreIROpcode::KeysList:
                case QoreIROpcode::KeysHash:
                case QoreIROpcode::ElementsAny:
                case QoreIROpcode::ElementsInt:
                case QoreIROpcode::CastAny:
                case QoreIROpcode::CastList:
                case QoreIROpcode::CastHash:
                case QoreIROpcode::CastComplexHash:
                case QoreIROpcode::CastObject:
                case QoreIROpcode::CastEnum:
                    return inv->operands.empty() ? &inv->expr : nullptr;

                case QoreIROpcode::ListAssignAny:
                    return inv->operands.size() >= 2 ? nullptr : &inv->expr;

                case QoreIROpcode::RangeSliceAny:
                case QoreIROpcode::RangeSliceInt:
                case QoreIROpcode::RangeSliceFloat:
                    return inv->operands.size() >= 3 ? nullptr : &inv->expr;

                default:
                    return &inv->expr;
            }
        }

        // Native access opcodes with expr fields
        case QoreIROpcode::LoadStaticVar:
            return &static_cast<const QoreIRStaticVarInstruction*>(inst)->expr;
        case QoreIROpcode::NewObject:
            // NewObject has no expr field — class/variant are stored directly
            // and args are IR operands, not AST nodes.
            return nullptr;
        case QoreIROpcode::LoadConstant:
            return &static_cast<const QoreIRLoadConstantInstruction*>(inst)->expr;

        // Closure/reference creation opcodes with expr fields
        case QoreIROpcode::CreateClosure:
            return &static_cast<const QoreIRCreateClosureInstruction*>(inst)->expr;
        case QoreIROpcode::CreateCallRef:
            return &static_cast<const QoreIRCreateCallRefInstruction*>(inst)->expr;
        case QoreIROpcode::CreateMethodRef:
            return &static_cast<const QoreIRCreateMethodRefInstruction*>(inst)->expr;
        case QoreIROpcode::CreateParseRef:
            return &static_cast<const QoreIRCreateParseRefInstruction*>(inst)->expr;

        // RefForeachInit evaluates its ParseReferenceNode through the AST reference evaluator
        // (qore_rt_ref_foreach_init() -> ParseReferenceNode::evalToRef()), which resolves the
        // lvalue's subscripts by reading the variables from the runtime local stack.  Any local
        // used only inside that expression - typically the key or index, as in
        // "foreach hash<auto> i in (\h{key}.member)" - must therefore stay AST-visible.  Without
        // this case the expression tree is never walked, the local is classified as IR-only and
        // never materialized on the runtime stack, so the subscript evaluates to NOTHING: the
        // reference silently addresses the empty-string key and every write through the loop
        // variable is discarded.
        case QoreIROpcode::RefForeachInit:
            return &static_cast<const QoreIRRefForeachInitInstruction*>(inst)->expr;

        // Container construction opcodes with expr fields
        case QoreIROpcode::NewHashDecl:
            return &static_cast<const QoreIRNewHashDeclInstruction*>(inst)->expr;
        case QoreIROpcode::NewComplexHash:
            return &static_cast<const QoreIRNewComplexHashInstruction*>(inst)->expr;
        case QoreIROpcode::NewComplexList:
            return &static_cast<const QoreIRNewComplexListInstruction*>(inst)->expr;
        case QoreIROpcode::NewComplexBuffer:
            return &static_cast<const QoreIRNewComplexBufferInstruction*>(inst)->expr;

        // VarRefNewObjectNode construction
        case QoreIROpcode::VrnConstruct:
            return &static_cast<const QoreIRVrnConstructInstruction*>(inst)->expr;

        // Native-lowered expression opcodes keep expr metadata for constants,
        // regex/type details, or fallback identity.  When operands are present,
        // the operand subtrees have already been lowered and are not evaluated
        // through the AST at runtime.
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastComplexHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            return expr_inst->operands.empty() ? &expr_inst->expr : nullptr;
        }

        case QoreIROpcode::ListAssignAny: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            return expr_inst->operands.size() >= 2 ? nullptr : &expr_inst->expr;
        }

        // DotEval opcodes delegate method calls to AST — the expr contains the
        // full QoreDotEvalOperatorNode including argument expressions that may
        // reference locals.
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        // NOTE: AddAssignLValue/SubAssignLValue are QoreIRLValueInstruction, not
        // QoreIRExprInstruction — they are handled by isLValueOp() + line 1008
        // Elements/size ops delegate to AST
        // Map/select/cast ops delegate to AST
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::InvokeSimError:
        // Lvalue-modifying expression ops (modify variables via AST LValueHelper)
        // These delegate to AST and read local variables from the thread-local stack.
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        // Keys/exists/background ops delegate to AST
        case QoreIROpcode::BackgroundInt:
            if (qore_ir_inst_cast<const QoreIRBackgroundInstruction*>(inst)) {
                return nullptr;
            }
            return &static_cast<const QoreIRExprInstruction*>(inst)->expr;

        default:
            return nullptr;
    }
}

#ifndef QORE_IR_TAKE_OVER_UNIT

//! Extracts an expression that is kept only so AOT serialization can rebuild
//! call identity metadata. These expressions are not runtime AST fallbacks, but
//! their local references still need slots in serialized nested IR functions.
static const QoreValue* getSerializationOnlyInstructionExpr(const QoreIRInstruction* inst) {
    switch (inst->opcode) {
        case QoreIROpcode::CallDirect:
            return &static_cast<const QoreIRCallDirectInstruction*>(inst)->expr;
        case QoreIROpcode::CallMethodDirect: {
            const QoreValue& expr = static_cast<const QoreIRCallMethodDirectInstruction*>(inst)->expr;
            return expr ? &expr : nullptr;
        }
        case QoreIROpcode::InvokeMethodDirect: {
            const QoreValue& expr = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst)->expr;
            return expr ? &expr : nullptr;
        }
        case QoreIROpcode::CallStaticDirect:
            return &static_cast<const QoreIRCallStaticDirectInstruction*>(inst)->expr;
        case QoreIROpcode::DotEvalMethodDirect:
            return &static_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst)->expr;
        case QoreIROpcode::InvokeDotEvalMethodDirect:
            return &static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst)->expr;
        default:
            return nullptr;
    }
}

static void collectLocalsFromIRFunction(const QoreIRFunction& func,
        std::unordered_set<const void*>& ast_locals,
        bool& unknown_node_found, std::unordered_set<const QoreIRFunction*>& visited_handlers);

//! Collects the locals of \a func and of its handlers, each handler function once (they can be shared, #6087)
static void collectLocalsFromIRFunction(const QoreIRFunction& func,
        std::unordered_set<const void*>& ast_locals,
        bool& unknown_node_found, std::unordered_set<const QoreIRFunction*>& visited_handlers) {
    auto collect_context_exprs = [&](const QoreIRContextInstruction* ctx_inst) {
        collectLocalsFromExpr(ctx_inst->exp, ast_locals, unknown_node_found);
        collectLocalsFromExpr(ctx_inst->where_exp, ast_locals, unknown_node_found);
        collectLocalsFromExpr(ctx_inst->sort_exp, ast_locals, unknown_node_found);
    };

    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode == QoreIROpcode::LoadLocal ||
                    inst->opcode == QoreIROpcode::StoreLocal ||
                    inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* linst = static_cast<const QoreIRLocalInstruction*>(inst.get());
                if (linst->local) {
                    ast_locals.insert(reinterpret_cast<const void*>(linst->local));
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst.get());
                if (fused->target) {
                    ast_locals.insert(reinterpret_cast<const void*>(fused->target));
                }
                if (fused->source) {
                    ast_locals.insert(reinterpret_cast<const void*>(fused->source));
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst.get());
                if (fused->local) {
                    ast_locals.insert(reinterpret_cast<const void*>(fused->local));
                }
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst.get());
                if (fused->lhs) {
                    ast_locals.insert(reinterpret_cast<const void*>(fused->lhs));
                }
                if (fused->rhs) {
                    ast_locals.insert(reinterpret_cast<const void*>(fused->rhs));
                }
            }

            if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                auto* pi = static_cast<const QoreIRLValuePathInstruction*>(inst.get());
                for (const auto& step : pi->path) {
                    if ((step.kind == LVPathStepKind::LocalVar
                            || step.kind == LVPathStepKind::ClosureVar)
                            && step.ref_ptr) {
                        ast_locals.insert(step.ref_ptr);
                    }
                }
            }

            if (isLValueOp(inst->opcode)) {
                auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst.get());
                collectLocalsFromExpr(lvinst->lvalue, ast_locals, unknown_node_found);
            }
            if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                collectLocalsFromExpr(*expr, ast_locals, unknown_node_found);
            }

            if (inst->opcode == QoreIROpcode::Find) {
                collectLocalsFromFindInstruction(
                    static_cast<const QoreIRFindInstruction*>(inst.get()),
                    ast_locals, unknown_node_found);
            }

            if (inst->opcode == QoreIROpcode::Context) {
                collect_context_exprs(static_cast<const QoreIRContextInstruction*>(inst.get()));
            }

            if (inst->opcode == QoreIROpcode::OnBlockExit) {
                auto* obe_inst = static_cast<const QoreIROnBlockExitInstruction*>(inst.get());
                if (obe_inst->stmt) {
                    collectLocalsFromStatementBlock(obe_inst->stmt->getCode(),
                            ast_locals, unknown_node_found);
                }
                if (obe_inst->handler_ir && visited_handlers.insert(obe_inst->handler_ir.get()).second) {
                    collectLocalsFromIRFunction(*obe_inst->handler_ir,
                            ast_locals, unknown_node_found, visited_handlers);
                }
            }
        }
    }
}

bool QoreIRFunction::isDirectParamsRuntimeSafe() const {
    if (!direct_params_eligible) {
        return false;
    }

    for (const auto& i : param_local_vars) {
        const LocalVar* lv = i.second;
        if (lv && lv->closureUse()) {
            return false;
        }
    }

    return true;
}

// documented in QoreIR.h
bool qore_ir_local_may_hold_reference(const LocalVar* lv) {
    if (!lv) {
        return false;
    }
    const QoreTypeInfo* ti = lv->getTypeInfo();
    return !QoreTypeInfo::hasType(ti) || QoreTypeInfo::isReference(ti);
}

// documented in QoreIR.h
bool qore_ir_local_is_written(const QoreIRFunction& ir_func, const LocalVar* lv) {
    if (!lv) {
        return false;
    }
    const void* key = reinterpret_cast<const void*>(lv);
    return ir_func.stored_locals.count(key) || ir_func.weak_store_locals.count(key)
        || ir_func.lvalue_path_locals.count(key) || ir_func.cow_container_locals.count(key);
}

// documented in QoreIR.h
bool QoreIRFunction::detectCreatesReference() const {
    std::unordered_set<const void*> ignored_locals;
    bool unknown_node_found = false;
    bool ref_created = false;
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (ref_created || unknown_node_found) {
                return true;
            }
            // reference-creating opcodes, plus opcodes whose AST bodies are walked by helpers
            // that cannot report reference creation
            if (inst->opcode == QoreIROpcode::CreateParseRef
                    || inst->opcode == QoreIROpcode::RefForeachInit
                    || inst->opcode == QoreIROpcode::Find
                    || inst->opcode == QoreIROpcode::OnBlockExit
                    || isDelegateToASTStatement(inst->opcode)) {
                return true;
            }
            if (isLValueOp(inst->opcode)) {
                auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst.get());
                collectLocalsFromExpr(lvinst->lvalue, ignored_locals, unknown_node_found,
                        &ref_created);
            }
            if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                collectLocalsFromExpr(*expr, ignored_locals, unknown_node_found, &ref_created);
            }
            if (inst->opcode == QoreIROpcode::Context) {
                auto* ctx_inst = static_cast<const QoreIRContextInstruction*>(inst.get());
                collectLocalsFromExpr(ctx_inst->exp, ignored_locals, unknown_node_found,
                        &ref_created);
                collectLocalsFromExpr(ctx_inst->where_exp, ignored_locals, unknown_node_found,
                        &ref_created);
                collectLocalsFromExpr(ctx_inst->sort_exp, ignored_locals, unknown_node_found,
                        &ref_created);
            }
        }
    }
    return ref_created || unknown_node_found;
}

//! Returns true if an instruction with this opcode never gives a reference as its value
/** A reference is a value only where <tt>\var</tt> creates it, where a container holds it (an element, a member),
    and where a call, an AST expression, an argument or a phi passes one on; a load of a variable dereferences a
    reference it holds.  This list is the opposite: the opcodes whose value is known not to be one.  An opcode that is
    not listed is treated as one that can give a reference, which is safe: it only keeps a local of no reference type
    assigned its value off the IR-only path (see QoreIRFunction::computeReferenceStoreLocals()).
*/
static bool qore_ir_opcode_never_gives_reference(QoreIROpcode op) {
    switch (op) {
        // constants
        case QoreIROpcode::ConstInt:
        case QoreIROpcode::ConstFloat:
        case QoreIROpcode::ConstBool:
        case QoreIROpcode::ConstBoolBoxed:
        case QoreIROpcode::ConstNothing:
        case QoreIROpcode::ConstNull:
        case QoreIROpcode::ConstString:
        case QoreIROpcode::ConstDate:
        case QoreIROpcode::ConstEnum:
        case QoreIROpcode::ConstChar:
        case QoreIROpcode::LoadConstant:
        // loads of variables, which dereference a reference that the variable holds
        case QoreIROpcode::LoadLocal:
        case QoreIROpcode::LoadGlobal:
        case QoreIROpcode::LoadThreadLocal:
        // new containers, objects and code values
        case QoreIROpcode::MakeList:
        case QoreIROpcode::MakeHash:
        case QoreIROpcode::MakeHashConstKeys:
        case QoreIROpcode::CreateEmptyList:
        case QoreIROpcode::CreateSizedList:
        case QoreIROpcode::NewHashDecl:
        case QoreIROpcode::NewHashDeclFromHash:
        case QoreIROpcode::NewComplexHash:
        case QoreIROpcode::NewComplexList:
        case QoreIROpcode::NewComplexBuffer:
        case QoreIROpcode::NewObject:
        case QoreIROpcode::CreateClosure:
        case QoreIROpcode::CreateCallRef:
        case QoreIROpcode::CreateMethodRef:
        // arithmetic, bit and string operations
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::AddString:
        case QoreIROpcode::AddNumber:
        case QoreIROpcode::AddTimeout:
        case QoreIROpcode::StringConcat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::SubNumber:
        case QoreIROpcode::SubTimeout:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::MulNumber:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::DivNumber:
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
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::IncrementLocalInt:
        case QoreIROpcode::AddAssignLocalInt:
        // compound assignments computing a variable's new value from the value loaded, which is dereferenced
        // (AddAssignAny is not listed: += on a variable with no value assigns the value added as it is, which can
        // be a reference; see doPlusEqualsOnLValue() and qore_ir_opcode_forwarded_operand())
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
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
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        // comparisons and conversions
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::EqString:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::NeString:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtString:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeString:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtString:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeString:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpString:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::IsCollectionType:
        case QoreIROpcode::ToString:
        case QoreIROpcode::ToInt:
        case QoreIROpcode::ToFloat:
        case QoreIROpcode::ToStringChecked:
        case QoreIROpcode::ToIntChecked:
        case QoreIROpcode::ToFloatChecked:
        case QoreIROpcode::Sprintf:
        case QoreIROpcode::SprintfIntFixed:
        // sizes and string results
        case QoreIROpcode::ListSize:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::Backquote:
        case QoreIROpcode::StringJoinStart:
        case QoreIROpcode::StringJoinAppend:
        case QoreIROpcode::FoldlStringJoin:
        case QoreIROpcode::FoldrStringJoin:
        case QoreIROpcode::ListStringJoin:
        case QoreIROpcode::ListStringJoinIdentityMap:
        case QoreIROpcode::StringMethodJoinStart:
        case QoreIROpcode::ListIntSprintfJoin:
        case QoreIROpcode::AppendStringCow:
        // a push gives the list
        case QoreIROpcode::ListPush:
            return true;
        default:
            return false;
    }
}

//! Returns the index of the operand whose value an instruction with this opcode can give as its own, or -1
/** The value of such an instruction is a reference if that operand is one: a phi gives one of its incoming values
    (handled separately), a guard its operand, and AddAssignAny (<tt>x += y</tt> computed as a value to store) gives
    its right operand as it is when the variable has no value, as doPlusEqualsOnLValue() assigns it, so that
    <tt>auto t; t += h.r;</tt> makes \c t hold the reference in \c h.r, as in the AST interpreter.
*/
static int qore_ir_opcode_forwarded_operand(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::GuardType:
        case QoreIROpcode::GuardNotNothing:
        case QoreIROpcode::RefSelf:
            return 0;
        case QoreIROpcode::AddAssignAny:
            return 1;
        default:
            return -1;
    }
}

// documented in QoreIR.h
void QoreIRFunction::computeReferenceStoreLocals() {
    reference_store_locals.clear();
    reference_element_locals.clear();
    non_reference_values.clear();
    // the instruction that defines each value
    std::unordered_map<uint32_t, const QoreIRInstruction*> defs;
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->result.isValid()) {
                defs[inst->result.id] = inst.get();
            }
        }
    }
    // the values that can be a reference: a fixed point, as a phi or a guard gives a reference if one of its
    // operands can (a value with no definition, such as a parameter value, can be one)
    std::unordered_set<uint32_t> may_ref;
    // the instructions whose value is a reference if one of their operands is one, with the index of that operand
    // (-1 for a phi, whose incoming values are its operands)
    std::vector<std::pair<const QoreIRInstruction*, int>> forwarders;
    for (const auto& [id, def] : defs) {
        if (def->opcode == QoreIROpcode::Phi) {
            forwarders.emplace_back(def, -1);
            continue;
        }
        // an instruction that can throw in a try block is an invoke, whose value is that of its operation
        QoreIROpcode op = def->opcode == QoreIROpcode::Invoke
            ? static_cast<const QoreIRInvokeInstruction*>(def)->invoke_opcode : def->opcode;
        int forwarded = qore_ir_opcode_forwarded_operand(op);
        if (forwarded >= 0) {
            forwarders.emplace_back(def, forwarded);
        } else if (!qore_ir_opcode_never_gives_reference(op)) {
            may_ref.insert(id);
        }
    }
    // a value whose facts show a representation or a type that cannot be a reference is none, whatever produced it
    // (ex: the int element of a list returned by a function of the same batch, which the facts give after projection)
    auto facts_exclude_reference = [&](uint32_t id) -> bool {
        const QoreIRValueFacts* facts = getValueFacts(QoreIRValue(id));
        if (!facts) {
            return false;
        }
        if (facts->representation == QoreIRValueRepresentation::NativeInt
                || facts->representation == QoreIRValueRepresentation::NativeFloat
                || facts->representation == QoreIRValueRepresentation::NativeBool) {
            return true;
        }
        return facts->type_info && QoreTypeInfo::hasType(facts->type_info)
            && QoreTypeInfo::parseReturns(facts->type_info, NT_REFERENCE) == QTI_NOT_EQUAL;
    };
    auto may_be_reference = [&](uint32_t id) -> bool {
        return (may_ref.count(id) || !defs.count(id)) && !facts_exclude_reference(id);
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto& [def, forwarded] : forwarders) {
            if (may_ref.count(def->result.id)) {
                continue;
            }
            bool any = false;
            if (forwarded < 0) {
                for (const auto& in : static_cast<const QoreIRPhiInstruction*>(def)->incoming) {
                    if (may_be_reference(in.value.id)) {
                        any = true;
                        break;
                    }
                }
            } else {
                // an operand that is missing is unknown
                any = def->operands.size() <= static_cast<size_t>(forwarded)
                    || may_be_reference(def->operands[forwarded].id);
            }
            if (any) {
                may_ref.insert(def->result.id);
                changed = true;
            }
        }
    }
    // the values known never to be a reference, for the engines (see non_reference_values)
    for (const auto& [id, def] : defs) {
        if (!may_ref.count(id)) {
            if (non_reference_values.size() <= id) {
                non_reference_values.resize(static_cast<size_t>(id) + 1, 0);
            }
            non_reference_values[id] = 1;
        }
    }
    // the variables of foreach loops that can be given a reference element (reference_element_locals), and the
    // locals that a store other than that of a foreach loop assigns
    std::unordered_set<const void*> normal_store_locals;
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode != QoreIROpcode::StoreLocal || inst->operands.empty()) {
                continue;
            }
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst.get());
            // a weak or opaque store assigns no reference
            if (!linst->local || (linst->mode != AssignmentMode::Normal && linst->mode != AssignmentMode::Element)
                    || !qore_ir_local_may_hold_reference(linst->local)
                    || QoreTypeInfo::isReference(linst->local->getTypeInfo())) {
                continue;
            }
            const void* key = reinterpret_cast<const void*>(linst->local);
            bool may_be_ref = may_be_reference(inst->operands.front().id);
            if (linst->mode == AssignmentMode::Element) {
                if (may_be_ref) {
                    reference_element_locals.insert(key);
                }
                continue;
            }
            normal_store_locals.insert(key);
            if (may_be_ref) {
                reference_store_locals.insert(key);
            }
        }
    }
    // the variable of a foreach loop, assigned each element as it is (AssignmentMode::Element), reads a reference
    // element through its own mechanism (see QoreIRToLLVM::loadLocalCacheValue() and IRLocalSlotCache), but a store
    // to it in the loop (v = 5) assigns the referenced lvalue, as in the AST interpreter, so such a variable is one
    // that a store can give a reference
    for (const void* key : reference_element_locals) {
        if (normal_store_locals.count(key)) {
            reference_store_locals.insert(key);
        }
    }
}

// documented in QoreIR.h
size_t QoreIRFunction::refineReferenceLocalsFromFacts() {
    if (reference_store_locals.empty()) {
        return 0;
    }
    std::unordered_set<const void*> before = reference_store_locals;
    computeReferenceStoreLocals();
    size_t changed = 0;
    for (const void* key : before) {
        if (reference_store_locals.count(key)) {
            continue;
        }
        // a reference slot local that no store can give a reference any longer is an IR-only local; a local on the
        // runtime stack stays there
        if (reference_slot_locals.erase(key)) {
            ir_only_locals.insert(key);
        }
        ++changed;
    }
    if (!changed) {
        return 0;
    }
    // the instructions' is_ref follows (see computeIROnlyLocals())
    bool creates_reference = detectCreatesReference();
    std::unordered_set<const void*> body_local_keys = buildBodyLocalKeys();
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (auto* local_inst = qore_ir_inst_cast<QoreIRLocalInstruction*>(inst.get())) {
                if (local_inst->local) {
                    local_inst->is_ref = localMayHoldReference(local_inst->local, creates_reference,
                        body_local_keys);
                }
            }
        }
    }
    return changed;
}

// documented in QoreIR.h
std::unordered_set<const void*> QoreIRFunction::buildBodyLocalKeys() const {
    std::unordered_set<const void*> keys;
    keys.reserve(all_body_locals.size());
    for (const LocalVar* lv : all_body_locals) {
        keys.insert(reinterpret_cast<const void*>(lv));
    }
    return keys;
}

// documented in QoreIR.h
bool QoreIRFunction::localMayHoldReference(const LocalVar* lv, bool creates_reference,
        const std::unordered_set<const void*>& body_local_keys) const {
    if (!qore_ir_local_may_hold_reference(lv)) {
        return false;
    }
    if (QoreTypeInfo::isReference(lv->getTypeInfo()) || creates_reference
            || reference_store_locals.count(reinterpret_cast<const void*>(lv))) {
        return true;
    }
    // An untyped local can still receive a reference from outside this function if it is a
    // parameter, because the caller decides what it is given.  A top-level (program-wide)
    // local is never a parameter, and neither is a body local; both can only ever hold a
    // reference created in this function, which is already ruled out above.
    if (lv->isTopLevel()) {
        return false;
    }
    return !body_local_keys.count(reinterpret_cast<const void*>(lv));
}

bool QoreIRFunction::collectHandlerLocals(std::unordered_set<const void*>& locals) const {
    bool unknown_node_found = false;
    std::unordered_set<const QoreIRFunction*> visited_handlers;
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (!inst || inst->opcode != QoreIROpcode::OnBlockExit) {
                continue;
            }
            auto* obe_inst = static_cast<const QoreIROnBlockExitInstruction*>(inst.get());
            if (obe_inst->stmt) {
                collectLocalsFromStatementBlock(obe_inst->stmt->getCode(), locals, unknown_node_found);
            }
            if (obe_inst->handler_ir && visited_handlers.insert(obe_inst->handler_ir.get()).second) {
                collectLocalsFromIRFunction(*obe_inst->handler_ir, locals, unknown_node_found, visited_handlers);
            }
        }
    }
    return !unknown_node_found;
}

bool QoreIRFunction::collectOnBlockExitLocals(const QoreIROnBlockExitInstruction& obe,
        std::unordered_set<const void*>& locals) {
    const StatementBlock* code = obe.stmt ? obe.stmt->getCode() : nullptr;
    if (obe.handler_ir) {
        return obe.handler_ir->collectHandlerBodyLocals(code, locals);
    }
    bool unknown_node_found = false;
    collectLocalsFromStatementBlock(code, locals, unknown_node_found);
    return !unknown_node_found;
}

bool QoreIRFunction::collectHandlerBodyLocals(const StatementBlock* code,
        std::unordered_set<const void*>& locals) const {
    bool unknown_node_found = false;
    collectLocalsFromStatementBlock(code, locals, unknown_node_found);
    std::unordered_set<const QoreIRFunction*> visited_handlers{this};
    collectLocalsFromIRFunction(*this, locals, unknown_node_found, visited_handlers);
    return !unknown_node_found;
}

void QoreIRFunction::computeIROnlyLocals() {
    // the write inventories are recomputed: the take-over analysis cached from them for the AOT IR is computed again
    // when it is needed (see take_over_params_state)
    take_over_params_state.store(0, std::memory_order_release);
    // the locals that a reference can reach other than from \var in this function: they are reference-capable
    // (localMayHoldReference()), and those that would otherwise be IR-only keep the reference in their own storage
    // (reference_slot_locals)
    computeReferenceStoreLocals();
    ir_only_locals.clear();
    reference_slot_locals.clear();
    real_reference_locals.clear();
    real_reference_ir_only_locals.clear();
    cow_container_locals.clear();
    lvalue_path_locals.clear();
    std::unordered_set<const void*> ensure_unique_roots;
    weak_store_locals.clear();
    element_store_locals.clear();
    stored_locals.clear();
    ast_referenced_locals.clear();
    non_structured_ast_referenced_locals.clear();
    // the handler functions walked, each once (they can be shared, #6087)
    std::unordered_set<const QoreIRFunction*> visited_handlers;
    has_opaque_ast_local_access = false;
    has_explicit_self_local_access = false;

    // Collect all locals referenced by LoadLocal/StoreLocal/UninstantiateLocal
    std::unordered_set<const void*> all_locals;

    // Collect all locals referenced by AST expression trees in Call/Invoke/Lvalue
    // instructions.  These locals are accessed through the runtime stack (not through
    // LoadLocal/StoreLocal) and MUST remain AST-visible.
    // Track whether the function has any delegate-to-AST statements
    bool has_delegate_to_ast = false;

    // If the AST walker encounters an unknown node type, we conservatively
    // mark ALL locals as AST-visible.
    bool unknown_node_found = false;

    // Set if the function can create a reference (\var) anywhere.  Untyped ("auto") locals can
    // only ever hold a reference if one is created in the same function: reading any variable
    // that holds a reference dereferences it, and references arriving in untyped parameters
    // land in parameter locals, which are not body locals.  When no reference is created here,
    // untyped body locals cannot alias anything and stay eligible for IR-only classification.
    bool reference_created = false;
    auto collect_context_exprs = [&](const QoreIRContextInstruction* ctx_inst) {
        collectLocalsFromExpr(ctx_inst->exp, ast_referenced_locals, unknown_node_found,
                &reference_created);
        collectLocalsFromExpr(ctx_inst->where_exp, ast_referenced_locals, unknown_node_found,
                &reference_created);
        collectLocalsFromExpr(ctx_inst->sort_exp, ast_referenced_locals, unknown_node_found,
                &reference_created);
    };

    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            // Collect locals from LoadLocal/StoreLocal/UninstantiateLocal
            if (inst->opcode == QoreIROpcode::LoadLocal ||
                    inst->opcode == QoreIROpcode::StoreLocal ||
                    inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* linst = static_cast<const QoreIRLocalInstruction*>(inst.get());
                if (linst->local) {
                    all_locals.insert(reinterpret_cast<const void*>(linst->local));
                    if (inst->opcode == QoreIROpcode::StoreLocal) {
                        stored_locals.insert(reinterpret_cast<const void*>(linst->local));
                    }
                    // A weak (":=") or opaque store assigns through LValueHelper, which
                    // requires the variable on the runtime local stack; an element store keeps
                    // the element in the variable's cache if the variable is IR-only
                    if (inst->opcode == QoreIROpcode::StoreLocal) {
                        if (linst->mode == AssignmentMode::Element) {
                            element_store_locals.insert(reinterpret_cast<const void*>(linst->local));
                        } else if (linst->mode != AssignmentMode::Normal) {
                            weak_store_locals.insert(reinterpret_cast<const void*>(linst->local));
                        }
                    }
                }
            }
            // Collect locals from fused local int instructions
            if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst.get());
                if (fused->target) {
                    all_locals.insert(reinterpret_cast<const void*>(fused->target));
                }
                if (fused->source) {
                    all_locals.insert(reinterpret_cast<const void*>(fused->source));
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst.get());
                if (fused->local) {
                    all_locals.insert(reinterpret_cast<const void*>(fused->local));
                }
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst.get());
                if (fused->lhs) {
                    all_locals.insert(reinterpret_cast<const void*>(fused->lhs));
                }
                if (fused->rhs) {
                    all_locals.insert(reinterpret_cast<const void*>(fused->rhs));
                }
            }

            // LValuePath operations: locals in path steps are accessed through the
            // runtime variable stack (via navigatePath), not through LoadLocal/StoreLocal.
            // They must NOT be classified as IR-only.
            if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                auto* pi = static_cast<const QoreIRLValuePathInstruction*>(inst.get());
                for (const auto& step : pi->path) {
                    if ((step.kind == LVPathStepKind::LocalVar
                            || step.kind == LVPathStepKind::ClosureVar)
                            && step.ref_ptr) {
                        // the unique step of a weak assignment from an untyped local (qore#5847) does not move the
                        // local to the runtime stack: the local keeps its IR storage if it has one, and the step
                        // changes the value in that storage (qore_ir_unique_container()), so that the lifetime of
                        // an object that the local holds does not depend on the weak assignment
                        if (inst->opcode == QoreIROpcode::LValuePathUnary
                                && static_cast<const QoreIRLValuePathInstruction*>(inst.get())->unary_op
                                    == LVUnaryOp::EnsureUnique
                                && pi->path.size() == 1 && step.kind == LVPathStepKind::LocalVar) {
                            const LocalVar* lv = static_cast<const LocalVar*>(step.ref_ptr);
                            const QoreTypeInfo* lti = lv->getTypeInfo();
                            bool body_local = false;
                            for (const LocalVar* body_lv : all_body_locals) {
                                if (body_lv == lv) {
                                    body_local = true;
                                    break;
                                }
                            }
                            if (body_local && !lv->closureUse() && !lv->isTopLevel()
                                    && qore_weak_source_type_may_hold_container(lti)) {
                                ensure_unique_roots.insert(step.ref_ptr);
                                continue;
                            }
                        }
                        lvalue_path_locals.insert(step.ref_ptr);
                    }
                }
            }

            // HashKeyStore/HashKeyStoreDynamic/ListIndexStore can trigger a
            // copy-on-write in their helper, which calls qore_rt_assign_local
            // to install the COW'd container back into the runtime slot. The
            // LLVM codegen explicitly calls reloadLocalFromRuntime() after the
            // helper so subsequent LoadLocal(container) reads the updated
            // hash/list — but reloadLocalFromRuntime() early-exits for IR-only
            // locals (the alloca cache is assumed authoritative). Mark these
            // container locals AST-visible so COW updates propagate back to
            // the alloca.
            if (inst->opcode == QoreIROpcode::HashKeyStore) {
                auto* hks = static_cast<const QoreIRHashKeyStoreInstruction*>(inst.get());
                if (hks->container_lv) {
                    cow_container_locals.insert(
                        reinterpret_cast<const void*>(hks->container_lv));
                } else if (hks->container && hks->container->ref.id) {
                    cow_container_locals.insert(hks->container->ref.id);
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStoreDynamic) {
                auto* hksd = static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst.get());
                if (hksd->container_lv) {
                    cow_container_locals.insert(
                        reinterpret_cast<const void*>(hksd->container_lv));
                } else if (hksd->container && hksd->container->ref.id) {
                    cow_container_locals.insert(hksd->container->ref.id);
                }
            } else if (inst->opcode == QoreIROpcode::ListIndexStore) {
                auto* lis = static_cast<const QoreIRListIndexStoreInstruction*>(inst.get());
                if (lis->container_lv) {
                    cow_container_locals.insert(
                        reinterpret_cast<const void*>(lis->container_lv));
                } else if (lis->container && lis->container->ref.id) {
                    cow_container_locals.insert(lis->container->ref.id);
                }
            }

            // Lvalue operations: walk the lvalue AST expression for local references
            if (isLValueOp(inst->opcode)) {
                auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst.get());
                collectLocalsFromExpr(lvinst->lvalue, ast_referenced_locals,
                        unknown_node_found, &reference_created);
            }

            // Walk the expr AST tree for any instruction type that stores one
            if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                collectLocalsFromExpr(*expr, ast_referenced_locals,
                        unknown_node_found, &reference_created);
            }

            // Reference-creating instructions, plus instructions whose AST bodies are walked by
            // helpers that do not report reference creation: all of these are treated as
            // creating a reference so untyped locals in the function stay AST-visible.
            if (inst->opcode == QoreIROpcode::CreateParseRef
                    || inst->opcode == QoreIROpcode::RefForeachInit
                    || inst->opcode == QoreIROpcode::Find
                    || inst->opcode == QoreIROpcode::OnBlockExit) {
                reference_created = true;
            }

            if (inst->opcode == QoreIROpcode::Find) {
                collectLocalsFromFindInstruction(
                    static_cast<const QoreIRFindInstruction*>(inst.get()),
                    ast_referenced_locals, unknown_node_found);
            }

            // ContextInit evaluates its context data, where, and sort
            // expressions through the AST runtime helper; locals referenced by
            // those expressions must therefore remain visible on the runtime
            // local stack and cannot be classified as IR-only.
            if (inst->opcode == QoreIROpcode::Context) {
                collect_context_exprs(static_cast<const QoreIRContextInstruction*>(inst.get()));
            }

            if (isDelegateToASTStatement(inst->opcode)) {
                has_delegate_to_ast = true;
                reference_created = true;
            }

            // OnBlockExit: walk handler body AST to find referenced locals.
            // Only those locals must remain AST-visible — others can be IR-only.
            if (inst->opcode == QoreIROpcode::OnBlockExit) {
                auto* obe_inst = static_cast<const QoreIROnBlockExitInstruction*>(inst.get());
                if (obe_inst->stmt) {
                    collectLocalsFromStatementBlock(obe_inst->stmt->getCode(),
                            ast_referenced_locals, unknown_node_found);
                }
                if (obe_inst->handler_ir && visited_handlers.insert(obe_inst->handler_ir.get()).second) {
                    collectLocalsFromIRFunction(*obe_inst->handler_ir,
                            ast_referenced_locals, unknown_node_found, visited_handlers);
                }
            }
        }
    }

    // A constructor's written argument values belong to its parameter/argv cleanup, after the evaluator records
    // successful body completion (#6086). If they lived only in the IR slot cache, execute() would release them
    // before returning and an argument's destructor error would be mistaken for a failed constructor body.
    // Read-only arguments and types that cannot hold destructible objects keep their existing classification.
    if (const auto* ctor = dynamic_cast<const UserConstructorVariant*>(source_variant)) {
        const UserSignature* sig = ctor->UserVariantBase::getUserSignature();
        auto keep_written_argument = [&](const LocalVar* local) {
            if (local && stored_locals.count(local) && QoreTypeInfo::needsScan(local->getTypeInfo())) {
                ast_referenced_locals.insert(local);
            }
        };
        size_t argument_count = 0;
        for (LocalVar* local : sig->lv) {
            if (++argument_count % 100 == 0
                    && qore_check_cancel(nullptr, "IR constructor argument ownership analysis")) {
                // Take the existing conservative AST-visible path below if compilation is cancelled.
                unknown_node_found = true;
                break;
            }
            keep_written_argument(local);
        }
        keep_written_argument(sig->argvid);
    }

    non_structured_ast_referenced_locals = ast_referenced_locals;
    ast_referenced_locals.insert(
        cow_container_locals.begin(), cow_container_locals.end());
    ast_referenced_locals.insert(
        lvalue_path_locals.begin(), lvalue_path_locals.end());

    // Store total local count for Phase 4 optimization (selective reload skip)
    total_local_count = all_locals.size();

    // If the function has delegate-to-AST statements, ALL locals are AST-visible
    // because the AST execution could access any local through the thread-local stack.
    if (has_delegate_to_ast) {
        has_opaque_ast_local_access = true;
        ast_visible_body_locals = all_body_locals;
        printd(5, "computeIROnlyLocals '%s': has_delegate_to_ast, all AST-visible\n", name.c_str());
        return;  // ir_only_locals stays empty — all locals are AST-visible
    }

    // If the AST walker hit an unknown node type, conservatively treat all locals
    // as AST-visible.  This ensures correctness even for unhandled AST structures.
    if (unknown_node_found) {
        has_opaque_ast_local_access = true;
        ast_visible_body_locals = all_body_locals;
        printd(5, "computeIROnlyLocals '%s': unknown_node_found, all AST-visible\n", name.c_str());
        return;
    }

    printd(5, "computeIROnlyLocals '%s': all_locals=%d ast_referenced=%d\n",
        name.c_str(), (int)all_locals.size(), (int)ast_referenced_locals.size());

    // Body locals are the only locals eligible for the untyped-local narrowing below;
    // parameters (and self/argv) receive their value from the caller and stay conservative.
    std::unordered_set<const void*> body_local_keys;
    body_local_keys.reserve(all_body_locals.size());
    for (const LocalVar* body_lv : all_body_locals) {
        body_local_keys.insert(reinterpret_cast<const void*>(body_lv));
    }

    // A local is IR-only if:
    // 1. It appears in LoadLocal/StoreLocal/UninstantiateLocal (the IR access path)
    // 2. It is NOT referenced by any AST expression tree (Call/Invoke/Lvalue exprs)
    // 3. It cannot hold a reference (which can alias other variables)
    for (const void* key : all_locals) {
        const LocalVar* lv = reinterpret_cast<const LocalVar*>(key);
        if (lv && lv->isSelf()) {
            has_explicit_self_local_access = true;
        }
        // Local appears in AST expression trees — must stay AST-visible
        if (ast_referenced_locals.count(key)) {
            printd(5, "  local '%s' (%p): AST-referenced (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        if (cow_container_locals.count(key)) {
            printd(5, "  local '%s' (%p): COW container (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        if (lvalue_path_locals.count(key)) {
            printd(5, "  local '%s' (%p): lvalue path (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Weak (":=") store targets are assigned through LValueHelper, which
        // resolves the variable on the runtime local stack; LocalVar::getLValue()
        // fails for a local that was never instantiated there.
        if (weak_store_locals.count(key)) {
            printd(5, "  local '%s' (%p): weak store target (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Closure-captured locals use the closure variable stack (not the regular
        // local variable stack).  qore_rt_assign_local routes through the closure
        // stack when closure_use is true, so we must NOT skip it.
        if (lv->closureUse()) {
            printd(5, "  local '%s' (%p): closure-captured (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Top-level locals are program-wide lexical variables.  A function can
        // read or write them without an AST subtree in the current function,
        // so intra-function IR-only analysis cannot prove runtime-stack sync
        // is unnecessary.
        if (lv->isTopLevel()) {
            printd(5, "  local '%s' (%p): top-level (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // Locals that can hold a reference alias other variables, so a write to one has to go
        // through the runtime local stack to reach the aliased variable — that requires the
        // local to stay AST-visible.  A declared reference always needs this.  An untyped
        // ("auto") local needs it only when a reference can actually reach it AND it is
        // written: a body local can only receive a reference from a \var created in this same
        // function (reading a variable that holds one dereferences it), a parameter receives
        // whatever the caller passes, and a read of either yields the dereferenced value even
        // on the private path.
        if (QoreTypeInfo::isReference(lv->getTypeInfo())
                || (localMayHoldReference(lv, reference_created, body_local_keys)
                    && qore_ir_local_is_written(*this, lv))) {
            // a body local that a reference reaches only as a value stored in it (an element of a container, the
            // value of a call), in a function that creates no reference, keeps its own storage: the reference it
            // holds is the value in its slot or native cache, which every read reads through and every store
            // writes through (see reference_slot_locals); no variable of the function can be the referenced
            // lvalue, as a reference to one is created only by \var in the function.  The variable of a foreach
            // loop, assigned each element as it is, keeps the runtime path.
            if (!reference_created && !QoreTypeInfo::isReference(lv->getTypeInfo()) && !lv->isTopLevel()
                    && body_local_keys.count(key) && reference_store_locals.count(key)
                    && !element_store_locals.count(key)) {
                printd(5, "  local '%s' (%p): reference slot (IR storage)\n", lv->getName(), key);
                reference_slot_locals.insert(key);
                continue;
            }
            printd(5, "  local '%s' (%p): may hold a reference (non-IR-only)\n", lv->getName(), key);
            continue;
        }
        // A body local of an object type (a class, object, also as an optional type: *T) keeps its value in IR
        // storage, where the object it holds has a real reference of its own, as on the runtime stack (see
        // real_reference_locals); a parameter stays on the runtime stack
        if (QoreTypeInfo::getBaseType(get_or_nothing_base_type(lv->getTypeInfo())) == NT_OBJECT) {
            if (body_local_keys.count(key)) {
                printd(5, "  local '%s' (%p): object type (IR storage, real reference)\n", lv->getName(), key);
                real_reference_locals.insert(key);
            } else {
                printd(5, "  local '%s' (%p): object type parameter (non-IR-only)\n", lv->getName(), key);
            }
            continue;
        }
        printd(5, "  local '%s' (%p): IR-ONLY\n", lv->getName(), key);
        ir_only_locals.insert(key);
        // a body local of no type can hold an object, which then has a real reference from its storage, as on the
        // runtime stack (see real_reference_ir_only_locals)
        const QoreTypeInfo* lti = lv->getTypeInfo();
        if (body_local_keys.count(key)
                && (!QoreTypeInfo::hasType(lti) || lti == anyTypeInfo || lti == autoNoNarrowTypeInfo)) {
            real_reference_ir_only_locals.insert(key);
        }
    }

    // the root of a unique step that has no IR storage is reached through the runtime stack, as any other path root
    for (const void* key : ensure_unique_roots) {
        if (!hasIRStorage(key)) {
            lvalue_path_locals.insert(key);
            ast_referenced_locals.insert(key);
        }
    }

    // Build filtered instantiation list: body locals NOT classified as IR-only, which have no storage on the runtime
    // stack
    ast_visible_body_locals.clear();
    for (LocalVar* lv : all_body_locals) {
        if (!hasIRStorage(reinterpret_cast<const void*>(lv))) {
            ast_visible_body_locals.push_back(lv);
        }
    }

    // Re-embed is_ref now that the body-local inventory is known.  computeSlotIdsAndEmbed()
    // can run before all_body_locals is populated, and an untyped body local is then
    // indistinguishable from a parameter, so it has to be marked reference-capable there —
    // which would disable every optimization keyed off is_ref for ordinary "auto" locals.
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (auto* local_inst = qore_ir_inst_cast<QoreIRLocalInstruction*>(inst.get())) {
                if (local_inst->local) {
                    local_inst->is_ref = localMayHoldReference(local_inst->local,
                        reference_created, body_local_keys);
                }
            }
        }
    }

}

QoreIRFunction::ContextUsage QoreIRFunction::getContextUsage(
        const LocalVar* argvid, const LocalVar* selfid,
        std::unordered_set<const QoreIRFunction*>* visited_handlers) const {
    std::unordered_set<const QoreIRFunction*> own_visited_handlers;
    if (!visited_handlers) {
        visited_handlers = &own_visited_handlers;
    }
    ContextUsage usage;
    if (has_opaque_ast_local_access) {
        usage.argv = argvid != nullptr;
        usage.self = selfid != nullptr;
    }

    const void* argv_key = reinterpret_cast<const void*>(argvid);
    const void* self_key = reinterpret_cast<const void*>(selfid);
    if (argvid && (argvid->closureUse() || ast_referenced_locals.count(argv_key))) {
        usage.argv = true;
    }
    if (selfid && (selfid->closureUse() || ast_referenced_locals.count(self_key)
            || has_explicit_self_local_access)) {
        usage.self = true;
    }

    size_t check_count = 0;
    for (const auto& block : blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (++check_count % 100 == 0
                    && qore_check_cancel(nullptr, "IR call context analysis")) {
                usage.argv = argvid != nullptr;
                usage.self = selfid != nullptr;
                return usage;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                const LocalVar* lv =
                    static_cast<const QoreIRLocalInstruction*>(inst)->local;
                usage.argv |= argvid && lv == argvid;
                usage.self |= selfid && lv == selfid;
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                const auto* local =
                    static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                usage.argv |= argvid
                    && (local->target == argvid || local->source == argvid);
                usage.self |= selfid
                    && (local->target == selfid || local->source == selfid);
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                const LocalVar* lv =
                    static_cast<const QoreIRIncrementLocalIntInstruction*>(inst)->local;
                usage.argv |= argvid && lv == argvid;
                usage.self |= selfid && lv == selfid;
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                const auto* local =
                    static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
                usage.argv |= argvid && (local->lhs == argvid || local->rhs == argvid);
                usage.self |= selfid && (local->lhs == selfid || local->rhs == selfid);
            }

            QoreIROpcode op = inst->opcode;
            if (op == QoreIROpcode::Invoke) {
                op = static_cast<const QoreIRInvokeInstruction*>(inst)->invoke_opcode;
            }
            if (op == QoreIROpcode::LoadImplicitArg
                    || op == QoreIROpcode::LoadImplicitArgv) {
                usage.argv = true;
            }
            if (op == QoreIROpcode::LoadSelfMember
                    || op == QoreIROpcode::CallMethod
                    || op == QoreIROpcode::CallMethodDirect
                    || op == QoreIROpcode::InvokeMethodDirect) {
                usage.self = selfid != nullptr;
            }
            if (op == QoreIROpcode::CreateMethodRef && inst->operands.empty()) {
                usage.self = selfid != nullptr;
            }
            if (op == QoreIROpcode::CreateClosure) {
                const QoreClosureParseNode* closure = nullptr;
                if (inst->opcode == QoreIROpcode::CreateClosure) {
                    const auto* create =
                        static_cast<const QoreIRCreateClosureInstruction*>(inst);
                    closure = create->closure_node;
                    if (!closure) {
                        closure = dynamic_cast<const QoreClosureParseNode*>(
                            create->expr.getInternalNode());
                    }
                } else {
                    const auto* invoke =
                        static_cast<const QoreIRInvokeInstruction*>(inst);
                    closure = dynamic_cast<const QoreClosureParseNode*>(
                        invoke->expr.getInternalNode());
                }
                if (closure && closure->isInMethod()) {
                    usage.self = selfid != nullptr;
                }
            }
            if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                const auto* path =
                    static_cast<const QoreIRLValuePathInstruction*>(inst);
                for (const LVPathStep& step : path->path) {
                    if (step.kind == LVPathStepKind::SelfMember) {
                        usage.self = selfid != nullptr;
                        break;
                    }
                }
            }

            if (inst->opcode == QoreIROpcode::OnBlockExit) {
                const auto* block_exit =
                    static_cast<const QoreIROnBlockExitInstruction*>(inst);
                if (block_exit->handler_ir && visited_handlers->insert(block_exit->handler_ir.get()).second) {
                    ContextUsage handler =
                        block_exit->handler_ir->getContextUsage(argvid, selfid, visited_handlers);
                    usage.argv |= handler.argv;
                    usage.self |= handler.self;
                }
            }
        }
    }
    return usage;
}

std::unordered_set<const LocalVar*> QoreIRFunction::deriveHandlerOwnClosureLocalsFromIR() const {
    std::unordered_set<const LocalVar*> own;
    if (!parent_slot_count) {
        return own;
    }
    // the instructions of the handler itself: a nested handler is a function of its own (handler_ir), and the IR of
    // a closure created in the handler is not part of it either
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode != QoreIROpcode::InstantiateLocal && inst->opcode != QoreIROpcode::UninstantiateLocal) {
                continue;
            }
            const LocalVar* lv = static_cast<const QoreIRLocalInstruction*>(inst.get())->local;
            if (!lv || !lv->closureUse()) {
                continue;
            }
            auto i = local_var_slots.find(lv);
            if (i != local_var_slots.end() && i->second < parent_slot_count) {
                own.insert(lv);
            }
        }
    }
    return own;
}

void QoreIRFunction::restoreHandlerPreInstantiatedLocals() {
    if (!parent_slot_count) {
        return;
    }
    if (!handler_own_locals_known) {
        handler_own_locals = deriveHandlerOwnClosureLocalsFromIR();
    }
    for (const auto& [lv, slot_id] : local_var_slots) {
        if (!lv || slot_id >= parent_slot_count || handler_own_locals.count(lv)) {
            continue;
        }
        pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        pre_instantiated_cache.insert(lv);
    }
}

void QoreIRFunction::embedLocalClosureFlags() {
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (auto* local_inst = qore_ir_inst_cast<QoreIRLocalInstruction*>(inst.get())) {
                if (local_inst->local) {
                    local_inst->is_closure = local_inst->local->closureUse();
                }
            }
        }
    }
}

void QoreIRFunction::computeSlotIdsAndEmbed() {
    // every function that runs passes here, also the ones that are not optimized (top-level code, handlers, closures,
    // functions read from an image): the release checks of values that cannot hold an object go, and the containers of
    // the others get their handler (qore#6072); qore_ir_optimize() runs it again with the facts it refines
    if (!defer_release_check_prune) {
        qore_ir_prune_release_checks(*this);
    }
    // Pass 1: Compute max value ID for right-sizing interpreter value vector
    // AND compute local variable slot IDs for flat array access in interpreter
    uint32_t max_vid = 0;
    std::unordered_map<const LocalVar*, uint32_t> slot_map;

    uint32_t next_local_slot = 0;
    // Preserve any slots assigned before this pass.  Function signatures seed
    // parameter/argv/self slots here so direct-parameter calls cannot collide
    // with scoped body locals; handler IR uses the same path for parent slots.
    std::unordered_set<uint32_t> used_local_slots;
    for (const auto& [lvar, slot_id] : local_var_slots) {
        if (lvar) {
            uint32_t sid = slot_id;
            if (!used_local_slots.insert(sid).second) {
                while (used_local_slots.find(next_local_slot) != used_local_slots.end()) {
                    ++next_local_slot;
                }
                sid = next_local_slot++;
                used_local_slots.insert(sid);
            }
            slot_map[lvar] = sid;
            if (sid >= next_local_slot) {
                next_local_slot = sid + 1;
            }
        }
    }

    // the place of each foreach iterator among the slots (see interpreter_iterator_slot_boundary): the slots assigned
    // before the instruction creating it, and the loop variable's if the first slot assigned after it is the loop
    // variable's, assigned by its first element store
    interpreter_iterator_slot_boundary.clear();
    const QoreIRInstruction* pending_iterator = nullptr;
    // one past the highest slot of the locals referenced so far in layout order: a slot is assigned where its local
    // first appears, but a later run of this pass keeps the slots of an earlier one (next_local_slot then starts
    // after all of them)
    uint32_t seen_slots = 0;
    has_generic_add_assign = false;
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            inst->auto_local_string_append = false;
            if (inst->opcode == QoreIROpcode::AddAssignAny || (inst->opcode == QoreIROpcode::Invoke
                    && static_cast<const QoreIRInvokeInstruction&>(*inst).invoke_opcode == QoreIROpcode::AddAssignAny)) {
                has_generic_add_assign = true;
            }
            const uint32_t slots_before = seen_slots;
            if (inst->result.isValid() && inst->result.id > max_vid) {
                max_vid = inst->result.id;
            }
            for (const auto& operand : inst->operands) {
                if (operand.isValid() && operand.id > max_vid) {
                    max_vid = operand.id;
                }
            }

            // Track LocalVar* pointers — assign slot IDs to all locals referenced
            // by any instruction type (LoadLocal, StoreLocal, fused ops, etc.)
            auto assign_slot = [&](const LocalVar* lv) {
                if (!lv) {
                    return;
                }
                auto it = slot_map.find(lv);
                if (it == slot_map.end()) {
                    it = slot_map.emplace(lv, next_local_slot++).first;
                    used_local_slots.insert(it->second);
                }
                if (it->second + 1 > seen_slots) {
                    seen_slots = it->second + 1;
                }
            };
            auto assign_expr_slots = [&](const QoreValue& expr) {
                std::vector<const LocalVar*> expr_locals;
                std::unordered_set<const void*> seen;
                collectLocalSlotsFromExpr(expr, expr_locals, seen);
                // Embedded closures contribute a pointer-ordered capture set. Slot allocation
                // must use lexical names rather than allocator-dependent collection order.
                std::stable_sort(expr_locals.begin(), expr_locals.end(),
                    [](const LocalVar* lhs, const LocalVar* rhs) {
                        return strcmp(lhs->getName(), rhs->getName()) < 0;
                    });
                for (const LocalVar* lv : expr_locals) {
                    assign_slot(lv);
                }
            };
            if (auto* local_inst = qore_ir_inst_cast<QoreIRLocalInstruction*>(inst.get())) {
                assign_slot(local_inst->local);
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* f = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst.get());
                assign_slot(f->target);
                assign_slot(f->source);
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                assign_slot(static_cast<QoreIRIncrementLocalIntInstruction*>(inst.get())->local);
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                auto* f = static_cast<QoreIRBranchIfLtLocalIntInstruction*>(inst.get());
                assign_slot(f->lhs);
                assign_slot(f->rhs);
            } else if (static_cast<int>(inst->opcode) >= static_cast<int>(QoreIROpcode::LoadLValue)
                    && static_cast<int>(inst->opcode) <= static_cast<int>(QoreIROpcode::SpliceLValue)) {
                // Lvalue instructions: ensure locals referenced through lvalue
                // expressions get slot assignments (needed for compound assignment
                // on types forced to the lvalue path: list, hash, etc.)
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst.get());
                const VarRefNode* base_var = extractLValueBaseVarRef(lval_inst->lvalue);
                if (base_var) {
                    qore_var_t vtype = base_var->getType();
                    if ((vtype == VT_LOCAL || vtype == VT_LOCAL_TS) && base_var->ref.id) {
                        assign_slot(reinterpret_cast<const LocalVar*>(base_var->ref.id));
                    }
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStore) {
                // HashKeyStore[Dynamic] reference their container LocalVar via
                // the `container` VarRefNode for the COW branch + auto-vivify
                // assign-back path. The container is NOT serialized — only
                // container_slot_id is — so the slot must be assigned here so
                // pass 2 can fill in container_slot_id and AOT deser can
                // resolve it back to the LocalVar* via slot_to_local.
                auto* hks = static_cast<QoreIRHashKeyStoreInstruction*>(inst.get());
                if (hks->container && hks->container->ref.id) {
                    assign_slot(reinterpret_cast<const LocalVar*>(hks->container->ref.id));
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStoreDynamic) {
                auto* hksd = static_cast<QoreIRHashKeyStoreDynamicInstruction*>(inst.get());
                if (hksd->container && hksd->container->ref.id) {
                    assign_slot(reinterpret_cast<const LocalVar*>(hksd->container->ref.id));
                }
            } else if (inst->opcode == QoreIROpcode::ListIndexStore) {
                auto* lis = static_cast<QoreIRListIndexStoreInstruction*>(inst.get());
                if (lis->container && lis->container->ref.id) {
                    assign_slot(reinterpret_cast<const LocalVar*>(lis->container->ref.id));
                }
            } else if (inst->opcode == QoreIROpcode::CreateParseRef) {
                auto* pr = static_cast<QoreIRCreateParseRefInstruction*>(inst.get());
                const ParseReferenceNode* prn = pr->node
                    ? pr->node
                    : dynamic_cast<const ParseReferenceNode*>(pr->expr.getInternalNode());
                if (prn) {
                    const VarRefNode* base_var = extractLValueBaseVarRef(prn->getLVExp());
                    if (base_var && base_var->ref.id) {
                        qore_var_t vtype = base_var->getType();
                        if (vtype == VT_LOCAL || vtype == VT_LOCAL_TS || vtype == VT_CLOSURE) {
                            assign_slot(reinterpret_cast<const LocalVar*>(base_var->ref.id));
                        }
                    }
                }
            } else if (static_cast<int>(inst->opcode) >= static_cast<int>(QoreIROpcode::LValuePathAssign)
                    && static_cast<int>(inst->opcode) <= static_cast<int>(QoreIROpcode::LValuePathTernary)) {
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst.get());
                if (!path_inst->path.empty()) {
                    const LVPathStep& root = path_inst->path[0];
                    if ((root.kind == LVPathStepKind::LocalVar || root.kind == LVPathStepKind::ClosureVar)
                            && root.ref_ptr) {
                        assign_slot(reinterpret_cast<const LocalVar*>(root.ref_ptr));
                    }
                }
            }
            if (isLValueOp(inst->opcode)) {
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst.get());
                assign_expr_slots(lval_inst->lvalue);
            }
            if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                assign_expr_slots(*expr);
            }
            if (const QoreValue* expr = getSerializationOnlyInstructionExpr(inst.get())) {
                assign_expr_slots(*expr);
            }
            if (inst->opcode == QoreIROpcode::Find) {
                const auto* find_inst = static_cast<const QoreIRFindInstruction*>(inst.get());
                assign_expr_slots(find_inst->exp);
                assign_expr_slots(find_inst->find_exp);
                assign_expr_slots(find_inst->where);
            }
            if (pending_iterator && seen_slots != slots_before) {
                // the first slot assigned after the iterator: the loop variable's goes before the iterator, which the
                // AST interpreter releases before the loop variable
                if (inst->opcode == QoreIROpcode::StoreLocal || inst->opcode == QoreIROpcode::StoreClosure) {
                    if (static_cast<const QoreIRLocalInstruction*>(inst.get())->foreach_assignment) {
                        interpreter_iterator_slot_boundary[pending_iterator] = seen_slots;
                    }
                }
                pending_iterator = nullptr;
            }
            if (inst->opcode == QoreIROpcode::IteratorCreate || inst->opcode == QoreIROpcode::IteratorCreateIterate
                    || inst->opcode == QoreIROpcode::IteratorCreateSelectors
                    || inst->opcode == QoreIROpcode::IteratorCreateSource
                    || inst->opcode == QoreIROpcode::IteratorCreateReverse) {
                interpreter_iterator_slot_boundary[inst.get()] = seen_slots;
                pending_iterator = inst.get();
            }
        }
    }
    max_value_id = max_vid;
    local_var_slots = slot_map;
    // Resolve this once before the IR is published, for native and interpreted calls alike.  Prefer the
    // synthetic self local; older/deserialized IR can identify it only by name.  Iterate the final map to retain
    // the same fallback ordering as the former per-invocation lookup.
    self_local = nullptr;
    for (const auto& [lv, slot_id] : local_var_slots) {
        (void)slot_id;
        if (!lv || !lv->getName()) {
            continue;
        }
        if (lv->isSelf()) {
            self_local = const_cast<LocalVar*>(lv);
            break;
        }
        if (!self_local && !strcmp(lv->getName(), "self")) {
            self_local = const_cast<LocalVar*>(lv);
        }
    }
    max_local_slot_id = next_local_slot > 0 ? next_local_slot - 1 : 0;

    // Pass 1.5: Set metadata flags on blocks and cache line numbers in instructions
    // This allows the interpreter to skip the phi loop when there are no phis,
    // and avoid repeated pointer dereferences when checking for line changes
    for (const auto& block : blocks) {
        block->has_phi_nodes = false;
        for (const auto& inst : block->instructions) {
            // Cache line number: -1 if no location, otherwise store start_line
            // This avoids two pointer dereferences per instruction in the debugger's line tests
            inst->cached_start_line = inst->loc ? static_cast<int32_t>(inst->loc->start_line) : -1;

            if (inst->opcode == QoreIROpcode::Phi) {
                block->has_phi_nodes = true;
                // Don't break here; continue to set cached_start_line for all instructions
            }
            // Note: Phi instructions are always at the beginning of blocks, so we can
            // check has_phi_nodes efficiently later by just looking at the first instruction
        }
    }

    // Pass 2: embed pre-computed slot_id, is_closure and is_ref in instructions
    // to eliminate hash map lookups on the hot path in the interpreter.
    // is_ref marks a local that can actually hold a reference, so that loads and stores use
    // the reference-aware paths; it is deliberately narrow, because marking every untyped
    // local reference-capable would disable the optimizations that key off it.
    const bool creates_reference = detectCreatesReference();
    const std::unordered_set<const void*> body_local_keys = buildBodyLocalKeys();
    for (const auto& block : blocks) {
        for (const auto& inst : block->instructions) {
            if (auto* local_inst = qore_ir_inst_cast<QoreIRLocalInstruction*>(inst.get())) {
                if (local_inst->local) {
                    auto it = slot_map.find(local_inst->local);
                    if (it != slot_map.end()) {
                        local_inst->slot_id = it->second;
                    }
                    // the flag that embedLocalClosureFlags() embeds alone
                    local_inst->is_closure = local_inst->local->closureUse();
                    local_inst->is_ref = localMayHoldReference(local_inst->local,
                        creates_reference, body_local_keys);
                }
            } else if (inst->opcode == QoreIROpcode::DebugBlock) {
                // the slots of the locals of the block that the debugger's release uses (#5921)
                auto* db = qore_ir_inst_cast<QoreIRDebugStatementInstruction*>(inst.get());
                if (db && db->block_locals) {
                    QoreIRDebugBlockLocals& bl = *db->block_locals;
                    bl.slots.assign(bl.locals.size(), UINT32_MAX);
                    for (size_t i = 0; i < bl.locals.size(); ++i) {
                        auto it = slot_map.find(bl.locals[i]);
                        if (it != slot_map.end()) {
                            bl.slots[i] = it->second;
                        }
                    }
                }
            } else if (inst->opcode == QoreIROpcode::ScopeEnter) {
                // the slots of the locals that an exception leaving the scope releases (#5867)
                auto* se = static_cast<QoreIRScopeEnterInstruction*>(inst.get());
                se->local_slots.assign(se->locals.size(), UINT32_MAX);
                for (size_t i = 0; i < se->locals.size(); ++i) {
                    auto it = slot_map.find(se->locals[i]);
                    if (it != slot_map.end()) {
                        se->local_slots[i] = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStore) {
                auto* hks = static_cast<QoreIRHashKeyStoreInstruction*>(inst.get());
                if (hks->container && hks->container->ref.id) {
                    auto it = slot_map.find(
                        reinterpret_cast<const LocalVar*>(hks->container->ref.id));
                    if (it != slot_map.end()) {
                        hks->container_slot_id = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::HashKeyStoreDynamic) {
                auto* hksd = static_cast<QoreIRHashKeyStoreDynamicInstruction*>(inst.get());
                if (hksd->container && hksd->container->ref.id) {
                    auto it = slot_map.find(
                        reinterpret_cast<const LocalVar*>(hksd->container->ref.id));
                    if (it != slot_map.end()) {
                        hksd->container_slot_id = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::ListIndexStore) {
                auto* lis = static_cast<QoreIRListIndexStoreInstruction*>(inst.get());
                if (lis->container && lis->container->ref.id) {
                    auto it = slot_map.find(
                        reinterpret_cast<const LocalVar*>(lis->container->ref.id));
                    if (it != slot_map.end()) {
                        lis->container_slot_id = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* fused = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst.get());
                if (fused->target) {
                    auto it = slot_map.find(fused->target);
                    if (it != slot_map.end()) {
                        fused->target_slot_id = it->second;
                    }
                }
                if (fused->source) {
                    auto it = slot_map.find(fused->source);
                    if (it != slot_map.end()) {
                        fused->source_slot_id = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                auto* fused = static_cast<QoreIRIncrementLocalIntInstruction*>(inst.get());
                if (fused->local) {
                    auto it = slot_map.find(fused->local);
                    if (it != slot_map.end()) {
                        fused->slot_id = it->second;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                auto* fused = static_cast<QoreIRBranchIfLtLocalIntInstruction*>(inst.get());
                if (fused->lhs) {
                    auto it = slot_map.find(fused->lhs);
                    if (it != slot_map.end()) {
                        fused->lhs_slot_id = it->second;
                    }
                }
                if (fused->rhs) {
                    auto it = slot_map.find(fused->rhs);
                    if (it != slot_map.end()) {
                        fused->rhs_slot_id = it->second;
                    }
                }
            } else if (static_cast<int>(inst->opcode) >= static_cast<int>(QoreIROpcode::LoadLValue)
                    && static_cast<int>(inst->opcode) <= static_cast<int>(QoreIROpcode::SpliceLValue)) {
                // Lvalue instruction: pre-compute the target variable's slot_id
                auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst.get());
                const VarRefNode* base_var = extractLValueBaseVarRef(lval_inst->lvalue);
                if (base_var) {
                    qore_var_t vtype = base_var->getType();
                    if ((vtype == VT_LOCAL || vtype == VT_LOCAL_TS) && base_var->ref.id) {
                        auto it = slot_map.find(
                            reinterpret_cast<const LocalVar*>(base_var->ref.id));
                        if (it != slot_map.end()) {
                            lval_inst->lvalue_slot_id = it->second;
                        }
                    } else {
                        lval_inst->lvalue_slot_id = QoreIRLValueInstruction::LVALUE_NON_LOCAL;
                    }
                } else {
                    lval_inst->lvalue_slot_id = QoreIRLValueInstruction::LVALUE_NON_LOCAL;
                }
            } else if (static_cast<int>(inst->opcode) >= static_cast<int>(QoreIROpcode::LValuePathAssign)
                    && static_cast<int>(inst->opcode) <= static_cast<int>(QoreIROpcode::LValuePathTernary)) {
                // LValuePath instruction: set root variable's slot_id for targeted cache invalidation
                auto* path_inst = static_cast<QoreIRLValuePathInstruction*>(inst.get());
                if (!path_inst->path.empty()) {
                    LVPathStep& root = path_inst->path[0];
                    if ((root.kind == LVPathStepKind::LocalVar || root.kind == LVPathStepKind::ClosureVar)
                            && root.ref_ptr) {
                        auto it = slot_map.find(reinterpret_cast<const LocalVar*>(root.ref_ptr));
                        if (it != slot_map.end()) {
                            path_inst->lvalue_slot_id = it->second;
                            // Also embed slot_id on the path step itself, so AOT
                            // serialization carries a stable reference that can
                            // rebuild ref_ptr when the instruction is deserialized
                            // for a closure/handler body (where AOTRuntime does
                            // not walk the path to re-resolve ref_ptr from
                            // ctx->locals).
                            root.slot_id = it->second;
                        }
                    } else {
                        path_inst->lvalue_slot_id = QoreIRLValuePathInstruction::LVALUE_NON_LOCAL;
                    }
                }
            }
        }
    }
    // Includes functions and handlers read from stripped AOT images.
    if (has_generic_add_assign && !defer_release_check_prune) {
        qore_ir_mark_auto_string_appends(*this);
    }
}
#endif // QORE_IR_TAKE_OVER_UNIT

#ifdef QORE_IR_TAKE_OVER_UNIT
//! The writes of a function's locals that its own inventories do not record, collected once for the function
/** A write through a reference that the function creates to a local (<tt>\\var</tt>, <tt>\\var.member</tt>), through
    which a callee can write it, and a write in a handler of the function (on_exit, on_error, on_success), which is a
    function of its own (QoreIROnBlockExitInstruction::handler_ir), whose writes to the locals of the function that
    registers it are not in that function's inventories and whose own inventories are not built (computeIROnlyLocals()
    runs for the function): the handlers' instructions are scanned, an Invoke (an operation in a try block) of a store
    or of an lvalue operation, a reference that cannot be walked and a handler without its IR counting as a write of
    every local (qore#6004)
*/
struct QoreIRUnrecordedWrites {
    //! Handler bodies entered by this collection (reported by QORE_IR_OPT_STATS).
    size_t handler_scans = 0;
    //! the locals written
    std::unordered_set<const void*> locals;
    //! true if any local can be written
    bool any = false;

    DLLLOCAL bool writes(const void* key) const {
        return any || locals.count(key);
    }

    //! Collects root references and the writes of reachable handlers.
    DLLLOCAL void collect(const QoreIRFunction& ir_func) {
        addReferences(ir_func);
        if (!any) {
            std::unordered_set<const QoreIRFunction*> visited;
            addHandlerWrites(ir_func, false, visited);
        }
        if (handler_scans && QORE_ENV_IS_SET("QORE_IR_OPT_STATS")) {
            fprintf(stderr, "IR-OPT-HANDLER-WRITES: %s: scans=%zu\n", ir_func.getDisplayName().c_str(), handler_scans);
        }
    }

    //! adds the locals that \a ir_func creates a reference to
    DLLLOCAL void addReferences(const QoreIRFunction& ir_func) {
        for (const auto& block : ir_func.blocks) {
            for (const auto& inst : block->instructions) {
                const QoreValue* expr = nullptr;
                if (inst->opcode == QoreIROpcode::CreateParseRef) {
                    expr = &static_cast<const QoreIRCreateParseRefInstruction*>(inst.get())->expr;
                } else if (inst->opcode == QoreIROpcode::Invoke) {
                    const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst.get());
                    if (inv->invoke_opcode == QoreIROpcode::CreateParseRef) {
                        expr = &inv->expr;
                    }
                }
                if (expr) {
                    bool unknown_node_found = false;
                    collectLocalsFromExpr(*expr, locals, unknown_node_found);
                    if (unknown_node_found) {
                        any = true;
                    }
                }
            }
        }
    }

    //! adds the writes of the handlers of \a ir_func and of their handlers; \a handler is true if \a ir_func is one
    DLLLOCAL void addHandlerWrites(const QoreIRFunction& ir_func, bool handler,
            std::unordered_set<const QoreIRFunction*>& visited) {
        if (handler) {
            ++handler_scans;
            addReferences(ir_func);
        }
        for (const auto& block : ir_func.blocks) {
            for (const auto& inst : block->instructions) {
                if (any) {
                    return;
                }
                if (handler) {
                    if (const LocalVar* written = qore_ir_get_written_local(inst.get())) {
                        locals.insert(reinterpret_cast<const void*>(written));
                    }
                    if (inst->opcode == QoreIROpcode::Invoke) {
                        QoreIROpcode op = static_cast<const QoreIRInvokeInstruction*>(inst.get())->invoke_opcode;
                        if (op == QoreIROpcode::StoreLocal || op == QoreIROpcode::StoreClosure || isLValueOp(op)) {
                            any = true;
                            return;
                        }
                    }
                }
                if (inst->opcode != QoreIROpcode::OnBlockExit) {
                    continue;
                }
                const QoreIRFunction* handler_ir = static_cast<const QoreIROnBlockExitInstruction*>(inst.get())
                    ->handler_ir.get();
                if (!handler_ir) {
                    any = true;
                    return;
                }
                // Inline and out-of-line registrations share handler_ir. Its writes enter the union once (#6647).
                if (visited.insert(handler_ir).second) {
                    addHandlerWrites(*handler_ir, true, visited);
                }
            }
        }
    }
};

//! Returns true if a value of the type can be or hold an object, whose destructor can run when it is released
/** As qore_ir_type_may_hold_object() in QoreIRLowering.cpp, whose code this unit does not compile
*/
static bool qore_ir_take_over_type_may_hold_object(const QoreTypeInfo* ti, int depth = 0) {
    if (!QoreTypeInfo::needsScan(ti)) {
        return false;
    }
    if (depth >= 8) {
        return true;
    }
    // the elements of a list and the values of a hash of a type are of that type
    if (const QoreTypeInfo* element = QoreTypeInfo::getComplexListValueType(ti)) {
        return qore_ir_take_over_type_may_hold_object(element, depth + 1);
    }
    if (const QoreTypeInfo* value = QoreTypeInfo::getComplexHashValueType(ti)) {
        return qore_ir_take_over_type_may_hold_object(value, depth + 1);
    }
    return true;
}

// documented in QoreIR.h
bool qore_ir_collect_take_over_params(const QoreIRFunction& ir_func, const LocalVar* const* params, unsigned num_params,
        std::unordered_set<const void*>* take_over) {
    bool rv = false;
    // the writes that the inventories do not record are collected once, for the first parameter that needs them
    std::optional<QoreIRUnrecordedWrites> unrecorded;
    for (unsigned i = 0; i < num_params; ++i) {
        const LocalVar* param = params[i];
        if (!param || !qore_ir_take_over_type_may_hold_object(param->getTypeInfo())) {
            continue;
        }
        const void* key = reinterpret_cast<const void*>(param);
        // a write that the IR records, or one in a closure that captures the parameter
        bool writes = qore_ir_local_is_written(ir_func, param) || param->closureUse();
        if (!writes) {
            if (!unrecorded) {
                unrecorded.emplace();
                unrecorded->collect(ir_func);
            }
            writes = unrecorded->writes(key);
        }
        if (writes) {
            rv = true;
            if (!take_over) {
                return true;
            }
            take_over->insert(key);
        }
    }
    return rv;
}

// documented in QoreIR.h
bool qore_ir_collect_take_over_params(const QoreIRFunction& ir_func, std::unordered_set<const void*>* take_over) {
    if (ir_func.param_local_vars.empty()) {
        return false;
    }
    std::vector<const LocalVar*> params;
    for (const auto& [idx, lv] : ir_func.param_local_vars) {
        if (idx >= 0) {
            if (params.size() <= static_cast<size_t>(idx)) {
                params.resize(static_cast<size_t>(idx) + 1, nullptr);
            }
            params[static_cast<size_t>(idx)] = lv;
        }
    }
    return qore_ir_collect_take_over_params(ir_func, params.data(), static_cast<unsigned>(params.size()), take_over);
}

// documented in QoreIR.h
bool qore_ir_function_may_deopt(const QoreIRFunction& ir_func) {
    for (const auto& block : ir_func.blocks) {
        for (const auto& inst : block->instructions) {
            switch (inst->opcode) {
                case QoreIROpcode::GuardInt:
                case QoreIROpcode::GuardFloat:
                case QoreIROpcode::GuardType:
                case QoreIROpcode::GuardNotNothing:
                    if (static_cast<const QoreIRGuardInstruction*>(inst.get())->deopt_target) {
                        return true;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}

// documented in QoreIR.h
//! Adds the locals of the AST expressions that the instructions of the handler functions of \a ir_func (and of their
//! handlers) evaluate to \a locals: such an expression can write a local that the write analysis of the instruction
//! does not attribute (qore#6314); returns false if an expression has a node that the walker does not know
static bool qore_ir_add_handler_ast_locals(const QoreIRFunction& ir_func, std::unordered_set<const void*>& locals,
        bool handler, std::unordered_set<const QoreIRFunction*>& visited) {
    bool unknown_node_found = false;
    for (const auto& block : ir_func.blocks) {
        for (const auto& inst : block->instructions) {
            if (handler) {
                if (isLValueOp(inst->opcode)) {
                    collectLocalsFromExpr(static_cast<const QoreIRLValueInstruction*>(inst.get())->lvalue, locals,
                        unknown_node_found);
                }
                if (const QoreValue* expr = getInstructionExpr(inst.get())) {
                    collectLocalsFromExpr(*expr, locals, unknown_node_found);
                }
                if (inst->opcode == QoreIROpcode::Find) {
                    collectLocalsFromFindInstruction(static_cast<const QoreIRFindInstruction*>(inst.get()), locals,
                        unknown_node_found);
                } else if (inst->opcode == QoreIROpcode::Context) {
                    const auto* ctx = static_cast<const QoreIRContextInstruction*>(inst.get());
                    collectLocalsFromExpr(ctx->exp, locals, unknown_node_found);
                    collectLocalsFromExpr(ctx->where_exp, locals, unknown_node_found);
                    collectLocalsFromExpr(ctx->sort_exp, locals, unknown_node_found);
                }
            }
            if (inst->opcode == QoreIROpcode::OnBlockExit) {
                const QoreIRFunction* handler_ir = static_cast<const QoreIROnBlockExitInstruction*>(inst.get())
                    ->handler_ir.get();
                if (handler_ir && visited.insert(handler_ir).second
                        && !qore_ir_add_handler_ast_locals(*handler_ir, locals, true, visited)) {
                    return false;
                }
            }
        }
    }
    return !unknown_node_found;
}

bool qore_ir_collect_handler_written_locals(const QoreIRFunction& ir_func, std::unordered_set<const void*>& written) {
    QoreIRUnrecordedWrites w;
    // a handler can write a local through a reference that this function created
    w.collect(ir_func);
    if (w.any) {
        return false;
    }
    // and in an AST expression that a handler evaluates (a write that the instruction does not record)
    std::unordered_set<const QoreIRFunction*> visited;
    if (!qore_ir_add_handler_ast_locals(ir_func, w.locals, false, visited)) {
        return false;
    }
    written = std::move(w.locals);
    return true;
}

// documented in QoreIR.h
void qore_ir_compute_param_take_over(QoreIRFunction& ir_func, LocalVar* const* params, unsigned num_params) {
    ir_func.param_take_over = qore_ir_collect_take_over_params(ir_func, params, num_params, nullptr);
}

// documented in QoreIR.h
bool qore_ir_is_sequence_temp(const QoreIRInstruction* inst) {
    // extract on an lvalue path (extract l, 0, 1) gives the new list, string or binary that it removed
    if (inst->opcode == QoreIROpcode::LValuePathTernary) {
        return static_cast<const QoreIRLValuePathInstruction*>(inst)->ternary_op == LVTernaryOp::Extract;
    }
    QoreIROpcode op = inst->opcode;
    if (op == QoreIROpcode::Invoke) {
        op = static_cast<const QoreIRInvokeInstruction*>(inst)->invoke_opcode;
    }
    return op == QoreIROpcode::RangeSliceAny || op == QoreIROpcode::RangeSliceInt
        || op == QoreIROpcode::RangeSliceFloat || op == QoreIROpcode::ExtractAny || op == QoreIROpcode::ExtractList
        || op == QoreIROpcode::ExtractString || op == QoreIROpcode::ExtractBinary;
}
#endif // QORE_IR_TAKE_OVER_UNIT
