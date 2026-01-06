# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore CsvWriter class definition

/*  CsvWriter.qc Copyright 2012 - 2024 Qore Technologies, s.r.o.

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
    #! The CsvWriter class for safe CSV data creation
    public class CsvWriter inherits AbstractCsvWriter {
        private {
            #! the output stream for the CSV data
            StreamWriter output;
        }

        #! creates the CsvWriter in single-type mode with the @ref Qore::OutputStream "OutputStream" and an optional option hash
        /**
            @param output the @ref Qore::OutputStream "OutputStream" for the CSV data generated
            @param opts @ref csvwriter_options

            @throw CSVSTREAMWRITER-ERROR in the case of incorrect options
        */
        constructor(Qore::OutputStream output, *hash opts) : AbstractCsvWriter("STREAM", opts) {
            # we do not use the encoding option with the StreamWriter constructor() here, because we always generate output data
            # with the output encoding in any case
            self.output = new StreamWriter(output);
            writeHeaders();
        }

        #! creates the CsvWriter in multi-type mode with the @ref Qore::OutputStream "OutputStream" and an option hash
        /**
            @param output the @ref Qore::OutputStream "OutputStream" for the CSV data generated
            @param spec a hash of field and type definition; see @ref abstractcsviterator_option_field_hash for more information
            @param opts @ref csvwriter_options

            @throw CSVSTREAMWRITER-ERROR in the case of incorrect options
        */
        constructor(Qore::OutputStream output, hash spec, hash opts) : AbstractCsvWriter("STREAM", spec, opts) {
            # we do not use the encoding option with the StreamWriter constructor() here, because we always generate output data
            # with the output encoding in any case
            self.output = new StreamWriter(output);
            writeHeaders();
        }

        #! renders the line and writes it to the output stream
        private writeRawLine(list values) {
            output.print(prepareRawLine(values));
        }
    } # CsvWriter class
} # CsvUtil namespace
