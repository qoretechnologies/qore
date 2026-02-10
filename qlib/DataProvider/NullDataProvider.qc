
# -*- mode: qore; indent-tabs-mode: nil -*-

/*
  Qorus Integration Engine

  Copyright (C) 2019 - 2024 Qore Technologies, s.r.o., all rights reserved

  **** NOTICE ****
    All information contained herein is, and remains the property of Qore
    Technologies, s.r.o. and its suppliers, if any.  The intellectual and
    technical concepts contained herein are proprietary to Qore Technologies,
    s.r.o. and its suppliers and may be covered by Czech, European, U.S. and
    Foreign Patents, patents in process, and are protected by trade secret or
    copyright law.  Dissemination of this information or reproduction of this
    material is strictly forbidden unless prior written permission is obtained
    from Qore Technologies, s.r.o..
*/

#! contains all public definitions in the DataProvider module
public namespace DataProvider {
#! A dummy request data provider that provides an empty hash as the request and response types
class NullDataProvider inherits AbstractDataProvider {
    public {
       #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "NullDataProvider",
            "supports_request": True,
        };
    }

    private {
        #! The default request and response type for this data provider
        static HashDataType type();
    }

    #! Creates the object
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", NOTHING, options);
    }

    #! Returns the name of this data provider (\c "NullRequest")
    string getName() {
        return "NullDataProvider";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "The null data provider is a data provider that provides a noop request-response API with no messages "
            "or data";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns an empty hash
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return {};
    }

    #! Returns an empty hash type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return type;
    }

    #! Returns an empty hash type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return type;
    }

    #! Returns @ref NOTHING
    private *hash<string, AbstractDataProviderType> getErrorResponseTypesImpl() {
    }
}
}
