# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestClientDataProvider module definition

/** GenericApiCallRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Constant type for the framework-injected generic "Make an API call" action
public const GenericApiCallRequestDataType = new GenericApiCallRequestDataType();

#! Request type used by @ref GenericApiCallProvider, the framework-injected generic API call provider
/** This type is purposely separate from @ref RestClientCallWithMethodRequestDataType so the framework
    auto-injection can expose a richer schema (body-type selector, path-variable templating,
    response-status assertions) without altering the standalone `RestClientDataProvider/call` action.

    The @c body field's effective type is data-dependent - see
    @ref GenericApiCallProvider::getRequestTypeWithDataImpl() and
    @ref GenericApiCallProvider::getRequestTypeWithOptionsImpl(). The static declaration here uses
    @c AutoType so that off-the-shelf consumers see an unconstrained value; the provider's dynamic
    methods refine the type to a typed shape based on the selected @c body_type at form-render time.

    @since RestClientDataProvider 2.4
*/
public class GenericApiCallRequestDataType inherits HashDataType {
    public {
        #! Allowed values for the body_type field; each value also drives Content-Type defaults
        const BodyTypeAllowedValues = (
            <AllowedValueInfo>{
                "display_name": "Auto",
                "short_desc": "Pass-through; use connection serialization and let the REST client decide",
                "desc": "Pass the body to the REST client unchanged; the client decides serialization based on "
                    "its configured `data` option and the value type",
                "value": "auto",
            },
            <AllowedValueInfo>{
                "display_name": "JSON",
                "short_desc": "Serialize body as JSON",
                "desc": "Serialize the body as JSON; sets `Content-Type: application/json`. The body should "
                    "be a hash, list, or scalar value.",
                "value": "json",
            },
            <AllowedValueInfo>{
                "display_name": "Form (URL-encoded)",
                "short_desc": "Serialize body as application/x-www-form-urlencoded form fields",
                "desc": "Serialize a hash of form fields as `application/x-www-form-urlencoded`. The body must "
                    "be a hash whose values are scalars (strings/numbers/booleans).",
                "value": "form",
            },
            <AllowedValueInfo>{
                "display_name": "Multipart Form",
                "short_desc": "Upload a file using multipart/form-data",
                "desc": "Upload a file using `multipart/form-data`. The body must be a "
                    "@ref DataProvider::FileDataType \"FileDataType\" structure (filename + content + "
                    "mime_type). The framework builds the multipart envelope and sets the boundary header.",
                "value": "multipart",
            },
            <AllowedValueInfo>{
                "display_name": "XML",
                "short_desc": "Send body as XML",
                "desc": "Send the body as XML. If the body is a hash it is serialized to XML; if it is a "
                    "string it is sent verbatim. Sets `Content-Type: application/xml`.",
                "value": "xml",
            },
            <AllowedValueInfo>{
                "display_name": "YAML",
                "short_desc": "Send body as YAML",
                "desc": "Send the body as YAML. If the body is a hash it is serialized to YAML; if it is a "
                    "string it is sent verbatim. Sets `Content-Type: application/x-yaml`.",
                "value": "yaml",
            },
            <AllowedValueInfo>{
                "display_name": "Plain Text",
                "short_desc": "Send body as text/plain",
                "desc": "Send the body as plain text. The body must be a string. Sets "
                    "`Content-Type: text/plain`.",
                "value": "text",
            },
            <AllowedValueInfo>{
                "display_name": "Raw Binary",
                "short_desc": "Send body as raw bytes",
                "desc": "Send the body as raw bytes. The body must be a binary value. Sets "
                    "`Content-Type: application/octet-stream` unless overridden by `hdr`.",
                "value": "binary",
            },
        );

        #! Field descriptions
        const Fields = {
            "method": {
                "display_name": "HTTP Method",
                "type": StringType,
                "desc": "The HTTP method to use for the request",
                "required": True,
                "default_value": "GET",
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "GET",    "short_desc": "HTTP GET",    "desc": "HTTP `GET`",    "value": "GET"},
                    <AllowedValueInfo>{"display_name": "POST",   "short_desc": "HTTP POST",   "desc": "HTTP `POST`",   "value": "POST"},
                    <AllowedValueInfo>{"display_name": "PUT",    "short_desc": "HTTP PUT",    "desc": "HTTP `PUT`",    "value": "PUT"},
                    <AllowedValueInfo>{"display_name": "PATCH",  "short_desc": "HTTP PATCH",  "desc": "HTTP `PATCH`",  "value": "PATCH"},
                    <AllowedValueInfo>{"display_name": "DELETE", "short_desc": "HTTP DELETE", "desc": "HTTP `DELETE`", "value": "DELETE"},
                    <AllowedValueInfo>{"display_name": "HEAD",   "short_desc": "HTTP HEAD",   "desc": "HTTP `HEAD`",   "value": "HEAD"},
                    <AllowedValueInfo>{"display_name": "OPTIONS","short_desc": "HTTP OPTIONS","desc": "HTTP `OPTIONS`","value": "OPTIONS"},
                    <AllowedValueInfo>{"display_name": "TRACE",  "short_desc": "HTTP TRACE",  "desc": "HTTP `TRACE`",  "value": "TRACE"},
                ),
            },
            "path": {
                "display_name": "URI Path",
                "type": StringType,
                "desc": "The URI path relative to the connection's API base URL. May contain `{name}` "
                    "placeholders that are replaced from `path_vars`. Example: `/users/{id}/comments`.",
                "default_value": "/",
            },
            "path_vars": {
                "display_name": "Path Variables",
                "short_desc": "Values for {name} placeholders in path",
                "type": AutoHashOrNothingType,
                "desc": "Hash of values substituted into `{name}` placeholders in the URI path. "
                    "Example: with `path = /users/{id}` and `path_vars = {id: 123}` the framework sends "
                    "`/users/123`. Values are URL-encoded.",
                "example_value": {"id": "123"},
            },
            "query_args": {
                "display_name": "Query Arguments",
                "type": AutoHashOrNothingType,
                "short_desc": "Query string parameters",
                "desc": "Hash of query string parameters to append to the URI path. Values are URL-encoded; "
                    "list values are joined with `,`.",
            },
            "body_type": {
                "display_name": "Body Type",
                "type": StringType,
                "short_desc": "How the body is serialized and what Content-Type is sent",
                "desc": "Selects the body serialization mode and the corresponding `Content-Type` header. "
                    "Changing this field reshapes the `body` editor - selecting JSON exposes a JSON editor, "
                    "selecting Multipart Form exposes a file picker, and so on.",
                # NOT marked required at the field level - Qorus's UI bucket-sort would split it
                # from `body` (which can't be required because GET/DELETE often have no body).
                # The default_value below means the form is still pre-filled with "auto".
                "default_value": "auto",
                "allowed_values": BodyTypeAllowedValues,
                # mark this as a controlling option for dynamic type resolution; the UI re-fetches the
                # request type when this value changes so the `body` field's editor adapts
                "structural_determinate": True,
                "has_dependents": True,
                "on_change": ("refetch",),
            },
            "body": {
                "display_name": "Body",
                "type": AutoType,
                "desc": "The request body. The effective type depends on `body_type`: JSON/form/YAML expect "
                    "a hash, XML/text expect a string, multipart expects a file structure, binary expects "
                    "raw bytes.",
                "depends_on": ("body_type",),
            },
            "hdr": {
                "display_name": "Custom Headers",
                "type": AutoHashOrNothingType,
                "short_desc": "Extra request headers",
                "desc": "Hash of custom headers added to the request. These override any default headers set "
                    "by `body_type` (e.g. supplying `Content-Type` here wins over the body-type default).",
            },
            "expected_status": {
                "display_name": "Expected Status Codes",
                "type": AutoListOrNothingType,
                "short_desc": "Response status codes treated as success",
                "desc": "Optional list of HTTP status codes treated as success. If set and the response "
                    "status is not in this list, the action throws `REST-STATUS-ERROR`. When unset, the "
                    "REST client's default behavior applies (typically: 2xx is success, non-2xx throws).",
                "example_value": (200, 201, 204),
            },
            "error_passthru": {
                "display_name": "Return Errors as Data",
                "type": SoftBoolOrNothingType,
                "short_desc": "Return non-2xx responses normally instead of throwing",
                "desc": "If `True`, non-2xx HTTP responses return as the normal response hash (with the "
                    "actual status code) instead of raising `REST-RESPONSE-ERROR`. Useful for handling "
                    "4xx/5xx as data - e.g. checking whether a resource exists. Overrides the "
                    "connection's `error_passthru` setting for this call only.",
            },
            "include_debug": {
                "display_name": "Include Debug Info",
                "type": SoftBoolOrNothingType,
                "short_desc": "Include request/timing/size debug info in the response",
                "desc": "If `True`, the response hash includes a `debug` sub-hash containing the request "
                    "method/URL/headers (with sensitive values redacted), request and response sizes, and "
                    "round-trip duration. Useful when investigating slow or failing calls. Off by default "
                    "to keep responses lean.",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Returns a copy of this type with the @c body field replaced by the given typed shape
    /** Used by @ref GenericApiCallProvider to surface a typed body editor in the UI based on the
        currently-selected @c body_type. The other fields are preserved verbatim.

        @param body_type one of the @ref BodyTypeAllowedValues values
        @return a copy of this type with the `body` field re-typed for the given body_type, or the
        original type for `body_type = "auto"` or unknown values
    */
    GenericApiCallRequestDataType withBodyType(*string body_type) {
        *AbstractDataProviderType new_body_type;
        switch (body_type) {
            case "json":
            case "form":
            case "yaml":
                # hash-shaped body (will be serialized in the requested format)
                new_body_type = AbstractDataProviderType::get(AutoHashOrNothingType);
                break;
            case "xml":
            case "text":
                # string body
                new_body_type = AbstractDataProviderType::get(SoftStringOrNothingType);
                break;
            case "binary":
                # raw bytes
                new_body_type = AbstractDataProviderType::get(SoftBinaryOrNothingType);
                break;
            case "multipart":
                # file structure (name + content + mime_type)
                new_body_type = FileDataType;
                break;
            case "auto":
            default:
                # unchanged
                return self;
        }
        GenericApiCallRequestDataType rv = copy();
        # replace the body field's type while keeping its display_name/desc
        AbstractDataField current = rv.getField("body");
        rv.replaceField(new QoreDataField({
            "name": "body",
            "display_name": current.getDisplayName(),
            "short_desc": current.getShortDescription(),
            "desc": current.getDescription(),
            "type": new_body_type,
            "depends_on": ("body_type",),
        }));
        return rv;
    }
}
}
