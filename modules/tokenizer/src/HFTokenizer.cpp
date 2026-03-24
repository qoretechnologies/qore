/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HFTokenizer.cpp

    Main HuggingFace tokenizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "HFTokenizer.h"
#include "utils/qore_helpers.h"

#include <algorithm>

namespace QoreTokenizer {

QoreHFTokenizer::QoreHFTokenizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-ERROR", "config hash is required");
        return;
    }

    // Helper to safely get a hash value from a key
    auto getHashKey = [](const QoreHashNode* h, const char* key) -> const QoreHashNode* {
        QoreValue v = h->getKeyValue(key);
        if (v.isNullOrNothing() || v.getType() != NT_HASH) {
            return nullptr;
        }
        return v.get<const QoreHashNode>();
    };

    // Parse normalizer (optional)
    const QoreHashNode* norm_config = getHashKey(config, "normalizer");
    if (norm_config) {
        normalizer = AbstractNormalizer::fromConfig(norm_config, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse pre-tokenizer (optional)
    const QoreHashNode* pretok_config = getHashKey(config, "pre_tokenizer");
    if (pretok_config) {
        pre_tokenizer = AbstractPreTokenizer::fromConfig(pretok_config, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse model (required)
    const QoreHashNode* model_config = getHashKey(config, "model");
    if (!model_config) {
        xsink->raiseException("TOKENIZER-ERROR", "model configuration is required");
        return;
    }
    model = AbstractTokenizerModel::fromConfig(model_config, xsink);
    if (*xsink) {
        return;
    }

    // Parse post-processor (optional)
    const QoreHashNode* post_config = getHashKey(config, "post_processor");
    if (post_config) {
        post_processor = AbstractPostProcessor::fromConfig(post_config, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse decoder (optional)
    const QoreHashNode* dec_config = getHashKey(config, "decoder");
    if (dec_config) {
        decoder = AbstractDecoder::fromConfig(dec_config, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse added_tokens
    QoreValue at_val = config->getKeyValue("added_tokens");
    const QoreListNode* at_list = (!at_val.isNullOrNothing() && at_val.getType() == NT_LIST)
        ? at_val.get<const QoreListNode>() : nullptr;
    if (at_list) {
        ConstListIterator li(at_list);
        while (li.next()) {
            const QoreHashNode* at = li.getValue().get<const QoreHashNode>();
            if (!at) {
                continue;
            }
            AddedToken tok;
            const QoreStringNode* content_str = safeGetString(
                at->getKeyValue("content"));
            tok.content = content_str ? content_str->c_str() : "";
            tok.id = (int)at->getKeyValue("id").getAsBigInt();
            tok.special = at->getKeyValue("special").getAsBool();
            tok.single_word = at->getKeyValue("single_word").getAsBool();

            if (tok.special) {
                special_token_ids.insert(tok.id);
            }
            added_tokens.push_back(std::move(tok));
        }
    }
}

std::vector<int> QoreHFTokenizer::tokenizePreToken(const std::string& pre_token) const {
    if (!model) {
        return {};
    }
    return model->tokenize(pre_token);
}

std::vector<int> QoreHFTokenizer::encodeText(const std::string& text) const {
    std::string normalized = text;

    // Step 1: Normalize
    if (normalizer) {
        normalized = normalizer->normalize(normalized);
    }

    // Step 2: Pre-tokenize
    std::vector<int> all_ids;

    if (pre_tokenizer) {
        auto pre_tokens = pre_tokenizer->pretokenize(normalized);
        for (const auto& pt : pre_tokens) {
            auto ids = tokenizePreToken(pt.text);
            all_ids.insert(all_ids.end(), ids.begin(), ids.end());
        }
    } else {
        // No pre-tokenizer: tokenize the whole text
        all_ids = tokenizePreToken(normalized);
    }

    return all_ids;
}

QoreHashNode* QoreHFTokenizer::encode(const QoreStringNode* text,
        const QoreStringNode* text_pair, bool add_special_tokens,
        ExceptionSink* xsink) {
    // No mutex needed: all pipeline components are immutable after construction

    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }

    // Encode first text
    std::string text_str = text ? text->c_str() : "";
    std::vector<int> ids_a = encodeText(text_str);

    // Encode second text (optional)
    std::vector<int> ids_b;
    bool has_pair = false;
    if (text_pair && text_pair->size() > 0) {
        ids_b = encodeText(text_pair->c_str());
        has_pair = true;
    }

    // Step 3: Post-process (add special tokens)
    std::vector<int> final_ids;
    std::vector<int> type_ids;
    std::vector<std::string> tokens;

    // Build token strings for post-processing
    std::vector<std::string> tokens_a;
    if (model) {
        for (int id : ids_a) {
            tokens_a.push_back(model->idToToken(id));
        }
    }
    std::vector<std::string> tokens_b_str;
    if (has_pair && model) {
        for (int id : ids_b) {
            tokens_b_str.push_back(model->idToToken(id));
        }
    }

    if (post_processor && add_special_tokens) {
        auto result = post_processor->process(ids_a,
            has_pair ? ids_b : std::vector<int>(),
            tokens_a,
            has_pair ? tokens_b_str : std::vector<std::string>(),
            true);
        final_ids = std::move(result.ids);
        type_ids = std::move(result.type_ids);
        tokens = std::move(result.tokens);
    } else {
        final_ids = std::move(ids_a);
        tokens = std::move(tokens_a);
        if (has_pair) {
            final_ids.insert(final_ids.end(), ids_b.begin(), ids_b.end());
            tokens.insert(tokens.end(), tokens_b_str.begin(), tokens_b_str.end());
        }
        type_ids.resize(final_ids.size(), 0);
    }

    // Build token strings if not already set
    if (tokens.empty() && model) {
        for (int id : final_ids) {
            tokens.push_back(model->idToToken(id));
        }
    }

    // Build result hash
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    // input_ids
    ReferenceHolder<QoreListNode> id_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int id : final_ids) {
        id_list->push(id, xsink);
    }
    result->setKeyValue("input_ids", id_list.release(), xsink);

    // token_type_ids
    ReferenceHolder<QoreListNode> type_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int tid : type_ids) {
        type_list->push(tid, xsink);
    }
    result->setKeyValue("token_type_ids", type_list.release(), xsink);

    // attention_mask (all 1s)
    ReferenceHolder<QoreListNode> mask_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (size_t i = 0; i < final_ids.size(); ++i) {
        mask_list->push(1, xsink);
    }
    result->setKeyValue("attention_mask", mask_list.release(), xsink);

    // tokens
    ReferenceHolder<QoreListNode> tok_list(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& t : tokens) {
        tok_list->push(new QoreStringNode(t), xsink);
    }
    result->setKeyValue("tokens", tok_list.release(), xsink);

    return result.release();
}

QoreStringNode* QoreHFTokenizer::decode(const QoreListNode* ids,
        bool skip_special_tokens, ExceptionSink* xsink) {

    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }

    // Convert IDs to token strings
    std::vector<std::string> tokens;
    ConstListIterator li(ids);
    while (li.next()) {
        int id = (int)li.getValue().getAsBigInt();
        if (skip_special_tokens && special_token_ids.count(id)) {
            continue;
        }
        tokens.push_back(model->idToToken(id));
    }

    // Apply decoder
    std::string result;
    if (decoder) {
        result = decoder->decode(tokens);
    } else {
        // Default: just join with spaces
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) {
                result += " ";
            }
            result += tokens[i];
        }
    }

    return new QoreStringNode(result);
}

int QoreHFTokenizer::getVocabSize() const {
    return model ? model->vocabSize() : 0;
}

QoreStringNode* QoreHFTokenizer::idToToken(int id, ExceptionSink* xsink) const {
    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }
    std::string token = model->idToToken(id);
    return new QoreStringNode(token);
}

int QoreHFTokenizer::tokenToId(const QoreStringNode* token) const {
    if (!model || !token) {
        return -1;
    }
    return model->tokenToId(token->c_str());
}

} // namespace QoreTokenizer
