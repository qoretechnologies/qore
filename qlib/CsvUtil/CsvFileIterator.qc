# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvFileIterator class definition

/*  CsvFileIterator.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
    #! The CsvFileIterator class allows CSV files to be iterated on a record basis
    /** The class is deprecated as @ref CsvIterator provides more flexibility.
        @see
        - @ref CsvUtil::CsvIterator "CsvIterator" for a stream-based class providing the same functionality as this class in a more generic way
        - @ref abstractcsviterator_options
        - @ref abstractcsviterator_option_field_types
        - @ref abstractcsviterator_option_field_hash
     */
    public class CsvFileIterator inherits CsvUtil::AbstractCsvIterator {
        private {
            #! the path of the file being iterated
            string m_file_path;
        }

        #! Creates the CsvFileIterator in single-type mode with the path of the file to read and an option hash
        /** @param path the path to the CSV file to read
            @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

            @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names" is @ref True "True" and \c "header_lines" is 0 or \c "headers" is also present; unknown field type
         */
        constructor(string path, *hash opts) : AbstractCsvIterator(new InputStreamLineIterator(new FileInputStream(path), opts.encoding, opts.eol), opts) {
            m_file_path = path;
        }

        #! Creates the CsvFileIterator in multi-type mode with the path of the file to read and optionally an option hash
        /** @param path the path to the CSV file to read
            @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more information
            @param opts a hash of optional options; see @ref abstractcsviterator_options for more information
         */
        constructor(string path, hash spec, hash opts) : AbstractCsvIterator(new InputStreamLineIterator(new FileInputStream(path), opts.encoding, opts.eol), spec, opts) {
            m_file_path = path;
        }

        #! calls AbstractCsvIterator::memberGate()
        auto memberGate(string name) {
            return AbstractCsvIterator::memberGate(name);
        }

        #! Returns the character encoding for the file
        string getEncoding() {
            return cast<InputStreamLineIterator>(lineIterator).getEncoding();
        }

        #! Returns the file path/name used to open the file
        string getFileName() {
            return m_file_path;
        }

        #! Returns a @ref Qore::StatInfo "StatInfo" hash of the underlying file
        hash<Qore::StatInfo> hstat() {
            return Qore::hstat(m_file_path);
        }

        #! Returns a @ref stat_list "stat list" of the underlying file
        list stat() {
            return Qore::stat(m_file_path);
        }
    } # CsvFileIterator class
} # CsvUtil namespace
