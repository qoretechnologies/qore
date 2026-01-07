# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvFileWriter class definition

/*  CsvFileWriter.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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

# assume local var scope, do not use "$" for vars, members, and method calls
%modern

#! the CsvUtil namespace. All classes used in the CsvUtil module should be inside this namespace
public namespace CsvUtil {
    #! The CsvFileWriter class for safe CSV file creation
    /**
        @see
        - @ref CsvUtil::CsvWriter "CsvWriter" for a stream-based class providing the same functionality as this class in a more generic way
     */
    public class CsvFileWriter inherits AbstractCsvWriter {
        private {
            #! the file to write
            Qore::File file;
        }

        #! creates the CsvFileWriter in single-type mode with the path of the file to create and an optional option hash
        /**
            @param path a file name (with path optionally) to write
            @param opts @ref csvwriter_options

            The file is created with @ref Qore::O_CREAT "O_CREAT", @ref Qore::O_TRUNC "O_TRUNC", @ref Qore::O_WRONLY "O_WRONLY" and \c 0644 access.

            @throw CSVFILEITER-ERROR in the case of incorrect options
        */
        constructor(string path, *hash<auto> opts) : AbstractCsvWriter("FILE", opts) {
            openFile(path);
        }

        #! creates the CsvFileWriter in multi-type mode with the path of the file to create and an optional option hash
        /**
            @param path a file name (with path optionally) to write
            @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more information
            @param opts @ref csvwriter_options

            The file is created with @ref Qore::O_CREAT "O_CREAT", @ref Qore::O_TRUNC "O_TRUNC", @ref Qore::O_WRONLY "O_WRONLY" and \c 0644 access.

            @throw CSVFILEITER-ERROR in the case of incorrect options
        */
        constructor(string path, hash<auto> spec, hash<auto> opts) : AbstractCsvWriter("FILE", spec, opts) {
            openFile(path);
        }

        #! returns the output file path
        /** @return the output file path

            @since %CsvUtil 1.7
        */
        string getFileName() {
            return file.getFileName();
        }

        private openFile(string path) {
            # we do not use the encoding option with the File constructor() here, because we always generate output data
            # with the output encoding in any case
            file = new File();
            file.open2(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            writeHeaders();
        }

        private writeRawLine(list<auto> values) {
            file.write(prepareRawLine(values));
        }
    } # CsvFileWriter class
} # CsvUtil namespace
