# -*- mode: qore; indent-tabs-mode: nil -*-
# @file TerminalProgress.qc progress display for terminals and logs

/*  TerminalProgress.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The TerminalFormat namespace
public namespace TerminalFormat {
#! Options for TerminalProgress
public hashdecl TerminalProgressOptions {
    #! the file the display is written to; @ref Qore::stderr "stderr" if not set
    *File output;

    #! @ref True "True" to redraw progress bars in place, @ref False "False" to write progress lines
    /** if not set, progress bars are redrawn in place if the output is a terminal; progress lines never contain
        cursor movements or carriage returns, so they are suitable for log files
    */
    *bool interactive;

    #! the width of the display in columns
    /** if not set, the width of the terminal is used, else the value of the \c COLUMNS environment variable, else 80
    */
    *int columns;

    #! @ref True "True" to draw bars with Unicode block characters, @ref False "False" to draw them with ASCII
    /** if not set, Unicode is used if the locale given by the \c LC_ALL, \c LC_CTYPE, or \c LANG environment
        variables uses UTF-8
    */
    *bool unicode;

    #! @ref True "True" to use colors in an interactive display
    /** if not set, colors are used unless the \c NO_COLOR environment variable is set; see TerminalColor
    */
    *bool color;

    #! the minimum interval between redraws of an interactive display in milliseconds
    int redraw_interval_ms = 100;

    #! the maximum interval between progress lines of a non-interactive display in milliseconds
    /** a progress line is also written when a transfer with a known total size reaches each further 10%
    */
    int line_interval_ms = 30000;

    #! the time in milliseconds without progress after which a transfer is shown as stalled
    int stall_ms = 5000;
}

#! The state of a transfer displayed by TerminalProgress
hashdecl TerminalProgressTransferInfo {
    #! the label
    string label;
    #! the total size, if known
    *int total;
    #! the amount done
    int done;
    #! the amount done when the transfer was added, which does not count toward the speed
    int start_done;
    #! the time the transfer was added
    int start;
    #! the time of the last speed sample
    int sample_time;
    #! the amount done at the last speed sample
    int sample_done;
    #! the time of the last progress
    int progress_time;
    #! the moving average of the speed in bytes per second, if sampled
    *float rate;
    #! the percentage at which the next progress line is written, if the total is known
    *int next_pct_line;
    #! the time the last progress line was written
    int last_line;
}

#! Displays the progress of one or more transfers, such as downloads
/** Each transfer is shown on a line of its own with its label, a bar, the percentage done, the amount done and the
    total, the current speed, and the estimated time remaining.  A display on a terminal is redrawn in place and
    fitted to the width of the terminal; a display that is written to a file or a pipe consists of progress lines
    written at intervals, which never contain cursor movements.

    The speed is a moving average over the last few seconds, so a transfer that slows down or stalls is shown as
    such; a transfer with no progress for longer than the \c stall_ms option is shown as stalled.

    All methods are thread safe, so the transfers can be updated from separate threads.

    @par Example:
    @code{.py}
TerminalProgress progress();
int id = progress.add("file.iso", total_size);
while (*binary data = input.read(65536)) {
    output.write(data);
    progress.update(id, received += data.size());
}
progress.finish(id, sprintf("saved %y", "file.iso"));
    @endcode

    @since TerminalFormat 1.0
*/
public class TerminalProgress {
    public {
        #! the time constant of the moving average used for the speed in seconds
        const SpeedTimeConstant = 2.0;

        #! the minimum interval between speed samples in microseconds
        const SpeedSampleInterval = 250000;

        #! the width of the label as a fraction of the display width
        const MaxLabelFraction = 0.4;

        #! Unicode block characters for the fractional part of a bar, in 1/8 steps
        const UnicodeBarParts = ("", "▏", "▎", "▍", "▌", "▋", "▊", "▉");
    }

    private:internal {
        hash<TerminalProgressOptions> opts;
        File output;
        bool interactive;
        bool unicode;
        TerminalColor color;
        Mutex m();

        #! id -> transfer; in the order the transfers were added
        hash<string, hash<TerminalProgressTransferInfo>> transfers;
        int next_id = 1;

        #! the number of lines of an interactive display currently drawn
        int lines_drawn = 0;
        #! the time of the last redraw
        int last_redraw = 0;
        #! True once close() has been called
        bool closed;
    }

    #! Creates the display
    /** @param opts display options; see TerminalProgressOptions
    */
    constructor(*hash<TerminalProgressOptions> opts) {
        self.opts = opts ?? <TerminalProgressOptions>{};
        output = self.opts.output ?? stderr;
        interactive = self.opts.interactive ?? TerminalProgress::isTty(output);
        unicode = self.opts.unicode ?? TerminalProgress::isUtf8Locale();
        color = new TerminalColor(False);
        color.enabled = interactive && (self.opts.color ?? !TerminalColor::isColorDisabled());
    }

    #! Clears an interactive display
    destructor() {
        close();
    }

    #! Adds a transfer to the display and returns its ID
    /** @param label the label of the transfer, such as a file name
        @param total the total size of the transfer, if known
        @param done the amount already done, such as the size of a download that is resumed

        @return the ID of the transfer for the other methods
    */
    int add(string label, *int total, int done = 0) {
        AutoLock al(m);
        int now = getMicros();
        int id = next_id++;
        transfers{id} = <TerminalProgressTransferInfo>{
            "label": label,
            "total": total,
            "done": done,
            "start_done": done,
            "start": now,
            "sample_time": now,
            "sample_done": done,
            "progress_time": now,
            "next_pct_line": TerminalProgress::getNextPctLine(done, total),
            "last_line": now,
        };
        drawIntern(now, True);
        return id;
    }

    #! Updates the amount done for a transfer
    /** @param id the ID of the transfer
        @param done the total amount done so far

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    update(int id, int done) {
        AutoLock al(m);
        hash<TerminalProgressTransferInfo> t = getTransferIntern(id);
        int now = getMicros();
        if (done != t.done) {
            t.progress_time = now;
        }
        t.done = done;
        # sample the speed
        int dt = now - t.sample_time;
        if (dt >= SpeedSampleInterval) {
            float inst = (done - t.sample_done) * 1000000.0 / dt;
            if (!exists t.rate) {
                t.rate = inst;
            } else {
                float alpha = 1.0 - exp(-(dt / 1000000.0) / SpeedTimeConstant);
                t.rate = alpha * inst + (1.0 - alpha) * t.rate;
            }
            t.sample_time = now;
            t.sample_done = done;
        }
        transfers{id} = t;
        if (interactive) {
            drawIntern(now);
        } else {
            writeProgressLineIntern(id, now);
        }
    }

    #! Sets or clears the total size of a transfer
    /** @param id the ID of the transfer
        @param total the total size, or @ref NOTHING if not known

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    setTotal(int id, *int total) {
        AutoLock al(m);
        hash<TerminalProgressTransferInfo> t = getTransferIntern(id);
        t.total = total;
        t.next_pct_line = TerminalProgress::getNextPctLine(t.done, total);
        transfers{id} = t;
        drawIntern(getMicros(), True);
    }

    #! Sets the label of a transfer
    /** @param id the ID of the transfer
        @param label the new label

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    setLabel(int id, string label) {
        AutoLock al(m);
        getTransferIntern(id);
        transfers{id}.label = label;
        drawIntern(getMicros(), True);
    }

    #! Removes a transfer from the display and writes a message in its place
    /** @param id the ID of the transfer
        @param message the message to write; no message is written if not given

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    finish(int id, *string message) {
        AutoLock al(m);
        getTransferIntern(id);
        remove transfers{id};
        writeLineIntern(message);
    }

    #! Writes a message line above the progress bars of an interactive display
    /** @param message the message to write
    */
    log(string message) {
        AutoLock al(m);
        writeLineIntern(message);
    }

    #! Returns a summary of a transfer: the amount done, the elapsed time, and the average speed
    /** @param id the ID of the transfer

        @return a summary such as \c "12.3 MiB in 2.5s (4.9 MiB/s)"

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    string getSummary(int id) {
        AutoLock al(m);
        hash<TerminalProgressTransferInfo> t = getTransferIntern(id);
        int elapsed = getMicros() - t.start;
        string rv = sprintf("%s in %s", TerminalProgress::formatBytes(t.done), TerminalProgress::formatElapsed(elapsed));
        int transferred = t.done - t.start_done;
        if (elapsed > 0 && transferred > 0) {
            rv += sprintf(" (%s/s)", TerminalProgress::formatBytes(transferred * 1000000 / elapsed));
        }
        return rv;
    }

    #! Returns @ref True "True" if the display is redrawn in place
    bool isInteractive() {
        return interactive;
    }

    #! Removes all progress bars of an interactive display; the display cannot be used afterwards
    close() {
        AutoLock al(m);
        if (closed) {
            return;
        }
        closed = True;
        transfers = {};
        if (interactive && lines_drawn) {
            writeOutput(sprintf("\033[%dF\033[J", lines_drawn));
            lines_drawn = 0;
        }
    }

    #! Returns an amount of bytes in binary units with one decimal, such as \c "12.3 MiB"
    /** @param bytes the number of bytes

        @return the formatted size
    */
    static string formatBytes(softint bytes) {
        if (bytes < 1024) {
            return sprintf("%d B", bytes);
        }
        float val = bytes;
        foreach string unit in ("KiB", "MiB", "GiB", "TiB") {
            val /= 1024.0;
            if (val < 1024.0) {
                return sprintf("%.1f %s", val, unit);
            }
        }
        return sprintf("%.1f PiB", val / 1024.0);
    }

    #! Returns a duration in microseconds in the form \c "M:SS" or \c "H:MM:SS"
    /** @param us the duration in microseconds

        @return the formatted duration
    */
    static string formatDuration(int us) {
        int secs = (us + 500000) / 1000000;
        if (secs >= 3600) {
            return sprintf("%d:%02d:%02d", secs / 3600, (secs % 3600) / 60, secs % 60);
        }
        return sprintf("%d:%02d", secs / 60, secs % 60);
    }

    #! Returns an elapsed time in microseconds with one decimal for seconds, such as \c "2.5s" or \c "1:02:03"
    /** @param us the elapsed time in microseconds

        @return the formatted elapsed time
    */
    static string formatElapsed(int us) {
        if (us < 60000000) {
            return sprintf("%.1fs", us / 1000000.0);
        }
        return formatDuration(us);
    }

    #! Returns the current time in microseconds; can be overridden to control time in tests
    private int getMicros() {
        return clock_getmicros();
    }

    #! Writes output to the display; can be overridden to capture the output
    private writeOutput(string str) {
        output.write(str);
    }

    #! Returns the width of the display
    private int getColumnsIntern() {
        if (opts.columns) {
            return opts.columns;
        }
        if (interactive) {
            try {
                int cols = TermIOS::getWindowSize().columns;
                if (cols > 0) {
                    return cols;
                }
            } catch (hash<ExceptionInfo> ex) {
                # the width is not available
            }
        }
        int cols = (getenv("COLUMNS") ?? "").toInt();
        return cols > 0 ? cols : 80;
    }

    #! Returns the percentage at which the next progress line is written, if the total is known
    private static *int getNextPctLine(int done, *int total) {
        if (!total) {
            return;
        }
        int tot = total;
        return (done * 10 / tot + 1) * 10;
    }

    #! Returns the transfer; throws an exception if the ID is unknown
    private hash<TerminalProgressTransferInfo> getTransferIntern(int id) {
        *hash<TerminalProgressTransferInfo> t = transfers{id};
        if (!t) {
            throw "TERMINALPROGRESS-ERROR", sprintf("unknown transfer ID %d", id);
        }
        return t;
    }

    #! Writes a message line above an interactive display, or as a line of a non-interactive display
    private writeLineIntern(*string message) {
        if (!interactive) {
            if (message.val()) {
                writeOutput(message + "\n");
            }
            return;
        }
        string str = lines_drawn ? sprintf("\033[%dF\033[J", lines_drawn) : "";
        lines_drawn = 0;
        if (message.val()) {
            str += message + "\n";
        }
        writeOutput(str);
        drawIntern(getMicros(), True);
    }

    #! Redraws an interactive display
    /** @param now the current time
        @param force redraw even if the redraw interval has not passed
    */
    private drawIntern(int now, *bool force) {
        if (!interactive || closed) {
            return;
        }
        if (!force && (now - last_redraw) < opts.redraw_interval_ms * 1000) {
            return;
        }
        last_redraw = now;
        int cols = getColumnsIntern();
        string str = lines_drawn ? sprintf("\033[%dF", lines_drawn) : "";
        foreach hash<TerminalProgressTransferInfo> t in (transfers.iterator()) {
            # the last column is left free, so a full line never wraps
            str += renderLineIntern(t, now, cols - 1) + "\033[K\n";
        }
        # clears lines of transfers that are no longer displayed
        if (lines_drawn > transfers.size()) {
            str += "\033[J";
        }
        lines_drawn = transfers.size();
        writeOutput(str);
    }

    #! Writes a progress line for a non-interactive display if one is due
    private writeProgressLineIntern(int id, int now) {
        hash<TerminalProgressTransferInfo> t = transfers{id};
        bool due = (now - t.last_line) >= opts.line_interval_ms * 1000;
        if (t.total && t.next_pct_line) {
            int total = t.total;
            int pct = t.done * 100 / total;
            if (pct >= t.next_pct_line) {
                due = True;
                transfers{id}.next_pct_line = (pct / 10 + 1) * 10;
            }
        }
        if (!due) {
            return;
        }
        transfers{id}.last_line = now;
        list<string> fields = (t.label + ":",);
        if (t.total) {
            fields += sprintf("%d%%", t.done * 100 / t.total);
            fields += sprintf("%s/%s", TerminalProgress::formatBytes(t.done), TerminalProgress::formatBytes(t.total));
        } else {
            fields += TerminalProgress::formatBytes(t.done);
        }
        fields += getSpeedIntern(t, now);
        if (*string eta = getEtaIntern(t, now)) {
            fields += "ETA " + eta;
        }
        writeOutput(fields.join(" ") + "\n");
    }

    #! Returns the speed of a transfer for display
    private string getSpeedIntern(hash<TerminalProgressTransferInfo> t, int now) {
        if ((now - t.progress_time) >= opts.stall_ms * 1000) {
            return "stalled";
        }
        if (!exists t.rate) {
            return "--";
        }
        return TerminalProgress::formatBytes(t.rate) + "/s";
    }

    #! Returns the estimated time remaining for a transfer, if known
    private *string getEtaIntern(hash<TerminalProgressTransferInfo> t, int now) {
        if (!t.total || t.done >= t.total) {
            return;
        }
        if (!t.rate || (now - t.progress_time) >= opts.stall_ms * 1000) {
            return "--:--";
        }
        return TerminalProgress::formatDuration(((t.total - t.done) / t.rate * 1000000.0).toInt());
    }

    #! Returns the line for a transfer in an interactive display
    private string renderLineIntern(hash<TerminalProgressTransferInfo> t, int now, int width) {
        # the fields after the bar
        string right;
        if (t.total) {
            right = sprintf("%3d%% %s/%s", min(100, t.done * 100 / t.total), TerminalProgress::formatBytes(t.done),
                TerminalProgress::formatBytes(t.total));
        } else {
            right = TerminalProgress::formatBytes(t.done);
        }
        right += " " + getSpeedIntern(t, now);
        if (*string eta = getEtaIntern(t, now)) {
            right += " ETA " + eta;
        }

        # the label takes the space that is left, up to a fraction of the width
        int label_width = min(t.label.length(), max(8, (width * MaxLabelFraction).toInt()));
        int bar_width = width - label_width - right.length() - 4;
        if (bar_width < 5) {
            # no bar; the label takes what is left
            label_width = max(0, width - right.length() - 1);
            bar_width = 0;
        }
        string label = TerminalProgress::truncateMiddle(t.label, label_width, unicode);
        string str = label + strmul(" ", label_width - label.length());
        if (bar_width) {
            str += " " + color.dim("[") + color.cyan(renderBarIntern(t, now, bar_width)) + color.dim("]");
        }
        str += " " + right;
        return str;
    }

    #! Returns the bar for a transfer
    private string renderBarIntern(hash<TerminalProgressTransferInfo> t, int now, int width) {
        if (!t.total) {
            # a marker that moves back and forth, as the progress is not known
            int size = min(3, width);
            int span = width - size;
            int pos = span ? (now / 100000) % (span * 2) : 0;
            if (pos > span) {
                pos = span * 2 - pos;
            }
            return strmul(" ", pos) + strmul(unicode ? "█" : "=", size) + strmul(" ", width - pos - size);
        }
        float frac = min(1.0, t.done / float(t.total));
        if (unicode) {
            int eighths = (frac * width * 8).toInt();
            string bar = strmul("█", eighths / 8) + UnicodeBarParts[eighths % 8];
            return bar + strmul(" ", width - bar.length());
        }
        int full = (frac * width).toInt();
        string bar = strmul("=", full);
        if (full < width) {
            bar += ">";
        }
        return bar + strmul(" ", width - bar.length());
    }

    #! Shortens a string to the given length by replacing its middle with an ellipsis
    /** @param str the string to shorten
        @param len the maximum length
        @param unicode @ref True "True" to use the Unicode ellipsis character, @ref False "False" for \c "..."

        @return the string, shortened if necessary
    */
    static string truncateMiddle(string str, int len, bool unicode = True) {
        if (str.length() <= len) {
            return str;
        }
        string ellipsis = unicode ? "…" : "...";
        if (len <= ellipsis.length()) {
            return str.substr(0, len);
        }
        int keep = len - ellipsis.length();
        int head = (keep + 1) / 2;
        int tail = keep - head;
        return str.substr(0, head) + ellipsis + (tail ? str.substr(-tail) : "");
    }

    #! Returns @ref True "True" if the file is a terminal
    private static bool isTty(File f) {
        try {
            return f.isTty();
        } catch (hash<ExceptionInfo> ex) {
            return False;
        }
    }

    #! Returns @ref True "True" if the locale uses UTF-8
    private static bool isUtf8Locale() {
        *string locale = getenv("LC_ALL") ?? getenv("LC_CTYPE") ?? getenv("LANG");
        return locale =~ /utf-?8/i;
    }
}
}
