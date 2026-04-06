# -*- mode: qore; indent-tabs-mode: nil -*-
#! QoreHistoryProvider — Linenoise history provider adapter

/** QoreHistoryProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @brief Adapter bridging QoreHistory::History to Linenoise::AbstractHistoryProvider
/**
    Implements the @ref Linenoise::AbstractHistoryProvider interface by delegating
    to a @ref QoreHistory::History instance, making QoreHistory the single source
    of truth for history while linenoise handles interactive line editing.

    @par Example
    @code{.py}
%requires QoreHistory

QoreHistory::History hist({"max_length": 500});
hist.load(history_path);

# Register QoreHistory as the history provider — single source of truth
Linenoise::set_history_provider(new QoreHistory::QoreHistoryProvider(hist));

# Main loop — only add to QoreHistory
while (True) {
    *string line = Linenoise::line("prompt> ");
    if (!exists line) {
        break;
    }
    hist.add(line);
    # ... process line ...
}

hist.save(history_path);
    @endcode

    @since QoreHistory 1.0
*/
public namespace QoreHistory {
public class QoreHistoryProvider inherits Linenoise::AbstractHistoryProvider {
    private {
        #! The QoreHistory instance to delegate to
        QoreHistory::History history;
    }

    #! Creates the provider with a QoreHistory::History instance
    constructor(QoreHistory::History history) {
        self.history = history;
    }

    #! Returns the previous (older) history entry
    *string prev(string prefix) {
        return history.prev(prefix);
    }

    #! Returns the next (newer) history entry
    *string next(string prefix) {
        *string rv = history.next(prefix);
        # QoreHistory returns "" when navigating past newest; linenoise expects NOTHING
        return rv == "" ? NOTHING : rv;
    }

    #! Resets the navigation position
    nothing reset() {
        history.resetNavigation();
    }

    #! Searches for a pattern in the given direction
    *string search(string pattern, int direction) {
        return history.searchNavigate(pattern, direction);
    }
}
} # namespace QoreHistory
