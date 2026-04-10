# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsRestClientDataProvider class definition

/** AwsRestClientDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated doContent-Typecumentation files (the "Software"),
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

#! contains all public definitions in the AwsRestClientDataProvider module
public namespace AwsRestClientDataProvider {
#! The AWS REST client data provider class, provides API classes as children
public class AwsRestClientDataProvider inherits AwsRestClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "AwsRestClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor arguments
        const ConstructorOptions = RestClientDataProvider::ConstructorOptions + {
            "data": <DataProviderOptionInfo>{
                "display_name": "Data Serialization",
                "short_desc": "The data serialization to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Data serialization options are limited to `json` with this object",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "json",
                        "desc": "use JSON serialization",
                    },
                ),
                "default_value": "json",
            },
            "aws_keyid": <DataProviderOptionInfo>{
                "display_name": "AWS Key ID",
                "short_desc": "The AWS key ID",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "AWS key ID",
            },
            "aws_secret": <DataProviderOptionInfo>{
                "display_name": "AWS Secret",
                "short_desc": "The AWS secret access key value",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The AWS secret access key value",
                "sensitive": True,
            },
            "aws_region": <DataProviderOptionInfo>{
                "display_name": "AWS Region",
                "short_desc": "The AWS region to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The AWS region to use (ex: `us-east-1`)",
            },
            "aws_service": <DataProviderOptionInfo>{
                "display_name": "AWS Service",
                "short_desc": "The AWS service to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The AWS service to use (ex: `iam`)",
                "example_value": "iam",
            },
            "aws_s3": <DataProviderOptionInfo>{
                "display_name": "AWS S3",
                "short_desc": "Flag for use with AWS S3",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Set to `True` to flag this object for use with AWS S3, which requires special "
                    "message encoding",
                "default_value": False,
            },
            "aws_token": <DataProviderOptionInfo>{
                "display_name": "AWS Token",
                "short_desc": "A temporary session token",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A temporary session token from AWS Security Token Service for this HTTP session",
            },
        };
    }

    private {
        const ChildMap = {
            "call": Class::forName("AwsRestClientDataProvider::AwsRestClientCallDataProvider"),
            "delete": Class::forName("AwsRestClientDataProvider::AwsRestClientDeleteDataProvider"),
            "get": Class::forName("AwsRestClientDataProvider::AwsRestClientGetDataProvider"),
            "head": Class::forName("AwsRestClientDataProvider::AwsRestClientHeadDataProvider"),
            "options": Class::forName("AwsRestClientDataProvider::AwsRestClientOptionsDataProvider"),
            "path": Class::forName("AwsRestClientDataProvider::AwsRestClientPatchDataProvider"),
            "post": Class::forName("AwsRestClientDataProvider::AwsRestClientPostDataProvider"),
            "put": Class::forName("AwsRestClientDataProvider::AwsRestClientPutDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = getAwsRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(AwsRestClientIo rest) : AwsRestClientDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "rest";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for REST servers";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a REST connection
    static AwsRestClientIo getAwsRestConnection(hash<auto> options) {
        options = RestClientDataProvider::processRestOptions(options);
        return new AwsRestClientIo(options);
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}