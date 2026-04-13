# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ScannedPdfDetector.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ScannedPdfDetector
public namespace QoreOcrUtils {

#! Heuristic detection of scanned (image-based) PDF pages
/** Determines whether a PDF page is scanned by checking if the extracted
    text is below a character threshold. This is useful for deciding whether
    to run OCR on a page.

    @par Example:
    @code{.py}
ScannedPdfDetector detector(50);

# Check individual pages
if (detector.isScannedPage("")) {
    # Page has no text — likely scanned
}
if (!detector.isScannedPage("This page has plenty of text content...")) {
    # Page has enough text — not scanned
}

# Find all scanned pages in a document
list<string> page_texts = ("", "Hello world", "", "Some text here");
list<int> scanned = detector.findScannedPages(page_texts);
# scanned = (0, 2) — pages 0 and 2 are likely scanned
    @endcode

    @since QoreOcrUtils 1.0
*/
public class ScannedPdfDetector {
    private {
        #! Minimum character threshold for a page to be considered digital
        int threshold;
    }

    #! Creates a ScannedPdfDetector
    /** @param char_threshold minimum number of characters for a page to be
            considered digital (not scanned); default 50
    */
    constructor(int char_threshold = 50) {
        if (char_threshold < 0) {
            throw "OCR-CONFIG-ERROR",
                sprintf("char_threshold must be >= 0, got %d", char_threshold);
        }
        threshold = char_threshold;
    }

    #! Checks if a single page is likely scanned
    /** @param extracted_text the text extracted from the page (e.g., via PDF text extraction)
        @return True if the page is likely scanned (text below threshold)
    */
    bool isScannedPage(string extracted_text) {
        return trim(extracted_text).length() < threshold;
    }

    #! Finds all scanned page indices in a document
    /** @param page_texts list of extracted text strings, one per page
        @return list of zero-based page indices that are likely scanned
    */
    list<int> findScannedPages(list<string> page_texts) {
        list<int> scanned = ();
        for (int i = 0; i < page_texts.size(); ++i) {
            if (isScannedPage(page_texts[i])) {
                push scanned, i;
            }
        }
        return scanned;
    }

    #! Returns the configured threshold
    int getThreshold() {
        return threshold;
    }
}

} # namespace QoreOcrUtils
