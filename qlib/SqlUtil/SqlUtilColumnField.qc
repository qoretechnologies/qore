# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SqlUtilColumnField class definition

/** SqlUtilColumnField.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! SqlUtil column field class
public class SqlUtilColumnField inherits QoreDataField {
    #! creates the field from the arguments
    /** if a default value is given, the value may be an expression executed server side
        which will fail the type checks in the assignment, so if we have a default value, in case we cannot set it,
        we set the type to nullable as anyway the value will be generated server side
    */
    constructor(string name, *string desc, Type type, auto default_value)
        : QoreDataField(name, desc, exists default_value ? type.getOrNothingType() : type, default_value) {
    }

    #! creates the field from the arguments
    constructor(string name, *string desc, AbstractDataProviderType type, auto default_value)
        : QoreDataField(name, desc, type, default_value) {
    }

    #! sets the default value for the field
    setDefaultValue(auto default_value) {
        # if an exception occurs, then we assume that the default value is an expression that is executed server side
        try {
            # check that the value can be assigned to the type
            QoreDataField::setDefaultValue(default_value);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "RUNTIME-TYPE-ERROR") {
                rethrow;
            }
        }
    }
}
}
