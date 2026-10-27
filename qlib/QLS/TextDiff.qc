# -*- mode: qore; indent-tabs-mode: nil -*-

/*  TextDiff.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QLS {

#! A range of lines in the old text replaced by a range of lines in the new text; ends are exclusive
public hashdecl LineHunk {
    #! The first replaced line of the old text
    int oldStart;

    #! The line after the replaced lines of the old text
    int oldEnd;

    #! The first replacing line of the new text
    int newStart;

    #! The line after the replacing lines of the new text
    int newEnd;
}

#! Line differences between two texts, for minimal text edits
/** Used to return only the changed lines of a formatted document, so that the editor keeps the cursor, folding
    and markers of unchanged lines, and to format only a range of a document.

    @par Example
    @code{.py}
# the LSP text edits that change the old text to the new one
list<hash<auto>> edits = TextDiff::getTextEdits(oldText, newText);
    @endcode
*/
public class TextDiff {
    public {
        #! The maximum number of differing lines for which a minimal difference is computed
        /** Beyond this, the lines between the common prefix and suffix are one hunk, which bounds the time and
            memory of the comparison.
        */
        const MaxDifferences = 2000;
    }

    #! Returns the hunks of lines that differ between two lists of lines
    /** Uses the Myers difference algorithm after removing the common prefix and suffix.

        @param a the old lines
        @param b the new lines

        @return the differing hunks in order; an empty list if the lines are the same
    */
    static list<hash<LineHunk>> diffLines(list<string> a, list<string> b) {
        int prefix = 0;
        while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) {
            ++prefix;
        }
        int suffix = 0;
        while (suffix < a.size() - prefix && suffix < b.size() - prefix
                && a[a.size() - 1 - suffix] == b[b.size() - 1 - suffix]) {
            ++suffix;
        }
        int n = a.size() - prefix - suffix;
        int m = b.size() - prefix - suffix;
        if (!n && !m) {
            return ();
        }
        if (!n || !m) {
            return (<LineHunk>{"oldStart": prefix, "oldEnd": prefix + n, "newStart": prefix, "newEnd": prefix + m},);
        }

        # Myers: v{k} is the furthest x on diagonal k = x - y after d differences; the trace keeps v for each d
        int max = n + m;
        int limit = max > MaxDifferences ? MaxDifferences : max;
        hash<string, int> v = {"1": 0};
        list<hash<string, int>> trace = ();
        int found = -1;
        for (int d = 0; d <= limit && found == -1; ++d) {
            push trace, v;
            hash<string, int> next = v;
            for (int k = -d; k <= d; k += 2) {
                int x;
                if (k == -d || (k != d && v{k - 1} < v{k + 1})) {
                    x = v{k + 1};
                } else {
                    x = v{k - 1} + 1;
                }
                int y = x - k;
                while (x < n && y < m && a[prefix + x] == b[prefix + y]) {
                    ++x;
                    ++y;
                }
                next{k} = x;
                if (x >= n && y >= m) {
                    found = d;
                    break;
                }
            }
            v = next;
        }
        if (found == -1) {
            # too many differences: one hunk between the common prefix and suffix
            return (<LineHunk>{"oldStart": prefix, "oldEnd": prefix + n, "newStart": prefix, "newEnd": prefix + m},);
        }

        # walk back through the trace and collect the edit script as single-line operations
        list<hash<auto>> ops = ();
        int x = n;
        int y = m;
        for (int d = found; d > 0; --d) {
            hash<string, int> pv = trace[d];
            int k = x - y;
            int prevK = (k == -d || (k != d && pv{k - 1} < pv{k + 1})) ? k + 1 : k - 1;
            int prevX = pv{prevK};
            int prevY = prevX - prevK;
            while (x > prevX && y > prevY) {
                --x;
                --y;
            }
            if (x == prevX) {
                # an insertion of b[prevY]
                unshift ops, {"a": x, "b": prevY, "insert": True};
            } else {
                # a deletion of a[prevX]
                unshift ops, {"a": prevX, "b": y, "insert": False};
            }
            x = prevX;
            y = prevY;
        }

        # merge adjacent operations into hunks
        list<hash<LineHunk>> hunks = ();
        foreach hash<auto> op in (ops) {
            int oldStart = prefix + op.a;
            int newStart = prefix + op.b;
            hash<LineHunk> h = op.insert
                ? <LineHunk>{"oldStart": oldStart, "oldEnd": oldStart, "newStart": newStart, "newEnd": newStart + 1}
                : <LineHunk>{"oldStart": oldStart, "oldEnd": oldStart + 1, "newStart": newStart, "newEnd": newStart};
            if (hunks && hunks.last().oldEnd == h.oldStart && hunks.last().newEnd == h.newStart) {
                hunks[hunks.size() - 1].oldEnd = h.oldEnd;
                hunks[hunks.size() - 1].newEnd = h.newEnd;
            } else {
                push hunks, h;
            }
        }
        return hunks;
    }

    #! Returns the LSP text edits that change the old text to the new text
    /** @param oldText the old text
        @param newText the new text
        @param hunkFilter if given, only the hunks for which it returns @ref True are returned

        @return a list of LSP \c TextEdit hashes; each replaces whole lines
    */
    static list<hash<auto>> getTextEdits(string oldText, string newText,
            *code<bool(hash<LineHunk>)> hunkFilter) {
        # each line includes its newline, so the replacing lines are joined as they are
        list<string> a = oldText.split("\n", True);
        list<string> b = newText.split("\n", True);
        list<hash<auto>> edits = ();
        foreach hash<LineHunk> h in (TextDiff::diffLines(a, b)) {
            if (hunkFilter && !hunkFilter(h)) {
                continue;
            }
            # the edit ends at the start of the line after the replaced lines, or at the end of a last line without
            # a newline
            hash<auto> end = (h.oldEnd == a.size() && a && a.last() !~ /\n$/)
                ? {"line": a.size() - 1, "character": a.last().length()}
                : {"line": h.oldEnd, "character": 0};
            push edits, {
                "range": {
                    "start": {"line": h.oldStart, "character": 0},
                    "end": end,
                },
                "newText": h.newEnd > h.newStart ? b[h.newStart..h.newEnd - 1].join("") : "",
            };
        }
        return edits;
    }
}

} # namespace QLS
