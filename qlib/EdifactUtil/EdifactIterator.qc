# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactIterator.qc EDIFACT stream-based iterator class

/*  EdifactIterator.qc Copyright 2026 Qore Technologies, s.r.o.

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
    #! Stream-based EDIFACT iterator
    /** This class reads EDIFACT data from an InputStream and iterates over segments.

        @par Example
        @code{.py}
FileInputStream fis("orders.edi");
EdifactIterator i(fis);
while (i.next()) {
    hash<auto> segment = i.getValue();
    printf("Segment: %s = %y\n", segment.tag, segment.data_elements);
}
        @endcode
    */
    public class EdifactIterator inherits EdifactAbstractIterator {
        #! Creates an EdifactIterator from an InputStream
        /** @param is the input stream to read EDIFACT data from
            @param opts optional options hash; see @ref EdifactIteratorOptions for valid options

            @throw EDIFACT-PARSE-ERROR if the data cannot be parsed
        */
        constructor(Qore::InputStream is, *hash<auto> opts) : EdifactAbstractIterator(opts) {
            @assert(is, "input stream cannot be null");
            # Read all data from stream
            string data = readInputStream(is);
            initializeFromData(data);
        }

        #! Creates an EdifactIterator from a string
        /** @param data the EDIFACT data string
            @param opts optional options hash; see @ref EdifactIteratorOptions for valid options

            @throw EDIFACT-PARSE-ERROR if the data cannot be parsed
        */
        constructor(string data, *hash<auto> opts) : EdifactAbstractIterator(opts) {
            initializeFromData(data);
        }

        #! Reads all data from an InputStream
        private string readInputStream(Qore::InputStream is) {
            # Read in chunks
            binary buffer = binary();
            int chunk_size = 65536;

            while (True) {
                *binary chunk = is.read(chunk_size);
                if (!exists chunk || chunk.empty()) {
                    break;
                }
                buffer += chunk;
            }

            # Convert to string with encoding if specified
            if (m_encoding) {
                return buffer.toString(m_encoding);
            }
            return buffer.toString();
        }
    }

    #! String data EDIFACT iterator
    /** This class reads EDIFACT data from a string and iterates over segments.

        @par Example
        @code{.py}
string edifact_data = "UNB+UNOC:3+SENDER+RECEIVER+260122:1200+REF001'UNH+1+ORDERS:D:96A:UN'...";
EdifactDataIterator i(edifact_data);
while (i.next()) {
    hash<auto> segment = i.getValue();
    printf("Segment: %s\n", segment.tag);
}
        @endcode
    */
    public class EdifactDataIterator inherits EdifactAbstractIterator {
        #! Creates an EdifactDataIterator from a string
        /** @param data the EDIFACT data string
            @param opts optional options hash; see @ref EdifactIteratorOptions for valid options

            @throw EDIFACT-PARSE-ERROR if the data cannot be parsed
        */
        constructor(string data, *hash<auto> opts) : EdifactAbstractIterator(opts) {
            @assert(data.val(), "EDIFACT data cannot be empty");
            initializeFromData(data);
        }
    }

    #! File-based EDIFACT iterator
    /** This class reads EDIFACT data from a file and iterates over segments.

        @par Example
        @code{.py}
EdifactFileIterator i("orders.edi");
while (i.next()) {
    hash<auto> segment = i.getValue();
    printf("Segment: %s = %y\n", segment.tag, segment.data_elements);
}
        @endcode
    */
    public class EdifactFileIterator inherits EdifactAbstractIterator {
        private {
            #! The file path
            string m_path;
        }

        #! Creates an EdifactFileIterator from a file path
        /** @param path the file path to read EDIFACT data from
            @param opts optional options hash; see @ref EdifactIteratorOptions for valid options

            @throw EDIFACT-PARSE-ERROR if the file cannot be read or data cannot be parsed
        */
        constructor(string path, *hash<auto> opts) : EdifactAbstractIterator(opts) {
            @assert(path.val(), "file path cannot be empty");
            @debug(printf("EdifactFileIterator::constructor(): reading from %y\n", path));
            m_path = path;

            # Use FileLocationHandler if available for location:// URLs
            string data;
            if (path =~ /^[a-z]+:\/\//) {
                data = FileLocationHandler::getTextFileFromLocation(path);
            } else {
                # Read from local file
                File f();
                f.open2(path, O_RDONLY);
                data = f.read(-1);
            }

            initializeFromData(data);
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
