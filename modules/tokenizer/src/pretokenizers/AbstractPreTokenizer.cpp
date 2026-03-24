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
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

//! Pass-through pre-tokenizer: returns the entire input as a single token
/** Used as a stub for pre-tokenizer types that are not yet fully implemented
    (Punctuation, Digits, Split).
*/
class PassthroughPreTokenizer : public AbstractPreTokenizer {
public:
    std::vector<PreToken> pretokenize(const std::string& input) const override {
        if (input.empty()) {
            return {};
        }
        return {{input, 0, input.size()}};
    }
};

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

    const QoreStringNode* type_str = safeGetString(type_val);
    if (!type_str) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "pre_tokenizer config 'type' is not a string");
        return nullptr;
    }

    const char* type = type_str->getBuffer();

    if (!strcmp(type, "BertPreTokenizer")) {
        return std::make_unique<BertPreTokenizer>();
    }

    if (!strcmp(type, "ByteLevel")) {
        return std::make_unique<ByteLevelPreTokenizer>(config, xsink);
    }

    if (!strcmp(type, "Metaspace")) {
        return std::make_unique<MetaspacePreTokenizer>(config, xsink);
    }

    if (!strcmp(type, "Whitespace")) {
        return std::make_unique<WhitespacePreTokenizer>(false);
    }

    if (!strcmp(type, "WhitespaceSplit")) {
        return std::make_unique<WhitespacePreTokenizer>(true);
    }

    if (!strcmp(type, "Sequence")) {
        return std::make_unique<SequencePreTokenizer>(config, xsink);
    }

    // Stub types: Punctuation, Digits, Split pass through for now
    if (!strcmp(type, "Punctuation") || !strcmp(type, "Digits") || !strcmp(type, "Split")) {
        return std::make_unique<PassthroughPreTokenizer>();
    }

    xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
        "unknown pre_tokenizer type '%s'", type);
    return nullptr;
}

} // namespace QoreTokenizer
