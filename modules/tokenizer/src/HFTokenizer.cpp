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

    // Step 2: Pre-tokenize + model tokenize with offset and word_id tracking
    if (pre_tokenizer) {
        auto pre_tokens = pre_tokenizer->pretokenize(normalized);
        for (int word_idx = 0; word_idx < (int)pre_tokens.size(); ++word_idx) {
            const auto& pt = pre_tokens[word_idx];
            if (model) {
                auto toks = model->tokenizeWithOffsets(pt.text);
                for (const auto& t : toks) {
                    result.ids.push_back(t.id);
                    result.tokens.push_back(model->idToToken(t.id));
                    // Compose: pre-token offset + model-internal offset
                    result.offsets.push_back({pt.start + t.start, pt.start + t.end});
                    result.word_ids.push_back(word_idx);
                }
            }
        }
    } else if (model) {
        // No pre-tokenizer: entire text is one word (word_id = 0)
        auto toks = model->tokenizeWithOffsets(normalized);
        for (const auto& t : toks) {
            result.ids.push_back(t.id);
            result.tokens.push_back(model->idToToken(t.id));
            result.offsets.push_back({t.start, t.end});
            result.word_ids.push_back(0);
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
    int word_id_offset = 0;  // tracks word index across segments

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
            // Adjust offsets and word_ids by base values
            for (auto& off : seg.offsets) {
                off.first += base_offset;
                off.second += base_offset;
            }
            for (auto& wid : seg.word_ids) {
                if (wid >= 0) {
                    wid += word_id_offset;
                }
            }
            result.ids.insert(result.ids.end(), seg.ids.begin(), seg.ids.end());
            result.tokens.insert(result.tokens.end(), seg.tokens.begin(), seg.tokens.end());
            result.offsets.insert(result.offsets.end(), seg.offsets.begin(), seg.offsets.end());
            result.word_ids.insert(result.word_ids.end(), seg.word_ids.begin(), seg.word_ids.end());
            break;
        }

        if (best_pos > 0) {
            std::string before = remaining.substr(0, best_pos);
            auto seg = encodeSegment(before);
            for (auto& off : seg.offsets) {
                off.first += base_offset;
                off.second += base_offset;
            }
            // Find max word_id in this segment to update offset
            int max_wid = -1;
            for (auto& wid : seg.word_ids) {
                if (wid >= 0) {
                    if (wid > max_wid) {
                        max_wid = wid;
                    }
                    wid += word_id_offset;
                }
            }
            result.ids.insert(result.ids.end(), seg.ids.begin(), seg.ids.end());
            result.tokens.insert(result.tokens.end(), seg.tokens.begin(), seg.tokens.end());
            result.offsets.insert(result.offsets.end(), seg.offsets.begin(), seg.offsets.end());
            result.word_ids.insert(result.word_ids.end(), seg.word_ids.begin(), seg.word_ids.end());
            if (max_wid >= 0) {
                word_id_offset += max_wid + 1;
            }
        }

        // Insert the added token with its offset in the original text
        // Added tokens get word_id = -1 (not a word)
        result.ids.push_back(best_id);
        result.tokens.push_back(model ? model->idToToken(best_id) : "");
        result.offsets.push_back({base_offset + best_pos,
            base_offset + best_pos + best_len});
        result.word_ids.push_back(-1);

        base_offset += best_pos + best_len;
        remaining = remaining.substr(best_pos + best_len);
    }

    return result;
}

QoreHashNode* QoreHFTokenizer::encode(const QoreStringNode* text,
        const QoreStringNode* text_pair, bool add_special_tokens,
        ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    // Cooperative cancellation check
    if (qore_check_cancel(xsink, "encoding text")) {
        return nullptr;
    }

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

    // Step 3: Post-process (add special tokens) with offset and word_id tracking
    EncodingResult post_result;

    if (post_processor && add_special_tokens) {
        post_result = post_processor->processWithOffsets(
            enc_a.ids,
            has_pair ? enc_b.ids : std::vector<int>(),
            enc_a.tokens,
            has_pair ? enc_b.tokens : std::vector<std::string>(),
            enc_a.offsets,
            has_pair ? enc_b.offsets : std::vector<std::pair<size_t, size_t>>(),
            enc_a.word_ids,
            has_pair ? enc_b.word_ids : std::vector<int>(),
            true);
    } else {
        post_result.ids = std::move(enc_a.ids);
        post_result.tokens = std::move(enc_a.tokens);
        post_result.offsets = std::move(enc_a.offsets);
        post_result.word_ids = std::move(enc_a.word_ids);
        if (has_pair) {
            post_result.ids.insert(post_result.ids.end(),
                enc_b.ids.begin(), enc_b.ids.end());
            post_result.tokens.insert(post_result.tokens.end(),
                enc_b.tokens.begin(), enc_b.tokens.end());
            post_result.offsets.insert(post_result.offsets.end(),
                enc_b.offsets.begin(), enc_b.offsets.end());
            post_result.word_ids.insert(post_result.word_ids.end(),
                enc_b.word_ids.begin(), enc_b.word_ids.end());
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

    // Build attention_mask (all 1s for encode — no padding)
    std::vector<int> attention_mask(post_result.ids.size(), 1);

    return buildEncodingHash(post_result, attention_mask, xsink);
}

QoreStringNode* QoreHFTokenizer::decode(const QoreListNode* ids,
        bool skip_special_tokens, ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

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
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    if (!model) {
        return 0;
    }
    return model->vocabSize() + dynamic_added_count;
}

QoreStringNode* QoreHFTokenizer::idToToken(int id, ExceptionSink* xsink) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return nullptr;
    }
    std::string token = model->idToToken(id);
    if (token.empty()) {
        // Check added tokens
        for (const auto& at : added_tokens) {
            if (at.id == id) {
                return new QoreStringNode(at.content);
            }
        }
    }
    return new QoreStringNode(token);
}

int QoreHFTokenizer::tokenToId(const QoreStringNode* token) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    if (!model || !token) {
        return -1;
    }
    int id = model->tokenToId(token->c_str());
    if (id >= 0) {
        return id;
    }
    // Check added tokens
    std::string tok_str = token->c_str();
    for (const auto& at : added_tokens) {
        if (at.content == tok_str) {
            return at.id;
        }
    }
    return -1;
}

QoreHashNode* QoreHFTokenizer::getVocab(ExceptionSink* xsink) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

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
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return encodeAdvancedIntern(text, options, xsink);
}

QoreHashNode* QoreHFTokenizer::encodeAdvancedIntern(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "encoding text")) {
        return nullptr;
    }
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
    bool is_pretokenized = options
        ? options->getKeyValue("is_pretokenized").getAsBool() : false;
    int stride = options
        ? (int)options->getKeyValue("stride").getAsBigInt() : 0;
    if (options && options->getKeyValue("stride").isNullOrNothing()) {
        stride = 0;
    }
    bool return_overflowing = options
        ? options->getKeyValue("return_overflowing_tokens").getAsBool() : false;

    // Encode first text
    InternalEncoding enc_a;
    if (is_pretokenized) {
        // Pre-tokenized: use words list from options, or split text on whitespace
        const QoreListNode* words_list = options
            ? safeGetList(options->getKeyValue("words")) : nullptr;
        std::vector<std::string> words;
        if (words_list) {
            ConstListIterator wli(words_list);
            while (wli.next()) {
                const QoreStringNode* w = safeGetString(wli.getValue());
                if (w) {
                    words.push_back(w->c_str());
                }
            }
        } else {
            // Fallback: split on whitespace
            std::string text_str = text ? text->c_str() : "";
            std::string word;
            for (char c : text_str) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    if (!word.empty()) {
                        words.push_back(word);
                        word.clear();
                    }
                } else {
                    word += c;
                }
            }
            if (!word.empty()) {
                words.push_back(word);
            }
        }
        enc_a = encodePreTokenizedWords(words);
    } else {
        enc_a = encodeText(text ? text->c_str() : "");
    }

    // Encode second text (optional)
    InternalEncoding enc_b;
    bool has_pair = false;
    if (text_pair_node && text_pair_node->size() > 0) {
        if (is_pretokenized) {
            // For pair, also treat as pre-tokenized words (split on whitespace)
            std::string pair_str = text_pair_node->c_str();
            std::vector<std::string> pair_words;
            std::string word;
            for (char c : pair_str) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    if (!word.empty()) {
                        pair_words.push_back(word);
                        word.clear();
                    }
                } else {
                    word += c;
                }
            }
            if (!word.empty()) {
                pair_words.push_back(word);
            }
            enc_b = encodePreTokenizedWords(pair_words);
        } else {
            enc_b = encodeText(text_pair_node->c_str());
        }
        has_pair = true;
    }

    // Save full encoding for sliding window overflow generation
    InternalEncoding enc_a_full, enc_b_full;
    bool truncated = false;
    int num_special = 0;
    int available = 0;

    // Apply truncation BEFORE post-processing
    if (max_length > 0 && !truncation.empty()) {
        if (post_processor && add_special) {
            num_special = post_processor->numAddedTokens(has_pair);
        }
        available = max_length - num_special;
        if (available < 0) {
            available = 0;
        }

        int total = (int)enc_a.ids.size() + (int)enc_b.ids.size();
        if (total > available) {
            // Save full encodings for overflow
            if (return_overflowing && stride > 0) {
                enc_a_full = enc_a;
                enc_b_full = enc_b;
            }
            truncated = true;

            if (truncation == "only_first" || !has_pair) {
                int keep_a = available - (int)enc_b.ids.size();
                if (keep_a < 0) {
                    keep_a = 0;
                }
                enc_a.ids.resize(keep_a);
                enc_a.tokens.resize(keep_a);
                enc_a.offsets.resize(keep_a);
                enc_a.word_ids.resize(keep_a);
            } else if (truncation == "only_second" && has_pair) {
                int keep_b = available - (int)enc_a.ids.size();
                if (keep_b < 0) {
                    keep_b = 0;
                }
                enc_b.ids.resize(keep_b);
                enc_b.tokens.resize(keep_b);
                enc_b.offsets.resize(keep_b);
                enc_b.word_ids.resize(keep_b);
            } else {
                // longest_first: trim the longer sequence iteratively
                while ((int)(enc_a.ids.size() + enc_b.ids.size()) > available) {
                    if (enc_a.ids.size() >= enc_b.ids.size() && !enc_a.ids.empty()) {
                        enc_a.ids.pop_back();
                        enc_a.tokens.pop_back();
                        enc_a.offsets.pop_back();
                        enc_a.word_ids.pop_back();
                    } else if (!enc_b.ids.empty()) {
                        enc_b.ids.pop_back();
                        enc_b.tokens.pop_back();
                        enc_b.offsets.pop_back();
                        enc_b.word_ids.pop_back();
                    } else {
                        break;
                    }
                }
            }
        }
    }

    // Helper lambda: post-process an InternalEncoding pair
    auto postProcess = [&](InternalEncoding& ea, InternalEncoding& eb,
            bool hp) -> EncodingResult {
        EncodingResult pr;
        if (post_processor && add_special) {
            pr = post_processor->processWithOffsets(
                ea.ids,
                hp ? eb.ids : std::vector<int>(),
                ea.tokens,
                hp ? eb.tokens : std::vector<std::string>(),
                ea.offsets,
                hp ? eb.offsets : std::vector<std::pair<size_t, size_t>>(),
                ea.word_ids,
                hp ? eb.word_ids : std::vector<int>(),
                true);
        } else {
            pr.ids = std::move(ea.ids);
            pr.tokens = std::move(ea.tokens);
            pr.offsets = std::move(ea.offsets);
            pr.word_ids = std::move(ea.word_ids);
            if (hp) {
                pr.ids.insert(pr.ids.end(), eb.ids.begin(), eb.ids.end());
                pr.tokens.insert(pr.tokens.end(), eb.tokens.begin(), eb.tokens.end());
                pr.offsets.insert(pr.offsets.end(), eb.offsets.begin(), eb.offsets.end());
                pr.word_ids.insert(pr.word_ids.end(), eb.word_ids.begin(), eb.word_ids.end());
            }
            pr.type_ids.resize(pr.ids.size(), 0);
            pr.special_tokens_mask.resize(pr.ids.size(), 0);
        }
        return pr;
    };

    // Helper lambda: apply padding and build attention_mask, then return hash
    auto finishEncoding = [&](EncodingResult& pr) -> QoreHashNode* {
        int effective_pad = override_pad_id >= 0 ? override_pad_id : pad_token_id;
        if (padding == "max_length" && max_length > 0
                && (int)pr.ids.size() < max_length) {
            if (effective_pad < 0) {
                xsink->raiseException("TOKENIZER-ERROR",
                    "padding requested but no pad_token_id available");
                return nullptr;
            }
            while ((int)pr.ids.size() < max_length) {
                pr.ids.push_back(effective_pad);
                pr.type_ids.push_back(0);
                pr.tokens.push_back(model->idToToken(effective_pad));
                pr.offsets.push_back({0, 0});
                pr.special_tokens_mask.push_back(1);
                pr.word_ids.push_back(-1);
            }
        }

        std::vector<int> attn_mask(pr.ids.size(), 1);
        if (padding == "max_length" && max_length > 0 && effective_pad >= 0) {
            for (int i = (int)pr.ids.size() - 1; i >= 0; --i) {
                if (pr.ids[i] == effective_pad) {
                    attn_mask[i] = 0;
                } else {
                    break;
                }
            }
        }

        return buildEncodingHash(pr, attn_mask, xsink);
    };

    // Build primary encoding
    EncodingResult post_result = postProcess(enc_a, enc_b, has_pair);
    QoreHashNode* result = finishEncoding(post_result);
    if (!result || *xsink) {
        return nullptr;
    }

    // Generate overflow chunks if sliding window is requested
    if (return_overflowing && stride > 0 && truncated && available > 0) {
        ReferenceHolder<QoreListNode> overflow_list(new QoreListNode(autoTypeInfo), xsink);

        // Sliding window over sequence A (the primary truncation target)
        int window = available - (int)enc_b_full.ids.size();
        if (window <= 0) {
            window = available;
        }
        int step = window - stride;
        if (step <= 0) {
            step = 1;
        }

        int total_a = (int)enc_a_full.ids.size();
        // Start from the second window (first window is the primary encoding)
        for (int start = step; start < total_a; start += step) {
            int end = start + window;
            if (end > total_a) {
                end = total_a;
            }

            InternalEncoding chunk_a;
            chunk_a.ids.assign(enc_a_full.ids.begin() + start,
                enc_a_full.ids.begin() + end);
            chunk_a.tokens.assign(enc_a_full.tokens.begin() + start,
                enc_a_full.tokens.begin() + end);
            chunk_a.offsets.assign(enc_a_full.offsets.begin() + start,
                enc_a_full.offsets.begin() + end);
            chunk_a.word_ids.assign(enc_a_full.word_ids.begin() + start,
                enc_a_full.word_ids.begin() + end);

            InternalEncoding chunk_b = enc_b_full;
            EncodingResult chunk_result = postProcess(chunk_a, chunk_b, has_pair);
            QoreHashNode* chunk_hash = finishEncoding(chunk_result);
            if (!chunk_hash || *xsink) {
                result->deref(xsink);
                return nullptr;
            }
            overflow_list->push(chunk_hash, xsink);
        }

        if (overflow_list->size() > 0) {
            result->setKeyValue("overflowing", overflow_list.release(), xsink);
        }
    }

    return result;
}

QoreListNode* QoreHFTokenizer::encodeBatch(const QoreListNode* texts,
        const QoreHashNode* options, ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

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
        QoreHashNode* enc = encodeAdvancedIntern(t, options, xsink);
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

                QoreListNode* wids = const_cast<QoreListNode*>(
                    safeGetList(enc->getKeyValue("word_ids")));
                if (wids) {
                    wids->push(QoreValue(), xsink);  // NOTHING for padding
                }

                ++cur_len;
            }
        }
        results->push(enc, xsink);
    }

    return results.release();
}

QoreHashNode* QoreHFTokenizer::buildEncodingHash(const EncodingResult& post_result,
        const std::vector<int>& attention_mask, ExceptionSink* xsink) const {
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

    ReferenceHolder<QoreListNode> wid_list(new QoreListNode(autoTypeInfo), xsink);
    for (int wid : post_result.word_ids) {
        if (wid < 0) {
            wid_list->push(QoreValue(), xsink);
        } else {
            wid_list->push(wid, xsink);
        }
    }
    result->setKeyValue("word_ids", wid_list.release(), xsink);

    return result.release();
}

QoreHFTokenizer::InternalEncoding QoreHFTokenizer::encodePreTokenizedWords(
        const std::vector<std::string>& words) const {
    InternalEncoding result;
    size_t byte_offset = 0;

    for (int word_idx = 0; word_idx < (int)words.size(); ++word_idx) {
        const std::string& word = words[word_idx];

        // Check if this word matches an added token exactly
        bool is_added = false;
        for (const auto& at : added_tokens) {
            if (at.content == word) {
                result.ids.push_back(at.id);
                result.tokens.push_back(word);
                result.offsets.push_back({byte_offset, byte_offset + word.size()});
                result.word_ids.push_back(at.special ? -1 : word_idx);
                is_added = true;
                break;
            }
        }

        if (!is_added && model) {
            // Tokenize directly through model (no normalize, no pre-tokenize)
            auto toks = model->tokenizeWithOffsets(word);
            for (const auto& t : toks) {
                result.ids.push_back(t.id);
                result.tokens.push_back(model->idToToken(t.id));
                result.offsets.push_back({byte_offset + t.start, byte_offset + t.end});
                result.word_ids.push_back(word_idx);
            }
        }

        byte_offset += word.size() + 1;  // +1 for implied separator
    }

    return result;
}

int QoreHFTokenizer::addTokens(const QoreListNode* tokens, ExceptionSink* xsink) {
    if (!tokens || !model) {
        return 0;
    }

    std::unique_lock<std::shared_mutex> lock(rw_mutex);

    int added = 0;
    int next_id = model->vocabSize();

    // Account for existing added_tokens that may have IDs beyond model vocab
    for (const auto& at : added_tokens) {
        if (at.id >= next_id) {
            next_id = at.id + 1;
        }
    }

    ConstListIterator li(tokens);
    while (li.next()) {
        QoreValue v = li.getValue();
        std::string content;
        bool special = false;
        bool single_word = false;

        if (v.getType() == NT_STRING) {
            content = v.get<const QoreStringNode>()->c_str();
        } else if (v.getType() == NT_HASH) {
            const QoreHashNode* h = v.get<const QoreHashNode>();
            const QoreStringNode* c = safeGetString(h->getKeyValue("content"));
            content = c ? c->c_str() : "";
            special = h->getKeyValue("special").getAsBool();
            single_word = h->getKeyValue("single_word").getAsBool();
        } else {
            continue;
        }

        if (content.empty()) {
            continue;
        }

        // Skip if already in model vocab
        if (model->tokenToId(content) >= 0) {
            continue;
        }
        // Skip if already in added_tokens
        bool already_added = false;
        for (const auto& at : added_tokens) {
            if (at.content == content) {
                already_added = true;
                break;
            }
        }
        if (already_added) {
            continue;
        }

        AddedToken tok;
        tok.content = content;
        tok.id = next_id++;
        tok.special = special;
        tok.single_word = single_word;

        if (tok.special) {
            special_token_ids.insert(tok.id);
        }

        added_tokens.push_back(std::move(tok));
        ++added;
        ++dynamic_added_count;
    }

    return added;
}

} // namespace QoreTokenizer
