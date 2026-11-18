# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore OptionHelper class definition

/*  OptionHelper.qc Copyright 2016 - 2024 Qore Technologies, s.r.o.

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

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
#! Class with helper methods to options in a hash
public class OptionHelper inherits Qore::Serializable {
    #! Returns a string value from a hash
    static *string getString(*hash<auto> opts, string key, *bool required, *string default_value) {
        auto v = opts{key};
        if (v === "") {
            remove v;
        }
        if (required && exists v && v.strp()) {
            return v.toString();
        }
        return OptionHelper::getOptionType(opts, key, NT_STRING, Type::String, required, default_value);
    }

    #! Returns a boolean value from a hash
    static *bool getBool(*hash<auto> opts, string key, *bool required, *bool default_value) {
        if (exists opts{key} && opts{key}.typeCode() == NT_STRING) {
            return parse_boolean(opts{key});
        }
        return OptionHelper::getOptionType(opts, key, NT_BOOLEAN, Type::Boolean, required, default_value);
    }

    #! Gets an integer option
    static *int getInt(*hash<auto> opts, string key, *bool required, *int default_value) {
        auto v = opts{key};
        if (exists v && v.intp()) {
            return opts{key}.toInt();
        }
        return OptionHelper::getOptionType(opts, key, NT_INT, Type::Int, required, default_value);
    }

    #! Returns a hash value from a hash
    static *hash<auto> getHash(*hash<auto> opts, string key, *bool required, *hash<auto> default_value) {
        return OptionHelper::getOptionType(opts, key, NT_HASH, Type::Hash, required, default_value);
    }

    #! gets an option value of the given type
    static auto getOptionType(*hash<auto> opts, string key, int type_code, string type_name, *bool required,
            auto default_value) {
        auto v = opts{key};
        if (!exists v || v === NULL) {
            if (exists default_value) {
                return default_value;
            }
            if (!required) {
                return;
            }
            throw "OPTION-ARG-ERROR", sprintf("missing required %s option %y in option argument", type_name,
                key);
        }
        if (v.typeCode() != type_code) {
            # only the type of the value, which can be a credential
            throw "OPTION-ARG-ERROR", sprintf("required option %y was passed as a %s; expecting %y", key,
                v.type(), type_name);
        }
        return v;
    }
}
}
