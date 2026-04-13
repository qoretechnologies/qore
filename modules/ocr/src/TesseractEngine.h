/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TesseractEngine.h

    Qore OCR module — Tesseract engine C++ wrapper

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_OCR_TESSERACTENGINE_H
#define _QORE_OCR_TESSERACTENGINE_H

#include "qore/Qore.h"

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <mutex>
#include <string>
#include <memory>

namespace QoreOcr {

class QoreTesseractEngine : public AbstractPrivateData {
public:
    //! Initialize Tesseract with the given data path and language(s)
    /** @param datapath path to tessdata directory (or empty for default)
        @param language language code(s) (e.g., "eng", "deu+fra")
        @param xsink exception sink
    */
    QoreTesseractEngine(const QoreStringNode* datapath, const QoreStringNode* language,
        ExceptionSink* xsink);

    ~QoreTesseractEngine();

    //! Extract text from image binary data
    /** @param data raw image bytes (PNG, JPEG, TIFF, BMP)
        @param xsink exception sink
        @return extracted text string
    */
    QoreStringNode* extractText(const BinaryNode* data, ExceptionSink* xsink);

    //! Extract text with metadata (text, confidence, blocks)
    /** @param data raw image bytes
        @param xsink exception sink
        @return hash with text, confidence, blocks
    */
    QoreHashNode* extractTextWithMetadata(const BinaryNode* data, ExceptionSink* xsink);

    //! Get engine capabilities
    /** @return hash with has_tesseract, languages, version
    */
    QoreHashNode* getCapabilities() const;

private:
    std::unique_ptr<tesseract::TessBaseAPI> api;
    std::mutex api_mutex;
    bool initialized = false;

    //! Set image from binary data and run OCR
    Pix* loadImage(const BinaryNode* data, ExceptionSink* xsink);
};

} // namespace QoreOcr

#endif // _QORE_OCR_TESSERACTENGINE_H
