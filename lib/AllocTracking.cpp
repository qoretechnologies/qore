/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AllocTracking.cpp

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

#include "qore/intern/config.h"

#ifdef QORE_HAVE_ALLOC_TRACKING

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <execinfo.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include <qore/Qore.h>
#include "qore/intern/AllocTracking.h"
#include "qore/intern/xxhash.h"

// Maximum backtrace depth to capture
static constexpr int BT_DEPTH = 10;
// Frames to skip (malloc wrapper + tracking code)
static constexpr int BT_SKIP = 3;

// Allocation site info
struct AllocSiteInfo {
    int64_t alloc_count = 0;
    int64_t alloc_bytes = 0;
    int64_t free_count = 0;
    int64_t free_bytes = 0;
    void* frames[BT_DEPTH]{};
    int frame_count = 0;
};

// Per-pointer tracking info
struct PtrInfo {
    size_t size;
    uint64_t bt_hash;
};

// Object class tracking
struct ObjClassInfo {
    int64_t created = 0;
    int64_t destroyed = 0;
};

// Global state
static std::atomic<bool> tracking_active{false};
static thread_local bool in_hook = false;

static std::mutex sites_mutex;
static std::unordered_map<uint64_t, AllocSiteInfo>* alloc_sites = nullptr;

static std::mutex ptrs_mutex;
static std::unordered_map<void*, PtrInfo>* live_ptrs = nullptr;

static std::mutex obj_mutex;
static std::unordered_map<std::string, ObjClassInfo>* obj_classes = nullptr;

// Real function pointers
static void* (*real_malloc)(size_t) = nullptr;
static void (*real_free)(void*) = nullptr;
static void* (*real_calloc)(size_t, size_t) = nullptr;
static void* (*real_realloc)(void*, size_t) = nullptr;

// Bootstrap buffer for dlsym's internal calloc calls
static char bootstrap_buf[8192];
static std::atomic<size_t> bootstrap_offset{0};
static std::atomic<bool> real_funcs_resolved{false};

static bool is_bootstrap_ptr(void* ptr) {
    return ptr >= bootstrap_buf && ptr < bootstrap_buf + sizeof(bootstrap_buf);
}

// RAII guard for re-entrancy protection
struct HookGuard {
    HookGuard() { in_hook = true; }
    ~HookGuard() { in_hook = false; }
};

static void init_real_functions() {
    if (real_funcs_resolved.load(std::memory_order_acquire)) {
        return;
    }
    // Resolve real functions - calloc must be last because dlsym may call calloc
    real_malloc = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
    real_free = reinterpret_cast<void(*)(void*)>(dlsym(RTLD_NEXT, "free"));
    real_realloc = reinterpret_cast<void*(*)(void*, size_t)>(dlsym(RTLD_NEXT, "realloc"));
    real_calloc = reinterpret_cast<void*(*)(size_t, size_t)>(dlsym(RTLD_NEXT, "calloc"));
    real_funcs_resolved.store(true, std::memory_order_release);
}

static uint64_t capture_backtrace(void** out_frames, int* out_count) {
    void* buf[BT_DEPTH + BT_SKIP];
    int n = backtrace(buf, BT_DEPTH + BT_SKIP);
    int start = std::min(BT_SKIP, n);
    int count = n - start;
    if (count > BT_DEPTH) {
        count = BT_DEPTH;
    }
    if (count > 0) {
        memcpy(out_frames, buf + start, count * sizeof(void*));
    }
    *out_count = count;
    return XXH64(out_frames, count * sizeof(void*), 0);
}

static void track_alloc(void* ptr, size_t size) {
    if (!ptr || !tracking_active.load(std::memory_order_relaxed) || in_hook) {
        return;
    }
    HookGuard guard;

    void* frames[BT_DEPTH];
    int frame_count = 0;
    uint64_t bt_hash = capture_backtrace(frames, &frame_count);

    {
        std::lock_guard<std::mutex> lg(sites_mutex);
        if (alloc_sites) {
            auto& site = (*alloc_sites)[bt_hash];
            if (site.frame_count == 0) {
                memcpy(site.frames, frames, frame_count * sizeof(void*));
                site.frame_count = frame_count;
            }
            site.alloc_count++;
            site.alloc_bytes += size;
        }
    }
    {
        std::lock_guard<std::mutex> lg(ptrs_mutex);
        if (live_ptrs) {
            (*live_ptrs)[ptr] = {size, bt_hash};
        }
    }
}

static void track_free(void* ptr) {
    if (!ptr || !tracking_active.load(std::memory_order_relaxed) || in_hook) {
        return;
    }
    HookGuard guard;

    std::lock_guard<std::mutex> lg(ptrs_mutex);
    if (!live_ptrs) {
        return;
    }
    auto it = live_ptrs->find(ptr);
    if (it == live_ptrs->end()) {
        return; // Pointer was allocated before tracking started
    }
    size_t size = it->second.size;
    uint64_t bt_hash = it->second.bt_hash;
    live_ptrs->erase(it);

    std::lock_guard<std::mutex> lg2(sites_mutex);
    if (alloc_sites) {
        auto sit = alloc_sites->find(bt_hash);
        if (sit != alloc_sites->end()) {
            sit->second.free_count++;
            sit->second.free_bytes += size;
        }
    }
}

// Public API

void qore_alloc_tracking_object_created(const char* class_name) {
    if (!tracking_active.load(std::memory_order_relaxed) || in_hook) {
        return;
    }
    HookGuard guard;
    std::lock_guard<std::mutex> lg(obj_mutex);
    if (obj_classes) {
        (*obj_classes)[class_name].created++;
    }
}

void qore_alloc_tracking_object_destroyed(const char* class_name) {
    if (!tracking_active.load(std::memory_order_relaxed) || in_hook) {
        return;
    }
    HookGuard guard;
    std::lock_guard<std::mutex> lg(obj_mutex);
    if (obj_classes) {
        (*obj_classes)[class_name].destroyed++;
    }
}

void qore_alloc_tracking_start() {
    init_real_functions();

    std::lock_guard<std::mutex> lg1(sites_mutex);
    std::lock_guard<std::mutex> lg2(ptrs_mutex);
    std::lock_guard<std::mutex> lg3(obj_mutex);

    delete alloc_sites;
    delete live_ptrs;
    delete obj_classes;
    alloc_sites = new std::unordered_map<uint64_t, AllocSiteInfo>();
    live_ptrs = new std::unordered_map<void*, PtrInfo>();
    obj_classes = new std::unordered_map<std::string, ObjClassInfo>();
    // Reserve space to reduce rehashing during tracking
    alloc_sites->reserve(4096);
    live_ptrs->reserve(65536);

    tracking_active.store(true, std::memory_order_release);
}

QoreHashNode* qore_alloc_tracking_stop(ExceptionSink* xsink) {
    tracking_active.store(false, std::memory_order_release);

    std::unordered_map<uint64_t, AllocSiteInfo>* sites;
    std::unordered_map<std::string, ObjClassInfo>* objs;
    {
        std::lock_guard<std::mutex> lg(sites_mutex);
        sites = alloc_sites;
        alloc_sites = nullptr;
    }
    {
        std::lock_guard<std::mutex> lg(ptrs_mutex);
        delete live_ptrs;
        live_ptrs = nullptr;
    }
    {
        std::lock_guard<std::mutex> lg(obj_mutex);
        objs = obj_classes;
        obj_classes = nullptr;
    }

    if (!sites) {
        delete objs;
        return nullptr;
    }

    // Sort by leak_bytes descending
    std::vector<std::pair<uint64_t, const AllocSiteInfo*>> sorted;
    sorted.reserve(sites->size());
    for (auto& kv : *sites) {
        sorted.emplace_back(kv.first, &kv.second);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return (a.second->alloc_bytes - a.second->free_bytes) > (b.second->alloc_bytes - b.second->free_bytes);
    });

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    for (auto& kv : sorted) {
        const AllocSiteInfo& site = *kv.second;
        int64_t leak_bytes = site.alloc_bytes - site.free_bytes;

        // Only include sites with positive net allocation
        if (leak_bytes <= 0) {
            continue;
        }

        ReferenceHolder<QoreHashNode> entry(new QoreHashNode(autoTypeInfo), xsink);
        entry->setKeyValue("alloc_count", site.alloc_count, xsink);
        entry->setKeyValue("alloc_bytes", site.alloc_bytes, xsink);
        entry->setKeyValue("free_count", site.free_count, xsink);
        entry->setKeyValue("free_bytes", site.free_bytes, xsink);
        entry->setKeyValue("leak_bytes", leak_bytes, xsink);

        // Symbolize backtrace frames
        ReferenceHolder<QoreListNode> frames_list(new QoreListNode(stringTypeInfo), xsink);
        if (site.frame_count > 0) {
            char** syms = backtrace_symbols(site.frames, site.frame_count);
            if (syms) {
                for (int i = 0; i < site.frame_count; i++) {
                    frames_list->push(new QoreStringNode(syms[i]), xsink);
                }
                real_free(syms); // backtrace_symbols uses malloc
            }
        }
        entry->setKeyValue("frames", frames_list.release(), xsink);

        // Key: hex backtrace hash
        QoreString key;
        key.sprintf("%016llx", (unsigned long long)kv.first);
        result->setKeyValue(key.c_str(), entry.release(), xsink);
    }

    delete sites;

    // Add object class tracking data sorted by net count (created - destroyed)
    if (objs && !objs->empty()) {
        std::vector<std::pair<std::string, const ObjClassInfo*>> obj_sorted;
        obj_sorted.reserve(objs->size());
        for (auto& kv : *objs) {
            obj_sorted.emplace_back(kv.first, &kv.second);
        }
        std::sort(obj_sorted.begin(), obj_sorted.end(), [](const auto& a, const auto& b) {
            return (a.second->created - a.second->destroyed) > (b.second->created - b.second->destroyed);
        });

        ReferenceHolder<QoreHashNode> obj_hash(new QoreHashNode(autoTypeInfo), xsink);
        for (auto& kv : obj_sorted) {
            int64_t net = kv.second->created - kv.second->destroyed;
            if (net <= 0) {
                continue;
            }
            ReferenceHolder<QoreHashNode> entry(new QoreHashNode(autoTypeInfo), xsink);
            entry->setKeyValue("created", kv.second->created, xsink);
            entry->setKeyValue("destroyed", kv.second->destroyed, xsink);
            entry->setKeyValue("net", net, xsink);
            obj_hash->setKeyValue(kv.first.c_str(), entry.release(), xsink);
        }
        result->setKeyValue("_objects", obj_hash.release(), xsink);
    }
    delete objs;

    return result.release();
}

bool qore_alloc_tracking_active() {
    return tracking_active.load(std::memory_order_relaxed);
}

// Global malloc/free/calloc/realloc interposers

extern "C" {

void* malloc(size_t size) {
    if (!real_funcs_resolved.load(std::memory_order_acquire)) {
        init_real_functions();
    }
    void* ptr = real_malloc(size);
    track_alloc(ptr, size);
    return ptr;
}

void free(void* ptr) {
    if (is_bootstrap_ptr(ptr)) {
        return;
    }
    if (!real_funcs_resolved.load(std::memory_order_acquire)) {
        init_real_functions();
    }
    track_free(ptr);
    real_free(ptr);
}

void* calloc(size_t nmemb, size_t size) {
    if (!real_funcs_resolved.load(std::memory_order_acquire)) {
        // Bootstrap: dlsym itself calls calloc
        size_t total = nmemb * size;
        size_t off = bootstrap_offset.fetch_add(total);
        if (off + total <= sizeof(bootstrap_buf)) {
            memset(bootstrap_buf + off, 0, total);
            return bootstrap_buf + off;
        }
        // Should not happen
        return nullptr;
    }
    void* ptr = real_calloc(nmemb, size);
    track_alloc(ptr, nmemb * size);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (is_bootstrap_ptr(ptr)) {
        // Can't realloc bootstrap memory — allocate new and copy
        if (!real_funcs_resolved.load(std::memory_order_acquire)) {
            return nullptr;
        }
        void* new_ptr = real_malloc(size);
        if (new_ptr && ptr) {
            memcpy(new_ptr, ptr, size); // May over-read, but bootstrap buf is safe
        }
        track_alloc(new_ptr, size);
        return new_ptr;
    }
    if (!real_funcs_resolved.load(std::memory_order_acquire)) {
        init_real_functions();
    }
    // Track free of old pointer
    track_free(ptr);
    void* new_ptr = real_realloc(ptr, size);
    // Track alloc of new pointer
    track_alloc(new_ptr, size);
    return new_ptr;
}

} // extern "C"

#endif // QORE_HAVE_ALLOC_TRACKING
