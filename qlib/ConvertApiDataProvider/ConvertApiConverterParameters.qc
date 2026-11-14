# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConverterParameters.qc typed request fields for the parameters of a ConvertAPI converter

/*  ConvertApiConverterParameters.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConvertApiDataProvider module
public namespace ConvertApiDataProvider {

#! Builds typed request fields from the parameters of a ConvertAPI converter
/** ConvertAPI describes the parameters of each converter in its \c info endpoint: their names, labels,
    descriptions, types, value lists, ranges and defaults.  The generic \c convert-file action offers the parameters
    of the selected converter as typed options with the fields built here (see
    @ref ConvertApiConvertDataProvider::getConverterRequestType()).

    A field has the ConvertAPI name of its parameter, so its value is sent as that parameter.  The parameters that
    the module sets from its own request fields are left out (see @ref ReservedParameters), and defaults are only
    described, never set: a parameter that a request does not give is not sent, and ConvertAPI applies its own
    default, which can depend on other parameters (for example the compression \c Preset).

    @since ConvertApiDataProvider 1.0
*/
public class ConvertApiConverterParameters {
    public {
        #! The converter parameters that the module sets from its own request fields, in lower case
        /** The input (\c File and \c Files from the \c file, \c url, \c files and \c urls fields, and \c Url, the web
            page of the \c html converters, from the \c url field), the output (\c StoreFile, \c FileName and
            \c Timeout from the \c store_file, \c file_name and \c timeout fields), and the authentication parameters,
            as the connection's token is used
        */
        const ReservedParameters = {
            "file": True,
            "files": True,
            "url": True,
            "storefile": True,
            "filename": True,
            "timeout": True,
            "secret": True,
            "token": True,
        };

        #! The name of the parameter group of the authentication parameters, which are not offered
        const AuthenticationGroup = "Authentication";
    }

    #! Returns the request fields for the parameters of a converter
    /** @param converter the converter, as returned by the \c info endpoint
        @param given the names of the parameters that a request gives in its \c parameters field, in upper case; a
        parameter that the converter requires is optional when it is given there
        @param exclude field names that the fields must not use, such as those of the action's own fields; compared
        case-insensitively

        @return the field definitions by parameter name, in the order of the converter's parameter groups
    */
    static hash<string, hash<auto>> getFields(hash<auto> converter, *hash<string, bool> given,
            *list<string> exclude) {
        hash<auto> skip = ReservedParameters + (map {$1.lwr(): True}, exclude);
        hash<string, hash<auto>> rv = {};
        foreach auto group in (converter.ConverterParameterGroups) {
            if (group.typeCode() != NT_HASH || group.Name == AuthenticationGroup) {
                continue;
            }
            foreach auto param in (group.ConverterParameters) {
                if (param.typeCode() != NT_HASH || param.Name.typeCode() != NT_STRING || !param.Name.val()
                    || skip{param.Name.lwr()}) {
                    continue;
                }
                skip{param.Name.lwr()} = True;
                rv{param.Name} = ConvertApiConverterParameters::getField(param,
                    param.Required && !given{param.Name.upr()});
            }
        }
        return rv;
    }

    #! Returns a parameter of a converter
    /** @param converter the converter, as returned by the \c info endpoint
        @param name the parameter name, compared case-insensitively

        @return the parameter, as returned by the \c info endpoint, or @ref nothing if the converter has no parameter
        with the name
    */
    static *hash<auto> getParameter(hash<auto> converter, string name) {
        name = name.lwr();
        foreach auto group in (converter.ConverterParameterGroups) {
            if (group.typeCode() != NT_HASH) {
                continue;
            }
            foreach auto param in (group.ConverterParameters) {
                if (param.typeCode() == NT_HASH && param.Name.typeCode() == NT_STRING && param.Name.lwr() == name) {
                    return param;
                }
            }
        }
    }

    #! Returns the request field for a converter parameter
    /** @param param the parameter, as returned by the \c info endpoint
        @param required @ref True if the field must be given

        @return the field definition
    */
    static hash<auto> getField(hash<auto> param, bool required) {
        hash<auto> rv = {
            "type": ConvertApiConverterParameters::getType(param, required),
            "display_name": AbstractDataField::getDisplayName(param.Label.typeCode() == NT_STRING && param.Label.val()
                ? param.Label
                : param.Name),
            "desc": ConvertApiConverterParameters::getDescription(param),
        };
        if (param.Type == "Collection" && param.Values.typeCode() == NT_HASH && param.Values) {
            # the values of a list parameter are the values of its elements
            rv{param.Array ? "element_allowed_values" : "allowed_values"} =
                ConvertApiConverterParameters::getAllowedValues(param);
        }
        hash<auto> attr;
        # ConvertAPI marks the parameters that most users of a converter set
        if (param.Featured) {
            attr.preselected = True;
        }
        # passwords, and the cookies and HTTP headers that a web page is requested with, can carry credentials
        if (param.Type == "String" && param.Name =~ /(password|^cookies$|^headers$)/i) {
            attr.sensitive = True;
        }
        if (attr) {
            rv.attr = attr;
        }
        return rv;
    }

    #! Returns the type of the request field for a converter parameter
    /** @param param the parameter, as returned by the \c info endpoint
        @param required @ref True if the field must be given

        @return the type: a boolean, integer or floating-point number type that also accepts the value as a string,
        a file, a URI, or a string for text, colors and value lists; a parameter of a type that is not known here
        accepts any value, which is sent as it is
    */
    static auto getType(hash<auto> param, bool required) {
        switch (param.Type) {
            case "Bool":
                return required ? SoftBoolType : SoftBoolOrNothingType;
            case "Integer":
                return required ? SoftIntType : SoftIntOrNothingType;
            case "Double":
                return required ? SoftFloatType : SoftFloatOrNothingType;
            case "File":
                if (param.Array) {
                    return new SoftListDataType("ConvertApiParameterFileList", FileDataType, !required);
                }
                return required ? FileDataType : FileOrNothingDataType;
            case "Uri":
                return required ? UriType : UriOrNothingType;
            case "String":
            case "Color":
            case "Collection":
                if (param.Array) {
                    return new SoftListDataType("ConvertApiParameterStringList", StringType, !required);
                }
                return required ? StringType : StringOrNothingType;
        }
        return AutoType;
    }

    #! Returns the description of the request field for a converter parameter
    /** @param param the parameter, as returned by the \c info endpoint

        @return ConvertAPI's description of the parameter, followed by its range and default if it has them, and by
        the file types of a file parameter
    */
    static string getDescription(hash<auto> param) {
        *string desc = param.DescriptionMarkdown.typeCode() == NT_STRING && param.DescriptionMarkdown.val()
            ? param.DescriptionMarkdown
            : (param.Description.typeCode() == NT_STRING ? param.Description : NOTHING);
        desc = trim(replace(desc ?? "", "\r\n", "\n"));
        if (!desc.val()) {
            desc = param.Label.typeCode() == NT_STRING && param.Label.val() ? param.Label : param.Name;
        }
        list<string> notes = ();
        if ((param.Type == "Integer" || param.Type == "Double") && param.Range.typeCode() == NT_HASH
            && exists param.Range.From && exists param.Range.To) {
            notes += sprintf("From `%s` to `%s`.", param.Range.From, param.Range.To);
        }
        # the default of a value list is given with its value
        if (param.Type != "Collection" && exists param.Default && param.Default !== "") {
            notes += sprintf("The default is `%s`.", ConvertApiConverterParameters::getValueText(param.Default));
        }
        if (param.Type == "File" && param.AllowedExtensions.typeCode() == NT_LIST && param.AllowedExtensions) {
            notes += sprintf("File types: %s.", (map sprintf("`%s`", $1), param.AllowedExtensions).join(", "));
        }
        if (notes) {
            desc += "\n\n" + notes.join(" ");
        }
        return desc;
    }

    #! Returns the allowed values of a parameter with a value list
    /** @param param the parameter, as returned by the \c info endpoint, with a \c Values hash of values to labels

        @return the allowed values, with ConvertAPI's labels as their display names; the default value is described
        as such
    */
    static list<hash<AllowedValueInfo>> getAllowedValues(hash<auto> param) {
        list<hash<AllowedValueInfo>> rv = ();
        foreach hash<auto> i in (param.Values.pairIterator()) {
            hash<AllowedValueInfo> av = <AllowedValueInfo>{
                "value": i.key,
                "display_name": i.value.typeCode() == NT_STRING && i.value.val() ? i.value : i.key,
            };
            if (exists param.Default && ConvertApiConverterParameters::getValueText(param.Default) == i.key) {
                av.desc = "The default";
            }
            rv += av;
        }
        return rv;
    }

    #! Returns the text of a parameter value for a description
    private static string getValueText(auto val) {
        switch (val.typeCode()) {
            case NT_BOOLEAN:
                return val ? "True" : "False";
            case NT_STRING:
                return val;
        }
        return sprintf("%y", val);
    }
}
}
