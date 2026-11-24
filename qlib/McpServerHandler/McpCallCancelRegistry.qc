# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpCallCancelRegistry.qc @brief The cancel handlers of the requests in progress

/*  McpCallCancelRegistry.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#! the %McpServerHandler namespace holds all public definitions in the %McpServerHandler module
public namespace McpServerHandler {
#! Identifies the request a call context belongs to, for its cancel handlers (internal)
hashdecl McpCallCancelScopeInfo {
    #! The key of the request in the registry
    string key;
    #! What the request is: \c "r" (a request in a session), \c "t" (a task in a session), \c "m" (a modern task)
    string kind;
    #! The session ID, for \c "r" and \c "t"
    *string sid;
    #! The JSON-RPC request ID, for \c "r", and for \c "t" when the task has one
    *string requestId;
    #! The task ID, for \c "t" and \c "m"
    *string taskId;
}

#! The cancel state of one request in progress (internal)
hashdecl McpCallCancelScopeState {
    #! The number of call contexts open for the request
    int refs = 0;
    #! True once the request was cancelled
    bool cancelled = False;
    #! The handlers not yet called, by handler ID
    hash<string, code> handlers = {};
    #! The number of threads calling the request's handlers
    int dispatching = 0;
}

#! The cancel handlers of the requests in progress of one McpServerHandler (internal)
/** A request's entry exists while a call context for it is open (begin() .. end()).  Handlers are called by the
    handler, outside this object's lock; this object only decides which handlers a cancel or a registration calls,
    so each handler is called at most once.
*/
class McpCallCancelRegistry {
    private:internal {
        #! Guards the members below
        Mutex m();
        #! Signalled when the last call of a request's handlers ends
        Condition cond();
        #! The requests in progress, by key
        hash<string, hash<McpCallCancelScopeState>> scopes = {};
        #! The key of the request of each handler not yet called, by handler ID
        hash<string, string> handler_keys = {};
        #! The last handler ID given
        int seq = 0;
    }

    #! A call context for the request opens
    begin(string key) {
        AutoLock al(m);
        if (!scopes{key}) {
            scopes{key} = <McpCallCancelScopeState>{};
        }
        ++scopes{key}.refs;
    }

    #! A call context for the request closes; the last one removes the request's handlers
    /** Waits until the handlers of the request being called by a cancel have returned, so no handler of a request
        runs after its request ended.
    */
    end(string key) {
        AutoLock al(m);
        if (!scopes{key}) {
            return;
        }
        if (--scopes{key}.refs > 0) {
            return;
        }
        while (scopes{key}.dispatching > 0) {
            cond.wait(m);
        }
        # a context opened for the same request while this one waited
        if (scopes{key}.refs > 0) {
            return;
        }
        foreach string id in (keys scopes{key}.handlers) {
            remove handler_keys{id};
        }
        remove scopes{key};
    }

    #! Registers a handler for the request
    /** @param key the request
        @param handler the handler
        @param cancelled True when the request's state shows it cancelled

        @return \c id: the handler's ID, \c run: True when the request was cancelled already, in which case the
        handler is not kept and the caller must call it, then call dispatched(); nothing when the request has no
        call context open
    */
    *hash<auto> add(string key, code handler, bool cancelled) {
        AutoLock al(m);
        if (!scopes{key}) {
            return;
        }
        int id = ++seq;
        if (cancelled || scopes{key}.cancelled) {
            scopes{key}.cancelled = True;
            ++scopes{key}.dispatching;
            return {"id": id, "run": True};
        }
        scopes{key}.handlers{id} = handler;
        handler_keys{id} = key;
        return {"id": id, "run": False};
    }

    #! Removes a handler not yet called
    /** @return True when the handler was removed; False when it was called already, its request ended, or the ID
        is unknown
    */
    bool removeHandler(int id) {
        AutoLock al(m);
        *string key = remove handler_keys{id};
        if (!key) {
            return False;
        }
        remove scopes{key}.handlers{id};
        return True;
    }

    #! Marks the requests cancelled and takes the handlers each cancel must call
    /** @return the handlers to call, in the order they were registered, by key; the caller must call dispatched()
        for every key returned, after its handlers were called
    */
    hash<string, list<code>> take(list<string> key_list) {
        hash<string, list<code>> rv = {};
        AutoLock al(m);
        foreach string key in (key_list) {
            if (!scopes{key} || scopes{key}.cancelled) {
                continue;
            }
            scopes{key}.cancelled = True;
            list<code> handlers = ();
            foreach string id in (sort(keys scopes{key}.handlers, int sub (string l, string r) {
                    return l.toInt() <=> r.toInt();
                })) {
                handlers += scopes{key}.handlers{id};
                remove handler_keys{id};
            }
            scopes{key}.handlers = {};
            ++scopes{key}.dispatching;
            rv{key} = handlers;
        }
        return rv;
    }

    #! The call of a request's handlers taken by take() or add() has ended
    dispatched(string key) {
        AutoLock al(m);
        if (!scopes{key}) {
            return;
        }
        if (!--scopes{key}.dispatching) {
            cond.broadcast();
        }
    }

    #! Returns the number of requests in progress and of handlers not yet called
    hash<string, int> getInfo() {
        AutoLock al(m);
        return {
            "requests": scopes.size(),
            "handlers": handler_keys.size(),
        };
    }
}
}
