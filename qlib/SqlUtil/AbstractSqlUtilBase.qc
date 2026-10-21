# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractSqlUtilBase class definition

/** AbstractSqlUtilBase.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! base class for abstract SqlUtil classes
public class AbstractSqlUtilBase inherits Qore::Serializable {
    private {
        #! datasource description
        string dsdesc;
        #! option hash
        *hash<auto> opts;
        #! the connection to the database server
        transient AbstractDatasource ds;
        #! mutex for atomic actions
        transient Mutex l();
    }

    # assigs the object's members after deserialization
    private:internal deserializeMembers(hash<auto> members) {
        # create datasource
        hash<auto> dsh = remove members.ds;
        # must initialize transient members explicitly when "deserializeMembers()" is defined
        ds = sqlutil_get_ds(dsh.ds_type, dsh.ds);

        # load any dependent module
        AbstractDatabase::loadModule(ds);

        l = new Mutex();

        self += members;
    }

    # serializes the object's data to a hash
    private:internal hash<auto> serializeMembers(hash<auto> members) {
        return members + {
            "ds": sqlutil_ds(ds),
        };
    }

    #! creates the object; private constructor
    /** @param nds the AbstractDatasource for the connection to the database
        @param nopts a hash of options for the table creation string; see @ref SqlUtil::AbstractTable::TableOptions for common options; each driver can support additional driver-specific options

        @throw OPTION-ERROR invalid or unsupported option passed
        */
    private constructor(AbstractDatasource nds, *hash nopts) {
        opts = nopts;
        ds = nds;

        # ensure datasource is open
        ds.getServerVersion();
        dsdesc = AbstractSqlUtilBase::makeDatasourceDesc(ds);
    }

    static string makeDatasourceDesc(AbstractDatasource ds) {
        string dsdesc = sprintf("%s:%s@%s", ds.getDriverName(), ds.getUserName(), ds.getDBName());
        if (ds.getHostName())
            dsdesc += "%" + ds.getHostName();
        if (ds.getPort())
            dsdesc += sprintf(":%d", ds.getPort());
        return dsdesc;
    }

    #! returns a name as an SQL identifier for the database
    /** A name that the database reads as one unquoted identifier, and that is not a reserved word, is returned as
        given, so the SQL generated for ordinary names is the same as before; any other name, such as one with
        spaces, quotes, semicolons, comment markers or other punctuation, is quoted as a delimited identifier with
        the rules of the database, each delimiter character in the name escaped, so that a name, wherever it comes
        from, can never change the SQL statement that it is part of; see @ref sql_identifiers

        @par Example:
        @code{.py}
# select "Order Date" from public.orders (on PostgreSQL)
string sql = "select " + table.getSqlIdentifier("Order Date") + " from " + table.getSqlName();
        @endcode

        @param name the name of a database object, such as a table, column, index, constraint, sequence or schema,
        without any schema prefix

        @return the name as an SQL identifier for the database: as given, or quoted and escaped

        @throw IDENTIFIER-ERROR the name cannot be an identifier on the database, such as an empty name, a name with
        a NUL character, or a name with a character that the database does not allow in an identifier, as a double
        quote on Oracle

        @note this base implementation, used only by a driver that does not implement identifier quoting, returns
        a name of ASCII letters, digits and underscores that does not start with a digit as given, and refuses any
        other name with \c IDENTIFIER-ERROR

        @since SqlUtil 3.11.0
    */
    string getSqlIdentifier(string name) {
        if (name =~ /\A[A-Za-z_][A-Za-z0-9_]*\z/) {
            return name;
        }
        AbstractSqlUtilBase::checkIdentifier(name, getDriverName());
        throw "IDENTIFIER-ERROR", sprintf("%s: the name %y is not a plain identifier, and the %y driver has no "
            "identifier quoting in SqlUtil", dsdesc, name, getDriverName());
    }

    #! checks that a name can be an identifier on any database
    /** @param name the name to check
        @param driver the name of the database driver for the exception message

        @throw IDENTIFIER-ERROR the name is empty or has a NUL character, which no database supported by SqlUtil
        allows in an identifier

        @since SqlUtil 3.11.0
    */
    static checkIdentifier(string name, string driver) {
        if (name == "") {
            throw "IDENTIFIER-ERROR", sprintf("%s: an empty name cannot be an SQL identifier", driver);
        }
        if (name.find(chr(0)) >= 0) {
            throw "IDENTIFIER-ERROR", sprintf("%s: the name \"%s\" has a NUL character (shown as \"\\0\"), "
                "which cannot be part of an SQL identifier", driver, replace(name, chr(0), "\\0"));
        }
    }

    #! returns a name of plain identifiers for SQL in a driver-independent implementation
    /** For SQL generated by driver-independent code, which does not know how the database quotes identifiers:
        a name of ASCII letters, digits, underscores and dollar signs that does not start with a digit or a dollar
        sign, optionally after a schema of the same form and a \c ".", is returned as given; drivers implement
        the methods that generate SQL with names that need quotes

        @param name the name of the database object, with an optional schema
        @param driver the name of the database driver for the exception message

        @return the name as given

        @throw IDENTIFIER-ERROR the name is not of plain identifiers

        @since SqlUtil 3.11.0
    */
    static string getPlainSqlName(string name, string driver) {
        if (name !~ /\A[A-Za-z_][A-Za-z0-9_$]*(?:\.[A-Za-z_][A-Za-z0-9_$]*)?\z/) {
            AbstractSqlUtilBase::checkIdentifier(name, driver);
            throw "IDENTIFIER-ERROR", sprintf("%s: the name %y is not a plain identifier, and the driver gives no "
                "SQL for it", driver, name);
        }
        return name;
    }

    #! returns a name as a delimited identifier, with each closing delimiter in the name doubled
    /** For use by drivers implementing @ref getSqlIdentifier()

        @par Example:
        @code{.py}
# returns "\"a\"\"b\""
string id = AbstractSqlUtilBase::getDelimitedIdentifier("a\"b", "\"", "\"", "pgsql");
        @endcode

        @param name the name of the database object
        @param open the opening delimiter, such as \c "\"", \c "`" or \c "["
        @param close the closing delimiter, such as \c "\"", \c "`" or \c "]"; each occurrence in the name is
        doubled, as SQL escapes it in a delimited identifier
        @param driver the name of the database driver for the exception message

        @return the name as a delimited identifier

        @throw IDENTIFIER-ERROR the name is empty or has a NUL character (see @ref checkIdentifier())

        @since SqlUtil 3.11.0
    */
    static string getDelimitedIdentifier(string name, string open, string close, string driver) {
        AbstractSqlUtilBase::checkIdentifier(name, driver);
        return open + replace(name, close, close + close) + close;
    }

    #! validates options and converts each one to the type of its option
    /** @param err the exception code to throw
        @param ropt the option types, keyed by option name
        @param opt0 the options; on return, they have driver-specific options applied, each option converted to the
        type of its option, and no option without a value, in a hash that takes values of any type
        @param tag a description of the object for exception messages

        @throw err an unknown option, or an option value that cannot be converted to the type of its option
    */
    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash<auto>> opt0, string tag) {
        # make sure driver-specific options take precedence over generic options
        string dn = getDriverName();
        # make sure types can be converted
        hash<auto> opt += opt0;
        AbstractDatabase::checkDriverOptions(\opt, dn);

        # check valid options and option value types
        foreach string k in (keys opt) {
            if (!exists opt{k}) {
                delete opt{k};
                continue;
            }

            if (!ropt.hasKey(k)) {
                throw err, sprintf("%s: unknown key %y; valid keys for %y: %y", tag, k, self.className(), keys ropt);
            }

            AbstractSqlUtilBase::validateOptionIntern(err, ropt{k}, \opt{k}, k, tag);
        }
        # return the options as they are converted
        opt0 = opt;
    }

    private static validateOptionIntern(string err, string type, reference<auto> opt, string k, string tag) {
        switch (type) {
            case Type::Int: {
                if (opt.type() != type) {
                    if (!opt.intp())
                        throw err, sprintf("%s: key %y has type %y, expecting \"int\" (value: %y)", tag, k,
                            opt.type(), opt);
                    opt = opt.toInt();
                }
                break;
            }
            case Type::Number: {
                if (opt.type() != type) {
                    if (!opt.intp())
                        throw err, sprintf("%s: key %y has type %y, expecting \"number\" (value: %y)", tag, k,
                            opt.type(), opt);
                    opt = number(opt);
                }
                break;
            }
            case Type::Boolean: {
                if (opt.type() != type) {
                    if (!opt.intp())
                        throw err, sprintf("%s: key %y has type %y, expecting \"boolean\" (value: %y)", tag, k,
                            opt.type(), opt);
                    opt = opt.toBool();
                }
                break;
            }
            case Type::String: {
                if (opt.type() != type) {
                    if (!opt.strp())
                        throw err, sprintf("%s: key %y has type %y, expecting \"string\" (value: %y)", tag, k,
                            opt.type(), opt);
                    opt = opt.toString();
                }
                break;
            }
            case "softstringlist": {
                softlist l = opt;
                foreach auto v in (l) {
                    if (v.typeCode() != NT_STRING)
                        throw err, sprintf("%s: key %y element %d/%d (first is 0) has type %y, expecting \"string\" "
                            "(value: %y)", tag, k, $#, l.size(), v.type(), opt);
                }

                opt = l;
                break;
            }
            case "stringhashlist": {
                softlist l = opt;
                foreach auto v in (l) {
                    switch (v.typeCode()) {
                        case NT_STRING:
                        case NT_HASH:
                            break;
                        default:
                            throw err, sprintf("%s: key %y element %d/%d (first is 0) has type %y, expecting "
                                "\"string\" or \"hash\" (value: %y)", tag, k, $#, l.size(), v.type(), opt);
                    }
                }

                opt = l;
                break;
            }
            case "softstringinthashlist": {
                softlist l = opt;
                for (int i = 0; i < l.size(); ++i) {
                    # a string that is an integer is a column number
                    if (l[i].typeCode() == NT_STRING && l[i].toInt() == l[i]) {
                        l[i] = l[i].toInt();
                    }
                    switch (l[i].typeCode()) {
                        case NT_INT:
                            if (l[i] < 1) {
                                throw err, sprintf("%s: key %y element %d/%d (first is 0) is %d, column numbers "
                                    "must be positive integers", tag, k, i, l.size(), l[i]);
                            }
                            break;
                        case NT_STRING:
                        case NT_HASH:
                            break;
                        default:
                            throw err, sprintf("%s: key %y element %d/%d (first is 0) has type %y, expecting "
                                "\"string\" or \"hash\" (value: %y)", tag, k, i, l.size(), l[i].type(), opt);
                    }
                }

                opt = l;
                break;
            }
            case "hash/list": {
                if (opt.typeCode() == NT_LIST) {
                    foreach auto v in (opt) {
                        if (v.typeCode() != NT_HASH)
                            throw err, sprintf("%s: key %y element %d/%d (first is 0) has type %y, expecting "
                                "\"hash\" (value: %y)", tag, k, $#, opt.size(), v.type(), opt);
                    }
                } else if (opt.typeCode() != NT_HASH)
                    throw err, sprintf("key %y requires type \"list\" or \"hash\" but type %y was passed (%y)", k,
                        opt.type(), opt);
                break;
            }
            case "Tables": {
                if (!(opt instanceof Tables))
                    throw err, sprintf("%s: key %y has type %s, expecting class \"Tables\"", tag, k,
                        opt.typeCode() == NT_OBJECT ? sprintf("class \"%s\"", opt.className()) : sprintf("\"%s\"",
                        opt.type()));
                break;
            }

            case "SchemaForeignConstraintAlignment": {
                if (!(opt instanceof SchemaForeignConstraintAlignment)) {
                    throw err, sprintf("%s: key %y has type %s, expecting class \"SchemaForeignConstraintAlignment\"",
                        tag, k, opt.typeCode() == NT_OBJECT ? sprintf("class \"%s\"", opt.className())
                            : sprintf("\"%s\"", opt.type()));
                }
                break;
            }

            # no validation for NOTHING
            case Type::NothingType: break;

            case "code": if (opt.typeCode() == NT_CLOSURE || opt.typeCode() == NT_CALLREF)
                break;

            # throw an exception if the type is not supported
            default:
                if (opt.type() != type)
                    throw err, sprintf("key %y requires type %y but type %y was passed (%y)", k, type, opt.type(),
                        opt);
        }
    }

    #! Check input node for all hash keys - if it contains a key with whitespace in the beginning or at the end -> error
    private validateHashKeysForWhitespaces(auto node) {
        switch (node.typeCode()) {
            case NT_HASH: {
                    HashIterator it(node);
                    while (it.next()) {
                        if (trim(it.getKey()) != it.getKey()) {
                            throw "VALIDATE-HASH-ERROR", sprintf("Key '%s' contains whitespaces at the end or in the "
                                "beginning.", it.getKey());
                        }
                        validateHashKeysForWhitespaces(it.getValue());
                    }
                }
                break;
            case NT_LIST:{
                    ListIterator it(node);
                    while (it.next()) {
                        validateHashKeysForWhitespaces(it.getValue());
                    }
                }
                break;
        } # switch
    }

    #! gets the underlying AbstractDatasource
    Qore::SQL::AbstractDatasource getDatasource() {
        return ds;
    }

    #! returns the database driver name
    string getDriverName() {
        return ds.getDriverName();
    }

    #! returns a descriptive string for the datasource
    string getDatasourceDesc() {
        return dsdesc;
    }
}
}
