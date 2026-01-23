# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactWriter.qc EDIFACT writer classes

/*  EdifactWriter.qc Copyright 2026 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! Stream-based EDIFACT writer
    /** This class writes EDIFACT data to an OutputStream.

        @par Example
        @code{.py}
FileOutputStream fos("orders.edi");
EdifactWriter w(fos);
w.writeInterchangeHeader(<EdifactInterchangeInfo>{
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
});
w.writeMessageHeader(<EdifactMessageInfo>{
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "96A",
});
w.writeSegment("BGM", (("220",), ("PO12345",), ("9",)));
w.writeMessageTrailer();
w.writeInterchangeTrailer();
        @endcode
    */
    public class EdifactWriter inherits EdifactAbstractWriter {
        private {
            #! The output stream
            Qore::OutputStream m_stream;
        }

        #! Creates an EdifactWriter that writes to an OutputStream
        /** @param os the output stream to write to
            @param opts optional options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(Qore::OutputStream os, *hash<auto> opts) : EdifactAbstractWriter(opts) {
            @assert(os, "output stream cannot be null");
            m_stream = os;
        }

        #! Creates an EdifactWriter with specified delimiters
        /** @param os the output stream to write to
            @param delim the delimiter configuration
            @param opts optional options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(Qore::OutputStream os, EdifactDelimiters delim, *hash<auto> opts)
                : EdifactAbstractWriter(delim, opts) {
            @assert(os, "output stream cannot be null");
            m_stream = os;
        }

        #! Writes data to the output stream
        private writeOutput(string data) {
            m_stream.write(convert_encoding(data, m_encoding).toBinary());
        }
    }

    #! String-based EDIFACT writer
    /** This class writes EDIFACT data to an internal string buffer.

        @par Example
        @code{.py}
EdifactDataWriter w();
w.writeInterchangeHeader(<EdifactInterchangeInfo>{
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
});
w.writeMessageHeader(<EdifactMessageInfo>{
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "96A",
});
w.writeSegment("BGM", (("220",), ("PO12345",), ("9",)));
w.writeMessageTrailer();
w.writeInterchangeTrailer();

string edifact_data = w.getData();
        @endcode
    */
    public class EdifactDataWriter inherits EdifactAbstractWriter {
        private {
            #! The output buffer
            string m_buffer = "";
        }

        #! Creates an EdifactDataWriter
        /** @param opts optional options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(*hash<auto> opts) : EdifactAbstractWriter(opts) {
        }

        #! Creates an EdifactDataWriter with specified delimiters
        /** @param delim the delimiter configuration
            @param opts optional options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(EdifactDelimiters delim, *hash<auto> opts) : EdifactAbstractWriter(delim, opts) {
        }

        #! Writes data to the internal buffer
        private writeOutput(string data) {
            m_buffer += data;
        }

        #! Returns the accumulated EDIFACT data
        /** @return the EDIFACT message string
        */
        string getData() {
            return m_buffer;
        }

        #! Clears the internal buffer
        clear() {
            m_buffer = "";
        }
    }

    #! File-based EDIFACT writer
    /** This class writes EDIFACT data to a file.

        @par Example
        @code{.py}
EdifactFileWriter w("orders.edi");
w.writeInterchangeHeader(<EdifactInterchangeInfo>{
    "sender_id": "SENDER",
    "recipient_id": "RECEIVER",
    "control_reference": "REF001",
});
w.writeMessageHeader(<EdifactMessageInfo>{
    "message_reference": "1",
    "message_type": "ORDERS",
    "message_version": "D",
    "message_release": "96A",
});
w.writeSegment("BGM", (("220",), ("PO12345",), ("9",)));
w.writeMessageTrailer();
w.writeInterchangeTrailer();
w.close();
        @endcode
    */
    public class EdifactFileWriter inherits EdifactAbstractWriter {
        private {
            #! The file path
            string m_path;

            #! The file object
            File m_file;

            #! Flag indicating if file is open
            bool m_open = False;
        }

        #! Creates an EdifactFileWriter that writes to a file
        /** @param path the file path to write to
            @param opts optional options hash; see @ref EdifactWriterOptions for valid options

            Additional options:
            - \c file_flags: additional file open flags (default: O_CREAT | O_WRONLY | O_TRUNC)

            @throw FILE-OPEN-ERROR if the file cannot be opened
        */
        constructor(string path, *hash<auto> opts) : EdifactAbstractWriter(opts) {
            @assert(path.val(), "file path cannot be empty");
            @debug(printf("EdifactFileWriter::constructor(): writing to %y\n", path));
            m_path = path;
            openFile(opts);
        }

        #! Creates an EdifactFileWriter with specified delimiters
        /** @param path the file path to write to
            @param delim the delimiter configuration
            @param opts optional options hash; see @ref EdifactWriterOptions for valid options
        */
        constructor(string path, EdifactDelimiters delim, *hash<auto> opts)
                : EdifactAbstractWriter(delim, opts) {
            @assert(path.val(), "file path cannot be empty");
            @debug(printf("EdifactFileWriter::constructor(): writing to %y\n", path));
            m_path = path;
            openFile(opts);
        }

        #! Destructor - closes the file if open
        destructor() {
            close();
        }

        #! Opens the file for writing
        private openFile(*hash<auto> opts) {
            m_file = new File();
            int flags = opts.file_flags ?? (O_CREAT | O_WRONLY | O_TRUNC);
            m_file.open2(m_path, flags, 0644, m_encoding);
            m_open = True;
        }

        #! Writes data to the file
        private writeOutput(string data) {
            if (!m_open) {
                throw "EDIFACT-WRITE-ERROR", "file is not open";
            }
            m_file.write(data);
        }

        #! Closes the file
        close() {
            if (m_open) {
                m_file.close();
                m_open = False;
            }
        }

        #! Returns the file path
        string getPath() {
            return m_path;
        }

        #! Returns the file name (without directory path)
        string getFileName() {
            return basename(m_path);
        }
    }
}
