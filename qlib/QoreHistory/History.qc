# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreHistory::History class definition

/*  History.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreHistory {

#! Shared history management class for Qore tools
/** Provides timestamped history entries with navigation, search, persistence,
    deduplication, and history expansion support.

    @since QoreHistory 1.0
*/
public class History {
    private {
        #! History entries with timestamps
        list<hash<HistoryEntry>> items = ();
        #! Navigation index (-1 = not navigating)
        int idx = -1;
        #! Maximum number of entries to keep
        int max_length = 500;
        #! Deduplication mode
        int dedup_mode = DEDUP_CONSECUTIVE;
        #! Active prefix filter for prev/next navigation
        *string nav_prefix;
        #! Search navigation index (-1 = not searching)
        int search_idx = -1;
        #! Active search pattern
        *string search_pattern;
    }

    #! Creates a new History instance
    /** @param opts optional configuration hash:
        - \c max_length: maximum number of history entries (default: 500)
        - \c dedup_mode: deduplication mode (default: DEDUP_CONSECUTIVE)
    */
    constructor(*hash<auto> opts) {
        if (opts.max_length > 0) {
            max_length = opts.max_length;
        }
        if (exists opts.dedup_mode) {
            validateDedupMode(opts.dedup_mode);
            dedup_mode = opts.dedup_mode;
        }
    }

    #! Adds an entry to history with the current timestamp
    /** Empty or whitespace-only entries are ignored. Deduplication is applied
        based on the current dedup_mode setting.

        @param text the text to add to history
    */
    nothing add(string text) {
        if (!text || !trim(text)) {
            return;
        }
        if (dedup_mode == DEDUP_CONSECUTIVE && items.size() > 0
                && items[items.size() - 1].text == text) {
            return;
        }
        if (dedup_mode == DEDUP_FULL) {
            # Remove all prior entries with the same text
            list<hash<HistoryEntry>> new_items = ();
            foreach hash<HistoryEntry> item in (items) {
                if (item.text != text) {
                    new_items += item;
                }
            }
            items = new_items;
        }
        items += <HistoryEntry>{
            "text": text,
            "timestamp": now_us(),
        };
        idx = -1;
        trimToMaxLength();
    }

    #! Returns the number of history entries
    int size() {
        return items.size();
    }

    #! Clears all history entries and resets navigation
    nothing clear() {
        items = ();
        resetNavigation();
    }

    #! Returns a copy of all history entries
    list<hash<HistoryEntry>> getItems() {
        return items;
    }

    #! Navigates to the previous history entry
    /** @param prefix optional prefix filter; only entries starting with this
        prefix will be returned. When the prefix changes, navigation is reset.

        @return the text of the previous matching entry, or NOTHING if already
        at the beginning of history or no match exists
    */
    *string prev(*string prefix) {
        if (!items.size()) {
            return NOTHING;
        }
        # Reset navigation if prefix changed
        if (prefix != nav_prefix) {
            nav_prefix = prefix;
            idx = -1;
        }
        if (exists prefix && prefix != "") {
            # Prefix-filtered navigation
            int start = idx == -1 ? items.size() - 1 : idx - 1;
            for (int i = start; i >= 0; i--) {
                if (items[i].text.find(prefix) == 0) {
                    idx = i;
                    return items[i].text;
                }
            }
            return NOTHING;
        }
        # Unfiltered navigation
        if (idx == -1) {
            idx = items.size() - 1;
        } else if (idx > 0) {
            idx -= 1;
        } else {
            # Already at the beginning of history
            return NOTHING;
        }
        return items[idx].text;
    }

    #! Navigates to the next history entry
    /** @param prefix optional prefix filter; only entries starting with this
        prefix will be returned

        @return the text of the next matching entry, or "" when leaving
        history, or NOTHING if not navigating
    */
    *string next(*string prefix) {
        if (idx == -1) {
            return NOTHING;
        }
        if (exists prefix && prefix != "") {
            for (int i = idx + 1; i < items.size(); i++) {
                if (items[i].text.find(prefix) == 0) {
                    idx = i;
                    return items[i].text;
                }
            }
            # No more matches - leave navigation
            idx = -1;
            nav_prefix = NOTHING;
            return "";
        }
        if (idx < items.size() - 1) {
            idx += 1;
            return items[idx].text;
        }
        idx = -1;
        nav_prefix = NOTHING;
        return "";
    }

    #! Jumps to the oldest (first) history entry
    /** @return the text of the oldest entry, or NOTHING if history is empty
    */
    *string first() {
        if (!items.size()) {
            return NOTHING;
        }
        idx = 0;
        nav_prefix = NOTHING;
        return items[0].text;
    }

    #! Jumps to the newest (last) history entry
    /** @return the text of the newest entry, or NOTHING if history is empty
    */
    *string last() {
        if (!items.size()) {
            return NOTHING;
        }
        idx = items.size() - 1;
        nav_prefix = NOTHING;
        return items[idx].text;
    }

    #! Resets navigation state (including search state)
    nothing resetNavigation() {
        idx = -1;
        nav_prefix = NOTHING;
        search_idx = -1;
        search_pattern = NOTHING;
    }

    #! Searches history incrementally in the given direction
    /** Used as the search callback for interactive search (e.g., Ctrl+R / Ctrl+S
        in linenoise). Maintains search state across calls — each call returns
        the next match in the given direction. The search position resets when
        the pattern changes.

        @param pattern the substring to search for
        @param direction \c -1 to search backward (older entries), \c 1 to search
        forward (newer entries)

        @return the text of the next matching entry, or NOTHING if no more
        matches exist in the given direction
    */
    *string searchNavigate(string pattern, int direction) {
        if (!items.size()) {
            return NOTHING;
        }
        # Reset search position if pattern changed
        if (pattern != search_pattern) {
            search_pattern = pattern;
            # Position beyond boundary so first step enters valid range
            search_idx = direction < 0 ? items.size() : -1;
        }
        if (direction < 0) {
            # Search backward (older entries)
            for (int i = search_idx - 1; i >= 0; i--) {
                if (items[i].text.find(pattern) >= 0) {
                    search_idx = i;
                    return items[i].text;
                }
            }
        } else {
            # Search forward (newer entries)
            for (int i = search_idx + 1; i < items.size(); i++) {
                if (items[i].text.find(pattern) >= 0) {
                    search_idx = i;
                    return items[i].text;
                }
            }
        }
        return NOTHING;
    }

    #! Searches history for entries matching a pattern
    /** @param pattern the pattern to search for
        @param mode the search mode: MATCH_PREFIX, MATCH_SUBSTRING, or
        MATCH_REGEX

        @return a list of matching entries (newest first)
    */
    list<hash<HistoryEntry>> search(string pattern, int mode = MATCH_PREFIX) {
        list<hash<HistoryEntry>> results = ();
        # Iterate backward to return newest first
        for (int i = items.size() - 1; i >= 0; i--) {
            bool match;
            switch (mode) {
                case MATCH_PREFIX:
                    match = items[i].text.find(pattern) == 0;
                    break;
                case MATCH_SUBSTRING:
                    match = items[i].text.find(pattern) >= 0;
                    break;
                case MATCH_REGEX:
                    match = regex(items[i].text, pattern);
                    break;
                default:
                    match = False;
                    break;
            }
            if (match) {
                results += items[i];
            }
        }
        return results;
    }

    #! Loads history from a file, replacing any existing entries
    /** Automatically detects the file format:
        - New format: lines starting with \c #<epoch> mark entry boundaries
          with timestamps (epoch-microseconds for new files, epoch-seconds for
          older files)
        - Legacy format: one entry per line (no timestamp markers)

        If the file does not exist or is empty, existing entries are cleared.

        @param path the file path to load from
    */
    nothing load(string path) {
        items = ();
        resetNavigation();
        if (!is_readable(path)) {
            return;
        }
        ReadOnlyFile f(path);
        *string content = f.read(-1);
        if (!exists content || content == "") {
            return;
        }
        list<string> lines = content.split("\n");
        # Remove trailing empty line from split
        if (lines.size() > 0 && lines[lines.size() - 1] == "") {
            splice lines, lines.size() - 1, 1;
        }
        if (!lines.size()) {
            return;
        }
        # Detect format: new format starts with #<epoch>
        if (regex(lines[0], "^#[0-9]+$")) {
            loadNewFormat(lines);
        } else {
            loadLegacyFormat(lines);
        }
        trimToMaxLength();
    }

    #! Saves history to a file in the timestamp-delimited format
    /** Writes to a temporary file and renames atomically to prevent data loss
        if an error occurs mid-write. The file is created with mode 0600 for
        security.

        @param path the file path to save to
    */
    nothing save(string path) {
        string tmp_path = path + ".tmp." + string(getpid());
        try {
            {
                File f();
                f.open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                foreach hash<HistoryEntry> item in (items) {
                    f.write("#" + string(item.timestamp.getEpochMicroseconds()) + "\n");
                    f.write(item.text + "\n");
                }
            }
            # File closed when f goes out of scope; rename atomically
            rename(tmp_path, path);
        } catch (hash<ExceptionInfo> ex) {
            # Clean up temp file on failure
            try {
                unlink(tmp_path);
            } catch () {
                # Ignore cleanup errors
            }
            rethrow;
        }
    }

    #! Removes an entry by index (0-based)
    /** @param index the 0-based index of the entry to remove

        @return True if the entry was removed, False if the index was out of
        range
    */
    bool removeAt(int index) {
        if (index < 0 || index >= items.size()) {
            return False;
        }
        splice items, index, 1;
        resetNavigation();
        return True;
    }

    #! Removes the first entry matching the given text
    /** @param text the text to match

        @return True if an entry was removed, False if no match was found
    */
    bool removeText(string text) {
        for (int i = 0; i < items.size(); i++) {
            if (items[i].text == text) {
                splice items, i, 1;
                resetNavigation();
                return True;
            }
        }
        return False;
    }

    #! Removes all entries matching the given text
    /** @param text the text to match

        @return the number of entries removed
    */
    int removeAll(string text) {
        list<hash<HistoryEntry>> new_items = ();
        int removed = 0;
        foreach hash<HistoryEntry> item in (items) {
            if (item.text == text) {
                removed++;
            } else {
                new_items += item;
            }
        }
        if (removed > 0) {
            items = new_items;
            resetNavigation();
        }
        return removed;
    }

    #! Expands history references in the input string
    /** Supports the following expansion patterns:
        - \c !!: last command
        - \c !n: entry by 1-based index
        - \c !-n: entry relative to end (e.g., !-1 = last entry)
        - \c !prefix: last entry starting with prefix
        - \c !$: last word of last command

        @param input the input string potentially containing history references

        @return the expanded string, or NOTHING if no expansion pattern was
        recognized or the reference could not be resolved
    */
    *string expand(string input) {
        if (!items.size() || !input || input.size() < 2 || input[0] != "!") {
            return NOTHING;
        }
        # !! - last command
        if (input == "!!") {
            return items[items.size() - 1].text;
        }
        # !$ - last word of last command
        if (input == "!$") {
            string last = items[items.size() - 1].text;
            list<string> words = last.split(" ");
            # Remove empty entries from trailing spaces
            while (words.size() > 0 && words[words.size() - 1] == "") {
                splice words, words.size() - 1, 1;
            }
            if (!words.size()) {
                return NOTHING;
            }
            return words[words.size() - 1];
        }
        string rest = input.substr(1);
        # !-n - relative from end
        if (rest[0] == "-") {
            string num_str = rest.substr(1);
            if (!regex(num_str, "^[0-9]+$")) {
                return NOTHING;
            }
            int n = int(num_str);
            int target = items.size() - n;
            if (target < 0 || target >= items.size()) {
                return NOTHING;
            }
            return items[target].text;
        }
        # !n - by 1-based index
        if (regex(rest, "^[0-9]+$")) {
            int n = int(rest);
            if (n < 1 || n > items.size()) {
                return NOTHING;
            }
            return items[n - 1].text;
        }
        # !prefix - last entry starting with prefix
        for (int i = items.size() - 1; i >= 0; i--) {
            if (items[i].text.find(rest) == 0) {
                return items[i].text;
            }
        }
        return NOTHING;
    }

    #! Sets the maximum number of history entries
    /** If the current history exceeds the new maximum, oldest entries are
        removed.

        @param max the new maximum (must be > 0)
    */
    nothing setMaxLength(int max) {
        if (max > 0) {
            max_length = max;
            trimToMaxLength();
        }
    }

    #! Returns the current maximum history length
    int getMaxLength() {
        return max_length;
    }

    #! Sets the deduplication mode
    /** @param mode one of DEDUP_NONE, DEDUP_CONSECUTIVE, or DEDUP_FULL

        @throw INVALID-DEDUP-MODE if the mode is not a valid dedup constant
    */
    nothing setDedupMode(int mode) {
        validateDedupMode(mode);
        dedup_mode = mode;
    }

    #! Returns the current deduplication mode
    int getDedupMode() {
        return dedup_mode;
    }

    #! Loads history in the new timestamp-delimited format
    /** Supports both epoch-seconds (legacy) and epoch-microseconds timestamps.
        Values >= 10^12 are interpreted as microseconds; smaller values as
        seconds.
    */
    private nothing loadNewFormat(list<string> lines) {
        *date ts;
        *string entry_text;
        foreach string line in (lines) {
            if (regex(line, "^#[0-9]+$")) {
                # Save previous entry if any
                if (exists entry_text) {
                    items += <HistoryEntry>{
                        "text": entry_text,
                        "timestamp": ts,
                    };
                }
                int val = int(line.substr(1));
                ts = parseEpochValue(val);
                entry_text = NOTHING;
            } else {
                if (exists entry_text) {
                    entry_text += "\n" + line;
                } else {
                    entry_text = line;
                }
            }
        }
        # Save the last entry
        if (exists entry_text) {
            items += <HistoryEntry>{
                "text": entry_text,
                "timestamp": ts,
            };
        }
    }

    #! Parses an epoch value that may be in seconds or microseconds
    /** @param val the epoch value
        @return the parsed date

        Values >= 10^12 are interpreted as epoch-microseconds; smaller values
        are interpreted as epoch-seconds (backward compatibility).
    */
    private date parseEpochValue(int val) {
        if (val >= 1000000000000) {
            # Microsecond epoch
            int secs = val / 1000000;
            int us = val % 1000000;
            return localtime(secs) + microseconds(us);
        }
        # Legacy second-precision epoch
        return localtime(val);
    }

    #! Loads history in the legacy one-line-per-entry format
    /** Assigns incrementing timestamps (1 second apart) starting from now()
        to preserve entry ordering.
    */
    private nothing loadLegacyFormat(list<string> lines) {
        date ts = now();
        int count = 0;
        foreach string line in (lines) {
            if (line != "") {
                items += <HistoryEntry>{
                    "text": line,
                    "timestamp": ts + seconds(count),
                };
                count++;
            }
        }
    }

    #! Trims history to the maximum length by removing oldest entries
    private nothing trimToMaxLength() {
        if (items.size() > max_length) {
            splice items, 0, items.size() - max_length;
        }
    }

    #! Validates a dedup mode value
    /** @throw INVALID-DEDUP-MODE if the mode is not valid
    */
    private nothing validateDedupMode(int mode) {
        if (mode != DEDUP_NONE && mode != DEDUP_CONSECUTIVE && mode != DEDUP_FULL) {
            throw "INVALID-DEDUP-MODE", sprintf("invalid dedup mode: %d; expected DEDUP_NONE (%d), "
                "DEDUP_CONSECUTIVE (%d), or DEDUP_FULL (%d)", mode, DEDUP_NONE, DEDUP_CONSECUTIVE,
                DEDUP_FULL);
        }
    }
}

} # namespace QoreHistory
