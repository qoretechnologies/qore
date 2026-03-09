# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformCreateSubmissionDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Data provider for creating JotForm submissions via request API
public class JotformCreateSubmissionDataProvider inherits JotformDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-submission",
            "desc": "Create a new submission in JotForm",
            "type": "JotformCreateSubmissionDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the submission data to create
        static JotformSubmissionCreateDataType RequestType();

        #! Response type - the created submission
        static JotformSubmissionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest) : JotformDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn) : JotformDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-submission";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new submission in JotForm";
    }

    #! Makes a request and returns the response
    /** @param req the request data (submission fields to create)
        @param request_options the request options

        @return the created submission data

        The request contains formID and all form fields at the top level.
        We extract formID and send the remaining fields as submission data.

        JotForm expects submission data in form-encoded format with keys like:
        - submission[qid] for simple fields
        - submission[qid_first], submission[qid_last] for composite fields

        @see https://api.jotform.com/docs/#post-form-id-submissions
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.formID.val(), "formID is required to create a submission");

        # Extract plain value if formID comes from ref_data
        string form_id = req.formID.value ?? req.formID;

        # Extract submission data - everything except formID
        hash<auto> submission_data = req - "formID";

        # Handle legacy format where submission data is nested
        if (submission_data.submission && submission_data.size() == 1) {
            submission_data = submission_data.submission;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating submission for form %s with raw data: %y",
            form_id, submission_data));

        # Transform to JotForm's expected format: submission[qid_subfield] = value
        hash<auto> transformed_data = transformSubmissionData(submission_data, form_id);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: transformed data: %y", transformed_data));

        # JotForm requires form-encoded POST for submissions, not JSON
        hash<auto> response = doPostFormEncoded(sprintf("/form/%s/submissions", form_id), transformed_data);

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: create response=%y", response));

        # Response contains the new submission ID - extract it
        *string submission_id;
        if (response.content) {
            if (response.content.typeCode() == NT_STRING) {
                # Old format: plain string ID
                submission_id = response.content;
            } else if (response.content.typeCode() == NT_HASH) {
                # New format: {submissionID: "...", URL: "..."}
                submission_id = response.content.submissionID;
            }
        }

        # Fetch the created submission to return full data
        if (submission_id) {
            hash<auto> getResponse = doGet(sprintf("/submission/%s", submission_id));
            return getResponse.content;
        }

        # Return whatever content we got if we couldn't extract the ID
        return response.content;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the request type based on the actual request data
    /** @param req the request data containing formID
        @return a dynamic request type with form-specific fields

        This method is called during request validation. We extract formID from the
        request and return a dynamic type that allows the form-specific fields.
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string form_id;
        if (req.typeCode() == NT_HASH) {
            form_id = req.formID.value ?? req.formID;
        }
        if (!form_id) {
            return RequestType;
        }
        return buildDynamicRequestType(form_id);
    }

    #! Returns the request type with dynamic form fields based on formID option
    /** @param options the options hash containing formID
        @return a dynamic request type with form-specific fields
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        *string form_id = options.formID.value ?? options.formID;
        if (!form_id) {
            # No form selected yet, return base type
            return RequestType;
        }

        # Build dynamic type with form-specific fields
        return buildDynamicRequestType(form_id);
    }

    #! Builds a dynamic request type with form-specific submission fields
    /** The form fields are returned at the top level of the request type, so they
        appear as individual options in the UI. This is the expected pattern for
        dynamic options - each field becomes its own option.
    */
    private AbstractDataProviderType buildDynamicRequestType(string form_id) {
        # Get form questions from API
        hash<auto> response = doGet(sprintf("/form/%s/questions", form_id));

        # Build fields hash starting with formID
        hash<string, AbstractDataField> fields = {
            "formID": new QoreDataField({
                "name": "formID",
                "display_name": "Form",
                "short_desc": "The form to submit to",
                "desc": "The form ID for the submission",
                "type": AbstractDataProviderTypeMap."string",
            }),
        };

        # Add each form question as a top-level field
        if (response.content) {
            foreach string qid in (keys response.content) {
                hash<auto> question = response.content{qid};
                string field_name = question.name ?? qid;
                string display_name = question.text ?? field_name;
                AbstractDataProviderType field_type = getFieldTypeForQuestion(question);

                fields{field_name} = new QoreDataField({
                    "name": field_name,
                    "display_name": display_name,
                    "short_desc": question.type ?? "field",
                    "desc": sprintf("%s (Question ID: %s)", display_name, qid),
                    "type": field_type,
                });
            }
        }

        return new HashDataType("CreateSubmissionRequest", fields);
    }

    #! Returns the appropriate Qore type for a JotForm question type
    /** Delegates to the base class method for consistent type mapping.
    */
    private AbstractDataProviderType getFieldTypeForQuestion(hash<auto> question) {
        return JotformDataProviderBase::getFieldTypeForQuestion(question);
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
