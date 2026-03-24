# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractSqlUtilBase class definition

/** AbstractSqlUtilBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

    private validateOptionsIntern(string err, hash<auto> ropt, reference<hash<auto>> opt0, string tag) {
        # make sure driver-specific options take precedence over generic options
        string dn = getDriverName();
        # make sure types can be converted
        hash<auto> opt += opt0;
        AbstractDatabase::checkDriverOptions(\opt, dn);
        opt0 = opt;

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
                foreach auto v in (l) {
                    int t = v.typeCode();
                    bool ok;
                    if (t == NT_STRING) {
                        if (v.toInt() == v)
                            v = v.toInt();
                        else
                            ok = True;
                    }
                    if (!ok) {
                        if (t == NT_INT) {
                            if (v < 1)
                                throw err, sprintf("%s: key %y element %d/%d (first is 0) is %d, column numbers must "
                                    "be positive integers", tag, k, $#, l.size(), v);
                        } else if (v.typeCode() != NT_HASH)
                            throw err, sprintf("%s: key %y element %d/%d (first is 0) has type %y, expecting "
                                "\"string\" or \"hash\" (value: %y)", tag, k, $#, l.size(), v.type(), opt);
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
