# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformDeleteSubmissionDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! Delete submission request type
public class JotformDeleteSubmissionRequestDataType inherits HashDataType {
    public {
        #! Field definitions for delete request
        const Fields = {
            "submissionID": {
                "type": StringType,
                "display_name": "Submission ID",
                "short_desc": "The submission ID to delete",
                "desc": "The ID of the submission to delete",
                "example_value": "5432109876543210987",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Delete submission response type
public class JotformDeleteSubmissionResponseDataType inherits HashDataType {
    public {
        #! Field definitions for delete response
        const Fields = {
            "success": {
                "type": BoolType,
                "display_name": "Success",
                "short_desc": "Whether the deletion was successful",
                "desc": "Indicates if the submission was successfully deleted",
                "example_value": True,
            },
            "message": {
                "type": StringOrNothingType,
                "display_name": "Message",
                "short_desc": "Response message",
                "desc": "A message describing the result of the deletion",
                "example_value": "Submission deleted successfully",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting JotForm submissions via request API
public class JotformDeleteSubmissionDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-submission",
            "desc": "Delete a submission from JotForm",
            "type": "JotformDeleteSubmissionDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the submission ID to delete
        static JotformDeleteSubmissionRequestDataType RequestType();

        #! Response type - delete confirmation
        static JotformDeleteSubmissionResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest) : JotformDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn) : JotformDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-submission";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a submission from JotForm";
    }

    #! Makes a request and returns the response
    /** @param req the request data (submission ID to delete)
        @param request_options the request options

        @return confirmation of deletion
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.submissionID.val(), "submissionID is required to delete a submission");

        # Extract plain value if submissionID comes from ref_data
        string submission_id = req.submissionID.value ?? req.submissionID;

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: deleting submission %s", submission_id));

        hash<auto> response = doDelete(sprintf("/submission/%s", submission_id));

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: delete response=%y", response));

        return {
            "success": True,
            "message": response.message ?? "Submission deleted successfully",
        };
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace JotformDataProvider
