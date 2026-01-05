# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvStringWriter class definition

/*  CsvStringWriter.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
    #! The CsvStringWriter class for in-memory string CSV creation
    /**
        @see
        - @ref CsvUtil::CsvWriter "CsvWriter" for a stream-based class providing the same functionality as this class in a more generic way
     */
    public class CsvStringWriter inherits AbstractCsvWriter {
        private {
            # a csv content
            string content;
        }

        #! creates the CsvStringWriter single-type mode with content in the memory
        /**
            @param opts @ref csvwriter_options

            @throw CSVSTRINGITER-ERROR in the case of incorrect options
        */
        constructor(*hash opts) : AbstractCsvWriter("STRING", opts) {
            initContent();
        }

        #! creates the CsvStringWriter single-type mode with content in the memory
        /**
            @param spec a hash of field and type definitions; see @ref abstractcsviterator_option_field_hash for more information
            @param opts @ref csvwriter_options

            @throw CSVSTRINGITER-ERROR in the case of incorrect options
        */
        constructor(hash spec, hash opts) : AbstractCsvWriter("STRING", spec, opts) {
            initContent();
        }

        private initContent() {
            writeHeaders();
        }

        private writeRawLine(list values) {
            content += prepareRawLine(values);
        }

        #! Stream iterator and return a CSV-formatted output string
        /**
            @param iterator an iterator to stream data

            @return the CSV-formatted output string corresponding to the input data

            The iterator has to contain @ref list or @ref hash as a return value of \c getValue() method.

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints or when \c getValue does not return list or hash
         */
        string write(Qore::AbstractIterator iterator) {
            AbstractCsvWriter::write(iterator);
            return content;
        }

        #! Stream the contents of the list and return CSV-formatted output as a string
        /**
            @param l a list of input data to format as CSV output

            @return the CSV-formatted output string corresponding to the input data

            The list has to contain @ref list or @ref hash elements that can be formatted according to the CSV definition

            @throw CSVFILEWRITER-DATA-ERROR when the data does not fit defined column constraints or when the list elements have the wrong type or values
         */
        string write(list l) {
            AbstractCsvWriter::write(l);
            return content;
        }

        #! Get the current in-memory content as a string
        string getContent() {
            return content;
        }
    } # CsvStringWriter class
} # CsvUtil namespace
