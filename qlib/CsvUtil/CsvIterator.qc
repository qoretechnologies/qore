# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvIterator class definition

/*  CsvIterator.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
#! The CsvIterator class allows CSV sources to be iterated on a record basis
/** The source of the input data is either a @ref Qore::AbstractLineIterator "AbstractLineIterator" object or an
    @ref Qore::InputStream "InputStream" object.

    @see
    - @ref abstractcsviterator_options
    - @ref abstractcsviterator_option_field_types
    - @ref abstractcsviterator_option_field_hash
*/
public class CsvIterator inherits CsvUtil::AbstractCsvIterator {
    #! Creates the CsvIterator in single-type mode with general line iterator to read and an option hash
    /** @param li line iterator of CSV file to read
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names" is
        @ref True "True" and \c "header_lines" is 0 or \c "headers" is also present; unknown field type
    */
    constructor(Qore::AbstractLineIterator li, *hash<auto> opts) : AbstractCsvIterator(li, opts) {
    }

    #! Creates the CsvIterator in multi-type mode with general line iterator to read and optionally an option hash
    /** @param li line iterator of CSV file to read
        @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more
        information
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names" is
        @ref True "True" and \c "header_lines" is 0 or \c "headers" is also present; unknown field type
    */
    constructor(Qore::AbstractLineIterator li, hash<auto> spec, hash<auto> opts)
            : AbstractCsvIterator(li, spec, opts) {
    }

    #! Creates the CsvIterator from an @ref Qore::InputStream "InputStream" and other arguments
    /** @param input the @ref Qore::InputStream "InputStream" providing data to iterate
        @param encoding the encoding of the input stream
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names" is
        @ref True "True" and \c "header_lines" is 0 or \c "headers" is also present; unknown field type
    */
    constructor(Qore::InputStream input, string encoding = "UTF-8", *hash<auto> opts)
            : AbstractCsvIterator(new InputStreamLineIterator(input, encoding, opts.eol), opts) {
    }

    #! Creates the CsvIterator in multi-type mode from an @ref Qore::InputStream "InputStream" and other arguments
    /** @param input the @ref Qore::InputStream "InputStream" providing data to iterate
        @param encoding the encoding of the input stream
        @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more
        information
        @param opts a hash of optional options; see @ref abstractcsviterator_options for more information

        @throw ABSTRACTCSVITERATOR-ERROR invalid or unknown option; invalid data type for option; \c "header_names" is
        @ref True "True" and \c "header_lines" is 0 or \c "headers" is also present; unknown field type
    */
    constructor(Qore::InputStream input, string encoding = "UTF-8", hash<auto> spec, hash<auto> opts)
            : AbstractCsvIterator(new InputStreamLineIterator(input, encoding, opts.eol), spec, opts) {
    }

    auto memberGate(string name) {
        return AbstractCsvIterator::memberGate(name);
    }
} # CsvIterator class
} # CsvUtil namespace
