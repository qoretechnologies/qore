# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ObjectClassNameDataType class definition


/** ObjectClassNameDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! An object type whose class is identified by its name
/** A description of an object type, such as \c "object<ServerSentEventClient>", can name a class that the program
    restoring the type cannot resolve, because the class is defined in a module or a program that the restoring code
    does not see.  @ref DataProvider::AbstractDataProviderType::get() "AbstractDataProviderType::get()" restores such a
    type as an object of this class, which has the described name and accepted and returned types, and accepts an
    object whose class or one of whose parent classes has the class name.

    @par Example:
    @code{.py}
ObjectClassNameDataType t("ServerSentEventClient");
# t.getName() == "object<ServerSentEventClient>"; t.getOrNothingType().getName() == "*object<ServerSentEventClient>"
    @endcode

    @since DataProvider 3.11
*/
public class ObjectClassNameDataType inherits QoreDataType {
    private {
        #! The name of the class of the objects that the type accepts
        string class_name;
    }

    #! Creates the type
    /** @param class_name the name of the class of the objects that the type accepts, as given in the type's name
        @param or_nothing @ref True if the type also accepts no value
        @param options any options of the type
        @param tags any tags of the type
        @param attr the display name and descriptions of the type; the name is that of the object type
    */
    constructor(string class_name, bool or_nothing = False, *hash<auto> options, *hash<auto> tags,
            *hash<NameDescInfo> attr) : QoreDataType(or_nothing ? ObjectOrNothingType : ObjectType, options, tags,
            NOTHING, ObjectClassNameDataType::getAttr(class_name, or_nothing, attr)) {
        self.class_name = class_name;
    }

    #! Returns the name of the class of the objects that the type accepts
    string getClassName() {
        return class_name;
    }

    #! Returns the class name, and \c "nothing" and \c "null" if the type also accepts no value
    hash<string, bool> getAcceptTypeHash(*bool simple) {
        hash<string, bool> rv = {class_name: True};
        if (or_nothing_type) {
            rv += {"nothing": True, "null": True};
        }
        return rv;
    }

    #! Returns the class name, and \c "nothing" if the type can return no value
    hash<string, bool> getReturnTypeHash(*bool simple) {
        hash<string, bool> rv = {class_name: True};
        if (or_nothing_type) {
            rv."nothing" = True;
        }
        return rv;
    }

    #! Returns the value if it is an object of the class or of a class that inherits it
    /** @param value the value to check

        @return the value

        @throw RUNTIME-TYPE-ERROR the value is not an object of the class or of a class that inherits it, nor no value
        when the type also accepts no value
    */
    auto acceptsValue(auto value) {
        if (!exists value || value === NULL) {
            if (or_nothing_type) {
                return value;
            }
        } else if (value.typeCode() == NT_OBJECT) {
            foreach Class cls in (Class::getClass(value).getClassHierarchy()) {
                if (cls.getName() == class_name || cls.getPathName() == class_name) {
                    return value;
                }
            }
        }
        throw "RUNTIME-TYPE-ERROR", sprintf("type %y expects an object of class %y, but got type %y instead",
            getName(), class_name, value.fullType());
    }

    #! Returns the name and descriptions of the type
    private static hash<NameDescInfo> getAttr(string class_name, bool or_nothing, *hash<NameDescInfo> attr) {
        string name = sprintf("%sobject<%s>", or_nothing ? "*" : "", class_name);
        return <NameDescInfo>{
            "name": name,
            "display_name": attr.display_name ?? name,
            "short_desc": attr.short_desc ?? sprintf("an object of class %y", class_name),
            "desc": attr.desc ?? attr.short_desc ?? sprintf("an object of class %y", class_name),
        };
    }
}
}
