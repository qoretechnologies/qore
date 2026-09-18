# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockDataProvider root class

/** BedrockDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BedrockDataProvider module
public namespace BedrockDataProvider {
#! The Bedrock root data provider class
public class BedrockDataProvider inherits BedrockDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bedrock",
            "type": "BedrockDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object from a pre-built AwsRestClientIo
    /** @param rest AWS client whose credentials and connection manager are retained

        @throw AWSRESTCLIENT-ERROR if a routing client cannot be constructed from the supplied options
    */
    constructor(AwsRestClient::AwsRestClientIo rest) : BedrockDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    /** @param options provider and AWS authentication options

        @throw CONSTRUCTOR-ERROR if the options are invalid or omit both `aws_region` and `restclient`
        @throw AWSRESTCLIENT-ERROR if the AWS client options are invalid
    */
    constructor(*hash<auto> options) : BedrockDataProviderCommon(options) {
    }

    #! Returns the data provider name
    /** @return `bedrock` */
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        list<string> names = ("embed",);
        foreach RestSchemaActionSet actions in (BedrockSchema::getActionSets()) {
            names += actions.getActionNames();
        }
        return names;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == "embed") {
            return new BedrockEmbedDataProvider(rest);
        }
        *RestSchemaActionSet actions = BedrockSchema::findActionSet(name);
        if (!actions) {
            return;
        }
        return new RestSchemaActionDataProvider(actions, actions.getOperationEx(name), rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
