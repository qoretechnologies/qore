/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractTokenizerModel.cpp

    Abstract base class for tokenizer models - factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "models/AbstractTokenizerModel.h"
#include "models/BPEModel.h"
#include "models/WordPieceModel.h"
#include "models/UnigramModel.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

std::unique_ptr<AbstractTokenizerModel> AbstractTokenizerModel::fromConfig(
        const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-MODEL-ERROR", "model config is null");
        return nullptr;
    }

    // Get the "type" field; if absent, infer from config keys (legacy format)
    std::string type = safeGetStringKey(config, "type");
    if (!type.empty()) {
        // type already set
    } else {
        // Infer model type from keys present:
        // - WordPiece: has "vocab" (object) + "continuing_subword_prefix" + no "merges"
        // - BPE: has "vocab" (object) + "merges" (list)
        // - Unigram: has "vocab" (list of [token, score] pairs)
        // - WordLevel: has "vocab" (object) + no "merges" + no "continuing_subword_prefix"
        bool has_merges = !config->getKeyValue("merges").isNullOrNothing();
        bool has_continuing = !config->getKeyValue("continuing_subword_prefix")
            .isNullOrNothing();
        const QoreListNode* vocab_list = safeGetListKey(config, "vocab");
        if (vocab_list) {
            type = "Unigram";
        } else if (has_merges) {
            type = "BPE";
        } else if (has_continuing) {
            type = "WordPiece";
        } else {
            type = "WordPiece"; // default fallback
        }
    }

    if (type == "BPE") {
        return std::make_unique<BPEModel>(config, xsink);
    } else if (type == "WordPiece") {
        return std::make_unique<WordPieceModel>(config, xsink);
    } else if (type == "Unigram") {
        return std::make_unique<UnigramModel>(config, xsink);
    } else {
        xsink->raiseException("TOKENIZER-MODEL-ERROR", "unknown model type '%s'", type.c_str());
        return nullptr;
    }
}

} // namespace QoreTokenizer
