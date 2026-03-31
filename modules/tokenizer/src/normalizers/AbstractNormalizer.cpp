/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractNormalizer.cpp

    Abstract normalizer factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/AbstractNormalizer.h"
#include "normalizers/BertNormalizer.h"
#include "normalizers/SequenceNormalizer.h"
#include "normalizers/UnicodeNormalizer.h"
#include "normalizers/ReplaceNormalizer.h"
#include "normalizers/PrependNormalizer.h"
#include "normalizers/PrecompiledNormalizer.h"
#include "utils/qore_helpers.h"

#include <utf8proc.h>

#include <algorithm>
#include <cctype>

using namespace QoreTokenizer;

namespace QoreTokenizer {

//! Simple lowercase normalizer (inline implementation)
class LowercaseNormalizer : public AbstractNormalizer {
public:
    std::string normalize(const std::string& input) const override {
        // Use utf8proc for proper Unicode lowercasing
        utf8proc_uint8_t* result = nullptr;
        utf8proc_ssize_t len = utf8proc_map(
            reinterpret_cast<const utf8proc_uint8_t*>(input.c_str()),
            static_cast<utf8proc_ssize_t>(input.size()),
            &result,
            static_cast<utf8proc_option_t>(UTF8PROC_CASEFOLD | UTF8PROC_STABLE));

        if (len < 0 || !result) {
            // Fallback to ASCII lowercasing if utf8proc fails
            std::string output = input;
            std::transform(output.begin(), output.end(), output.begin(),
                [](unsigned char c) { return std::tolower(c); });
            return output;
        }

        std::string output(reinterpret_cast<const char*>(result), static_cast<size_t>(len));
        free(result);
        return output;
    }
};

//! Simple strip (trim whitespace) normalizer (inline implementation)
class StripNormalizer : public AbstractNormalizer {
public:
    std::string normalize(const std::string& input) const override {
        // Find the first non-whitespace character
        size_t start = 0;
        while (start < input.size() && isWhitespace(input[start])) {
            ++start;
        }

        // Find the last non-whitespace character
        size_t end = input.size();
        while (end > start && isWhitespace(input[end - 1])) {
            --end;
        }

        return input.substr(start, end - start);
    }

private:
    static bool isWhitespace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }
};

std::unique_ptr<AbstractNormalizer> AbstractNormalizer::fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR", "normalizer config is null");
        return nullptr;
    }

    // Get the "type" field
    std::string type = safeGetStringKey(config, "type");
    if (type.empty()) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "normalizer config missing 'type' field");
        return nullptr;
    }

    if (type == "BertNormalizer") {
        return std::make_unique<BertNormalizer>(config, xsink);
    } else if (type == "Lowercase") {
        return std::make_unique<LowercaseNormalizer>();
    } else if (type == "NFC") {
        return std::make_unique<UnicodeNormalizer>(UnicodeNormalizer::Form::NFC);
    } else if (type == "NFD") {
        return std::make_unique<UnicodeNormalizer>(UnicodeNormalizer::Form::NFD);
    } else if (type == "NFKC") {
        return std::make_unique<UnicodeNormalizer>(UnicodeNormalizer::Form::NFKC);
    } else if (type == "NFKD") {
        return std::make_unique<UnicodeNormalizer>(UnicodeNormalizer::Form::NFKD);
    } else if (type == "StripAccents") {
        return std::make_unique<UnicodeNormalizer>(UnicodeNormalizer::Form::NFD, true);
    } else if (type == "Sequence") {
        return std::make_unique<SequenceNormalizer>(config, xsink);
    } else if (type == "Replace") {
        return std::make_unique<ReplaceNormalizer>(config, xsink);
    } else if (type == "Prepend") {
        return std::make_unique<PrependNormalizer>(config, xsink);
    } else if (type == "Precompiled") {
        return std::make_unique<PrecompiledNormalizer>(config, xsink);
    } else if (type == "Strip") {
        return std::make_unique<StripNormalizer>();
    } else {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "unknown normalizer type '%s'", type.c_str());
        return nullptr;
    }
}

} // namespace QoreTokenizer
