/*
    Statement.cpp

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

#include <qore/Qore.h>
#include "qore/intern/StatementBlock.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRInterpreter.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreIRPrinter.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreAOT.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

VNode::VNode(LocalVar* lv, const QoreProgramLocation* n_loc, int n_refs, bool n_top_level) :
        lvar(lv), refs(n_refs), loc(n_loc), block_start(false), top_level(n_top_level) {
    next = update_get_vstack(this);

    //printd(5, "VNode::VNode() this: %p '%s' %p top_level: %d\n", this, lvar ? lvar->getName() : "n/a", lvar, top_level);

    if (top_level) {
        save_global_vnode(this);
    }
}

VNode::~VNode() {
    //printd(5, "VNode::~VNode() this: %p '%s' %p top_level: %d\n", this, lvar ? lvar->getName() : "n/a", lvar,
    //    top_level);

    if (lvar && !refs) {
        const QoreTypeInfo* ti = lvar->parseGetTypeInfo();
        if (!QoreTypeInfo::parseAcceptsReturns(ti, NT_OBJECT) || !lvar->isAssigned()) {
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_UNREFERENCED_VARIABLE,
                "UNREFERENCED-VARIABLE", "local variable '%s' was declared in this block but not referenced; to " \
                "disable this warning, use '%%disable-warning unreferenced-variable' in your code", lvar->getName());
        }
    }

    if (top_level) {
        save_global_vnode(nullptr);
        //printd(5, "VNode::~VNode() this: %p deleting top-level global vnode\n", this);
    }
}

void VNode::appendLocation(QoreString& str) {
    if (loc) {
        str.concat(" at ");
        loc->toString(str);
    }
}

const char* VNode::getName() const {
    return lvar->getName();
}

// searches to marker and then jumps to global thread-local variables
VNode* VNode::nextSearch() const {
    //printd(5, "VNode::nextSearch() next->lvar: %p top_level: %d\n", next ? next->lvar : 0, top_level);

    if ((next && next->lvar) || top_level)
        return !next || next->lvar ? next : nullptr;

    // skip to global thread-local variables
    VNode* rv = get_global_vnode();
    assert(!rv || rv->lvar);
    //printd(5, "VNode::nextSearch() returning global VNode %p '%s'\n", rv, rv ? rv->getName() : "n/a");
    return rv;
}

class BlockStartHelper {
public:
    DLLLOCAL BlockStartHelper(QoreParseContext& parse_context) : parse_context(parse_context) {
        lvids = parse_context.lvids;
        parse_context.lvids = 0;
        VNode* v = getVStack();
        //printd(5, "BlockStartHelper::BlockStartHelper() v=%p ibs=%d\n", v, v ? v->isBlockStart() : 0);
        bs = v ? v->setBlockStart(true) : true;
    }

    DLLLOCAL ~BlockStartHelper() {
        //printd(5, "BlockStartHelper::~BlockStartHelper() bs=%d\n", bs);
        if (!bs) {
            getVStack()->setBlockStart(false);
        }
        if (parse_context.lvids != lvids) {
            parse_context.lvids = lvids;
        }
    }

protected:
    QoreParseContext& parse_context;
    int lvids;
    bool bs;
};

VariableBlockHelper::VariableBlockHelper() {
   new VNode(0);
   //printd(5, "VariableBlockHelper::VariableBlockHelper() this=%p pushed %p\n", this, 0);
}

VariableBlockHelper::~VariableBlockHelper() {
   std::unique_ptr<VNode> vnode(getVStack());
   assert(vnode.get());
   updateVStack(vnode->next);
   //printd(5, "VariableBlockHelper::~VariableBlockHelper() this=%p got %p\n", this, vnode->lvar);
}

StatementBlock::StatementBlock(qore_program_private_base* p) : AbstractStatement(p) {
}

StatementBlock::StatementBlock(int sline, int eline) : AbstractStatement(sline, eline) {
}

StatementBlock::StatementBlock(int sline, int eline, AbstractStatement* s) : AbstractStatement(sline, eline) {
    addStatement(s);
}

QoreValue StatementBlock::exec(ExceptionSink* xsink) {
    //QORE_TRACE("StatementBlock::exec()");
    QoreValue return_value{};
    RuntimeConfig& rc = rc_get_current_ref();
    ThreadLocalProgramData* tlpd = rc.getThreadLocalProgramData()
        ? rc.getThreadLocalProgramData()
        : get_thread_local_program_data();
    if (tlpd && tlpd->runtimeCheck()) {
        tlpd->dbgFunctionEnter(this, xsink);
    }
    execImpl(rc, return_value, xsink);
    if (tlpd && tlpd->runtimeCheck()) {
        tlpd->dbgFunctionExit(this, return_value, xsink);
    }
    return return_value;
}

void StatementBlock::addStatement(AbstractStatement* s) {
    //QORE_TRACE("StatementBlock::addStatement()");

    if (s) {
        statement_list.push_back(s);
        OnBlockExitStatement* obe = dynamic_cast<OnBlockExitStatement*>(s);
        if (obe)
            on_block_exit_list.push_front(std::make_pair(obe->getType(), obe->getCode()));
    }
}

void StatementBlock::del() {
    //QORE_TRACE("StatementBlock::del()");
    for (statement_list_t::iterator i = statement_list.begin(), e = statement_list.end(); i != e; ++i)
        delete *i;

    statement_list.clear();

    if (lvars) {
        delete lvars;
        lvars = nullptr;
    }
}

int StatementBlock::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return execImpl(rc, return_value, xsink);
}

int StatementBlock::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    //QORE_TRACE("StatementBlock::execImpl()");
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    return execIntern(rc, return_value, xsink);
}

int StatementBlock::execIntern(QoreValue& return_value, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return execIntern(rc, return_value, xsink);
}

int StatementBlock::execIntern(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    //QORE_TRACE("StatementBlock::execIntern()");
    int stmt_rc = 0;

    assert(xsink);

    //printd(5, "StatementBlock::execIntern() this=%p, lvars=%p, %ld vars\n", this, lvars, lvars->size());

    bool obe = !on_block_exit_list.empty();
    // push "on block exit" iterator if necessary
    if (obe) {
        pushBlock(on_block_exit_list.end());
    }

    ThreadLocalProgramData* tlpd = rc.getThreadLocalProgramData()
        ? rc.getThreadLocalProgramData()
        : get_thread_local_program_data();
    // to execute even when block is empty, e.g. while(true);
    if (tlpd->runtimeCheck()) {
        stmt_rc = tlpd->dbgStep(this, nullptr, xsink);
    }
    if (!stmt_rc && !*xsink) {
        // execute block
        for (auto i : statement_list) {
            if (tlpd->runtimeCheck()) {
                stmt_rc = tlpd->dbgStep(this, i, xsink);
                if (stmt_rc || *xsink) {
                    break;
                }
            }
            stmt_rc = i->exec(rc, return_value, xsink);
            if (*xsink && tlpd->runtimeCheck()) {
                tlpd->dbgException(i, xsink);
                if (*xsink) {
                    break;
                }
            }
            if (stmt_rc) break;
        }
    }
    // execute "on block exit" code if applicable
    if (obe) {
        ExceptionSink obe_xsink;
        int nrc = 0;
        bool error = xsink->isException();
        for (block_list_t::iterator i = popBlock(), e = on_block_exit_list.end(); i != e; ++i) {
            enum obe_type_e type = (*i).first;
            if (type == OBE_Unconditional || (!error && type == OBE_Success) || (error && type == OBE_Error)) {
                if ((*i).second) {
                    {
                        // instantiate exception for on_error blocks as an implicit arg
                        std::unique_ptr<SingleArgvContextHelper> argv_helper;
                        std::unique_ptr<CatchExceptionHelper> ex_helper;
                        if (type == OBE_Error) {
                            QoreException* except = xsink->getException();
                            assert(except);
                            ex_helper.reset(new CatchExceptionHelper(except));
                            argv_helper.reset(new SingleArgvContextHelper(except->makeExceptionObject(), xsink));
                        }
                        nrc = (*i).second->execImpl(rc, return_value, &obe_xsink);
                        if (type == OBE_Error) {
                            if (qore_es_private::get(obe_xsink)->rethrown) {
                                xsink->clear();
                            }
                        }
                    }
                    // bug 380: make sure and merge every exception after every conditional execution to ensure
                    // that all on_(exit|error) statements are executed even if exceptions are thrown
                    if (obe_xsink) {
                        xsink->assimilate(obe_xsink);
                        if (!error)
                            error = true;
                    }
                }
            }
        }
        if (nrc)
            stmt_rc = nrc;
    }

    return stmt_rc;
}

// top-level block (program) execution member function
void StatementBlock::exec() {
    ExceptionSink xsink;
    exec(&xsink);
}

static bool isIRTerminatorOpcode(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
        case QoreIROpcode::ThreadExit:
            return true;
        default:
            return false;
    }
}

static bool irBlockHasTerminator(const QoreIRBasicBlock* block) {
    if (!block || block->instructions.empty()) {
        return false;
    }
    return isIRTerminatorOpcode(block->instructions.back()->opcode);
}

static void push_top_level_local_var(LocalVar* lv, const QoreProgramLocation* loc) {
    new VNode(lv, loc, 1, true);
}

// used for constructor methods sharing a common "self" local variable
void push_local_var(LocalVar* lv, const QoreProgramLocation* loc) {
    new VNode(lv, loc, 1);
}

LocalVar* push_local_var(const char* name, const QoreProgramLocation* loc,
        const QoreTypeInfo* typeInfo, int& err, bool is_auto, int n_refs, int pflag) {
    QoreProgram* pgm = getProgram();

    if ((pflag & PF_TOP_LEVEL) && (pflag & PF_NO_TOP_LEVEL_LVARS)) {
        parseException(*loc, "ILLEGAL-TOP-LEVEL-LOCAL-VARIABLE", "cannot declare local variable '%s' in the " \
            "top-level block; local variables in the top-level block of a Program object can only be declared in " \
            "the very first parse transaction to the Program object", name);
        if (!err) {
            err = -1;
        }
    }

    LocalVar* lv = qore_program_private::get(*pgm)->createLocalVar(name, typeInfo);

    /*
    QoreString ls;
    loc->toString(ls);
    printd(5, "push_local_var() lv: %p name: %s type: %s %s\n", lv, name, QoreTypeInfo::getName(typeInfo),
        ls.c_str());
    */

    bool found_block = false;
    // check stack for duplicate entries
    bool avs = parse_check_parse_option(PO_ASSUME_LOCAL);
    if (is_auto) {
        lv->parseAssigned();
    } else {
        if (pgm->checkWarning(QP_WARN_DUPLICATE_LOCAL_VARS | QP_WARN_DUPLICATE_BLOCK_VARS) || avs) {
            VNode* vnode = getVStack();
            while (vnode) {
                if (vnode->lvar) {
                    if (!found_block && vnode->isBlockStart())
                        found_block = true;
                    if (!strcmp(vnode->getName(), name)) {
                        if (!found_block) {
                            QoreStringNode* desc = new QoreStringNodeMaker("local variable '%s' was already " \
                                "declared in the same block", name);
                            if (avs) {
                                vnode->appendLocation(*desc);
                                parseException(*loc, "PARSE-ERROR", desc);
                            } else {
                                vnode->appendLocation(*desc);
                                qore_program_private::makeParseWarning(pgm, *loc,
                                    QP_WARN_DUPLICATE_BLOCK_VARS, "DUPLICATE-BLOCK-VARIABLE", desc);
                            }
                        } else if ((pflag & PF_TOP_LEVEL) || !vnode->isTopLevel()) {
                            QoreStringNode* desc = new QoreStringNodeMaker("local variable '%s' was already " \
                                "declared in this lexical scope", name);
                            vnode->appendLocation(*desc);
                            qore_program_private::makeParseWarning(pgm, *loc,
                                QP_WARN_DUPLICATE_LOCAL_VARS, "DUPLICATE-LOCAL-VARIABLE", desc);
                        }
                        break;
                    }
                }
                vnode = vnode->nextSearch();
            }
        }
    }

    //printd(5, "push_local_var(): pushing var %s\n", name);
    new VNode(lv, loc, n_refs, pflag & PF_TOP_LEVEL);
    return lv;
}

int pop_local_var_get_id() {
   std::unique_ptr<VNode> vnode(getVStack());
   assert(vnode.get());
   int refs = vnode->refCount();
   printd(5, "pop_local_var_get_id(): popping var %s (refs=%d)\n", vnode->lvar->getName(), refs);
   updateVStack(vnode->next);
   return refs;
}

LocalVar* pop_local_var(bool set_unassigned) {
   std::unique_ptr<VNode> vnode(getVStack());
   assert(vnode.get());
   LocalVar* rc = vnode->lvar;
   if (set_unassigned)
      rc->parseUnassigned();
   printd(5, "pop_local_var(): popping var %s\n", rc->getName());
   updateVStack(vnode->next);
   return rc;
}

LocalVar* find_local_var(const char* name, bool& in_closure) {
    VNode* vnode = getVStack();
    ClosureParseEnvironment* cenv = thread_get_closure_parse_env();
    in_closure = false;

    if (vnode && !vnode->lvar)
        vnode = vnode->nextSearch();

    //printd(5, "find_local_var('%s' %p) vnode: %p\n", name, name, vnode);

    while (vnode) {
        assert(vnode->lvar);
        if (cenv && !in_closure && cenv->getHighWaterMark() == vnode)
            in_closure = true;

        //printd(5, "find_local_var('%s' %p) v: '%s' %p in_closure: %d match: %d\n", name, name, vnode->getName(),
        //    vnode->getName(), in_closure, !strcmp(vnode->getName(), name));

        if (!strcmp(vnode->getName(), name)) {
            //printd(5, "find_local_var() %s in_closure: %d\n", name, in_closure);
            if (in_closure)
                cenv->add(vnode->lvar);
            vnode->setRef();
            return vnode->lvar;
        }
        vnode = vnode->nextSearch();
    }

    //printd(5, "find_local_var('%s' %p) returning 0 NOT FOUND\n", name, name);
    return 0;
}

int StatementBlock::parseInitIntern(QoreParseContext& parse_context, statement_list_t::iterator start) {
    QORE_TRACE("StatementBlock::parseInitIntern");

    AbstractStatement* ret = nullptr;

    if (start != statement_list.end()) {
        ++start;
    } else {
        start = statement_list.begin();
    }

    int err = 0;

    for (statement_list_t::iterator i = start, l = statement_list.last(), e = statement_list.end(); i != e; ++i) {
        if ((*i)->parseInit(parse_context) && !err) {
            err = -1;
        }
        if (!ret && i != l && (*i)->endsBlock()) {
            // unreachable code found
            qore_program_private::makeParseWarning(parse_context.pgm, *(*i)->loc, QP_WARN_UNREACHABLE_CODE,
                "UNREACHABLE-CODE", "code after this statement can never be reached");
            ret = *i;
        }
    }

    return err;
}

void StatementBlock::parseCommit(QoreProgram* pgm) {
    // add block to the list only when no statements inside
    qore_program_private::registerStatement(pgm, this, statement_list.empty());
    for (statement_list_t::iterator i = statement_list.begin(), e = statement_list.end(); i != e; ++i) {
        // register and add statements
        (*i)->parseCommit(pgm);
    }
}

int StatementBlock::parseInitImpl(QoreParseContext& parse_context) {
    QORE_TRACE("StatementBlock::parseInitImpl");

    printd(4, "StatementBlock::parseInitImpl(b=%p, oflag=%p)\n", this, parse_context.oflag);

    BlockStartHelper bsh(parse_context);

    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);
    int err = parseInitIntern(parse_context, statement_list.end());

    // this call will pop all local vars off the stack
    setupLVList(parse_context);

    //printd(5, "StatementBlock::parseInitImpl(this=%p): done (lvars=%p, %d vars, vstack = %p)\n", this, lvars, lvids,
    //    getVStack());
    return err;
}

int StatementBlock::parseInit(UserVariantBase* uvb, const QoreClass* class_ctx) {
    QORE_TRACE("StatementBlock::parseInit");

    VariableBlockHelper vbh;

    UserParamListLocalVarHelper ph(uvb);

    // initialize code block
    QoreParseContext parse_context(class_ctx);
    int err = parseInitImpl(parse_context);
    if (parseCheckReturn() && !err) {
        err = -1;
    }
    return err;
}

int StatementBlock::parseCheckReturn() {
    const QoreTypeInfo* returnTypeInfo = getReturnTypeInfo();
    if (QoreTypeInfo::hasType(returnTypeInfo) && !QoreTypeInfo::parseAccepts(returnTypeInfo, nothingTypeInfo)) {
        // make sure the last statement is a return statement if the block has a return type
        if (statement_list.empty() || !(*statement_list.last())->hasFinalReturn()) {
            QoreStringNode* desc = new QoreStringNode("this code block has declared return type ");
            QoreTypeInfo::getThisType(returnTypeInfo, *desc);
            desc->concat(" but does not have a return statement as the last statement in the block");
            qore_program_private::makeParseException(getProgram(), *loc, "MISSING-RETURN", desc);
            return -1;
        }
    }
    return 0;
}

int StatementBlock::parseInitMethod(const QoreTypeInfo* typeInfo, UserVariantBase* uvb) {
    QORE_TRACE("StatementBlock::parseInitMethod");

    VariableBlockHelper vbh;

    UserParamListLocalVarHelper ph(uvb, typeInfo);

    // initialize code block
    QoreParseContext parse_context(uvb->getUserSignature()->selfid);
    int err = parseInitImpl(parse_context);
    if (parseCheckReturn() && !err) {
        err = -1;
    }
    return err;
}

int StatementBlock::parseInitConstructor(const QoreTypeInfo* typeInfo, UserVariantBase* uvb, BCAList* bcal,
        const QoreClass& cls) {
    QORE_TRACE("StatementBlock::parseInitConstructor");

    BCList* bcl = qore_class_private::getBaseClassList(cls);

    VariableBlockHelper vbh;

    UserParamListLocalVarHelper ph(uvb, typeInfo);

    int err = 0;

    // if there is a base constructor list, resolve all classes and
    // ensure that all classes referenced are base classes of this class
    if (bcal) {
        // ensure that parse flags are set before initializing
        ParseWarnHelper pwh(pwo);

        for (auto& i : *bcal) {
            assert(QoreTypeInfo::getUniqueReturnClass(typeInfo));
            if (i->parseInit(bcl, QoreTypeInfo::getUniqueReturnClass(typeInfo)->getName()) && !err) {
                err = -1;
            }
        }
    }

    // initialize code block
    QoreParseContext parse_context(qore_class_private::getSelfId(cls));
    if (parseInitImpl(parse_context) && !err) {
        err = -1;
    }
    return err;
}

int StatementBlock::parseInitClosure(UserVariantBase* uvb, UserClosureFunction* cf) {
    QORE_TRACE("StatementBlock::parseInitClosure");

    ClosureParseEnvironment cenv(cf->getVList());
    UserParamListLocalVarHelper ph(uvb, cf->getClassType());

    // initialize code block
    QoreParseContext parse_context(uvb->getUserSignature()->selfid);
    int err = parseInitImpl(parse_context);
    if (parseCheckReturn() && !err) {
        err = -1;
    }
    return err;
}

int TopLevelStatementBlock::parseInit() {
    QORE_TRACE("TopLevelStatementBlock::parseInit");

    //printd(5, "TopLevelStatementBlock::parseInit(rns=%p) first=%d\n", &rns, first);

    // resolve global variables before initializing the top-level statements
    if (!qore_root_ns_private::parseResolveGlobalVarsAndClassHierarchies()) {
        return -1;
    }

    // Check if we're in REPL mode (allows new local vars in subsequent parse transactions)
    int64 current_parse_options = qore_program_private::getParseWarnOptions(getProgram()).parse_options;
    bool repl_mode = current_parse_options & PO_ALLOW_REPARSE;

    if (!first && lvars) {
        // push already-registered local variables on the stack
        for (unsigned i = 0; i < lvars->size(); ++i)
            push_top_level_local_var(lvars->lv[i], loc);
    }

    QoreParseContext parse_context;
    parse_context.setFlags(PF_TOP_LEVEL);
    if (!first && !repl_mode) {
        // In REPL mode, allow new local variables in subsequent parse transactions
        parse_context.setFlags(PF_NO_TOP_LEVEL_LVARS);
    }
    int err = parseInitIntern(parse_context, hwm);

    //printd(5, "TopLevelStatementBlock::parseInit(rns=%p) first=%d, lvids=%d\n", &rns, first, parse_context.lvids);

    // now initialize root namespace and functions before local variables are popped off the stack
    if (qore_root_ns_private::get(*getRootNS())->parseInit() && !err) {
        err = -1;
    }

    if (first) {
        // if parsing a module, then initialize the init function
        QoreModuleDefContext* qmd = get_module_def_context();
        if (qmd && qmd->parseInit() && !err) {
            err = -1;
        }

        // this call will pop all local vars off the stack
        setupLVList(parse_context);
        first = false;
    } else {
        // Save count of existing local vars before adding new ones
        unsigned existing_lvars = lvars ? lvars->size() : 0;

        if (repl_mode) {
            // In REPL mode, save new local variables to lvars instead of discarding
            if (parse_context.lvids) {
                // setupLVList pops new vars from vstack and adds them to lvars
                setupLVList(parse_context);
            }
        } else if (parse_context.lvids) {
            // In non-REPL mode, discard new variables immediately
            for (int i = 0; i < parse_context.lvids; ++i) {
                pop_local_var();
            }
        }

        // pop existing local vars off the stack
        for (unsigned i = 0; i < existing_lvars; ++i) {
            pop_local_var();
        }
    }

    //assert(!getVStack());

    //printd(5, "TopLevelStatementBlock::parseInitTopLevel(this=%p): done (lvars=%p, %d vars, vstack = %p)\n", this,
    //  lvars, lvids, getVStack());
    return err;
}

void TopLevelStatementBlock::parseCommit(QoreProgram* pgm) {
    //printd(5, "TopLevelStatementBlock::parseCommit(this=%p)\n", this);
    statement_list_t::iterator start = hwm;
    if (start != statement_list.end()) {
        ++start;
    } else {
        start = statement_list.begin();
    }

    while (start != statement_list.end()) {
        //printd(5, "TopLevelStatementBlock::parseCommit (this=%p): (hwm=%p)\n", this, *start);
        // register and add statements
        (*start)->parseCommit(pgm);
        start++;
    }
    hwm = statement_list.last();
}

TopLevelStatementBlock::~TopLevelStatementBlock() {
    delete cached_toplevel_aot_ctx;
    delete cached_toplevel_ir;
}

void TopLevelStatementBlock::setLVarsFromAOTContext(QoreAOTContext* ctx) {
    // Copy LocalVar* pointers from the AOT context to the statement block's LVList
    // This ensures pointer consistency between AOT-compiled code and runtime
    if (!ctx || !ctx->locals || ctx->num_locals == 0) {
        return;
    }
    const LVList* lv_list = getLVList();
    if (!lv_list) {
        // v2 path: no parse() was called, so lvars was never created.
        // Create an LVList from the AOT context locals so doTopLevelInstantiation() works.
        lvars = new LVList(ctx->locals, ctx->num_locals);
        return;
    }
    // Update the LVList entries from the AOT context
    size_t count = std::min(static_cast<size_t>(lv_list->size()),
                            static_cast<size_t>(ctx->num_locals));
    for (size_t i = 0; i < count; ++i) {
        // Note: this modifies const data, but it's needed for pointer consistency
        const_cast<LVList*>(lv_list)->lv[i] = ctx->locals[i];
    }
}

int TopLevelStatementBlock::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return execImpl(rc, return_value, xsink);
}

int TopLevelStatementBlock::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    // do not instantiate local vars here; they are instantiated by the QoreProgram object for each thread

    // Get the parse options from the current program at runtime.
    // Use getProgram() (the thread-local current program set by ProgramThreadCountContextHelper)
    // rather than rc.getProgram() which may return the outer/calling program.
    // NOTE: We can't use pwo.parse_options because the TopLevelStatementBlock is constructed
    // before the program's pwo is initialized (due to C++ member initialization order).
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        pgm = rc.getProgram();
    }
    int64 runtime_parse_options = qore_program_private::getParseWarnOptions(pgm).parse_options;

    // AOT pre-compiled top-level function — execute directly if registered
    if (!(runtime_parse_options & PO_ALLOW_REPARSE)) {
        if (cached_toplevel_aot_fn && cached_toplevel_aot_ctx) {
            // Instantiate nested body locals before AOT execution.
            // Top-level locals are pre-instantiated by QoreProgram, but nested
            // locals from for/while/try blocks need to be instantiated here.
            const LVList* toplevel_lvars = getLVList();
            std::unordered_set<const LocalVar*> toplevel_set;
            if (toplevel_lvars) {
                for (unsigned i = 0; i < toplevel_lvars->size(); ++i) {
                    toplevel_set.insert(toplevel_lvars->lv[i]);
                }
            }
            std::vector<LocalVar*> nested_locals;
            for (LocalVar* lv : cached_toplevel_aot_ctx->all_body_locals) {
                if (toplevel_set.count(lv) == 0) {
                    lv->instantiate(runtime_parse_options);
                    nested_locals.push_back(lv);
                }
            }

            uint64_t result_bits = cached_toplevel_aot_fn(cached_toplevel_aot_ctx, xsink);

            // Uninstantiate nested locals after AOT execution (reverse order)
            for (auto it = nested_locals.rbegin(); it != nested_locals.rend(); ++it) {
                (*it)->uninstantiate(xsink);
            }

            QoreValue result;
            std::memcpy(&result, &result_bits, sizeof(result));
            if (!*xsink) {
                return_value = result;
            }
            return 0;
        }
        if (cached_toplevel_jit_fn) {
            uint64_t result_bits = cached_toplevel_jit_fn(xsink);
            QoreValue result;
            std::memcpy(&result, &result_bits, sizeof(result));
            if (!*xsink) {
                return_value = result;
            }
            return 0;
        }
    }

    // IR/JIT/Tiered execution dispatch — only for non-REPARSE mode
    if (!(runtime_parse_options & PO_ALLOW_REPARSE)) {
        qore_exec_mode_t exec_mode = qore_program_private::get(*pgm)->exec_mode;
        // QEM_TIERED uses IR/JIT immediately for top-level code (same as QEM_JIT)
        // but only for %modern programs — legacy parse options are not supported by IR
        if (exec_mode == QEM_IR || exec_mode == QEM_JIT
            || (exec_mode == QEM_TIERED
                && (runtime_parse_options & PO_MODERN) == PO_MODERN)) {
            // Try to use cached IR if available
            QoreIRFunction* ir_func = cached_toplevel_ir;
            bool need_lower = !ir_func && !toplevel_ir_failed;

            if (need_lower) {
                std::call_once(toplevel_ir_once, [this, pgm]() {
                    // Make the top-level function name unique per TopLevelStatementBlock
                    // to avoid name collisions in the JIT's compiled_functions map.
                    // Different child Programs each have their own TopLevelStatementBlock
                    // with different LocalVar* pointers baked into JIT code.
                    // A monotonic counter is needed in addition to the address because
                    // when a Program is destroyed and a new one allocated at the same
                    // address, the old JIT cache entry would be returned with stale
                    // LocalVar pointers.
                    static std::atomic<uint64_t> toplevel_counter{0};
                    std::string unique_name = std::string("_toplevel@")
                        + std::to_string((uintptr_t)this) + "_"
                        + std::to_string(toplevel_counter.fetch_add(1));
                    QoreIRFunction* func = new QoreIRFunction(unique_name.c_str());

                    // Collect ALL body locals from the statement tree (top-level + nested blocks
                    // from fully-lowered statements like if/for/while/try/switch).  These are
                    // marked as pre-instantiated so the JIT skips instantiation/uninstantiation.
                    // Using collectAllStatementLocals ensures the same LocalVar* pointers are
                    // captured in all_body_locals and used for pre_instantiated_locals, avoiding
                    // pointer mismatches at runtime.
                    collectAllStatementLocals(this, func->all_body_locals);
                    for (LocalVar* lv : func->all_body_locals) {
                        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }

                    QoreIRBuilder builder(func);
                    auto* entry = func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(pgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string error;
                    if (!lowering.lowerStatementBlock(this, error)) {
                        toplevel_ir_failed = true;
                        delete func;
                        printd(1, "IR lowering failed: %s\n", error.c_str());
                        if (qore_program_private::get(*pgm)->ir_fallback_warn) {
                            printe("IR exec fallback to AST: %s\n", error.c_str());
                        }
                        qore_program_private::get(*pgm)->recordIRFallback(std::string("lowering: ") + error);
                        return;
                    }
                    if (!irBlockHasTerminator(builder.getBlock())) {
                        builder.createReturnNothing();
                    }
                    if (!QoreIRVerifier::verify(*func, error)) {
                        toplevel_ir_failed = true;
                        delete func;
                        printd(1, "IR verification failed: %s\n", error.c_str());
                        if (qore_program_private::get(*pgm)->ir_fallback_warn) {
                            printe("IR exec fallback to AST: verification failed: %s\n", error.c_str());
                        }
                        qore_program_private::get(*pgm)->recordIRFallback(std::string("verification: ") + error);
                        return;
                    }
                    // NOTE: do NOT call func->computeIROnlyLocals() for top-level code.
                    // Top-level locals are accessible by any called function/sub through the
                    // thread-local variable stack, but the IR-only analysis doesn't track
                    // cross-function access.  Marking a top-level local as IR-only would cause
                    // StoreLocal to only update the IR cache without syncing to the thread-local
                    // stack, making the variable invisible to called functions.
                    cached_toplevel_ir = func;
                });
                ir_func = cached_toplevel_ir;
            }

            if (ir_func) {
                if (qore_program_private::get(*pgm)->ir_dump) {
                    QoreIRPrinter::print(*ir_func, std::cout);
                }
                QoreValue ir_return_value;
                bool ok;
                // Build set of pre-instantiated local variables from the IR function's
                // all_body_locals.  This ensures the pointers match those embedded in
                // the JIT-compiled code (captured at IR creation time).
                std::unordered_set<const LocalVar*> pre_instantiated;
                for (LocalVar* lv : ir_func->all_body_locals) {
                    pre_instantiated.insert(lv);
                }

                // Instantiate nested body locals before JIT execution.
                // Top-level locals are pre-instantiated by QoreProgram, but nested
                // locals from for/while/try blocks need to be instantiated here.
                const LVList* toplevel_lvars = getLVList();
                std::unordered_set<const LocalVar*> toplevel_set;
                if (toplevel_lvars) {
                    for (unsigned i = 0; i < toplevel_lvars->size(); ++i) {
                        toplevel_set.insert(toplevel_lvars->lv[i]);
                    }
                }
                std::vector<LocalVar*> nested_locals;
                for (LocalVar* lv : ir_func->all_body_locals) {
                    if (toplevel_set.count(lv) == 0) {
                        lv->instantiate(runtime_parse_options);
                        nested_locals.push_back(lv);
                    }
                }

                std::string error;
                if (exec_mode == QEM_JIT || exec_mode == QEM_TIERED) {
                    ok = QoreJIT::instance().executeWithFallback(*ir_func, ir_return_value, xsink, error,
                        &pre_instantiated);
                    // Clear any pending deopt request from top-level JIT execution.
                    // Top-level code handles guard failures internally (e.g., via
                    // try-catch) and must not propagate deopt to subsequent calls.
                    qore_jit_deopt_requested();
                } else {
                    // suppress_guard_deopt=true: top-level code must not deopt on
                    // guard failure because re-executing the entire block from the
                    // beginning would duplicate side effects (I/O, mutations).
                    ok = QoreIRInterpreter::execute(*ir_func, ir_return_value, xsink, nullptr,
                        nullptr, nullptr, &pre_instantiated, nullptr, nullptr, true);
                }

                // Uninstantiate nested locals after JIT execution (reverse order)
                for (auto it = nested_locals.rbegin(); it != nested_locals.rend(); ++it) {
                    (*it)->uninstantiate(xsink);
                }

                if (ok && !*xsink) {
                    return_value = ir_return_value;
                    return 0;
                }
                // If IR execution raised an exception, propagate it
                if (*xsink) {
                    return 0;
                }
                // IR execution failed without exception — fall through to AST
                printd(1, "IR execution failed without exception, falling back to AST\n");
                if (qore_program_private::get(*pgm)->ir_fallback_warn) {
                    printe("IR exec fallback to AST: execution failed\n");
                }
                qore_program_private::get(*pgm)->recordIRFallback("execution: runtime failure");
            }
        }
    }

    // In REPARSE mode (PO_ALLOW_REPARSE), only execute statements that haven't been executed yet
    // This is determined by the execution high water mark (ehwm)
    if (runtime_parse_options & PO_ALLOW_REPARSE) {
        int stmt_rc = 0;

        // Determine start position - one past the execution high water mark
        statement_list_t::iterator start = ehwm;
        if (start != statement_list.end()) {
            ++start;
        } else {
            start = statement_list.begin();
        }

        // If nothing new to execute, return early
        if (start == statement_list.end()) {
            return 0;
        }

        ThreadLocalProgramData* tlpd = rc.getThreadLocalProgramData()
            ? rc.getThreadLocalProgramData()
            : get_thread_local_program_data();
        // Execute only new statements
        for (statement_list_t::iterator i = start; i != statement_list.end(); ++i) {
            if (tlpd->runtimeCheck()) {
                stmt_rc = tlpd->dbgStep(this, *i, xsink);
                if (stmt_rc || *xsink) {
                    break;
                }
            }
            stmt_rc = (*i)->exec(rc, return_value, xsink);
            // Update ehwm to current statement after successful execution
            // so that on exception, only successfully executed statements are marked
            if (!stmt_rc && !*xsink) {
                ehwm = i;
            }
            if (*xsink && tlpd->runtimeCheck()) {
                tlpd->dbgException(*i, xsink);
                if (*xsink) {
                    break;
                }
            }
            if (stmt_rc) break;
        }
        return stmt_rc;
    }

    // Normal mode - execute all statements
    return execIntern(rc, return_value, xsink);
}

QoreParseContextLvarHelper::~QoreParseContextLvarHelper() {
    if (parse_context.lvids) {
        lvars = new LVList(parse_context.lvids);
    }
    parse_context.lvids = lvids;
}

// NarrowedTypeHelper implementation

void NarrowedTypeHelper::saveState() {
    saved_types.clear();
    VNode* vnode = getVStack();
    while (vnode) {
        if (vnode->lvar && vnode->lvar->isAutoType()) {
            saved_types.push_back({vnode->lvar, vnode->lvar->parseGetNarrowedType()});
        }
        vnode = vnode->nextSearch();
    }
}

void NarrowedTypeHelper::restoreState() {
    for (const auto& entry : saved_types) {
        // Reset or set the narrowed type
        if (entry.second) {
            entry.first->parseSetNarrowedType(entry.second);
        } else {
            entry.first->parseResetNarrowedType();
        }
    }
}

void NarrowedTypeHelper::recordBranchAndRestore() {
    // Record current narrowed types for this branch
    type_map_t branch;
    VNode* vnode = getVStack();
    while (vnode) {
        if (vnode->lvar && vnode->lvar->isAutoType()) {
            branch.push_back({vnode->lvar, vnode->lvar->parseGetNarrowedType()});
        }
        vnode = vnode->nextSearch();
    }
    branch_types.push_back(std::move(branch));

    // Restore to pre-branch state
    restoreState();
}

void NarrowedTypeHelper::recordSavedAsImplicitBranch() {
    // Record the saved types as an implicit branch
    // This represents the code path where the if condition was false
    branch_types.push_back(saved_types);
}

void NarrowedTypeHelper::mergeAndApply() {
    if (branch_types.empty()) {
        return;
    }

    // For each saved variable, find the common type across all branches
    for (const auto& saved_entry : saved_types) {
        LocalVar* lvar = saved_entry.first;
        const QoreTypeInfo* common_type = nullptr;
        bool found_in_all = true;
        bool first = true;

        for (const auto& branch : branch_types) {
            bool found = false;
            for (const auto& branch_entry : branch) {
                if (branch_entry.first == lvar) {
                    found = true;
                    if (first) {
                        common_type = branch_entry.second;
                        first = false;
                    } else if (branch_entry.second != common_type) {
                        // Merge types using matchCommonType
                        if (common_type && branch_entry.second) {
                            const QoreTypeInfo* merged = common_type;
                            if (!QoreTypeInfo::matchCommonType(merged, branch_entry.second)) {
                                // Types incompatible, reset to original (un-narrowed)
                                common_type = nullptr;
                            } else {
                                common_type = merged;
                            }
                        } else {
                            // One branch has null (reset), use null
                            common_type = nullptr;
                        }
                    }
                    break;
                }
            }
            if (!found) {
                found_in_all = false;
                break;
            }
        }

        // Apply the merged type
        if (found_in_all && common_type) {
            lvar->parseSetNarrowedType(common_type);
        } else if (found_in_all) {
            // All branches found but no common type - keep original
            if (saved_entry.second) {
                lvar->parseSetNarrowedType(saved_entry.second);
            } else {
                lvar->parseResetNarrowedType();
            }
        }
    }
}
