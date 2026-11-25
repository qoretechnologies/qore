# -*- mode: qore; indent-tabs-mode: nil -*-

/*  PdfOcrEngine.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see PdfOcrEngine
public namespace QoreOcrUtils {

#! Result from processing a single PDF page
public hashdecl PdfPageOcrResult {
    #! Zero-based page index
    int page_index;
    #! Extracted text (from text layer or OCR)
    string text;
    #! Whether OCR was used (True) or text was extracted from the digital layer (False)
    bool ocr_used;
    #! Page outcome: digital, ocr, blank (verified white PDF raster), partial, or unreadable
    string status;
    #! Diagnostic for a page requiring review
    *string error;
    #! Description of the diagnostic
    *string error_description;
    #! Digital text scores 1.0, recognized text uses the OCR score, and pages requiring review score 0.0
    float confidence;
}

#! Result from processing an entire PDF document
public hashdecl PdfOcrResult {
    #! Combined text from all pages
    string text;
    #! Per-page results
    list<hash<PdfPageOcrResult>> pages;
    #! Number of pages that required OCR
    int ocr_page_count;
    #! Total number of pages
    int total_pages;
}

#! Extracts text from PDF documents, using OCR for scanned pages
/** Processes PDF documents by first trying to extract the digital text layer
    from each page. If a page has little or no text (indicating it is a scanned
    image), the page is rasterized to a bitmap and OCR is applied.

    Requires the \c pdf module for PDF reading and rendering, and the \c ocr
    module for Tesseract OCR on scanned pages.

    @par Example: Extract text from a PDF with mixed digital and scanned pages
    @code{.py}
%modern
%requires QoreOcrUtils

PdfOcrEngine engine(<PdfOcrConfig>{
    "language": "eng",
    "dpi": 300,
    "scanned_threshold": 50,
});
binary pdf_data = ReadOnlyFile::readBinaryFile("contract.pdf");
hash<PdfOcrResult> result = engine.processPdf(pdf_data);
printf("Extracted %d pages (%d required OCR):\n%s\n",
    result.total_pages, result.ocr_page_count, result.text);
    @endcode
*/
public class PdfOcrEngine {
    private {
        #! OCR engine (reused across pages)
        OcrEngine ocr;
        #! DPI for rasterization
        int dpi;
        #! Page separator string
        string page_separator;
        #! Scanned page detector
        ScannedPdfDetector detector;
    }

    #! Creates a PdfOcrEngine
    /** @param config a @ref PdfOcrConfig hash

        @throw OCR-DEPENDENCY-ERROR if the pdf module cannot be loaded, or native mode needs an unavailable ocr module
    */
    constructor(*hash<PdfOcrConfig> config) {
        # Load in this module's program, including when the caller uses an AOT
        # DataProviderML. Image-only DocumentOcrEngine calls do not need pdf.
        try {
            load_module("pdf");
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "LOAD-MODULE-ERROR") {
                rethrow;
            }
            throw "OCR-DEPENDENCY-ERROR", sprintf("cannot load the pdf module: %s", ex.desc);
        }
        dpi = config.dpi ?? 300;
        page_separator = config.page_separator ?? "\n\n--- Page Break ---\n\n";
        int scanned_threshold = config.scanned_threshold ?? 50;
        detector = new ScannedPdfDetector(scanned_threshold);

        # Create a single OCR engine reused across all pages
        string ocr_mode = config.ocr_mode ?? "native";
        hash<OcrConfig> ocr_config(<OcrConfig>{
            "mode": ocr_mode,
            "language": config.language ?? "eng",
            "datapath": config.datapath ?? "",
        });
        if (ocr_mode == "http") {
            ocr_config.url = config.ocr_url;
            ocr_config.api_key = config.ocr_api_key;
        }
        ocr = new OcrEngine(ocr_config);
    }

    #! Processes a PDF document, extracting text from all pages
    /** @param pdf_data binary PDF document data
        @return a @ref PdfOcrResult hash with combined text and per-page details

        @throw PDF-ERROR if the PDF cannot be read
        Page-level rendering and recognition failures are reported in the corresponding page result.
    */
    hash<PdfOcrResult> processPdf(binary pdf_data) {
        # Reader errors (including a password requirement) describe the input document.
        auto reader = create_object("Pdf::PdfReader", pdf_data);
        int page_count = call_object_method(reader, "pageCount");

        list<hash<PdfPageOcrResult>> page_results = ();
        list<string> all_text = ();
        int ocr_count = 0;

        for (int i = 0; i < page_count; ++i) {
            hash<PdfPageOcrResult> page_result = processPage(pdf_data, i);
            push page_results, page_result;
            push all_text, page_result.text;
            if (page_result.ocr_used) {
                ++ocr_count;
            }
        }

        return <PdfOcrResult>{
            "text": all_text.join(page_separator),
            "pages": page_results,
            "ocr_page_count": ocr_count,
            "total_pages": page_count,
        };
    }

    #! Processes a single PDF page
    /** @param pdf_data binary PDF data
        @param page_index zero-based page index
        @return a @ref PdfPageOcrResult hash
    */
    hash<PdfPageOcrResult> processPage(binary pdf_data, int page_index) {
        if (!call_static_method("Pdf::PdfRenderer", "isAvailable")) {
            return reviewPage(page_index, "", "OCR-RENDERER-UNAVAILABLE",
                "the pdf module requires PDFium for text extraction and page rendering");
        }

        string digital_text = "";
        *hash<ExceptionInfo> extraction_error;
        try {
            digital_text = call_static_method("Pdf::PdfRenderer", "extractTextFromData", pdf_data, page_index);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "PDF-ERROR") {
                rethrow;
            }
            extraction_error = ex;
        }
        if (trim(digital_text).val() && !detector.isScannedPage(digital_text)) {
            return <PdfPageOcrResult>{
                "page_index": page_index, "text": digital_text, "status": "digital",
                "ocr_used": False, "confidence": 1.0,
            };
        }

        bool ocr_used = False;
        try {
            hash<auto> raster = call_static_method("Pdf::PdfRenderer", "renderPageFromData",
                pdf_data, page_index, dpi);
            binary bmp = convertPixelsToBmp(raster);
            ocr_used = True;
            hash<OcrResult> result = ocr.extractText(bmp);
            if (trim(result.text).val()) {
                return <PdfPageOcrResult>{
                    "page_index": page_index, "text": result.text, "status": "ocr",
                    "ocr_used": True, "confidence": result.confidence,
                };
            }
            if (extraction_error) {
                return reviewPage(page_index, digital_text, extraction_error.err, extraction_error.desc, True);
            }
            if (!trim(digital_text).val() && isWhitePage(raster)) {
                return <PdfPageOcrResult>{
                    "page_index": page_index, "text": "", "status": "blank", "ocr_used": True, "confidence": 0.0,
                };
            }
            return reviewPage(page_index, digital_text, "OCR-NO-TEXT",
                "OCR returned no text for a page with content", True);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "PDF-ERROR" && ex.err != "OCR-ERROR") {
                rethrow;
            }
            return reviewPage(page_index, digital_text, ex.err, ex.desc, ocr_used);
        }
    }

    private static hash<PdfPageOcrResult> reviewPage(int page_index, string text, string err,
            string description, bool ocr_used = False) {
        return <PdfPageOcrResult>{
            "page_index": page_index, "text": text, "status": trim(text).val() ? "partial" : "unreadable",
            "ocr_used": ocr_used, "confidence": 0.0, "error": err, "error_description": description,
        };
    }

    # PDFium paints into an opaque white BGRA bitmap. Ignore stride padding.
    private static bool isWhitePage(hash<auto> raster) {
        int width = raster.width * 4;
        binary white = parse_hex_string(strmul("ff", width));
        binary pixels = raster.data;
        for (int y = 0; y < raster.height; ++y) {
            if (pixels.substr(y * raster.stride, width) != white) {
                return False;
            }
        }
        return True;
    }

    #! Encodes PDFium BGRA pixels as a lossless BMP for Tesseract
    private binary convertPixelsToBmp(hash<auto> render_result) {
        int w = render_result.width;
        int h = render_result.height;
        int stride = render_result.stride;
        binary pixels = render_result.data;

        int row_size = w * 4;
        int pixel_data_size = row_size * h;
        int file_size = 54 + pixel_data_size;

        # BMP file header (14 bytes) + DIB header (40 bytes)
        BinaryOutputStream output();
        StreamWriter writer(output);
        writer.write(<424D>);
        writer.writei4LSB(file_size);
        writer.writei4LSB(0);
        writer.writei4LSB(54);
        writer.writei4LSB(40);
        writer.writei4LSB(w);
        writer.writei4LSB(h);
        writer.writei2LSB(1);
        writer.writei2LSB(32);
        writer.writei4LSB(0);
        writer.writei4LSB(pixel_data_size);
        writer.writei4LSB(2835);
        writer.writei4LSB(2835);
        writer.writei4LSB(0);
        writer.writei4LSB(0);
        for (int y = h - 1; y >= 0; --y) {
            writer.write(pixels.substr(y * stride, row_size));
        }
        return output.getData();
    }
}

#! Configuration for PdfOcrEngine
public hashdecl PdfOcrConfig {
    #! OCR language code(s) for Tesseract (e.g., "eng", "deu+fra"); default "eng"
    string language = "eng";
    #! Path to tessdata directory (empty = system default)
    string datapath = "";
    #! OCR mode: "native" for local Tesseract, "http" for HTTP OCR service; default "native"
    string ocr_mode = "native";
    #! HTTP OCR service URL (required when ocr_mode is "http")
    *string ocr_url;
    #! HTTP OCR service API key
    *string ocr_api_key;
    #! DPI for rasterizing scanned pages — higher values improve OCR accuracy but
    #! use more memory; default 300
    int dpi = 300;
    #! Minimum characters of extracted text for a page to be considered digital;
    #! pages below this threshold are rasterized and OCR'd; default 50
    int scanned_threshold = 50;
    #! Text inserted between pages; default includes page break marker
    string page_separator = "\n\n--- Page Break ---\n\n";
}

} # namespace QoreOcrUtils
