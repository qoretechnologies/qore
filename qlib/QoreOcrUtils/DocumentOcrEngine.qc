# -*- mode: qore; indent-tabs-mode: nil -*-

/*  DocumentOcrEngine.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see DocumentOcrEngine
public namespace QoreOcrUtils {

#! Result from processing any document
public hashdecl DocumentOcrResult {
    #! The extracted text from the entire document
    string text;
    #! Detected input format (e.g., "PDF", "PNG", "JPEG", "TIFF", "SVG")
    string detected_format;
    #! Number of pages (1 for single images, N for PDFs and multi-page TIFFs)
    int page_count;
    #! Number of pages that required OCR
    int ocr_page_count;
    #! Average OCR confidence across OCR'd pages (0.0 to 1.0); 1.0 for digital text
    float avg_confidence;
    #! Per-page details
    list<hash<PdfPageOcrResult>> pages;
}

#! Universal document text extraction engine
/** Accepts any supported file format and extracts text automatically.  The
    engine detects the file type and applies the best extraction strategy:

    - \b PDF documents: extracts the digital text layer from each page; pages
      with little or no text (scanned pages) are rasterized and OCR'd
    - \b Images (PNG, JPEG, TIFF, BMP, GIF, WebP, etc.): runs OCR directly
      via Tesseract
    - \b Multi-page TIFF: processes each page separately, returning per-page
      results
    - \b SVG files: converts to PNG via ImageMagick, then OCR's
    - \b Other formats: converts to PNG via ImageMagick, then OCR's

    The engine uses optional modules for maximum format coverage:
    - \c ocr (Tesseract) — required for any OCR (native mode)
    - \c pdf — required for PDF processing
    - \c imagemagick — enables 100+ additional image formats, multi-page TIFF
      extraction, and SVG-to-PNG conversion

    @par Example: Process any document
    @code{.py}
%modern
%requires QoreOcrUtils

DocumentOcrEngine engine(<DocumentOcrConfig>{"language": "eng"});

# Works with any format
hash<DocumentOcrResult> result = engine.processDocument(
    ReadOnlyFile::readBinaryFile("invoice.pdf"));
printf("Text: %s\n", result.text);

result = engine.processDocument(
    ReadOnlyFile::readBinaryFile("scan.tiff"));
printf("Text: %s\n", result.text);
    @endcode

    @since QoreOcrUtils 1.0
*/
public class DocumentOcrEngine {
    private {
        #! Reusable OCR engine (created once, used for all images)
        OcrEngine ocr;
        #! OCR mode for PdfOcrEngine
        string ocr_mode;
        #! OCR URL for HTTP mode
        *string ocr_url;
        #! OCR API key for HTTP mode
        *string ocr_api_key;
        #! OCR language
        string language;
        #! Tessdata path
        string datapath;
        #! DPI for PDF rasterization
        int dpi;
        #! Scanned page threshold for PDFs
        int scanned_threshold;
        #! Page separator
        string page_separator;
    }

    #! Creates a DocumentOcrEngine
    /** @param config optional configuration hash

        @see DocumentOcrConfig
    */
    constructor(*hash<DocumentOcrConfig> config) {
        language = config.language ?? "eng";
        datapath = config.datapath ?? "";
        dpi = config.dpi ?? 300;
        scanned_threshold = config.scanned_threshold ?? 50;
        page_separator = config.page_separator ?? "\n\n--- Page Break ---\n\n";
        ocr_mode = config.ocr_mode ?? "native";
        ocr_url = config.ocr_url;
        ocr_api_key = config.ocr_api_key;

        # Create a single OcrEngine reused for all images
        hash<OcrConfig> ocr_config(<OcrConfig>{
            "mode": ocr_mode,
            "language": language,
            "datapath": datapath,
        });
        if (ocr_mode == "http") {
            ocr_config.url = ocr_url;
            ocr_config.api_key = ocr_api_key;
        }
        ocr = new OcrEngine(ocr_config);
    }

    #! Processes any supported document and extracts text
    /** Automatically detects the file format and applies the appropriate
        extraction strategy.

        @param data binary file data (PDF, image, SVG, etc.)
        @param mime_type optional MIME type hint (e.g., \c "application/pdf",
            \c "image/tiff"); if not provided, format is detected from content
        @return a @ref DocumentOcrResult hash

        @throw OCR-ERROR if the format is not supported or OCR fails
        @throw OCR-DEPENDENCY-ERROR if required modules are not available
    */
    hash<DocumentOcrResult> processDocument(binary data, *string mime_type) {
        string format = detectFormat(data, mime_type);

        switch (format) {
            case "PDF":
                return processPdf(data);
            case "TIFF":
                return processTiff(data);
            default:
                return processImage(data, format);
        }
    }

    #! Detects the file format from content or MIME type
    /** @param data binary file data
        @param mime_type optional MIME type hint
        @return detected format string (e.g., "PDF", "PNG", "JPEG", "SVG", "TIFF")
    */
    string detectFormat(binary data, *string mime_type) {
        # Check MIME type hint first
        if (mime_type.val()) {
            if (mime_type == "application/pdf") {
                return "PDF";
            }
            if (mime_type == "image/svg+xml") {
                return "SVG";
            }
            if (mime_type =~ /^image\//) {
                string fmt = (mime_type =~ x/image\/([\w+-]+)/)[0] ?? "PNG";
                return fmt.upr();
            }
        }

        # Detect from magic bytes
        if (data.size() >= 4) {
            if (data.substr(0, 4) == <25504446>) {
                return "PDF";
            }
            if (data.substr(0, 4) == <89504E47>) {
                return "PNG";
            }
            if (data.substr(0, 3) == <FFD8FF>) {
                return "JPEG";
            }
            if (data.substr(0, 4) == <49492A00> || data.substr(0, 4) == <4D4D002A>) {
                return "TIFF";
            }
            if (data.substr(0, 4) == <47494638>) {
                return "GIF";
            }
            if (data.substr(0, 2) == <424D>) {
                return "BMP";
            }
            if (data.size() >= 12 && data.substr(0, 4) == <52494646>
                    && data.substr(8, 4) == <57454250>) {
                return "WEBP";
            }
            if (data.size() >= 5) {
                string head = data.substr(0, min(200, data.size())).toString();
                if (head =~ /<svg\b/i || head =~ /<!DOCTYPE\s+svg/i) {
                    return "SVG";
                }
            }
        }

        # Try ImageMagick for format detection as last resort
%ifndef NoImageMagick
        try {
            auto img = create_object("ImageMagick::MagickImage", data);
            hash<auto> info = call_object_method(img, "getInfo");
            if (info.format.val()) {
                return info.format.upr();
            }
        } catch () {
            # ImageMagick can't identify it either
        }
%endif

        throw "OCR-FORMAT-ERROR",
            sprintf("cannot detect file format from %d bytes of data; "
                "provide a mime_type hint or use a supported format "
                "(PDF, PNG, JPEG, TIFF, BMP, GIF, WebP, SVG)", data.size());
    }

    #! Processes a PDF document
    private hash<DocumentOcrResult> processPdf(binary data) {
        PdfOcrEngine pdf_engine(<PdfOcrConfig>{
            "language": language,
            "datapath": datapath,
            "ocr_mode": ocr_mode,
            "ocr_url": ocr_url,
            "ocr_api_key": ocr_api_key,
            "dpi": dpi,
            "scanned_threshold": scanned_threshold,
            "page_separator": page_separator,
        });

        hash<PdfOcrResult> pdf_result = pdf_engine.processPdf(data);

        float total_conf = 0.0;
        int conf_count = 0;
        foreach hash<PdfPageOcrResult> page in (pdf_result.pages) {
            if (page.ocr_used) {
                total_conf += page.confidence;
                ++conf_count;
            } else if (page.text.val()) {
                total_conf += 1.0;
                ++conf_count;
            }
        }

        return <DocumentOcrResult>{
            "text": pdf_result.text,
            "detected_format": "PDF",
            "page_count": pdf_result.total_pages,
            "ocr_page_count": pdf_result.ocr_page_count,
            "avg_confidence": conf_count > 0 ? total_conf / conf_count : 0.0,
            "pages": pdf_result.pages,
        };
    }

    #! Processes a TIFF image, handling multi-page TIFFs via ImageMagick
    private hash<DocumentOcrResult> processTiff(binary data) {
%ifndef NoImageMagick
        try {
            # Check if multi-page
            auto img = create_object("ImageMagick::MagickImage", data);
            hash<auto> info = call_object_method(img, "getInfo");
            int num_images = info.number_images ?? 1;

            if (num_images > 1) {
                # Multi-page TIFF: extract each page as PNG and OCR
                list<hash<PdfPageOcrResult>> pages = ();
                list<string> all_text = ();
                float total_conf = 0.0;

                for (int i = 0; i < num_images; ++i) {
                    # Use ImageMagick to extract each page
                    # Read the specific frame by index
                    binary page_png;
                    try {
                        auto page_img = create_object("ImageMagick::MagickImage",
                            data, sprintf("TIFF[%d]", i));
                        page_png = call_object_method(page_img, "toData", "PNG");
                    } catch () {
                        # If frame extraction fails, try the whole image
                        page_png = call_object_method(img, "toData", "PNG");
                    }

                    hash<OcrResult> ocr_result = ocr.extractText(page_png);
                    push pages, <PdfPageOcrResult>{
                        "page_index": i,
                        "text": ocr_result.text,
                        "ocr_used": True,
                        "confidence": ocr_result.confidence,
                    };
                    push all_text, ocr_result.text;
                    total_conf += ocr_result.confidence;
                }

                return <DocumentOcrResult>{
                    "text": all_text.join(page_separator),
                    "detected_format": "TIFF",
                    "page_count": num_images,
                    "ocr_page_count": num_images,
                    "avg_confidence": num_images > 0
                        ? total_conf / num_images : 0.0,
                    "pages": pages,
                };
            }
        } catch () {
            # ImageMagick not available or failed — treat as single image
        }
%endif

        # Single-page TIFF or no ImageMagick — OCR directly
        # (Tesseract reads TIFF natively)
        return ocrSingleImage(data, "TIFF");
    }

    #! Processes a raster image
    private hash<DocumentOcrResult> processImage(binary data, string format) {
        # Formats Tesseract reads natively
        list<string> native_formats = ("PNG", "JPEG", "TIFF", "BMP", "GIF", "PNM", "PBM");
        binary ocr_data;

        if (inlist(format, native_formats)) {
            ocr_data = data;
        } else {
            # Convert to PNG via ImageMagick (handles SVG, WebP, HEIC, etc.)
            ocr_data = convertToPng(data);
        }

        return ocrSingleImage(ocr_data, format);
    }

    #! Runs OCR on a single image and returns a DocumentOcrResult
    private hash<DocumentOcrResult> ocrSingleImage(binary image_data, string format) {
        hash<OcrResult> result = ocr.extractText(image_data);

        return <DocumentOcrResult>{
            "text": result.text,
            "detected_format": format,
            "page_count": 1,
            "ocr_page_count": 1,
            "avg_confidence": result.confidence,
            "pages": (
                <PdfPageOcrResult>{
                    "page_index": 0,
                    "text": result.text,
                    "ocr_used": True,
                    "confidence": result.confidence,
                },
            ),
        };
    }

    #! Converts any image format to PNG via ImageMagick
    /** @throw OCR-DEPENDENCY-ERROR if imagemagick module is not available
    */
    private binary convertToPng(binary data) {
%ifndef NoImageMagick
        try {
            auto img = create_object("ImageMagick::MagickImage", data);
            return call_object_method(img, "toData", "PNG");
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "CREATE-OBJECT-ERROR") {
                throw "OCR-DEPENDENCY-ERROR",
                    "the imagemagick module is required to convert this image format; "
                    "install the imagemagick module";
            }
            throw "OCR-FORMAT-ERROR",
                sprintf("failed to convert image to PNG: %s", ex.desc);
        }
%endif
        throw "OCR-DEPENDENCY-ERROR",
            "the imagemagick module is required to convert this image format; "
            "install the imagemagick module";
    }
}

#! Configuration for DocumentOcrEngine
public hashdecl DocumentOcrConfig {
    #! OCR language code(s) for Tesseract; default "eng"
    string language = "eng";
    #! Path to tessdata directory (empty = system default)
    string datapath = "";
    #! OCR mode: "native" for local Tesseract, "http" for HTTP OCR service;
    #! default "native"
    string ocr_mode = "native";
    #! HTTP OCR service URL (required when ocr_mode is "http")
    *string ocr_url;
    #! HTTP OCR service API key
    *string ocr_api_key;
    #! DPI for rasterizing scanned PDF pages; default 300
    int dpi = 300;
    #! Minimum characters for a PDF page to be considered digital; default 50
    int scanned_threshold = 50;
    #! Text inserted between pages; default includes page break marker
    string page_separator = "\n\n--- Page Break ---\n\n";
}

} # namespace QoreOcrUtils
