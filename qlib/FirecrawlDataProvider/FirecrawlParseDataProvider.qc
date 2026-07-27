# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlParseDataProvider.qc Firecrawl parse action data provider

/*  FirecrawlParseDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Firecrawl parse action data provider
/** Sends a `multipart/form-data` POST to `/parse` with two parts:
    - `file` — the binary file content (with filename + MIME type)
    - `options` — a JSON-encoded object holding the parse options

    The provider handles base64→binary conversion of `req.file.content`,
    builds the multipart envelope manually, and invokes the connection
    manager directly so that the high-level REST encoding machinery does
    not interfere with the binary body.
*/
public class FirecrawlParseDataProvider inherits FirecrawlDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "parse",
            "desc": "Uploads a local document (PDF, DOCX, XLSX, HTML, etc.) and converts "
                "it to clean, LLM-ready data",
            "type": "FirecrawlParseDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! File extensions accepted by the Firecrawl /parse endpoint
        const SupportedExtensions = ("html", "htm", "pdf", "docx", "doc", "odt", "rtf",
            "xlsx", "xls");
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : FirecrawlDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "parse";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Uploads a local document (PDF, DOCX, XLSX, HTML, etc.) and converts it to "
            "clean, LLM-ready data";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new FirecrawlParseRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FirecrawlParseResponseDataType();
    }

    #! Performs the parse request as a multipart upload
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Pull file out and treat the rest of req as the options JSON payload
        hash<auto> file = req.file;
        hash<auto> options = req - "file";
        # Drop NOTHING-valued option keys so the JSON body stays compact
        options = map {$1.key: $1.value}, options.pairIterator(), exists $1.value;

        string filename = file.name ?? "upload";
        *string ext;
        if (filename =~ /\.[^.\/]+$/) {
            ext = (filename =~ x/\.([^.\/]+)$/)[0].lwr();
        }
        if (!ext.val() || !inlist(ext, SupportedExtensions)) {
            throw "FIRECRAWL-UNSUPPORTED-FILE",
                sprintf("file %y has unsupported extension %y; Firecrawl /parse supports: %y",
                    filename, ext ?? "(none)", SupportedExtensions);
        }
        string mime_type = (file.mime_type.val() && file.mime_type != "auto")
            ? file.mime_type
            : (filename.val() ? get_mime_type_from_ext(filename) : NOTHING)
                ?? "application/octet-stream";
        binary content = file.content.typeCode() == NT_BINARY
            ? file.content
            : binary(parse_base64_string(file.content));

        string boundary = replace(make_base64_string(string(now_us())), "=", "");
        binary body = binary();
        body += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"file\"; "
            "filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
            boundary, filename, mime_type));
        body += content;
        if (options) {
            body += binary(sprintf("\r\n--%s\r\nContent-Disposition: form-data; "
                "name=\"options\"\r\nContent-Type: %s\r\n\r\n%s",
                boundary, MimeTypeJson, make_json(options)));
        }
        body += binary(sprintf("\r\n--%s--\r\n", boundary));

        # Lower-case the default headers (Authorization etc.) and override Content-Type
        hash<auto> hdr = map {$1.key.lwr(): $1.value}, rest.getDefaultHeaders().pairIterator();
        hdr."content-type" = sprintf("%s; boundary=\"%s\"", MimeTypeMultipartFormData, boundary);
        hdr."accept" = MimeTypeJson;

        # Build path with API base prefix (typically "/v2")
        *string base = rest.getConnectionPath();
        string full_path;
        if (base.val() && base != "/") {
            base =~ s/\/+$//;
            if (base !~ /^\//) {
                base = "/" + base;
            }
            full_path = base + "/parse";
        } else {
            full_path = "/parse";
        }

        int retries = 0;
        while (True) {
            try {
                hash<auto> raw = rest.getConnectionManager().request(rest.getUrl(),
                    "POST", full_path, hdr, body);
                int status = raw.status_code;
                *hash<auto> parsed;
                if (raw.body.typeCode() == NT_STRING && raw.body.val()) {
                    try {
                        parsed = parse_json(raw.body);
                    } catch (hash<ExceptionInfo> ex) {
                        # leave parsed as NOTHING; fall through to error or raw return
                    }
                }
                if (status >= 400) {
                    *string formatted = formatFirecrawlBody(parsed);
                    string desc = sprintf("HTTP %d from POST %s", status, full_path);
                    if (formatted.val()) {
                        desc += ": " + formatted;
                    }
                    throw "FIRECRAWL-API-ERROR", desc, parsed ?? raw.body;
                }
                checkResponse(parsed);
                return parsed ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FirecrawlDataProvider
