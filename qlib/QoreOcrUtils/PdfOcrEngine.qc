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
    #! OCR confidence (0.0 to 1.0); only meaningful when ocr_used is True
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

        @throw OCR-DEPENDENCY-ERROR if the ocr module is not available for
            native mode
    */
    constructor(*hash<PdfOcrConfig> config) {
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

        @throw OCR-DEPENDENCY-ERROR if required modules are not available
        @throw PDF-ERROR if the PDF cannot be read
        @throw OCR-ERROR if OCR fails on a scanned page
    */
    hash<PdfOcrResult> processPdf(binary pdf_data) {
        # Create PdfReader once, reuse for page count and text extraction
        auto reader;
        try {
            reader = create_object("Pdf::PdfReader", pdf_data);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-CLASS" || ex.err == "CREATE-OBJECT-ERROR") {
                throw "OCR-DEPENDENCY-ERROR",
                    "the pdf module is required for PDF processing but is not available; "
                    "install the pdf module to enable PDF OCR";
            }
            rethrow;
        }

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
        # Step 1: Try to extract digital text
        string digital_text = "";
        try {
            digital_text = call_static_method("Pdf::PdfRenderer", "extractTextFromData",
                pdf_data, page_index);
        } catch (hash<ExceptionInfo> ex) {
            # If text extraction fails, treat as scanned
        }

        # Step 2: Check if the page has enough text (digital)
        if (!detector.isScannedPage(digital_text)) {
            return <PdfPageOcrResult>{
                "page_index": page_index,
                "text": digital_text,
                "ocr_used": False,
                "confidence": 1.0,
            };
        }

        # Step 3: Page appears scanned — rasterize and OCR
        try {
            return ocrPage(pdf_data, page_index);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.desc =~ /PDFium/) {
                # PDFium not available — cannot rasterize; return what we have
                return <PdfPageOcrResult>{
                    "page_index": page_index,
                    "text": digital_text,
                    "ocr_used": False,
                    "confidence": 0.0,
                };
            }
            rethrow;
        }
    }

    #! Rasterizes a PDF page and runs OCR on it
    private hash<PdfPageOcrResult> ocrPage(binary pdf_data, int page_index) {
        hash<auto> render_result;
        try {
            render_result = call_static_method("Pdf::PdfRenderer", "renderPageFromData",
                pdf_data, page_index, dpi);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-CLASS") {
                throw "OCR-DEPENDENCY-ERROR",
                    "the pdf module is required for PDF page rasterization";
            }
            rethrow;
        }

        # Convert raw BGRA pixels to an image format Tesseract can read
        binary image_data = convertPixelsToImage(render_result);

        hash<OcrResult> ocr_result = ocr.extractText(image_data);

        return <PdfPageOcrResult>{
            "page_index": page_index,
            "text": ocr_result.text,
            "ocr_used": True,
            "confidence": ocr_result.confidence,
        };
    }

    #! Converts raw BGRA pixels to an image format for OCR
    /** Tries ImageMagick (PNG output) first for better quality; falls back
        to a built-in BMP encoder if ImageMagick is not available.
    */
    private binary convertPixelsToImage(hash<auto> render_result) {
        # Try ImageMagick for high-quality PNG conversion
%ifndef NoImageMagick
        try {
            auto img = create_object("ImageMagick::MagickImage",
                render_result.width, render_result.height, "white");
            # Import raw BGRA pixel data and export as PNG
            call_object_method(img, "readData", render_result.data, "BGRA");
            return call_object_method(img, "toData", "PNG");
        } catch () {
            # ImageMagick failed — fall through to BMP
        }
%endif
        return convertPixelsToBmp(render_result);
    }

    #! Fallback: converts BGRA pixels to BMP format without dependencies
    /** BMP supports BGRA natively. Tesseract can read BMP files.
    */
    private binary convertPixelsToBmp(hash<auto> render_result) {
        int w = render_result.width;
        int h = render_result.height;
        int stride = render_result.stride;
        binary pixels = render_result.data;

        int row_size = w * 4;
        int pixel_data_size = row_size * h;
        int file_size = 54 + pixel_data_size;

        # BMP file header (14 bytes) + DIB header (40 bytes)
        binary bmp = binary();
        bmp += <424D>;                          # "BM"
        bmp += int_to_le_bytes(file_size, 4);
        bmp += <00000000>;                      # reserved
        bmp += int_to_le_bytes(54, 4);          # pixel data offset

        bmp += int_to_le_bytes(40, 4);          # DIB header size
        bmp += int_to_le_bytes(w, 4);
        bmp += int_to_le_bytes(h, 4);           # positive = bottom-up
        bmp += int_to_le_bytes(1, 2);           # color planes
        bmp += int_to_le_bytes(32, 2);          # bits per pixel (BGRA)
        bmp += int_to_le_bytes(0, 4);           # compression
        bmp += int_to_le_bytes(pixel_data_size, 4);
        bmp += int_to_le_bytes(2835, 4);        # h resolution (72 DPI)
        bmp += int_to_le_bytes(2835, 4);        # v resolution
        bmp += int_to_le_bytes(0, 4);           # palette colors
        bmp += int_to_le_bytes(0, 4);           # important colors

        # Pixel data (bottom-up: last row first)
        for (int y = h - 1; y >= 0; --y) {
            int row_offset = y * stride;
            bmp += pixels.substr(row_offset, row_size);
        }

        return bmp;
    }

    #! Converts an integer to little-endian bytes
    private binary int_to_le_bytes(int val, int byte_count) {
        binary result = binary();
        for (int i = 0; i < byte_count; ++i) {
            result += chr(val & 0xFF).toBinary();
            val >>= 8;
        }
        return result;
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
