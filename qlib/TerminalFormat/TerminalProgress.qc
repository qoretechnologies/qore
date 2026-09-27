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

    #! @ref True "True" to show a total line below the transfers of an interactive display
    /** the total line shows the number of transfers active and finished, the amount done, and the combined speed
    */
    bool total_line = False;

    #! @ref True "True" to report the progress to the terminal with \c OSC \c 9;4 sequences
    /** terminals that support them (such as Windows Terminal, ConEmu, and Ghostty) show the progress in the tab or
        the taskbar, and other terminals ignore them; used only by an interactive display, and by default when the
        display is interactive
    */
    *bool taskbar;
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
    #! additional information shown when there is room, such as the protocol
    *string info;
    #! a status shown instead of the estimated time remaining, such as a retry
    *string status;
    #! the speed in bytes per second in each of the last seconds, oldest first
    list<float> history = ();
    #! the time and the amount done at the start of the current history interval
    int hist_time;
    int hist_done;
}

#! A field of the line for a transfer in an interactive display
hashdecl TerminalProgressField {
    #! the visible text
    string text;
    #! colors the text
    code<string(string)> color;
    #! the minimum width of the field, so the field keeps its width while its value changes
    int min_width = 0;
    #! True if the text is aligned to the right of the field
    bool right = False;
}

#! The layout of the lines of an interactive display, which is shared by all lines so that their fields line up
hashdecl TerminalProgressLayout {
    #! the width of the label column
    int label_width;
    #! the width of the bar, 0 if there is no bar
    int bar_width;
    #! the fields shown, in order
    list<string> fields = ();
    #! the width of each field shown
    hash<string, int> widths = {};
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

        #! Unicode block characters for the speed history, from the lowest to the highest speed
        const SparkChars = ("▁", "▂", "▃", "▄", "▅", "▆", "▇", "█");

        #! the number of seconds of speed history shown
        const HistorySize = 12;

        #! the minimum width of a bar
        const MinBarWidth = 10;

        #! the maximum width of a bar
        const MaxBarWidth = 60;

        #! the minimum width of the speed, such as \c "1023.9 KiB/s" or \c "stalled 12.5s"
        const SpeedWidth = 12;

        #! the width of the estimated time remaining, up to \c "ETA 99:59:59"
        const EtaWidth = 12;

        #! the longest estimated time remaining shown, in seconds; a longer one is shown as \c ">99h"
        const MaxEtaSecs = 100 * 3600 - 1;
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

        #! the number of transfers finished and the amount they did, for the total line
        int finished_count = 0;
        int finished_done = 0;

        #! True if OSC 9;4 progress is reported
        bool taskbar;
        #! the last OSC 9;4 sequence written
        *string last_taskbar;

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
        taskbar = interactive && (self.opts.taskbar ?? True);
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
            "hist_time": now,
            "hist_done": done,
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
        # the speed history for the sparkline
        int hdt = now - t.hist_time;
        if (hdt >= 1000000) {
            # the average speed is recorded for each second that passed, so the history keeps its time scale
            float rate = (done - t.hist_done) * 1000000.0 / hdt;
            for (int i = min(HistorySize, hdt / 1000000); i; --i) {
                t.history += rate;
            }
            if (t.history.size() > HistorySize) {
                splice t.history, 0, t.history.size() - HistorySize;
            }
            t.hist_time = now;
            t.hist_done = done;
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

    #! Sets or clears additional information about a transfer that is shown when the line has room
    /** @param id the ID of the transfer
        @param info the information, such as the protocol (\c "h3"), or @ref NOTHING to clear it

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    setInfo(int id, *string info) {
        AutoLock al(m);
        getTransferIntern(id);
        transfers{id}.info = info;
        drawIntern(getMicros(), True);
    }

    #! Sets or clears the status of a transfer, which is shown instead of the estimated time remaining
    /** A transfer with a status is shown in the warning color

        @param id the ID of the transfer
        @param status the status, such as \c "retry 2/5 in 4s", or @ref NOTHING to clear it

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    setStatus(int id, *string status) {
        AutoLock al(m);
        getTransferIntern(id);
        transfers{id}.status = status;
        if (interactive) {
            drawIntern(getMicros(), True);
        } else if (status) {
            writeOutput(sprintf("%s: %s\n", transfers{id}.label, status));
        }
    }

    #! Removes a transfer from the display and writes a message in its place
    /** @param id the ID of the transfer
        @param message the message to write; no message is written if not given

        @throw TERMINALPROGRESS-ERROR unknown transfer ID
    */
    finish(int id, *string message) {
        AutoLock al(m);
        hash<TerminalProgressTransferInfo> t = getTransferIntern(id);
        ++finished_count;
        finished_done += t.done;
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
        string rv = sprintf("%s in %s", TerminalProgress::formatBytes(t.done),
            TerminalProgress::formatElapsed(elapsed));
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
        string str;
        if (interactive && lines_drawn) {
            str = sprintf("\033[%dF\033[J", lines_drawn);
            lines_drawn = 0;
        }
        # the progress reported to the terminal is removed
        if (last_taskbar && last_taskbar != "\033]9;4;0;0\a") {
            str += "\033]9;4;0;0\a";
        }
        remove last_taskbar;
        if (str) {
            writeOutput(str);
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
        int lines = 0;
        list<hash<TerminalProgressTransferInfo>> tl = ();
        list<hash<string, hash<TerminalProgressField>>> fields = ();
        foreach hash<TerminalProgressTransferInfo> t in (transfers.iterator()) {
            tl += t;
            fields += getFieldsIntern(t, now);
        }
        # the last column is left free, so a full line never wraps
        hash<TerminalProgressLayout> layout = getLayoutIntern(tl, fields, cols - 1);
        foreach hash<TerminalProgressTransferInfo> t in (tl) {
            str += renderLineIntern(t, fields[$#], layout, now) + "\033[K\n";
            ++lines;
        }
        if (opts.total_line && (transfers.size() + finished_count) > 1) {
            str += renderTotalLineIntern(now, cols - 1) + "\033[K\n";
            ++lines;
        }
        # clears lines of transfers that are no longer displayed
        if (lines_drawn > lines) {
            str += "\033[J";
        }
        lines_drawn = lines;
        str += getTaskbarIntern(now);
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
        float secs = (t.total - t.done) / t.rate;
        if (secs > MaxEtaSecs) {
            return ">99h";
        }
        return TerminalProgress::formatDuration((secs * 1000000.0).toInt());
    }

    #! Returns the fields of the line for a transfer in an interactive display
    /** @return field name -> (visible text, color method, minimum width, right-aligned)
    */
    private hash<string, hash<TerminalProgressField>> getFieldsIntern(hash<TerminalProgressTransferInfo> t,
            int now) {
        bool stalled = (now - t.progress_time) >= opts.stall_ms * 1000 && t.done != t.total;
        hash<string, hash<TerminalProgressField>> f = {};
        if (t.total) {
            f.pct = TerminalProgress::field(sprintf("%d%%", min(100, t.done * 100 / t.total)), \color.bold(), 4,
                True);
            f.sizes = TerminalProgress::field(TerminalProgress::formatSizes(t.done, t.total), \color.value(),
                TerminalProgress::formatSizes(t.total, t.total).length(), True);
        } else {
            # the widest amount in any unit, such as "1023.9 MiB", is 10 characters
            f.sizes = TerminalProgress::field(TerminalProgress::formatBytes(t.done), \color.value(), 10, True);
        }
        if (stalled) {
            f.speed = TerminalProgress::field(sprintf("stalled %s",
                TerminalProgress::formatElapsed(now - t.progress_time)), \color.yellow(), SpeedWidth, True);
        } else {
            f.speed = TerminalProgress::field(getSpeedIntern(t, now), \color.green(), SpeedWidth, True);
        }
        if (unicode) {
            # the space is reserved before there is a history, so the bar does not change width
            f.spark = TerminalProgress::field(t.history.size() > 1 ? TerminalProgress::renderSparkline(t.history) : "",
                \color.cyan(), HistorySize);
        }
        if (t.info) {
            f.info = TerminalProgress::field(t.info, \color.dim());
        }
        if (t.status) {
            f.eta = TerminalProgress::field(t.status, \color.yellow(), EtaWidth);
        } else if (*string eta = getEtaIntern(t, now)) {
            f.eta = TerminalProgress::field("ETA " + eta, \color.dim(), EtaWidth);
        }
        return f;
    }

    #! Returns a field of a line
    private static hash<TerminalProgressField> field(string text, code<string(string)> color, int min_width = 0,
            bool right = False) {
        return <TerminalProgressField>{
            "text": text,
            "color": color,
            "min_width": min_width,
            "right": right,
        };
    }

    #! Returns the layout shared by the lines of an interactive display
    /** Each field has the width of its widest value on any line, but at least its minimum width, so the fields
        line up and the bar keeps its width while values change; fields that do not fit are left out on all lines in
        order of importance: the info and the speed history; then the fields only get the width of their values, and
        finally the sizes and the bar are left out

        @param tl the transfers displayed
        @param fields the fields of each line, as returned by getFieldsIntern()
        @param width the width available
    */
    private hash<TerminalProgressLayout> getLayoutIntern(list<hash<TerminalProgressTransferInfo>> tl,
            list<hash<string, hash<TerminalProgressField>>> fields, int width) {
        hash<TerminalProgressLayout> layout();
        int label_len = 0;
        foreach hash<TerminalProgressTransferInfo> t in (tl) {
            label_len = max(label_len, t.label.length());
        }
        # the widths of the fields with their minimum widths, and the widths of their values
        hash<string, int> widths = {};
        hash<string, int> natural = {};
        foreach hash<string, hash<TerminalProgressField>> f in (fields) {
            foreach hash<auto> i in (f.pairIterator()) {
                hash<TerminalProgressField> field = i.value;
                natural{i.key} = max(natural{i.key} ?? 0, field.text.length());
                widths{i.key} = max(widths{i.key} ?? 0, field.min_width, natural{i.key});
            }
        }
        # the order of the fields on the line
        list<string> order = ("pct", "sizes", "speed", "spark", "info", "eta");
        # what is given up, in order, when there is no room: the info, the speed history, the alignment of the
        # fields, the sizes, and the bar
        list<string> steps = ("info", "spark", "compact", "sizes", "bar");

        int min_label = min(label_len, 12);
        bool bar = True;
        # returns the width of the line with the given bar width
        code<int(int)> line_width = int sub (int bar_width) {
            int w = min_label;
            if (bar_width) {
                w += bar_width + 3;
            }
            foreach string k in (order) {
                if (exists widths{k}) {
                    w += widths{k} + 1;
                }
            }
            return w;
        };
        while (steps && line_width(bar ? MinBarWidth : 0) > width) {
            string step = shift steps;
            switch (step) {
                case "compact":
                    # the fields only get the width of their values, so they no longer line up while values change
                    foreach string k in (keys widths) {
                        widths{k} = natural{k};
                    }
                    break;
                case "bar":
                    bar = False;
                    break;
                default:
                    remove widths{step};
                    break;
            }
        }
        # the label gets the space left, up to its length and a fraction of the width
        int rest = width - line_width(bar ? MinBarWidth : 0) + min_label;
        layout.label_width = max(0, min(label_len, rest, max(min_label, (width * MaxLabelFraction).toInt())));
        layout.bar_width = bar ? min(MaxBarWidth, MinBarWidth + rest - layout.label_width) : 0;
        foreach string k in (order) {
            if (exists widths{k}) {
                layout.fields += k;
                layout.widths{k} = widths{k};
            }
        }
        return layout;
    }

    #! Returns the line for a transfer in an interactive display
    /** @param t the transfer
        @param f the fields of the line, as returned by getFieldsIntern()
        @param layout the layout shared by the lines of the display
        @param now the current time
    */
    private string renderLineIntern(hash<TerminalProgressTransferInfo> t, hash<string, hash<TerminalProgressField>> f,
            hash<TerminalProgressLayout> layout, int now) {
        bool stalled = (now - t.progress_time) >= opts.stall_ms * 1000 && t.done != t.total;
        bool complete = t.total && t.done >= t.total;

        string label = TerminalProgress::truncateMiddle(t.label, layout.label_width, unicode);
        string str = color.bold(label) + strmul(" ", layout.label_width - label.length());
        if (layout.bar_width) {
            code<string(string)> bar_color = stalled || t.status
                ? \color.yellow()
                : (complete ? \color.green() : \color.cyan());
            str += " " + color.dim(unicode ? "▕" : "[")
                + bar_color(renderBarIntern(t, now, layout.bar_width))
                + color.dim(unicode ? "▏" : "]");
        }
        int last = layout.fields.size() - 1;
        foreach string k in (layout.fields) {
            # a field can be missing on this line, such as the info
            string text = f{k}.text ?? "";
            bool right = f{k}.right ?? False;
            # a left-aligned value at the end of the line needs no padding
            if (!right && $# == last) {
                if (text) {
                    str += " " + f{k}.color(text);
                }
                break;
            }
            string pad = strmul(" ", layout.widths{k} - text.length());
            if (text) {
                text = f{k}.color(text);
            }
            str += " " + (right ? pad + text : text + pad);
        }
        return str;
    }

    #! Returns the total line of an interactive display
    private string renderTotalLineIntern(int now, int width) {
        int done = finished_done;
        float rate = 0.0;
        foreach hash<TerminalProgressTransferInfo> t in (transfers.iterator()) {
            done += t.done;
            if (t.rate && (now - t.progress_time) < opts.stall_ms * 1000) {
                rate += t.rate;
            }
        }
        list<string> fields = (
            color.bold("total"),
            sprintf("%s active", color.number(string(transfers.size()))),
            sprintf("%s done", color.number(string(finished_count))),
            color.value(TerminalProgress::formatBytes(done)),
        );
        if (rate > 0) {
            fields += color.green(TerminalProgress::formatBytes(rate) + "/s");
        }
        string str = fields.join(color.dim(" · "));
        # the line is shortened if the terminal is too narrow for it
        if (TerminalColor::visibleLength(str) > width) {
            str = color.bold("total") + " " + color.value(TerminalProgress::formatBytes(done));
        }
        return str;
    }

    #! Returns the OSC 9;4 sequence that reports the progress to the terminal, if it changed
    private string getTaskbarIntern(int now) {
        if (!taskbar) {
            return "";
        }
        int done = 0;
        int total = 0;
        bool known = True;
        bool stalled = False;
        foreach hash<TerminalProgressTransferInfo> t in (transfers.iterator()) {
            if (!t.total) {
                known = False;
            } else {
                done += min(t.done, t.total);
                total += t.total;
            }
            if ((now - t.progress_time) >= opts.stall_ms * 1000 || t.status) {
                stalled = True;
            }
        }
        string seq;
        if (!transfers) {
            seq = "\033]9;4;0;0\a";
        } else if (!known || !total) {
            # the progress is not known
            seq = "\033]9;4;3;0\a";
        } else {
            # state 4 is the warning state
            seq = sprintf("\033]9;4;%d;%d\a", stalled ? 4 : 1, done * 100 / total);
        }
        if (seq == last_taskbar) {
            return "";
        }
        last_taskbar = seq;
        return seq;
    }

    #! Returns the done and total sizes in a compact form, such as \c "3.4/5.9 GiB"
    /** @param done the amount done
        @param total the total size

        @return the formatted sizes; both are given in the unit of the total, so the text only gets longer when the
        amount done gets more digits
    */
    static string formatSizes(int done, int total) {
        if (total < 1024) {
            return sprintf("%d/%d B", done, total);
        }
        float div = 1.0;
        foreach string unit in ("KiB", "MiB", "GiB", "TiB") {
            div *= 1024.0;
            if (total / div < 1024.0) {
                return sprintf("%.1f/%.1f %s", done / div, total / div, unit);
            }
        }
        div *= 1024.0;
        return sprintf("%.1f/%.1f PiB", done / div, total / div);
    }

    #! Returns a sparkline of the given values
    /** @param values the values, oldest first

        @return one Unicode block character per value, scaled to the largest value
    */
    static string renderSparkline(list<float> values) {
        float max_val = 0.0;
        foreach float v in (values) {
            max_val = max(max_val, v);
        }
        string rv;
        foreach float v in (values) {
            int idx = max_val > 0 ? min(7, (v * 8 / max_val).toInt()) : 0;
            rv += SparkChars[idx];
        }
        return rv;
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
