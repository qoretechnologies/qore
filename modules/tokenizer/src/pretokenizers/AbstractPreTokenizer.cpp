/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPreTokenizer.cpp

    Abstract pre-tokenizer factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "pretokenizers/AbstractPreTokenizer.h"
#include "pretokenizers/BertPreTokenizer.h"
#include "pretokenizers/ByteLevelPreTokenizer.h"
#include "pretokenizers/MetaspacePreTokenizer.h"
#include "pretokenizers/WhitespacePreTokenizer.h"
#include "pretokenizers/SequencePreTokenizer.h"
#include "pretokenizers/SplitPreTokenizer.h"
#include "pretokenizers/PunctuationPreTokenizer.h"
#include "pretokenizers/DigitsPreTokenizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {


std::unique_ptr<AbstractPreTokenizer> AbstractPreTokenizer::fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR", "pre_tokenizer config is null");
        return nullptr;
    }

    // Get the "type" key from the config hash
    QoreValue type_val = config->getKeyValue("type");
    if (type_val.isNullOrNothing()) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "pre_tokenizer config missing 'type' key");
        return nullptr;
    }

    std::string type = safeGetStdString(type_val);
    if (type.empty()) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "pre_tokenizer config 'type' is not a string");
        return nullptr;
    }

    if (type == "BertPreTokenizer") {
        return std::make_unique<BertPreTokenizer>();
    }

    if (type == "ByteLevel") {
        return std::make_unique<ByteLevelPreTokenizer>(config, xsink);
    }

    if (type == "Metaspace") {
        return std::make_unique<MetaspacePreTokenizer>(config, xsink);
    }

    if (type == "Whitespace") {
        return std::make_unique<WhitespacePreTokenizer>(false);
    }

    if (type == "WhitespaceSplit") {
        return std::make_unique<WhitespacePreTokenizer>(true);
    }

    if (type == "Sequence") {
        return std::make_unique<SequencePreTokenizer>(config, xsink);
    }

    if (type == "Punctuation") {
        return std::make_unique<PunctuationPreTokenizer>();
    }

    if (type == "Digits") {
        bool individual = true;
        QoreValue iv = config->getKeyValue("individual_digits");
        if (!iv.isNullOrNothing()) {
            individual = iv.getAsBool();
        }
        return std::make_unique<DigitsPreTokenizer>(individual);
    }

    if (type == "Split") {
        return std::make_unique<SplitPreTokenizer>(config, xsink);
    }

    xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
        "unknown pre_tokenizer type '%s'", type.c_str());
    return nullptr;
}

} // namespace QoreTokenizer
