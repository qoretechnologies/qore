# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConvertDataProvider.qc ConvertAPI generic convert action data provider

/*  ConvertApiConvertDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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


#! Request fields for the source and destination formats of a converter
/** The fields also give the reference data of their action options: the destination formats depend on the source
    format, and are fetched again when it changes
*/
public const ConvertApiFormatFields = {
    "from": {
        "type": StringType,
        "display_name": "Source Format",
        "short_desc": "The source format of the converter",
        "desc": "The source format of the converter, for example `docx`, `html`, `pdf`, `jpg` or `eml`.\n\n"
            "The list of converters is in the [ConvertAPI documentation](https://www.convertapi.com/doc).",
        "example_value": "docx",
        "ref_data": "source_formats",
        "has_dependents": True,
        "on_change": ("refetch",),
    },
    "to": {
        "type": StringType,
        "display_name": "Destination Format",
        "short_desc": "The destination format of the converter",
        "desc": "The destination format of the converter, for example `pdf`, `jpg` or `txt`.\n\n"
            "Some converters are PDF tools, whose destination format names the operation, for example `pdf` to "
            "`merge`, `split`, `compress`, `protect`, `ocr` or `pdfa`.",
        "example_value": "pdf",
        "ref_data": "destination_formats",
        "depends_on": ("from",),
    },
};

#! Request type for the generic convert action
public class ConvertApiConvertRequestDataType inherits HashDataType {
    public {
        #! Field definitions: the formats first, as they are required, then the source fields, which form no
        #! required group for this action, then the output fields
        const Fields = ConvertApiFormatFields + ConvertApiAnySourceFields + ConvertApiOutputFields;
    }

    #! Creates the type
    constructor(string name = "ConvertApiConvertRequest") : HashDataType(name) {
        addQoreFields(Fields);
    }
}

#! Request type for the generic convert action that also takes converter parameters in other fields
/** This type is used to check a request when the converter's parameters cannot be read: the fields that the action
    does not know are sent as converter parameters, and ConvertAPI checks them.
*/
public class ConvertApiConvertAnyParameterRequestDataType inherits ConvertApiConvertRequestDataType {
    #! Creates the type
    constructor() : ConvertApiConvertRequestDataType("ConvertApiConvertAnyParameterRequest") {
        setDefaultOtherFieldType(AbstractDataProviderType::get(AutoType));
    }
}

#! ConvertAPI generic convert action data provider
/** Converts files with any ConvertAPI converter given by its source and destination formats.

    The action's options depend on the formats: when both are given, the converter's parameters are added as typed
    options, built from ConvertAPI's description of the converter (see getConverterRequestType()).
*/
public class ConvertApiConvertDataProvider inherits ConvertApiConversionDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "convert-file",
            "desc": "Converts files with any ConvertAPI converter, given by its source and destination formats",
            "type": "ConvertApiConvertDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiConvertRequestDataType RequestType();

        #! The request type for checking a request when the converter's parameters cannot be read
        static ConvertApiConvertAnyParameterRequestDataType AnyParameterRequestType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiConversionDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "convert-file";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Converts files with any ConvertAPI converter, given by its source and destination formats";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the request type for the converter given by the \c from and \c to option values
    /** @param options the option values given so far

        @return the request type with the converter's parameters as typed fields (see getConverterRequestType()),
        or the request type of the action if a format is not given yet or the converter's parameters cannot be read
    */
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return getConverterRequestType(options.from, options.to) ?? RequestType;
    }

    #! Returns the request type for the converter of a request, and checks the request with it
    /** A parameter that the converter requires is optional when the request gives it in its \c parameters field.

        @param req the request

        @return the request type with the converter's parameters as typed fields (see getConverterRequestType()); if
        the request gives no formats, the request type of the action; if the converter's parameters cannot be read,
        a request type that takes the fields that the action does not know as converter parameters, which ConvertAPI
        then checks (see @ref ConvertApiConvertAnyParameterRequestDataType)

        @throw TYPE-VALUE-ERROR a field of the request has a value that its type does not accept, or the request has
        a field that is neither a field of the action nor a parameter of the converter
    */
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        AbstractDataProviderType rv = RequestType;
        if (req.typeCode() == NT_HASH && req.from.typeCode() == NT_STRING && req.from.val()
            && req.to.typeCode() == NT_STRING && req.to.val()) {
            *hash<string, bool> given;
            if (req.parameters.typeCode() == NT_HASH) {
                given = map {$1.upr(): True}, keys req.parameters;
            }
            rv = getConverterRequestType(req.from, req.to, given) ?? AnyParameterRequestType;
        }
        if (exists req) {
            try {
                rv.acceptsValue(req);
            } catch (hash<ExceptionInfo> ex) {
                # the data can be partial
                if (ex.err != "MISSING-VALUE-ERROR") {
                    rethrow;
                }
            }
        }
        return rv;
    }

    #! Returns the options of the action for the option values given so far
    /** The registered options are returned as they are registered, with their order, preselection and dependencies,
        followed by the parameters of the converter when both formats are given.

        @param action the action
        @param options the option values given so far

        @return the action options
    */
    private *hash<string, hash<ActionOptionInfo>> tryGetActionOptionsForRequestWithOptionsImpl(
            hash<DataProviderActionInfo> action, *hash<auto> options) {
        *hash<string, hash<ActionOptionInfo>> rv =
            ConvertApiConversionDataProviderBase::tryGetActionOptionsForRequestWithOptionsImpl(action, options);
        if (!rv || !action.options) {
            return rv ?? action.options;
        }
        return action.options + (rv - keys action.options);
    }

    #! Returns the request type for a converter: the fields of the action with the converter's parameters
    /** The fields of the converter's parameters follow the source fields and precede the output fields; they have
        the ConvertAPI names of the parameters and are built from ConvertAPI's description of the converter (see
        @ref ConvertApiConverterParameters), which is cached (see getCachedConverterInfo()).

        @param from the source format
        @param to the destination format
        @param given the names of the parameters given in the \c parameters field of a request, in upper case; a
        parameter that the converter requires is optional when it is given there

        @return the request type, or @ref nothing if a format is not given or the converter's description cannot be
        read; in that case the error is logged, and a conversion reports an unknown converter itself
    */
    *HashDataType getConverterRequestType(auto from, auto to, *hash<string, bool> given) {
        if (from.typeCode() != NT_STRING || !from.val() || to.typeCode() != NT_STRING || !to.val()) {
            return;
        }
        hash<auto> converter;
        try {
            converter = getCachedConverterInfo(from, to);
        } catch (hash<ExceptionInfo> ex) {
            logCaughtError("%s (%s): the parameters of the selected converter cannot be read, so they are not "
                "offered as typed options: %s: %s", getName(), self.className(), ex.err, ex.desc);
            return;
        }
        HashDataType rv(sprintf("ConvertApiConvertRequest-%s-%s", from.lwr(), to.lwr()));
        rv.addQoreFields(ConvertApiFormatFields + ConvertApiAnySourceFields
            + ConvertApiConverterParameters::getFields(converter, given,
                keys ConvertApiConvertRequestDataType::Fields)
            + ConvertApiOutputFields);
        return rv;
    }

    #! Returns the source and destination formats
    private hash<auto> getFormats(hash<auto> req) {
        return req{"from", "to"};
    }

    #! Returns the request fields that are not converter parameters
    private *list<string> getNonParameterFields() {
        return ("from", "to");
    }

    #! Returns @ref True for a request without a source file or URL to a converter that takes no source file
    /** Converters such as \c data to \c qrcode take their input in a converter parameter (\c Data); the converter
        list is only checked when the request gives no source file or URL
    */
    private bool inputInParameters(hash<auto> formats, hash<auto> req) {
        return !ConvertApiDataProviderBase::hasInput(req) && !takesFileInput(formats.from, formats.to);
    }

    #! Returns the name of the parameter for the \c url field
    /** The \c html converters render the web page given with the \c Url parameter; all other converters download
        the source file from the URL
    */
    private *string getUrlParameter(hash<auto> req) {
        if (req.from.lwr() == "html") {
            return "Url";
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
