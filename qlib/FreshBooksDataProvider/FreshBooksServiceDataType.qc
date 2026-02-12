# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksServiceDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks service record type
public class FreshBooksServiceDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks services
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The service ID",
                "desc": "The unique identifier of the service",
            },
            "business_id": {
                "type": IntOrNothingType,
                "display_name": "Business ID",
                "short_desc": "The business ID",
                "desc": "The ID of the business the service belongs to",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The service name",
                "desc": "The name of the service",
                "example_value": "Development",
            },
            "billable": {
                "type": SoftBoolType,
                "display_name": "Billable",
                "short_desc": "Whether the service is billable",
                "desc": "Whether the service is billable to clients",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The visibility state of the service (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks service create request type
public class FreshBooksServiceCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating services
        const Fields = FreshBooksDataProviderBase::BusinessIdField + {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The service name (required)",
                "desc": "The name of the service",
                "example_value": "Development",
                "required": True,
            },
            "billable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Billable",
                "short_desc": "Whether the service is billable",
                "desc": "Whether the service is billable to clients",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! FreshBooks service update request type
public class FreshBooksServiceUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating services
        const Fields = FreshBooksDataProviderBase::BusinessIdField + {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The service ID (required)",
                "desc": "The unique identifier of the service to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The service name",
                "desc": "The new name of the service",
                "example_value": "Development",
            },
            "billable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Billable",
                "short_desc": "Whether the service is billable",
                "desc": "Whether the service is billable to clients",
            },
            "vis_state": {
                "type": IntOrNothingType,
                "display_name": "Visibility State",
                "short_desc": "The visibility state",
                "desc": "The new visibility state of the service (0=active, 1=deleted, 2=archived)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
