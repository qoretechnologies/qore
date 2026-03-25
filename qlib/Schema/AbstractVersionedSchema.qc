# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractVersionedSchema class definition

/** AbstractVersionedSchema.qc Copyright 2014 - 2026 Qore Technologies, s.r.o.

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

    #! this class extends @ref Schema::AbstractSchema by providing version logic based on a schema version string contained within the schema itself
    /** the location of the version string is defined by the following abstract methods that must be reimplemented in subclasses:
        - getVersionTableImpl()
        - getVersionColumnImpl()
        - getVersionWhereImpl()

        @note the schema version string (as designated by the return values of the methods listed above)
        must be included in reference data as returned by either getStrictReferenceDataHashImpl() or
        getReferenceDataHashImpl().
    */
    public class AbstractVersionedSchema inherits AbstractSchema {
        private {
            #! the name of the table containing the version string
            string version_table;
            #! the name of the column containing the version string
            string version_column;
            #! a @ref where_clauses "where clause hash" defining the row where the schema version string is located
            hash version_where;
        }

        #! creates the object based on the arguments giving the location of the schema
        /** @param ds the datasource for the schema
            @param dts an optional data tablespace name for the schema
            @param its an optional index tablespace name for the schema
            @param opts options for @ref SqlUtil::Database::constructor()
        */
        constructor(Qore::SQL::AbstractDatasource ds, *string dts, *string its, *hash opts) : AbstractSchema(ds, dts, its, opts) {
            version_table = getVersionTable();
            version_column = getVersionColumn();
            version_where = getVersionWhere();
        }

        #! Protected constructor for subclasses that call init() themselves
        /** @since Schema 1.1
        */
        private constructor() {
        }

        #! returns the existing schema version or @ref nothing if not present
        private *softstring getSchemaVersion() {
            Table vt(ds, version_table);
            if (!vt.checkExistence())
                return;

            return vt.selectRow(("columns": version_column, "where": version_where)){version_column};
        }

        #! this method returns @ref True "True" if the schema can be dropped
        private bool checkDropSchema(bool force) {
            *string current_version = getSchemaVersion();
            if (!current_version) {
                if (!force) {
                    logpf("schema is corrupt: force option not set; not executing schema drop");
                    return False;
                }
                logpf("schema is corrupt: forcing schema drop");
                return True;
            }

            if (!compare_version(current_version, version)) {
                logpf("dropping schema %y", version);
                return True;
            }

            if (force) {
                logpf("schema is at unexpected version %y (executing with %y): forcing drop", current_version, version);
                return True;
            }
            logpf("schema is at unexpected version %y (executing with %y): force option not set; not executing schema drop", current_version, version);
            return False;
        }

        #! this method returns @ref True "True" if the schema can be updated
        private bool checkUpdateSchema(bool force, reference<hash<auto>> initial_schema_info) {
            *string current_version = getSchemaVersion();
            if (!current_version) {
                logpf("schema is corrupt; aligning to schema template");
                return True;
            }

            initial_schema_info.current_version = current_version;

            switch (compare_version(current_version, version)) {
                case < 0: {
                    logpf("schema is at version %y targeting %y: running schema alignment", current_version, version);
                    return checkUpgrade(current_version);
                }
                case 0: {
                    if (force) {
                        logpf("schema is already at target version %y: forcing check", version);
                        return True;
                    }
                    logpf("schema is already at target version %y: no alignment necessary", version);
                    return False;
                }
                case > 0: {
                    if (force) {
                        logpf("schema is at later version %y targeting %y: forcing check", current_version, version);
                        return checkDowngrade(current_version);
                    }
                    logpf("schema is at later version %y targeting %y: force option not set; not executing schema alignment to earlier schema version", current_version, version);
                    return False;
                }
            }
        }

        #! this method is called if the current schema version is less than the target version; returns @ref True "True" if the schema can be updated
        private bool checkUpgrade(string current_version) {
            return True;
        }

        #! this method is called if the current schema version is greater than the target version; returns @ref True "True" if the schema can be downgraded
        private bool checkDowngrade(string current_version) {
            return True;
        }

        #! returns the name of the table holding the schema version string by calling getVersionTableImpl()
        /** this function wraps getVersionTableImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getVersionTableImpl()
        */
        private string getVersionTable() {
            return getVersionTableImpl();
        }

        #! returns the name of the column holding the schema version string by calling getVersionColumnImpl()
        /** this function wraps getVersionColumnImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getVersionColumnImpl()
        */
        private string getVersionColumn() {
            return getVersionColumnImpl();
        }

        #! returns the @ref where_clauses "where clause hash" defining the row where the schema version string is located by calling getVersionWhereImpl()
        /** this function wraps getVersionWhereImpl(); for forwards-compatibility, do not override this
            method in subclasses, instead re-implement getVersionWhereImpl()
        */
        private hash getVersionWhere() {
            return getVersionWhereImpl();
        }

        #! returns the name of the table holding the schema version string
        private abstract string getVersionTableImpl();

        #! returns the name of the column holding the schema version string
        private abstract string getVersionColumnImpl();

        #! returns the @ref where_clauses "where clause hash" defining the row where the schema version string is located
        private abstract hash getVersionWhereImpl();
    }
} # namespace Schema
