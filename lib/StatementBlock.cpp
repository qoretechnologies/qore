/*
    Statement.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    QoreValue return_value;
    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    if (tlpd->runtimeCheck()) {
        tlpd->dbgFunctionEnter(this, xsink);
    }
    execImpl(return_value, xsink);
    if (tlpd->runtimeCheck()) {
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
    //QORE_TRACE("StatementBlock::execImpl()");
    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    return execIntern(return_value, xsink);
}

int StatementBlock::execIntern(QoreValue& return_value, ExceptionSink* xsink) {
    //QORE_TRACE("StatementBlock::execIntern()");
    int rc = 0;

    assert(xsink);

    //printd(5, "StatementBlock::execIntern() this=%p, lvars=%p, %ld vars\n", this, lvars, lvars->size());

    bool obe = !on_block_exit_list.empty();
    // push "on block exit" iterator if necessary
    if (obe) {
        pushBlock(on_block_exit_list.end());
    }

    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    // to execute even when block is empty, e.g. while(true);
    if (tlpd->runtimeCheck()) {
        rc = tlpd->dbgStep(this, nullptr, xsink);
    }
    if (!rc && !*xsink) {
        // execute block
        for (auto i : statement_list) {
            if (tlpd->runtimeCheck()) {
                rc = tlpd->dbgStep(this, i, xsink);
                if (rc || *xsink) {
                    break;
                }
            }
            rc = i->exec(return_value, xsink);
            if (*xsink && tlpd->runtimeCheck()) {
                tlpd->dbgException(i, xsink);
                if (*xsink) {
                    break;
                }
            }
            if (rc) break;
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
                        nrc = (*i).second->execImpl(return_value, &obe_xsink);
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
            rc = nrc;
    }

    return rc;
}

// top-level block (program) execution member function
void StatementBlock::exec() {
    ExceptionSink xsink;
    exec(&xsink);
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

    if (!first && lvars) {
        // push already-registered local variables on the stack
        for (unsigned i = 0; i < lvars->size(); ++i)
            push_top_level_local_var(lvars->lv[i], loc);
    }

    QoreParseContext parse_context;
    parse_context.setFlags(PF_TOP_LEVEL);
    if (!first) {
        parse_context.setFlags(PF_NO_TOP_LEVEL_LVARS);
    }
    int err = parseInitIntern(parse_context, hwm);

    //printd(5, "TopLevelStatementBlock::parseInit(rns=%p) first=%d, lvids=%d\n", &rns, first, parse_context.lvids);

    if (!first && parse_context.lvids) {
        // discard variables immediately
        for (int i = 0; i < parse_context.lvids; ++i) {
            pop_local_var();
        }
    }

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
    } else if (lvars) {
        for (unsigned i = 0; i < lvars->size(); ++i) {
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

int TopLevelStatementBlock::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    // do not instantiate local vars here; they are instantiated by the QoreProgram object for each thread

    // Get the parse options from the current program at runtime
    // NOTE: We can't use pwo.parse_options here because the TopLevelStatementBlock is constructed
    // before the program's pwo is initialized (due to C++ member initialization order)
    int64 parse_options = qore_program_private::getParseWarnOptions(getProgram()).parse_options;

    // In REPARSE mode (PO_ALLOW_REPARSE), only execute statements that haven't been executed yet
    // This is determined by the execution high water mark (ehwm)
    if (parse_options & PO_ALLOW_REPARSE) {
        int rc = 0;

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

        ThreadLocalProgramData* tlpd = get_thread_local_program_data();
        // Execute only new statements
        for (statement_list_t::iterator i = start; i != statement_list.end(); ++i) {
            if (tlpd->runtimeCheck()) {
                rc = tlpd->dbgStep(this, *i, xsink);
                if (rc || *xsink) {
                    break;
                }
            }
            rc = (*i)->exec(return_value, xsink);
            if (*xsink && tlpd->runtimeCheck()) {
                tlpd->dbgException(*i, xsink);
                if (*xsink) {
                    break;
                }
            }
            if (rc) break;
        }

        // Update execution high water mark to last statement in list
        ehwm = statement_list.last();
        return rc;
    }

    // Normal mode - execute all statements
    return execIntern(return_value, xsink);
}

QoreParseContextLvarHelper::~QoreParseContextLvarHelper() {
    if (parse_context.lvids) {
        lvars = new LVList(parse_context.lvids);
    }
    parse_context.lvids = lvids;
}