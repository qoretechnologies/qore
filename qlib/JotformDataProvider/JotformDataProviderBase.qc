# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Base class for JotForm data providers
public class JotformDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for JotForm data providers
        const BaseConstructorOptions = {
            "jotformrestclient": <DataProviderOptionInfo>{
                "display_name": "JotForm REST Client",
                "short_desc": "The JotformRestClient object",
                "type": AbstractDataProviderType::get(new Type("JotformRestClient::JotformRestClient")),
                "desc": "The JotformRestClient object to use for API calls",
            },
            "jotformrestclient_options": <DataProviderOptionInfo>{
                "display_name": "JotForm REST Client Options",
                "short_desc": "Options for creating a JotformRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a JotformRestClient object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "JotForm API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The JotForm API key for authentication",
                "sensitive": True,
                "required": True,
            },
            "api_domain": <DataProviderOptionInfo>{
                "display_name": "API Domain",
                "short_desc": "JotForm API domain",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The JotForm API domain (api.jotform.com, eu-api.jotform.com, or hipaa-api.jotform.com)",
                "required": True,
                "default_value": JotformRestClient::JotformRestClient::DefaultApiDomain,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "api.jotform.com",
                        "desc": "Standard JotForm API for global users",
                    },
                    <AllowedValueInfo>{
                        "value": "eu-api.jotform.com",
                        "desc": "EU-based API for GDPR compliance",
                    },
                    <AllowedValueInfo>{
                        "value": "hipaa-api.jotform.com",
                        "desc": "HIPAA-compliant API for healthcare data",
                    },
                ),
            },
        };
    }

    private {
        #! The REST client object for API calls
        JotformRestClient::JotformRestClient rest;

        #! The connection object, if applicable
        *JotformRestClient::JotformRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(JotformRestClient::JotformRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.jotformrestclient) {
            rest = copts.jotformrestclient;
        } else {
            hash<auto> client_opts = copts.jotformrestclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            if (copts.api_domain) {
                client_opts.api_domain = copts.api_domain;
            }
            rest = new JotformRestClient::JotformRestClient(client_opts);
        }
    }

    #! Returns the REST client
    JotformRestClient::JotformRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *JotformRestClient::JotformRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the JotForm API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw JOTFORM-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doGet: GET %s with params: %y", path, params));
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doGet: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the JotForm API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw JOTFORM-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPost: POST %s with body: %y", path, body));
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPost: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a DELETE request to the JotForm API
    /** @param path the API path
        @return the response data

        @throw JOTFORM-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doDelete: DELETE %s", path));
        hash<auto> response = rest.del(path);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doDelete: success"));
        return response.body;
    }

    #! Checks the JotForm API response for errors
    /** @param response the API response hash

        @throw JOTFORM-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # JotForm API returns responseCode in the body
        if (response.body.responseCode.val() && response.body.responseCode != 200) {
            string err_msg = response.body.message ?? "Unknown JotForm API error";
            @debug(log(LoggerLevel::DEBUG, "checkResponse: JotForm API error: %s (code: %y)",
                err_msg, response.body.responseCode));
            throw "JOTFORM-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.apikey) {
            rv.apikey = copts.apikey;
        }
        if (copts.api_domain) {
            rv.api_domain = copts.api_domain;
        }
        foreach string key in (keys copts.jotformrestclient_options) {
            rv{key} = copts.jotformrestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "forms": True,
            "form_questions": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "forms": return getReferenceForms();
            case "form_questions": return getReferenceFormQuestions(action_opts.formID);
        }
    }

    #! Returns available form questions for reference data (for dropdown selections)
    private *list<hash<AllowedValueInfo>> getReferenceFormQuestions(*string formID) {
        if (!formID) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doGet(sprintf("/form/%s/questions", formID));
        if (response.content) {
            foreach string qid in (keys response.content) {
                hash<auto> question = response.content{qid};
                string display_name = question.text ?? question.name ?? sprintf("Question %s", qid);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": question.type ?? "field",
                    "desc": sprintf("%s (ID: %s, Type: %s)", display_name, qid, question.type ?? "unknown"),
                    "value": qid,
                };
            }
        }
        return rv;
    }

    #! Returns form questions as a hash type for dynamic submission options
    /** @param formID the form ID to get questions for
        @return a HashDataType with fields for each form question
    */
    AbstractDataProviderType getFormQuestionsType(*string formID) {
        if (!formID) {
            # Return a generic hash type if no form is selected
            return AbstractDataProviderTypeMap."hash";
        }

        hash<auto> response = doGet(sprintf("/form/%s/questions", formID));
        if (!response.content) {
            return AbstractDataProviderTypeMap."hash";
        }

        # Build fields hash from form questions
        hash<string, AbstractDataField> fields;
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

        return new HashDataType("SubmissionData", fields);
    }

    #! Returns the appropriate Qore type for a JotForm question type
    /** @param question the JotForm question definition hash
        @return the appropriate data provider type for the question

        This method maps JotForm control types to proper structured data types.
        Complex fields like fullname, address, and phone have their own data types
        with properly defined subfields.

        This is a static method so it can be called from child classes without
        needing to access instance members.
    */
    static AbstractDataProviderType getFieldTypeForQuestion(hash<auto> question) {
        switch (question.type) {
            # Numeric types
            case "control_number":
            case "control_spinner":
            case "control_scale":
            case "control_rating":
                return AbstractDataProviderTypeMap."*int";

            # Boolean types
            case "control_checkbox":
                # Checkboxes can have multiple selections, return as list of strings
                return AbstractDataProviderTypeMap."*softlist<string>";
            case "control_yesno":
                return AbstractDataProviderTypeMap."*bool";

            # Complex structured types - use proper data types with defined subfields
            case "control_fullname":
                return new JotformFullNameDataType();
            case "control_address":
                return new JotformAddressDataType();
            case "control_phone":
                return new JotformPhoneDataType();
            case "control_email":
                # Email can be simple string or structured (with confirmation)
                # Check if confirmation is enabled
                if (question.confirmEmail == "Yes") {
                    return new JotformEmailDataType();
                }
                return AbstractDataProviderTypeMap."*string";
            case "control_datetime":
                return new JotformDateTimeDataType();
            case "control_birthdate":
                return new JotformBirthDateDataType();
            case "control_time":
                return new JotformTimeDataType();
            case "control_signature":
                return new JotformSignatureDataType();
            case "control_payment":
                return new JotformPaymentDataType();
            case "control_matrix":
                # Matrix fields have dynamic structure based on form configuration
                return new JotformMatrixDataType();

            # File uploads are typically URLs or base64 data
            case "control_fileupload":
                return AbstractDataProviderTypeMap."*string";

            # List types for multi-select fields
            case "control_dropdown":
            case "control_radio":
                # Single selection - string
                return AbstractDataProviderTypeMap."*string";

            default:
                # Most fields are strings (textbox, textarea, etc.)
                return AbstractDataProviderTypeMap."*string";
        }
    }

    #! Transforms submission data from our format to JotForm's expected format
    /** JotForm expects submission data in form-encoded format with keys like:
        - submission[qid] for simple fields
        - submission[qid_first], submission[qid_last] for composite fields like fullname
        - submission[qid_addr_line1], submission[qid_city] for address fields

        Our format uses field names like "q2_fullname0" where "2" is the question ID.

        @param data the submission data hash with our field names
        @param form_id the form ID to get question definitions for field name to qid mapping

        @return a hash with JotForm-formatted submission keys
    */
    hash<auto> transformSubmissionData(hash<auto> data, string form_id) {
        # Get form questions to build field name to qid mapping
        hash<auto> response = doGet(sprintf("/form/%s/questions", form_id));
        if (!response.content) {
            # No questions, return data as-is (shouldn't happen)
            return data;
        }

        # Build field name to qid mapping
        hash<string, string> name_to_qid;
        foreach string qid in (keys response.content) {
            hash<auto> question = response.content{qid};
            string field_name = question.name ?? qid;
            name_to_qid{field_name} = qid;
        }

        @debug(log(LoggerLevel::DEBUG, "transformSubmissionData: name_to_qid mapping: %y", name_to_qid));

        # Transform the data
        hash<auto> result;
        foreach string key in (keys data) {
            auto value = data{key};

            # Get the qid for this field name
            *string qid = name_to_qid{key};
            if (!qid) {
                # Field not found in questions, try to extract qid from field name pattern q{N}_{name}
                if (key =~ /^q(\d+)_/) {
                    qid = (key =~ x/^q(\d+)_/)[0];
                } else {
                    # Unknown field, pass through as-is
                    result{sprintf("submission[%s]", key)} = value;
                    continue;
                }
            }

            if (value.typeCode() == NT_HASH) {
                # Composite field - flatten subfields
                foreach string subkey in (keys value) {
                    auto subvalue = value{subkey};
                    result{sprintf("submission[%s_%s]", qid, subkey)} = subvalue;
                }
            } else {
                # Simple field
                result{sprintf("submission[%s]", qid)} = value;
            }
        }

        @debug(log(LoggerLevel::DEBUG, "transformSubmissionData: result: %y", result));
        return result;
    }

    #! Makes a form-encoded POST request to the JotForm API
    /** JotForm submission endpoints require form-encoded data, not JSON.

        IMPORTANT: JotForm requires brackets [ and ] in keys to be sent as literal characters,
        NOT URL-encoded. If brackets are encoded as %5B and %5D, field values will not be saved.

        @param path the API path
        @param body the request body hash with submission[qid] keys
        @return the response data

        @throw JOTFORM-API-ERROR if the response indicates an error
    */
    hash<auto> doPostFormEncoded(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: POST %s with body: %y", path, body));

        # Build form-encoded body string manually
        # IMPORTANT: JotForm requires literal brackets [ ] in submission keys
        # URL-encoding them to %5B and %5D causes field values to not be saved
        # RestClient's "url" serialization encodes brackets, so we build manually
        string encoded_body = "";
        if (body) {
            list<string> parts;
            foreach string key in (keys body) {
                auto val = body{key};
                if (exists val) {
                    # Only encode the value, keep key brackets as literal characters
                    parts += sprintf("%s=%s", key, encode_url(string(val), True));
                }
            }
            encoded_body = parts.join("&");
        }

        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: encoded body: %s", encoded_body));

        # Set Content-Type header and send as raw text
        hash<auto> headers = {
            "Content-Type": "application/x-www-form-urlencoded",
        };

        # Save current serialization and switch to "text" to send raw body
        # This prevents RestClient from serializing the body (e.g., JSON-encoding it)
        string old_serialization = rest.getSerialization();
        rest.setSerialization("text");
        on_exit rest.setSerialization(old_serialization);

        # rest.post signature: post(path, body, *reference<hash<auto>> info, *hash<auto> hdr)
        hash<auto> response = rest.post(path, encoded_body, NOTHING, headers);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPostFormEncoded: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Returns available forms for reference data
    private *list<hash<AllowedValueInfo>> getReferenceForms() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = doGet("/user/forms", {"limit": DefaultPageSize});
        if (response.content) {
            foreach hash<auto> form in (response.content) {
                string display_name = form.title ?? sprintf("Form %s", form.id);
                string desc = sprintf("%s (ID: %s)", display_name, form.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": display_name,
                    "desc": desc,
                    "value": form.id,
                };
            }
        }
        return rv;
    }
}

} # namespace JotformDataProvider
