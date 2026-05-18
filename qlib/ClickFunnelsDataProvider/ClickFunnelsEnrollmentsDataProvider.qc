# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsEnrollmentsDataProvider.qc ClickFunnels enrollments collection and actions

/*  ClickFunnelsEnrollmentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

public class ClickFunnelsEnrollmentsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "enrollments",
            "desc": "ClickFunnels course enrollments data provider",
            "type": "ClickFunnelsEnrollmentsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = ClickFunnelsDataProviderBase::WorkspaceIdSearchOption + {
            "course_id": <DataProviderOptionInfo>{
                "display_name": "Course ID",
                "short_desc": "The course to list enrollments for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The course ID to list enrollments for (required for listing)",
                "ref_data": "courses",
            },
            "enrollment_id": <DataProviderOptionInfo>{
                "display_name": "Enrollment ID",
                "short_desc": "Get a specific enrollment",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by enrollment ID for single record lookup",
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The cursor value for fetching the next page of results",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateEnrollmentDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateEnrollmentDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "enrollments"; }
    *string getDesc() { return "ClickFunnels course enrollments data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsEnrollmentDataType().getFields(); }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        setWorkspaceIdFromSearchOpts(search_options);
        if (search_options.enrollment_id) {
            auto result = doGet("courses/enrollments/" + search_options.enrollment_id);
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator((result,)), where_cond, search_options);
        }
        if (!search_options.course_id) {
            throw "CLICKFUNNELS-ERROR", "course_id is required to list enrollments";
        }
        hash<auto> params;
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages("courses/" + search_options.course_id + "/enrollments", params, search_options.limit);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Enrollment ---
public class ClickFunnelsCreateEnrollmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "course_id": { "type": StringType, "display_name": "Course ID", "short_desc": "The course to enroll in", "desc": "The ID of the course", "required": True, "ref_data": "courses", "example_value": "12345" },
            "contact_id": { "type": StringType, "display_name": "Contact ID", "short_desc": "The contact to enroll", "desc": "The ID of the contact to enroll", "required": True, "ref_data": "contacts", "example_value": "67890" },
            "suspended": { "type": SoftBoolOrNothingType, "display_name": "Suspended", "short_desc": "Whether enrollment is suspended", "desc": "Whether the enrollment should be suspended" },
            "suspension_reason": { "type": StringOrNothingType, "display_name": "Suspension Reason", "short_desc": "Reason for suspension", "desc": "The reason for suspending the enrollment", "example_value": "Awaiting payment confirmation" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateEnrollmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "create", "desc": "Create a course enrollment in ClickFunnels", "type": "ClickFunnelsCreateEnrollmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsEnrollmentDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateEnrollmentRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a course enrollment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateEnrollmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsEnrollmentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string course_id = remove req.course_id;
        return doPost("courses/" + course_id + "/enrollments", {"courses_enrollment": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Enrollment ---
public class ClickFunnelsUpdateEnrollmentRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "enrollment_id": { "type": StringType, "display_name": "Enrollment ID", "short_desc": "The enrollment to update", "desc": "The unique identifier of the enrollment to update", "required": True, "ref_data": "enrollments" },
            "suspended": { "type": SoftBoolOrNothingType, "display_name": "Suspended", "short_desc": "Whether enrollment is suspended", "desc": "Whether the enrollment should be suspended" },
            "completed": { "type": SoftBoolOrNothingType, "display_name": "Completed", "short_desc": "Whether enrollment is completed", "desc": "Whether the enrollment has been completed" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateEnrollmentDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a course enrollment in ClickFunnels", "type": "ClickFunnelsUpdateEnrollmentDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsEnrollmentDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateEnrollmentRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a course enrollment in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateEnrollmentRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsEnrollmentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string id = remove req.enrollment_id;
        return doPut("courses/enrollments/" + id, {"courses_enrollment": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
