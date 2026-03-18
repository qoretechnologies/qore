# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SchemaCallbackHelper class definition

/** SchemaCallbackHelper.qc Copyright 2014 - 2026 Qore Technologies, s.r.o.

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

%modern

#! contains all public definitions in the Schema module
public namespace Schema {

    #! this class provides callback support for schema operations
    class SchemaCallbackHelper {
        public {
            AbstractSchema schema;
            int verbose;
            int change_count = 0;
            int dot_count = 0;
            int error_count = 0;

            list<string> sql_cache = ();

            #! Change map; action code -> count
            hash<string, int> change_map;

            #! Summary map for data changes; upsert action code -> count
            hash<string, int> data_change_map;

            #! Set to True if only creations / additions were made
            /** If not assigned after a schema alignment action, then no actions were taken
            */
            bool created;

            #! Map of create actions
            const CreateActionMap = {
                AbstractDatabase::AC_Create: True,
                AbstractDatabase::AC_Add: True,
                AbstractDatabase::AC_Insert: True,
            };
        }

        #! creates the callback object from the given arguments
        /** @param sc the Schema::AbstractSchema "AbstractSchema" object for output logging and SQL execution
            @param v the verbosity level; larger numbers mean more output; negative numbers mean no output
        */
        constructor(AbstractSchema sc, int v = 0) {
            schema = sc;
            verbose = v;
        }

        #! Returns information about schema changes made
        hash<SchemaAlignmentInfo> getInfo() {
            return <SchemaAlignmentInfo>{
                "created": created ?? False,
                "num_changes": ((foldl $1 + $2, change_map.iterator()) + (foldl $1 + $2, data_change_map.iterator())) ?? 0,
                "change_map": change_map,
                "data_change_map": data_change_map,
            };
        }

        #! this is the informational callback method for schema operations
        infoCallback(string str, int ac, *string type, *string name, *string table, *string new_name, *string info) {
            # maintain change map
            ++change_map{AbstractDatabase::ActionMap{ac}};

            if (!exists created) {
                if (CreateActionMap{ac}) {
                    created = True;
                }
            } else if (created && !CreateActionMap{ac}) {
                created = False;
            }

            # verbosity threadhold
            int t = 0;
            if (ac != AbstractDatabase::AC_Unchanged && ac != AbstractDatabase::AC_NotFound) {
                ++change_count;
            } else {
                t = 1;
            }
            if (verbose < 0) {
                return;
            }
            if (verbose > t) {
                if (dot_count) {
                    schema.logProgress("\n");
                    dot_count = 0;
                }
                schema.log("%s %s", t ? "+++" : "***", str);
            } else {
                ++dot_count;
                schema.logProgress(AbstractDatabase::ActionLetterMap{ac});
            }
        }

        #! this is the SQL callback method for schema operations
        sqlCallback(string str) {
            sql_cache += str;
            if (verbose > 1) {
                schema.log("%s", str);
            }
            schema.getDatasource().execRaw(str);
        }

        #! this is the SQL callback method for forced schema operations
        nothing sqlForceCallback(string str) {
            sql_cache += str;
            if (verbose > 1) {
                schema.log("%s", str);
            }
            try {
                schema.getDatabase().tryExecRaw(str);
            } catch (hash<ExceptionInfo> ex) {
                schema.log("%s: %s: %s", get_ex_pos(ex), ex.err, ex.desc);
                ++error_count;
            }
        }

        #! returns a callback option hash usable with <a href="../../SqlUtil/html/index.html">SqlUtil</a> schema operations
        hash<auto> getCallbacks(bool force = False) {
            return (
                "info_callback": \infoCallback(),
                "sql_callback": force ? \sqlForceCallback() : \sqlCallback(),
                "sql_callback_executed": True,
            );
        }
    }
} # namespace Schema
