# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Database class definition

/** Database.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

# minimum required Qore version
%requires qore >= 0.9.4
# assume local scope for variables, do not use "$" signs
%modern
# require type definitions everywhere
#! strict argument handling
# enable all warnings

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! represents a database; this class embeds an AbstractDatabase object that is created automatically in the constructor based on the database driver for the AbstractDatasource object providing the database connection
/** Driver-specific modules that provide the AbstractDatabase implementation embedded in this class are loaded on demand based on the driver's name.
    The driver-specific module's name is generated based on the db-driver's name with the first letter capitalized then with \c "SqlUtil" appended.

    For example:
    - \c "oracle": <a href="../../OracleSqlUtil/html/index.html">OracleSqlUtil</a>
    - \c "pgsql": <a href="../../PgsqlSqlUtil/html/index.html">PgsqlSqlUtil</a>
    - \c "mysql": <a href="../../MysqlSqlUtil/html/index.html">MysqlSqlUtil</a>

    etc.
*/
public class Database {
    private {
        #! the embedded AbstractDatabase object that actually provides the functionality for this class
        AbstractDatabase db;
    }

    #! creates the Database object
    /** @par Example:
        @code{.py}
Database db(ds);
        @endcode

        @param ds the AbstractDatasource object used to provide the DB connection
        @param opts a hash of options for the function creation string; see \c AbstractDatabase::DatabaseOptions for common options; each driver can support additional driver-specific options

        @throw DATABASE-DRIVER-ERROR no database-specific module can be loaded
        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(AbstractDatasource ds, *hash<auto> opts) {
        db = AbstractDatabase::getDatabase(ds, opts);
    }

    #! creates the AbstractDatabase object
    /** @par Example:
        @code{.py}
AbstractDatabase db("pgsql:user/pass@db%host");
        @endcode

        @param ds a datasource description string in the format that can be parsed by parse_datasource()
        @param opts a hash of options for the function creation string; see \c AbstractDatabase::DatabaseOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(string ds, *hash<auto> opts) {
        db = AbstractDatabase::getDatabase(ds, opts);
    }

    #! creates the AbstractDatabase object
    /** @par Example:
        @code{.py}
AbstractDatabase db("pgsql:user/pass@db%host");
        @endcode

        @param ds a hash giving parameters for the new datasource with the following possible keys (the \c "type" key is mandatory, also usable with the output of the parse_datasource() function):
        - \c type: (@ref string_or_nothing_type "*string") The name of the database driver to use; this key is mandatory; if not present, an exception will be raised. See @ref sql_constants for builtin constants for DBI drivers shipped with Qore, or see the DBI driver documentation to use an add-on driver (this string should be the name of the driver to be loaded)
        - \c user: (@ref string_or_nothing_type "*string") The user name for the new connection
        - \c pass: (@ref string_or_nothing_type "*string") The password for the new connection
        - \c db: (@ref string_or_nothing_type "*string") The database name for the new connection
        - \c charset: (@ref string_or_nothing_type "*string") The database-specific name of the character encoding to use for the new connection. Also see Qore::SQL::Datasource::setDBCharset() for a method that allows this parameter to be set after the constructor. If no value is passed for this parameter, then the database character encoding corresponding to the default character encoding for the %Qore process will be used instead.
        - \c host: (@ref string_or_nothing_type "*string") The host name for the new connection
        - \c port: (@ref softint_type "softint") The port number for the new connection
        - \c options: (@ref hash_or_nothing_type "*hash") Any options for the new connection
        @param opts a hash of options for the table creation string; see \c AbstractDatabase::DatabaseOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
    */
    constructor(hash ds, *hash<auto> opts) {
        db = AbstractDatabase::getDatabase(ds, opts);
    }

    #! gets the underlying AbstractDatabase
    SqlUtil::AbstractDatabase getDatabase() {
        return db;
    }

    #! executes a method on the contained AbstractDatabase object
    auto methodGate(string meth) {
        return call_object_method_args(db, meth, argv);
    }
}
}
