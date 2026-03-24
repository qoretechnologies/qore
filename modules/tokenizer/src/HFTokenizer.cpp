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

// Internal: tokenize a text segment through normalize + pre-tokenize + model
QoreHFTokenizer::InternalEncoding QoreHFTokenizer::encodeSegment(
        const std::string& text) const {
    InternalEncoding result;
    std::string normalized = text;

    // Step 1: Normalize (note: offsets are best-effort when normalizer changes length)
    if (normalizer) {
        normalized = normalizer->normalize(normalized);
    }

    // Step 2: Pre-tokenize + model tokenize with offset tracking
    if (pre_tokenizer) {
        auto pre_tokens = pre_tokenizer->pretokenize(normalized);
        for (const auto& pt : pre_tokens) {
            if (model) {
                auto toks = model->tokenizeWithOffsets(pt.text);
                for (const auto& t : toks) {
                    result.ids.push_back(t.id);
                    result.tokens.push_back(model->idToToken(t.id));
                    // Compose: pre-token offset + model-internal offset
                    result.offsets.push_back({pt.start + t.start, pt.start + t.end});
                }
            }
        }
    } else if (model) {
        auto toks = model->tokenizeWithOffsets(normalized);
        for (const auto& t : toks) {
            result.ids.push_back(t.id);
            result.tokens.push_back(model->idToToken(t.id));
            result.offsets.push_back({t.start, t.end});
        }
    }

    return result;
}

QoreHFTokenizer::InternalEncoding QoreHFTokenizer::encodeText(
        const std::string& text) const {
    if (added_tokens.empty()) {
        return encodeSegment(text);
    }

    // Split text on added tokens, tokenize segments between them
    InternalEncoding result;
    std::string remaining = text;
    size_t base_offset = 0;  // tracks position in original text

    while (!remaining.empty()) {
        size_t best_pos = std::string::npos;
        size_t best_len = 0;
        int best_id = -1;

        for (const auto& at : added_tokens) {
            if (at.content.empty()) {
                continue;
            }
            size_t pos = remaining.find(at.content);
            if (pos != std::string::npos && (pos < best_pos
                    || (pos == best_pos && at.content.size() > best_len))) {
                best_pos = pos;
                best_len = at.content.size();
                best_id = at.id;
            }
        }

        if (best_pos == std::string::npos) {
            auto seg = encodeSegment(remaining);
            // Adjust offsets by base_offset
            for (auto& off : seg.offsets) {
                off.first += base_offset;
                off.second += base_offset;
            }
            result.ids.insert(result.ids.end(), seg.ids.begin(), seg.ids.end());
            result.tokens.insert(result.tokens.end(), seg.tokens.begin(), seg.tokens.end());
            result.offsets.insert(result.offsets.end(), seg.offsets.begin(), seg.offsets.end());
            break;
        }

        if (best_pos > 0) {
            std::string before = remaining.substr(0, best_pos);
            auto seg = encodeSegment(before);
            for (auto& off : seg.offsets) {
                off.first += base_offset;
                off.second += base_offset;
            }
            result.ids.insert(result.ids.end(), seg.ids.begin(), seg.ids.end());
            result.tokens.insert(result.tokens.end(), seg.tokens.begin(), seg.tokens.end());
            result.offsets.insert(result.offsets.end(), seg.offsets.begin(), seg.offsets.end());
        }

        // Insert the added token with its offset in the original text
        result.ids.push_back(best_id);
        result.tokens.push_back(model ? model->idToToken(best_id) : "");
        result.offsets.push_back({base_offset + best_pos,
            base_offset + best_pos + best_len});

        base_offset += best_pos + best_len;
        remaining = remaining.substr(best_pos + best_len);
    }

    return result;
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
    InternalEncoding enc_a = encodeText(text_str);

    // Encode second text (optional)
    InternalEncoding enc_b;
    bool has_pair = false;
    if (text_pair && text_pair->size() > 0) {
        enc_b = encodeText(text_pair->c_str());
        has_pair = true;
    }

    // Step 3: Post-process (add special tokens) with offset tracking
    EncodingResult post_result;

    if (post_processor && add_special_tokens) {
        post_result = post_processor->processWithOffsets(
            enc_a.ids,
            has_pair ? enc_b.ids : std::vector<int>(),
            enc_a.tokens,
            has_pair ? enc_b.tokens : std::vector<std::string>(),
            enc_a.offsets,
            has_pair ? enc_b.offsets : std::vector<std::pair<size_t, size_t>>(),
            true);
    } else {
        post_result.ids = std::move(enc_a.ids);
        post_result.tokens = std::move(enc_a.tokens);
        post_result.offsets = std::move(enc_a.offsets);
        if (has_pair) {
            post_result.ids.insert(post_result.ids.end(),
                enc_b.ids.begin(), enc_b.ids.end());
            post_result.tokens.insert(post_result.tokens.end(),
                enc_b.tokens.begin(), enc_b.tokens.end());
            post_result.offsets.insert(post_result.offsets.end(),
                enc_b.offsets.begin(), enc_b.offsets.end());
        }
        post_result.type_ids.resize(post_result.ids.size(), 0);
        post_result.special_tokens_mask.resize(post_result.ids.size(), 0);
    }

    // Ensure tokens are populated
    if (post_result.tokens.empty() && model) {
        for (int id : post_result.ids) {
            post_result.tokens.push_back(model->idToToken(id));
        }
    }

    // Build result hash
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    // input_ids
    ReferenceHolder<QoreListNode> id_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int id : post_result.ids) {
        id_list->push(id, xsink);
    }
    result->setKeyValue("input_ids", id_list.release(), xsink);

    // token_type_ids
    ReferenceHolder<QoreListNode> type_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int tid : post_result.type_ids) {
        type_list->push(tid, xsink);
    }
    result->setKeyValue("token_type_ids", type_list.release(), xsink);

    // attention_mask (all 1s for real tokens)
    ReferenceHolder<QoreListNode> mask_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (size_t i = 0; i < post_result.ids.size(); ++i) {
        mask_list->push(1, xsink);
    }
    result->setKeyValue("attention_mask", mask_list.release(), xsink);

    // tokens
    ReferenceHolder<QoreListNode> tok_list(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& t : post_result.tokens) {
        tok_list->push(new QoreStringNode(t), xsink);
    }
    result->setKeyValue("tokens", tok_list.release(), xsink);

    // offset_mapping — list of (start, end) pairs
    ReferenceHolder<QoreListNode> off_list(new QoreListNode(autoTypeInfo), xsink);
    for (const auto& off : post_result.offsets) {
        ReferenceHolder<QoreListNode> pair(new QoreListNode(bigIntTypeInfo), xsink);
        pair->push((int64)off.first, xsink);
        pair->push((int64)off.second, xsink);
        off_list->push(pair.release(), xsink);
    }
    result->setKeyValue("offset_mapping", off_list.release(), xsink);

    // special_tokens_mask
    ReferenceHolder<QoreListNode> stm_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int m : post_result.special_tokens_mask) {
        stm_list->push(m, xsink);
    }
    result->setKeyValue("special_tokens_mask", stm_list.release(), xsink);

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

QoreHashNode* QoreHFTokenizer::getVocab(ExceptionSink* xsink) const {
    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }

    auto vocab = model->getVocab();

    // Include added tokens
    for (const auto& at : added_tokens) {
        if (!at.content.empty()) {
            vocab[at.content] = at.id;
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(bigIntTypeInfo), xsink);
    for (const auto& entry : vocab) {
        result->setKeyValue(entry.first.c_str(), entry.second, xsink);
    }
    return result.release();
}

} // namespace QoreTokenizer
