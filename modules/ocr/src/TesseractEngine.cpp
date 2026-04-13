/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TesseractEngine.cpp

    Qore OCR module — Tesseract engine implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "TesseractEngine.h"

#include <tesseract/resultiterator.h>

namespace QoreOcr {

QoreTesseractEngine::QoreTesseractEngine(const QoreStringNode* datapath,
        const QoreStringNode* language, ExceptionSink* xsink) {
    api = std::make_unique<tesseract::TessBaseAPI>();

    const char* dp = datapath && datapath->strlen() ? datapath->c_str() : nullptr;
    const char* lang = language && language->strlen() ? language->c_str() : "eng";

    int rv = api->Init(dp, lang);
    if (rv != 0) {
        xsink->raiseException("OCR-INIT-ERROR",
            "failed to initialize Tesseract with datapath=%s language=%s",
            dp ? dp : "(default)", lang);
        api.reset();
        return;
    }
    initialized = true;
}

QoreTesseractEngine::~QoreTesseractEngine() {
    if (api) {
        api->End();
    }
}

Pix* QoreTesseractEngine::loadImage(const BinaryNode* data, ExceptionSink* xsink) {
    if (!data || !data->size()) {
        xsink->raiseException("OCR-ERROR", "empty image data");
        return nullptr;
    }

    Pix* pix = pixReadMem(
        static_cast<const l_uint8*>(data->getPtr()),
        static_cast<size_t>(data->size())
    );

    if (!pix) {
        xsink->raiseException("OCR-ERROR",
            "failed to decode image data (%lld bytes); "
            "supported formats: PNG, JPEG, TIFF, BMP, GIF",
            static_cast<long long>(data->size()));
        return nullptr;
    }

    return pix;
}

QoreStringNode* QoreTesseractEngine::extractText(const BinaryNode* data,
        ExceptionSink* xsink) {
    if (!initialized) {
        xsink->raiseException("OCR-ERROR", "TesseractEngine not initialized");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(api_mutex);

    Pix* pix = loadImage(data, xsink);
    if (!pix) {
        return nullptr;
    }

    api->SetImage(pix);

    char* text = api->GetUTF8Text();
    if (!text) {
        pixDestroy(&pix);
        xsink->raiseException("OCR-ERROR", "failed to extract text from image");
        return nullptr;
    }

    QoreStringNode* result = new QoreStringNode(text);
    delete[] text;
    pixDestroy(&pix);
    api->Clear();

    return result;
}

QoreHashNode* QoreTesseractEngine::extractTextWithMetadata(const BinaryNode* data,
        ExceptionSink* xsink) {
    if (!initialized) {
        xsink->raiseException("OCR-ERROR", "TesseractEngine not initialized");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(api_mutex);

    Pix* pix = loadImage(data, xsink);
    if (!pix) {
        return nullptr;
    }

    api->SetImage(pix);

    char* text = api->GetUTF8Text();
    if (!text) {
        pixDestroy(&pix);
        xsink->raiseException("OCR-ERROR", "failed to extract text from image");
        return nullptr;
    }

    int confidence = api->MeanTextConf();

    // Build block-level results
    ReferenceHolder<QoreListNode> blocks(new QoreListNode(autoTypeInfo), xsink);

    tesseract::ResultIterator* ri = api->GetIterator();
    if (ri) {
        tesseract::PageIteratorLevel level = tesseract::RIL_BLOCK;
        while (!ri->Empty(level)) {
            const char* block_text = ri->GetUTF8Text(level);
            if (block_text) {
                ReferenceHolder<QoreHashNode> block(new QoreHashNode(autoTypeInfo), xsink);
                block->setKeyValue("text", new QoreStringNode(block_text), xsink);
                block->setKeyValue("confidence",
                    (double)(ri->Confidence(level) / 100.0), xsink);

                int x1, y1, x2, y2;
                if (ri->BoundingBox(level, &x1, &y1, &x2, &y2)) {
                    ReferenceHolder<QoreHashNode> bbox(new QoreHashNode(autoTypeInfo), xsink);
                    bbox->setKeyValue("x1", x1, xsink);
                    bbox->setKeyValue("y1", y1, xsink);
                    bbox->setKeyValue("x2", x2, xsink);
                    bbox->setKeyValue("y2", y2, xsink);
                    block->setKeyValue("bbox", bbox.release(), xsink);
                }

                blocks->push(block.release(), xsink);
                delete[] block_text;
            }
            ri->Next(level);
        }
        delete ri;
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("text", new QoreStringNode(text), xsink);
    result->setKeyValue("confidence", (double)(confidence / 100.0), xsink);
    result->setKeyValue("blocks", blocks.release(), xsink);

    delete[] text;
    pixDestroy(&pix);
    api->Clear();

    return result.release();
}

QoreHashNode* QoreTesseractEngine::getCapabilities() const {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), &xsink);

    result->setKeyValue("has_tesseract", true, &xsink);

    const char* version = tesseract::TessBaseAPI::Version();
    result->setKeyValue("version", new QoreStringNode(version ? version : "unknown"), &xsink);

    // Get available languages if initialized
    if (initialized && api) {
        std::vector<std::string> langs;
        api->GetAvailableLanguagesAsVector(&langs);
        ReferenceHolder<QoreListNode> lang_list(new QoreListNode(stringTypeInfo), &xsink);
        for (const auto& lang : langs) {
            lang_list->push(new QoreStringNode(lang), &xsink);
        }
        result->setKeyValue("languages", lang_list.release(), &xsink);
    }

    return result.release();
}

} // namespace QoreOcr
