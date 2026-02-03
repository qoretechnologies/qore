/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOT.h

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

#ifndef _QORE_QOREAOT_H
#define _QORE_QOREAOT_H

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

class ExceptionSink;
class LocalVar;
class QoreIRFunction;
class QoreNamespace;
class QoreProgram;
class QoreStringNode;
class StatementBlock;
class Var;

//! AOT context: runtime-resolved pointer tables for AOT-compiled functions.
/** At compile time, each process-specific pointer (LocalVar*, Var*, QoreValue expr)
    is assigned a numeric slot index. At runtime, the fresh AST is re-lowered in the
    same deterministic order to collect fresh pointers into these arrays. The AOT-compiled
    native code uses index-based lookups instead of embedded pointers.
*/
struct QoreAOTContext {
    LocalVar** locals = nullptr;    //!< LoadLocal/StoreLocal/LoadClosure/StoreClosure/instantiate
    int num_locals = 0;
    Var** globals = nullptr;        //!< LoadGlobal/StoreGlobal/LoadThreadLocal/StoreThreadLocal
    int num_globals = 0;
    uint64_t* exprs = nullptr;      //!< NaN-boxed QoreValue for Invoke/Call/CallMethod/CallStatic/LValue
    int num_exprs = 0;
    StatementBlock** stmts = nullptr;   //!< OnBlockExit statement blocks
    int num_stmts = 0;

    //! All body locals from the fresh IR (needed by evalTiered for instantiation)
    std::vector<LocalVar*> all_body_locals;

    //! Destructor: deref all held expression values, then free arrays.
    //! Implemented in QoreAOT.cpp because it needs QoreValue.
    ~QoreAOTContext();

    //! Allocate arrays based on slot counts
    void allocate() {
        if (num_locals > 0) {
            locals = static_cast<LocalVar**>(calloc(num_locals, sizeof(LocalVar*)));
        }
        if (num_globals > 0) {
            globals = static_cast<Var**>(calloc(num_globals, sizeof(Var*)));
        }
        if (num_exprs > 0) {
            exprs = static_cast<uint64_t*>(calloc(num_exprs, sizeof(uint64_t)));
        }
        if (num_stmts > 0) {
            stmts = static_cast<StatementBlock**>(calloc(num_stmts, sizeof(StatementBlock*)));
        }
    }
};

//! Compile-time slot assignment map for AOT pointer indirection.
/** Assigns monotonically increasing slot indices to each unique pointer encountered
    during IR lowering. The same source produces the same AST produces the same IR
    produces the same walk order, guaranteeing that slot indices match between compile
    time and runtime.
*/
struct AOTSlotMap {
    std::unordered_map<const void*, int32_t> local_slots;   //!< LocalVar* -> slot index
    std::unordered_map<const void*, int32_t> global_slots;  //!< Var* -> slot index
    std::unordered_map<uint64_t, int32_t> expr_slots;       //!< NaN-boxed expr bits -> slot index
    std::unordered_map<const void*, int32_t> stmt_slots;    //!< StatementBlock* -> slot index (OnBlockExit)

    //! Get or assign a slot for a LocalVar*
    int32_t getLocalSlot(const void* local) {
        auto it = local_slots.find(local);
        if (it != local_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(local_slots.size());
        local_slots[local] = slot;
        return slot;
    }

    //! Get or assign a slot for a Var* (global or thread-local)
    int32_t getGlobalSlot(const void* var) {
        auto it = global_slots.find(var);
        if (it != global_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(global_slots.size());
        global_slots[var] = slot;
        return slot;
    }

    //! Get or assign a slot for an expression (NaN-boxed QoreValue bits)
    int32_t getExprSlot(uint64_t expr_bits) {
        auto it = expr_slots.find(expr_bits);
        if (it != expr_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(expr_slots.size());
        expr_slots[expr_bits] = slot;
        return slot;
    }

    //! Get or assign a slot for a StatementBlock* (OnBlockExit)
    int32_t getStmtSlot(const void* stmt) {
        auto it = stmt_slots.find(stmt);
        if (it != stmt_slots.end()) {
            return it->second;
        }
        int32_t slot = static_cast<int32_t>(stmt_slots.size());
        stmt_slots[stmt] = slot;
        return slot;
    }
};

//! AOT function pointer type: takes QoreAOTContext* and ExceptionSink*
using AotFunctionPtr = uint64_t (*)(QoreAOTContext*, ExceptionSink*);

//! Descriptor for a pre-compiled AOT function
struct QoreAOTFunc {
    const char* name;                           //!< unique function identifier (from IR lowering)
    AotFunctionPtr fn_ptr;                      //!< pre-compiled native code pointer (ctx, xsink)
    int num_locals;                             //!< number of local variable slots
    int num_globals;                            //!< number of global variable slots
    int num_exprs;                              //!< number of expression slots
    int num_stmts;                              //!< number of statement slots (OnBlockExit)
};

//! C ABI entry point called by AOT-compiled binaries from their generated main()
/** Initializes the Qore runtime, re-parses the embedded source to build the AST/type system,
    registers pre-compiled function pointers, and runs the program.
    Functions without pre-compiled pointers fall back to runtime JIT compilation.

    @param argc command-line argument count
    @param argv command-line argument vector
    @param source embedded Qore source text
    @param source_len length of source text in bytes
    @param label label for the source (e.g. script filename)
    @param parse_options parse options used during original compilation
    @param functions array of pre-compiled function descriptors
    @param num_functions number of entries in functions array
    @return exit code (0 = success)
*/
extern "C" int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT binaries from their generated main()
/** Deserializes binary metadata to reconstruct the namespace tree (instead of re-parsing
    source), builds AOT contexts from slot maps, and runs the program.
    Functions with unsupported expression types use the embedded fallback source.

    @param argc command-line argument count
    @param argv command-line argument vector
    @param metadata serialized binary metadata blob
    @param metadata_len length of metadata blob in bytes
    @param label label for the source (e.g. script filename)
    @param parse_options parse options used during original compilation
    @param functions array of pre-compiled function descriptors
    @param num_functions number of entries in functions array
    @return exit code (0 = success)
*/
extern "C" int qore_aot_run_v2(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
);

//! Module metadata extracted from .qm source
struct QoreAOTModuleInfo {
    std::string name;           //!< module feature name
    std::string version;        //!< version string
    std::string desc;           //!< description
    std::string author;         //!< author
    std::string url;            //!< URL (optional)
    std::string license;        //!< license string (optional, default "MIT")
};

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_init()
/** Parses the embedded source to build the module namespace tree, registers pre-compiled
    function pointers, and returns nullptr on success or an error string on failure.
*/
extern "C" QoreStringNode* qore_aot_module_init(
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by source-stripped AOT modules from their generated qore_module_init()
/** Deserializes binary metadata instead of re-parsing source.
*/
extern "C" QoreStringNode* qore_aot_module_init_v2(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
);

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_ns_init()
extern "C" void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns);

//! C ABI entry point called by AOT-compiled modules from their generated qore_module_delete()
extern "C" void qore_aot_module_delete();

//! AOT compiler class — compiles a parsed QoreProgram to a standalone executable
class QoreAOT {
public:
    //! Compile a parsed program to a standalone executable
    /** @param pgm parsed QoreProgram (must have been parsed successfully)
        @param source_text original source text to embed in the binary
        @param source_len length of source text
        @param label source label (filename)
        @param output_path path for the output executable
        @param parse_options parse options to embed
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @param static_link statically link libqore (requires libqore_static.a)
        @return true on success, false on failure
    */
    static bool compile(QoreProgram* pgm,
                       const char* source_text, int source_len,
                       const char* label,
                       const std::string& output_path,
                       int64_t parse_options,
                       std::string& error,
                       int opt_level = 2,
                       const char* target_triple = nullptr,
                       bool static_link = false,
                       bool strip_source = false);

    //! Compile a .qm user module to a native shared library (.so) binary module
    /** @param source_text original module source text to embed
        @param source_len length of source text
        @param label source label (filename)
        @param output_path path for the output shared library
        @param parse_options parse options to embed
        @param error error message on failure
        @param opt_level LLVM optimization level 0-3 (default: 2)
        @param target_triple target triple for cross-compilation (nullptr = native)
        @return true on success, false on failure
    */
    static bool compileModule(const char* source_text, int source_len,
                              const char* label,
                              const std::string& output_path,
                              int64_t parse_options,
                              std::string& error,
                              int opt_level = 2,
                              const char* target_triple = nullptr,
                              bool strip_source = false);
};

//! Build an AOTSlotMap by walking an IR function's instructions in deterministic order.
/** Assigns slot indices to each unique LocalVar*, Var*, and expression QoreValue.
    @param func the IR function to walk
    @param slots output slot map
*/
void buildAOTSlotMap(const QoreIRFunction& func, AOTSlotMap& slots);

//! Build a QoreAOTContext by walking a freshly-lowered IR function in the same order.
/** The fresh IR must have been produced from the same source as the compile-time IR,
    guaranteeing the same walk order and slot index correspondence.
    @param func the freshly-lowered IR function
    @param slots the slot map from compile time (used only for validation)
    @return heap-allocated context (caller takes ownership), or nullptr on mismatch
*/
QoreAOTContext* buildAOTContext(const QoreIRFunction& func, int num_locals, int num_globals, int num_exprs, int num_stmts);

#endif
