# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractSavepointHelper class definition

/** AbstractSavepointHelper.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Abstract base class for savepoint helpers for epheremal transaction support
public class AbstractSavepointHelper {
    private {
        #! The datasource where the transaction is handled
        AbstractDatasource ds;

        #! The name of the savepoint
        string savepoint;
    }

    #! Creates the object
    constructor(AbstractDatasource ds, *string savepoint) {
        if (!savepoint.val()) {
            savepoint = "temp_" + self.uniqueHash();
        }
        self.ds = ds;
        self.savepoint = savepoint;
        # remove the savepoint if there's an error creating it
        on_error remove savepoint;
        createSavepointImpl();
    }

    #! Deletes the savepoint
    destructor() {
        if (savepoint) {
            deleteSavepointImpl();
        }
    }

    #! Rolls back to the savepoint
    rollback() {
        # remove the savepoint unconditionally on exit
        on_exit remove savepoint;
        rollbackImpl();
    }

    #! Creates the savepoint
    private abstract createSavepointImpl();

    #! Rolls back to the savepoint
    private abstract rollbackImpl();

    #! Deletes the savepoint
    private abstract deleteSavepointImpl();
}
}
