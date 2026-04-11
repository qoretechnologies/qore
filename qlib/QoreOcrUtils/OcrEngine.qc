# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OcrEngine.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see OcrEngine
public namespace QoreOcrUtils {

#! Configuration for OcrEngine
public hashdecl OcrConfig {
    #! Mode: "native" for Tesseract binary module, "http" for HTTP endpoint
    string mode;
    #! Language code(s) for native mode (e.g., "eng", "deu+fra")
    string language = "eng";
    #! Tessdata directory path for native mode (empty = system default)
    string datapath = "";
    #! HTTP mode: base URL of the OCR service
    *string url;
    #! HTTP mode: optional Bearer token
    *string api_key;
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 120000
    int timeout_ms = 120000;
}

#! OCR extraction result
public hashdecl OcrResult {
    #! Extracted text
    string text;
    #! Overall confidence score (0.0 to 1.0)
    float confidence;
    #! Block-level results (native mode only)
    *list<hash<auto>> blocks;
}

#! Dual-mode OCR abstraction (native Tesseract + HTTP)
/** Provides a unified interface for OCR text extraction from images.

    @par Example: Native mode
    @code{.py}
OcrEngine engine(<OcrConfig>{"mode": "native", "language": "eng"});
hash<OcrResult> result = engine.extractText(image_data);
    @endcode

    @par Example: HTTP mode
    @code{.py}
OcrEngine engine(<OcrConfig>{"mode": "http", "url": "http://ocr-service:8080"});
hash<OcrResult> result = engine.extractText(image_data);
    @endcode

    @since QoreOcrUtils 1.0
*/
public class OcrEngine {
    private {
        #! Backend mode
        string mode;
%ifndef NoOcr
        #! Native Tesseract engine
        *Ocr::TesseractEngine native_engine;
%endif
        #! HTTP configuration
        *string url;
        *string api_key;
        int connect_timeout_ms;
        int timeout_ms;
%ifndef NoRestClient
        #! REST client (lazy-initialized)
        *RestClient rest_client;
        #! Mutex for thread-safe lazy initialization
        Mutex rest_mutex();
%endif
    }

    #! Creates an OcrEngine with the given configuration
    /** @param config an @ref OcrConfig hash

        @throw OCR-CONFIG-ERROR if mode is invalid or required parameters missing
        @throw OCR-DEPENDENCY-ERROR if required module is not available
    */
    constructor(hash<OcrConfig> config) {
        mode = config.mode;
        if (mode != "native" && mode != "http") {
            throw "OCR-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"native\" or \"http\"", mode);
        }

        if (mode == "native") {
%ifdef NoOcr
            throw "OCR-DEPENDENCY-ERROR",
                "ocr module is required for native mode but is not available";
%endif
%ifndef NoOcr
            native_engine = new Ocr::TesseractEngine(config.datapath, config.language);
%endif
        } else if (mode == "http") {
            if (!config.url.val()) {
                throw "OCR-CONFIG-ERROR", "url is required for HTTP mode";
            }
%ifdef NoRestClient
            throw "OCR-DEPENDENCY-ERROR",
                "RestClient module is required for HTTP mode but is not available";
%endif
            url = config.url;
            api_key = config.api_key;
            connect_timeout_ms = config.connect_timeout_ms;
            timeout_ms = config.timeout_ms;
        }
    }

    #! Extracts text from image binary data
    /** @param image_data raw image bytes (PNG, JPEG, TIFF, BMP, GIF)
        @return an @ref OcrResult hash with text, confidence, and optional blocks

        @throw OCR-ERROR if extraction fails
    */
    hash<OcrResult> extractText(binary image_data) {
        if (mode == "native") {
            return extractTextNative(image_data);
        }
        if (mode == "http") {
            return extractTextHttp(image_data);
        }
        throw "OCR-ERROR", sprintf("unsupported mode %y", mode);
    }

    #! Extracts text via native Tesseract
    private hash<OcrResult> extractTextNative(binary image_data) {
%ifndef NoOcr
        hash<auto> meta = native_engine.extractTextWithMetadata(image_data);
        return <OcrResult>{
            "text": meta.text,
            "confidence": meta.confidence.toFloat(),
            "blocks": meta.blocks,
        };
%endif
        throw "OCR-ERROR", "ocr module not available";
    }

    #! Extracts text via HTTP OCR endpoint
    private hash<OcrResult> extractTextHttp(binary image_data) {
%ifndef NoRestClient
        ensureRestClient();

        # Send image as base64 in JSON body
        hash<auto> body = {
            "image": make_base64_string(image_data),
        };

        hash<auto> hdr = {"Content-Type": "application/json"};
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        try {
            resp = rest_client.post("/ocr", body, NOTHING, \hdr);
        } catch (hash<ExceptionInfo> ex) {
            throw "OCR-ERROR", sprintf("HTTP OCR request failed: %s", ex.desc), ex;
        }

        if (resp.body.typeCode() == NT_STRING) {
            resp.body = parse_json(resp.body);
        }

        return <OcrResult>{
            "text": resp.body.text ?? "",
            "confidence": (resp.body.confidence ?? 0).toFloat(),
        };
%endif
        throw "OCR-ERROR", "RestClient not available";
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
%ifndef NoRestClient
        if (!rest_client) {
            rest_mutex.lock();
            on_exit rest_mutex.unlock();
            if (!rest_client) {
                rest_client = new RestClient({
                    "url": url,
                    "connect_timeout": connect_timeout_ms,
                    "timeout": timeout_ms,
                });
            }
        }
%endif
    }
}

} # namespace QoreOcrUtils
