# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiConversionDataProviderBase.qc ConvertAPI conversion action base class

/*  ConvertApiConversionDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Base class for ConvertAPI conversion action data providers
/** Subclasses return the converter's source and destination formats with getFormats(); the common request
    fields (@ref ConvertApiCommonFields) are handled here, and all other request fields are sent as converter
    parameters with their names as given.
*/
public class ConvertApiConversionDataProviderBase inherits ConvertApiDataProviderBase {
    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiDataProviderBase(rest) {
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConvertApiConversionResponseDataType();
    }

    #! Performs the conversion
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> formats = getFormats(req);
        hash<auto> common = req{ConvertApiCommonFieldNames};
        hash<auto> converter_params = req - ConvertApiCommonFieldNames;
        *list<string> other = getNonParameterFields();
        if (other) {
            converter_params -= other;
        }
        return convert(formats.from, formats.to, common, converter_params, getUrlParameter(req));
    }

    #! Returns the request fields that are not converter parameters, other than the common fields
    private *list<string> getNonParameterFields() {
    }

    #! Returns the name of the parameter for the \c url request field, if the URL is not a source file URL
    /** By default, the \c url request field is sent as the URL of the source file

        @param req the request
    */
    private *string getUrlParameter(hash<auto> req) {
    }

    #! Returns the source and destination formats for the request as a hash with \c from and \c to keys
    abstract private hash<auto> getFormats(hash<auto> req);
}

} # namespace ConvertApiDataProvider
