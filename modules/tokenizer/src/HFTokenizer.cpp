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

    // Resolve pad_token_id from added tokens
    for (const auto& at : added_tokens) {
        if (at.content == "[PAD]" || at.content == "<pad>"
                || at.content == "<|endoftext|>") {
            pad_token_id = at.id;
            break;
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

QoreHashNode* QoreHFTokenizer::encodeAdvanced(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink) {
    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }

    // Parse options
    const QoreStringNode* text_pair_node = options
        ? safeGetString(options->getKeyValue("text_pair")) : nullptr;
    int max_length = options
        ? (int)options->getKeyValue("max_length").getAsBigInt() : 0;
    std::string truncation = options
        ? safeGetStringKey(options, "truncation") : "";
    std::string padding = options
        ? safeGetStringKey(options, "padding") : "";
    bool add_special = options
        ? options->getKeyValue("add_special_tokens").getAsBool() : true;
    // Default add_special_tokens to true if not specified
    if (options && options->getKeyValue("add_special_tokens").isNullOrNothing()) {
        add_special = true;
    }
    int override_pad_id = options
        ? (int)options->getKeyValue("pad_token_id").getAsBigInt() : -1;
    if (options && options->getKeyValue("pad_token_id").isNullOrNothing()) {
        override_pad_id = -1;
    }

    // Encode first text
    InternalEncoding enc_a = encodeText(text ? text->c_str() : "");

    // Encode second text (optional)
    InternalEncoding enc_b;
    bool has_pair = false;
    if (text_pair_node && text_pair_node->size() > 0) {
        enc_b = encodeText(text_pair_node->c_str());
        has_pair = true;
    }

    // Apply truncation BEFORE post-processing
    if (max_length > 0 && !truncation.empty()) {
        int num_special = 0;
        if (post_processor && add_special) {
            num_special = post_processor->numAddedTokens(has_pair);
        }
        int available = max_length - num_special;
        if (available < 0) {
            available = 0;
        }

        int total = (int)enc_a.ids.size() + (int)enc_b.ids.size();
        if (total > available) {
            if (truncation == "only_first" || !has_pair) {
                int keep_a = available - (int)enc_b.ids.size();
                if (keep_a < 0) {
                    keep_a = 0;
                }
                enc_a.ids.resize(keep_a);
                enc_a.tokens.resize(keep_a);
                enc_a.offsets.resize(keep_a);
            } else if (truncation == "only_second" && has_pair) {
                int keep_b = available - (int)enc_a.ids.size();
                if (keep_b < 0) {
                    keep_b = 0;
                }
                enc_b.ids.resize(keep_b);
                enc_b.tokens.resize(keep_b);
                enc_b.offsets.resize(keep_b);
            } else {
                // longest_first: trim the longer sequence iteratively
                while ((int)(enc_a.ids.size() + enc_b.ids.size()) > available) {
                    if (enc_a.ids.size() >= enc_b.ids.size() && !enc_a.ids.empty()) {
                        enc_a.ids.pop_back();
                        enc_a.tokens.pop_back();
                        enc_a.offsets.pop_back();
                    } else if (!enc_b.ids.empty()) {
                        enc_b.ids.pop_back();
                        enc_b.tokens.pop_back();
                        enc_b.offsets.pop_back();
                    } else {
                        break;
                    }
                }
            }
        }
    }

    // Post-process
    EncodingResult post_result;
    if (post_processor && add_special) {
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

    // Apply padding
    int effective_pad_id = override_pad_id >= 0 ? override_pad_id : pad_token_id;
    if (padding == "max_length" && max_length > 0
            && (int)post_result.ids.size() < max_length) {
        if (effective_pad_id < 0) {
            xsink->raiseException("TOKENIZER-ERROR",
                "padding requested but no pad_token_id available");
            return nullptr;
        }
        while ((int)post_result.ids.size() < max_length) {
            post_result.ids.push_back(effective_pad_id);
            post_result.type_ids.push_back(0);
            post_result.tokens.push_back(model->idToToken(effective_pad_id));
            post_result.offsets.push_back({0, 0});
            post_result.special_tokens_mask.push_back(1);
        }
    }

    // Build attention_mask (1 for real tokens, 0 for padding)
    std::vector<int> attention_mask;
    attention_mask.reserve(post_result.ids.size());
    for (size_t i = 0; i < post_result.ids.size(); ++i) {
        bool is_pad = (effective_pad_id >= 0 && post_result.ids[i] == effective_pad_id
            && i >= (post_result.ids.size() - (max_length > 0 ? max_length : 0)));
        // Simpler: if we padded, last N tokens are padding
        attention_mask.push_back(1);
    }
    // Fix attention_mask for padded tokens
    if (padding == "max_length" && max_length > 0 && effective_pad_id >= 0) {
        // Walk backwards and set 0 for trailing pad tokens
        for (int i = (int)post_result.ids.size() - 1; i >= 0; --i) {
            if (post_result.ids[i] == effective_pad_id) {
                attention_mask[i] = 0;
            } else {
                break;
            }
        }
    }

    // Build result hash
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    ReferenceHolder<QoreListNode> id_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int id : post_result.ids) {
        id_list->push(id, xsink);
    }
    result->setKeyValue("input_ids", id_list.release(), xsink);

    ReferenceHolder<QoreListNode> type_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int tid : post_result.type_ids) {
        type_list->push(tid, xsink);
    }
    result->setKeyValue("token_type_ids", type_list.release(), xsink);

    ReferenceHolder<QoreListNode> mask_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int m : attention_mask) {
        mask_list->push(m, xsink);
    }
    result->setKeyValue("attention_mask", mask_list.release(), xsink);

    ReferenceHolder<QoreListNode> tok_list(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& t : post_result.tokens) {
        tok_list->push(new QoreStringNode(t), xsink);
    }
    result->setKeyValue("tokens", tok_list.release(), xsink);

    ReferenceHolder<QoreListNode> off_list(new QoreListNode(autoTypeInfo), xsink);
    for (const auto& off : post_result.offsets) {
        ReferenceHolder<QoreListNode> pair(new QoreListNode(bigIntTypeInfo), xsink);
        pair->push((int64)off.first, xsink);
        pair->push((int64)off.second, xsink);
        off_list->push(pair.release(), xsink);
    }
    result->setKeyValue("offset_mapping", off_list.release(), xsink);

    ReferenceHolder<QoreListNode> stm_list(new QoreListNode(bigIntTypeInfo), xsink);
    for (int m : post_result.special_tokens_mask) {
        stm_list->push(m, xsink);
    }
    result->setKeyValue("special_tokens_mask", stm_list.release(), xsink);

    return result.release();
}

QoreListNode* QoreHFTokenizer::encodeBatch(const QoreListNode* texts,
        const QoreHashNode* options, ExceptionSink* xsink) {
    if (!texts) {
        return new QoreListNode(autoTypeInfo);
    }

    // Encode each text individually
    ReferenceHolder<QoreListNode> results(new QoreListNode(autoTypeInfo), xsink);
    int max_len_in_batch = 0;

    // First pass: encode all
    std::vector<QoreHashNode*> encodings;
    ConstListIterator li(texts);
    while (li.next()) {
        const QoreStringNode* t = li.getValue().get<const QoreStringNode>();
        QoreHashNode* enc = encodeAdvanced(t, options, xsink);
        if (*xsink) {
            // Clean up already encoded
            for (auto* e : encodings) {
                e->deref(xsink);
            }
            return nullptr;
        }
        encodings.push_back(enc);
        const QoreListNode* ids = safeGetList(enc->getKeyValue("input_ids"));
        if (ids && (int)ids->size() > max_len_in_batch) {
            max_len_in_batch = (int)ids->size();
        }
    }

    // Second pass: pad to batch max if padding == "True" or "batch_longest"
    std::string padding = options ? safeGetStringKey(options, "padding") : "";
    int effective_pad_id = pad_token_id;
    if (options) {
        QoreValue pid = options->getKeyValue("pad_token_id");
        if (!pid.isNullOrNothing()) {
            effective_pad_id = (int)pid.getAsBigInt();
        }
    }

    bool batch_pad = (padding == "True" || padding == "true"
        || padding == "batch_longest");

    for (auto* enc : encodings) {
        if (batch_pad && effective_pad_id >= 0) {
            QoreListNode* ids = const_cast<QoreListNode*>(
                safeGetList(enc->getKeyValue("input_ids")));
            int cur_len = ids ? (int)ids->size() : 0;
            while (cur_len < max_len_in_batch) {
                // Append padding to each array in the hash
                // For simplicity, just add to input_ids and attention_mask
                ids->push(effective_pad_id, xsink);

                QoreListNode* attn = const_cast<QoreListNode*>(
                    safeGetList(enc->getKeyValue("attention_mask")));
                if (attn) {
                    attn->push(0, xsink);
                }

                QoreListNode* tids = const_cast<QoreListNode*>(
                    safeGetList(enc->getKeyValue("token_type_ids")));
                if (tids) {
                    tids->push(0, xsink);
                }

                QoreListNode* stm = const_cast<QoreListNode*>(
                    safeGetList(enc->getKeyValue("special_tokens_mask")));
                if (stm) {
                    stm->push(1, xsink);
                }

                ++cur_len;
            }
        }
        results->push(enc, xsink);
    }

    return results.release();
}

} // namespace QoreTokenizer
