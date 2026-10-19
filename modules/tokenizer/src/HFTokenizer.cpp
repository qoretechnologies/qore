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
#include <cassert>
#include <limits>
#include <mutex>
#include <string_view>

extern const TypedHashDecl* hashdeclTokenizerModelInputBatch;

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
            tok.content = safeGetStdString(at->getKeyValue("content"));
            tok.id = (int)at->getKeyValue("id").getAsBigInt();
            tok.special = at->getKeyValue("special").getAsBool();
            tok.single_word = at->getKeyValue("single_word").getAsBool();

            if (tok.special) {
                special_token_ids.insert(tok.id);
            }
            addAddedToken(std::move(tok));
        }
    }

    // Resolve special token IDs from added tokens
    for (const auto& at : added_tokens) {
        // PAD
        if (pad_token_id < 0 && (at.content == "[PAD]" || at.content == "<pad>"
                || at.content == "<|endoftext|>")) {
            pad_token_id = at.id;
        }
        // EOS
        if (eos_token_id < 0 && (at.content == "</s>" || at.content == "<|endoftext|>"
                || at.content == "<|end_of_text|>" || at.content == "[EOS]"
                || at.content == "<eos>")) {
            eos_token_id = at.id;
        }
        // BOS
        if (bos_token_id < 0 && (at.content == "<s>" || at.content == "<|startoftext|>"
                || at.content == "<|begin_of_text|>" || at.content == "[BOS]"
                || at.content == "<bos>")) {
            bos_token_id = at.id;
        }
        // CLS
        if (cls_token_id < 0 && at.content == "[CLS]") {
            cls_token_id = at.id;
        }
        // SEP
        if (sep_token_id < 0 && at.content == "[SEP]") {
            sep_token_id = at.id;
        }
        // UNK
        if (unk_token_id < 0 && (at.content == "[UNK]" || at.content == "<unk>")) {
            unk_token_id = at.id;
        }
    }
}

std::vector<int> QoreHFTokenizer::tokenizePreToken(const std::string& pre_token) const {
    if (!model) {
        return {};
    }
    return model->tokenize(pre_token);
}

void QoreHFTokenizer::addAddedToken(AddedToken&& tok) {
    int id = tok.id;
    // the token is stored first, so that the index never has an entry for a token that is not stored
    added_tokens.push_back(std::move(tok));
    added_token_index.emplace(id, added_tokens.size() - 1);
}

std::string QoreHFTokenizer::tokenForId(int id) const {
    if (model) {
        std::string token = model->idToToken(id);
        if (!token.empty()) {
            return token;
        }
    }
    auto i = added_token_index.find(id);
    return i == added_token_index.end() ? std::string() : added_tokens[i->second].content;
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
                    result.tokens.push_back(tokenForId(t.id));
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
            result.tokens.push_back(tokenForId(t.id));
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
        // the token is the content of the added token that was matched
        result.tokens.push_back(remaining.substr(best_pos, best_len));
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
    std::string text_str = qoreStdString(text);
    InternalEncoding enc_a = encodeText(text_str);

    // Encode second text (optional)
    InternalEncoding enc_b;
    bool has_pair = false;
    if (text_pair && text_pair->size() > 0) {
        enc_b = encodeText(qoreStdString(text_pair));
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
            post_result.tokens.push_back(tokenForId(id));
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
        tokens.push_back(tokenForId(id));
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
    return new QoreStringNode(tokenForId(id));
}

int QoreHFTokenizer::tokenToId(const QoreStringNode* token) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    if (!model || !token) {
        return -1;
    }
    std::string tok_str = qoreStdString(token);
    int id = model->tokenToId(tok_str);
    if (id >= 0) {
        return id;
    }
    // Check added tokens
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
    size_t n = 0;
    for (const auto& entry : vocab) {
        if (n && !(n % 100) && qore_check_cancel(xsink, "building tokenizer vocabulary")) {
            return nullptr;
        }
        ++n;
        // the tokens are the keys of the hash; a token with a NUL character cannot be a key
        if (QoreHashNode::checkKey(entry.first.data(), entry.first.size(), xsink)) {
            return nullptr;
        }
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
    std::string text_pair = options
        ? safeGetStdString(options->getKeyValue("text_pair")) : "";
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
                std::string w = safeGetStdString(wli.getValue());
                if (!w.empty()) {
                    words.push_back(std::move(w));
                }
            }
        } else {
            // Fallback: split on whitespace
            std::string text_str = qoreStdString(text);
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
        enc_a = encodeText(qoreStdString(text));
    }

    // Encode second text (optional)
    InternalEncoding enc_b;
    bool has_pair = false;
    if (!text_pair.empty()) {
        if (is_pretokenized) {
            // For pair, also treat as pre-tokenized words (split on whitespace)
            std::vector<std::string> pair_words;
            std::string word;
            for (char c : text_pair) {
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
            enc_b = encodeText(text_pair);
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
                pr.tokens.push_back(tokenForId(effective_pad));
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
        QoreStringNodeValueHelper t(li.getValue());
        QoreHashNode* enc = encodeAdvancedIntern(*t, options, xsink);
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

int QoreHFTokenizer::encodeForModelIntern(const QoreStringNode* text, const QoreHashNode* options,
        const QoreStringNode* text_pair_override, ModelInputEncoding& out, ExceptionSink* xsink) const {
    if (qore_check_cancel(xsink, "encoding model input")) {
        return -1;
    }
    if (!model) {
        xsink->raiseException("TOKENIZER-ERROR", "tokenizer model not initialized");
        return -1;
    }

    if (options && options->getKeyValue("return_overflowing_tokens").getAsBool()) {
        xsink->raiseException("TOKENIZER-ERROR",
            "encodeForModel() and encodeBatchForModel() return rectangular model inputs and do not support "
            "return_overflowing_tokens; use encodeAdvanced() for sliding-window token chunks");
        return -1;
    }

    std::string text_pair = text_pair_override
        ? qoreStdString(text_pair_override)
        : (options ? safeGetStdString(options->getKeyValue("text_pair")) : "");
    int max_length = options ? static_cast<int>(options->getKeyValue("max_length").getAsBigInt()) : 0;
    std::string truncation = options ? safeGetStringKey(options, "truncation") : "";
    std::string padding = options ? safeGetStringKey(options, "padding") : "";
    bool add_special = options ? options->getKeyValue("add_special_tokens").getAsBool() : true;
    if (options && options->getKeyValue("add_special_tokens").isNullOrNothing()) {
        add_special = true;
    }
    int override_pad_id = options ? static_cast<int>(options->getKeyValue("pad_token_id").getAsBigInt()) : -1;
    if (options && options->getKeyValue("pad_token_id").isNullOrNothing()) {
        override_pad_id = -1;
    }
    bool is_pretokenized = options ? options->getKeyValue("is_pretokenized").getAsBool() : false;

    InternalEncoding enc_a;
    if (is_pretokenized) {
        const QoreListNode* words_list = options ? safeGetList(options->getKeyValue("words")) : nullptr;
        std::vector<std::string> words;
        if (words_list) {
            ConstListIterator wli(words_list);
            size_t count = 0;
            while (wli.next()) {
                if (count && !(count % 100) && qore_check_cancel(xsink, "encoding pre-tokenized words")) {
                    return -1;
                }
                std::string w = safeGetStdString(wli.getValue());
                if (!w.empty()) {
                    words.push_back(std::move(w));
                }
                ++count;
            }
        } else {
            std::string text_str = qoreStdString(text);
            std::string word;
            for (size_t i = 0; i < text_str.size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "splitting pre-tokenized text")) {
                    return -1;
                }
                char c = text_str[i];
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
        enc_a = encodeText(qoreStdString(text));
    }

    InternalEncoding enc_b;
    bool has_pair = false;
    if (!text_pair.empty()) {
        if (is_pretokenized) {
            std::vector<std::string> pair_words;
            std::string word;
            for (size_t i = 0; i < text_pair.size(); ++i) {
                if (i && !(i % 100) && qore_check_cancel(xsink, "splitting pre-tokenized text pair")) {
                    return -1;
                }
                char c = text_pair[i];
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
            enc_b = encodeText(text_pair);
        }
        has_pair = true;
    }

    if (max_length > 0 && !truncation.empty()) {
        int num_special = (post_processor && add_special) ? post_processor->numAddedTokens(has_pair) : 0;
        int available = max_length - num_special;
        if (available < 0) {
            available = 0;
        }

        int total = static_cast<int>(enc_a.ids.size() + enc_b.ids.size());
        if (total > available) {
            if (truncation == "only_first" || !has_pair) {
                int keep_a = available - static_cast<int>(enc_b.ids.size());
                if (keep_a < 0) {
                    keep_a = 0;
                }
                enc_a.ids.resize(keep_a);
                enc_a.tokens.resize(keep_a);
                enc_a.offsets.resize(keep_a);
                enc_a.word_ids.resize(keep_a);
            } else if (truncation == "only_second" && has_pair) {
                int keep_b = available - static_cast<int>(enc_a.ids.size());
                if (keep_b < 0) {
                    keep_b = 0;
                }
                enc_b.ids.resize(keep_b);
                enc_b.tokens.resize(keep_b);
                enc_b.offsets.resize(keep_b);
                enc_b.word_ids.resize(keep_b);
            } else {
                size_t trim_count = 0;
                while (static_cast<int>(enc_a.ids.size() + enc_b.ids.size()) > available) {
                    if (trim_count && !(trim_count % 100) && qore_check_cancel(xsink, "truncating model input")) {
                        return -1;
                    }
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
                    ++trim_count;
                }
            }
        }
    }

    EncodingResult post_result;
    if (post_processor && add_special) {
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
        post_result.type_ids.resize(post_result.ids.size(), 0);
        if (has_pair) {
            post_result.ids.insert(post_result.ids.end(), enc_b.ids.begin(), enc_b.ids.end());
            post_result.type_ids.resize(post_result.ids.size(), 0);
        }
    }

    int effective_pad = override_pad_id >= 0 ? override_pad_id : pad_token_id;
    if (padding == "max_length" && max_length > 0
            && static_cast<int>(post_result.ids.size()) < max_length) {
        if (effective_pad < 0) {
            xsink->raiseException("TOKENIZER-ERROR",
                "padding requested but no pad_token_id is available; pass pad_token_id explicitly");
            return -1;
        }
        out.sequence_length = post_result.ids.size();
        size_t pad_count = 0;
        while (static_cast<int>(post_result.ids.size()) < max_length) {
            if (pad_count && !(pad_count % 100) && qore_check_cancel(xsink, "padding model input")) {
                return -1;
            }
            post_result.ids.push_back(effective_pad);
            post_result.type_ids.push_back(0);
            ++pad_count;
        }
    } else {
        out.sequence_length = post_result.ids.size();
    }

    out.ids = std::move(post_result.ids);
    out.token_type_ids = std::move(post_result.type_ids);
    if (out.token_type_ids.size() < out.ids.size()) {
        out.token_type_ids.resize(out.ids.size(), 0);
    }

    out.attention_mask.assign(out.ids.size(), 1);
    if (padding == "max_length" && max_length > 0 && effective_pad >= 0) {
        size_t pad_scan_count = 0;
        for (int i = static_cast<int>(out.ids.size()) - 1; i >= 0; --i) {
            if (pad_scan_count && !(pad_scan_count % 100) && qore_check_cancel(xsink, "building model attention mask")) {
                return -1;
            }
            if (out.ids[static_cast<size_t>(i)] == effective_pad) {
                out.attention_mask[static_cast<size_t>(i)] = 0;
            } else {
                break;
            }
            ++pad_scan_count;
        }
    }
    return 0;
}

static QoreListNode* makeModelInputShapeList(size_t rows, size_t width, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> shape(new QoreListNode(bigIntTypeInfo), xsink);
    shape->push(static_cast<int64>(rows), xsink);
    shape->push(static_cast<int64>(width), xsink);
    return shape.release();
}

// returns the pad token ID of the options, else the default pad token ID; called with the tokenizer's lock held, so
// it must not call a public method that takes the lock
static int getModelInputPadId(int default_pad_id, const QoreHashNode* options) {
    if (options) {
        QoreValue pid = options->getKeyValue("pad_token_id");
        if (!pid.isNullOrNothing()) {
            return static_cast<int>(pid.getAsBigInt());
        }
    }
    return default_pad_id;
}

QoreHashNode* QoreHFTokenizer::encodeBatchForModelIntern(const QoreListNode* texts,
        const QoreListNode* text_pairs, const QoreHashNode* options, ExceptionSink* xsink) const {
    if (!texts) {
        xsink->raiseException("TOKENIZER-ERROR", "texts argument is required");
        return nullptr;
    }
    size_t rows = texts->size();
    if (text_pairs && text_pairs->size() != rows) {
        xsink->raiseException("TOKENIZER-ERROR",
            "text_pairs size %zu does not match texts size %zu", text_pairs->size(), rows);
        return nullptr;
    }
    if (rows > static_cast<size_t>(std::numeric_limits<int64>::max())) {
        xsink->raiseException("TOKENIZER-ERROR", "batch size exceeds int64 limits");
        return nullptr;
    }

    std::vector<ModelInputEncoding> encodings;
    encodings.reserve(rows);
    size_t max_width = 0;
    bool variable_width = false;
    ConstListIterator li(texts);
    size_t row = 0;
    while (li.next()) {
        if (row && !(row % 100) && qore_check_cancel(xsink, "batch model tokenization")) {
            return nullptr;
        }
        QoreStringNodeValueHelper text(li.getValue());
        QoreStringNodeValueHelper pair(text_pairs ? text_pairs->retrieveEntry(row) : QoreValue());
        ModelInputEncoding enc;
        if (encodeForModelIntern(*text, options, text_pairs ? *pair : nullptr, enc, xsink)) {
            return nullptr;
        }
        if (!encodings.empty() && enc.ids.size() != encodings.front().ids.size()) {
            variable_width = true;
        }
        if (enc.ids.size() > max_width) {
            max_width = enc.ids.size();
        }
        encodings.push_back(std::move(enc));
        ++row;
    }

    std::string padding = options ? safeGetStringKey(options, "padding") : "";
    int max_length = options ? static_cast<int>(options->getKeyValue("max_length").getAsBigInt()) : 0;
    if (padding == "max_length" && max_length > 0) {
        max_width = static_cast<size_t>(max_length);
    } else if ((padding == "false" || padding == "False" || padding == "none") && variable_width) {
        xsink->raiseException("TOKENIZER-ERROR",
            "encodeBatchForModel() requires rectangular tensors; remove padding=%s or use "
            "padding=\"batch_longest\"/\"max_length\" for variable-length inputs", padding.c_str());
        return nullptr;
    }

    if (max_width > static_cast<size_t>(std::numeric_limits<int64>::max())) {
        xsink->raiseException("TOKENIZER-ERROR", "encoded sequence length exceeds int64 limits");
        return nullptr;
    }
    if (rows && max_width > std::numeric_limits<size_t>::max() / rows) {
        xsink->raiseException("TOKENIZER-ERROR", "encoded tensor element count overflows size_t");
        return nullptr;
    }

    int pad_id = getModelInputPadId(pad_token_id, options);
    bool needs_padding = false;
    size_t scan_row = 0;
    for (const auto& enc : encodings) {
        if (scan_row && !(scan_row % 100) && qore_check_cancel(xsink, "checking model input padding")) {
            return nullptr;
        }
        if (enc.ids.size() < max_width) {
            needs_padding = true;
            break;
        }
        ++scan_row;
    }
    if (needs_padding && pad_id < 0) {
        xsink->raiseException("TOKENIZER-ERROR",
            "model-input batch requires padding to %zu tokens but no pad_token_id is available; "
            "pass pad_token_id explicitly", max_width);
        return nullptr;
    }

    size_t elements = rows * max_width;
    ReferenceHolder<QoreBufferNode> input_ids(
        new QoreBufferNode(QoreBufferElementType::Int64, false, elements), xsink);
    ReferenceHolder<QoreBufferNode> attention_mask(
        new QoreBufferNode(QoreBufferElementType::Int64, false, elements), xsink);
    ReferenceHolder<QoreBufferNode> token_type_ids(
        new QoreBufferNode(QoreBufferElementType::Int64, false, elements), xsink);
    ReferenceHolder<QoreBufferNode> sequence_lengths(
        new QoreBufferNode(QoreBufferElementType::Int64, false, rows), xsink);
    int64* ids_data = static_cast<int64*>(input_ids->getRawData());
    int64* mask_data = static_cast<int64*>(attention_mask->getRawData());
    int64* type_data = static_cast<int64*>(token_type_ids->getRawData());
    int64* len_data = static_cast<int64*>(sequence_lengths->getRawData());

    for (size_t i = 0; i < rows; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "packing model token buffers")) {
            return nullptr;
        }
        const auto& enc = encodings[i];
        len_data[i] = static_cast<int64>(enc.sequence_length);
        size_t offset = i * max_width;
        for (size_t j = 0; j < max_width; ++j) {
            if (j && !(j % 100) && qore_check_cancel(xsink, "packing model token row")) {
                return nullptr;
            }
            size_t pos = offset + j;
            if (j < enc.ids.size()) {
                ids_data[pos] = enc.ids[j];
                mask_data[pos] = j < enc.attention_mask.size() ? enc.attention_mask[j] : 1;
                type_data[pos] = j < enc.token_type_ids.size() ? enc.token_type_ids[j] : 0;
            } else {
                ids_data[pos] = pad_id;
                mask_data[pos] = 0;
                type_data[pos] = 0;
            }
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(hashdeclTokenizerModelInputBatch, xsink), xsink);
    result->setKeyValue("input_ids", input_ids.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("attention_mask", attention_mask.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("token_type_ids", token_type_ids.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("sequence_lengths", sequence_lengths.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("shape", makeModelInputShapeList(rows, max_width, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("rows", static_cast<int64>(rows), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("columns", static_cast<int64>(max_width), xsink);
    if (*xsink) {
        return nullptr;
    }
    result->setKeyValue("dtype", new QoreStringNode("int64"), xsink);
    if (*xsink) {
        return nullptr;
    }
    return result.release();
}

QoreHashNode* QoreHFTokenizer::encodeForModel(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    ReferenceHolder<QoreListNode> texts(new QoreListNode(stringTypeInfo), xsink);
    texts->push(text ? text->refSelf() : new QoreStringNode(""), xsink);
    return encodeBatchForModelIntern(*texts, nullptr, options, xsink);
}

QoreHashNode* QoreHFTokenizer::encodeBatchForModel(const QoreListNode* texts,
        const QoreHashNode* options, ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return encodeBatchForModelIntern(texts, nullptr, options, xsink);
}

QoreHashNode* QoreHFTokenizer::encodeBatchPairsForModel(const QoreListNode* texts,
        const QoreListNode* text_pairs, const QoreHashNode* options, ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);
    return encodeBatchForModelIntern(texts, text_pairs, options, xsink);
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
                result.tokens.push_back(tokenForId(t.id));
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
            content = safeGetStdString(v);
        } else if (v.getType() == NT_HASH) {
            const QoreHashNode* h = v.get<const QoreHashNode>();
            content = safeGetStdString(h->getKeyValue("content"));
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

        addAddedToken(std::move(tok));
        ++added;
        ++dynamic_added_count;
    }

    return added;
}

QoreListNode* QoreHFTokenizer::decodeBatch(const QoreListNode* batch,
        bool skip_special_tokens, ExceptionSink* xsink) {
    std::shared_lock lock(rw_mutex);

    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    ConstListIterator li(batch);
    while (li.next()) {
        const QoreListNode* ids = li.getValue().get<const QoreListNode>();
        if (!ids) {
            rv->push(new QoreStringNode(""), xsink);
            continue;
        }

        // Collect tokens
        std::vector<std::string> tokens;
        ConstListIterator id_iter(ids);
        while (id_iter.next()) {
            int id = (int)id_iter.getValue().getAsBigInt();
            if (skip_special_tokens && special_token_ids.count(id)) {
                continue;
            }
            std::string tok = tokenForId(id);
            if (!tok.empty()) {
                tokens.push_back(std::move(tok));
            }
        }

        // Apply decoder
        std::string result;
        if (decoder) {
            result = decoder->decode(tokens);
        } else {
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (i > 0) result += " ";
                result += tokens[i];
            }
        }
        rv->push(new QoreStringNode(result), xsink);
    }
    return rv.release();
}

void QoreHFTokenizer::loadConfig(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        return;
    }

    // the configuration is read by other threads under the shared lock
    std::unique_lock<std::shared_mutex> lock(rw_mutex);

    // Extract model_max_length
    QoreValue mml = config->getKeyValue("model_max_length");
    if (!mml.isNullOrNothing()) {
        model_max_length = (int)mml.getAsBigInt();
    }

    // Extract chat_template
    std::string ct = safeGetStdString(config->getKeyValue("chat_template"));
    if (!ct.empty()) {
        chat_template = std::move(ct);
    }

    // Override special token IDs from explicit config
    auto resolveToken = [this](const QoreHashNode* config, const char* key) -> int {
        QoreValue v = config->getKeyValue(key);
        if (v.isNullOrNothing()) return -1;
        std::string s;
        if (v.getType() == NT_STRING) {
            s = safeGetStdString(v);
        } else if (v.getType() == NT_HASH) {
            // Some configs use {"content": "token_string"}
            s = safeGetStdString(v.get<const QoreHashNode>()->getKeyValue("content"));
        }
        if (s.empty()) return -1;
        return model ? model->tokenToId(s) : -1;
    };

    int id;
    if ((id = resolveToken(config, "eos_token")) >= 0) eos_token_id = id;
    if ((id = resolveToken(config, "bos_token")) >= 0) bos_token_id = id;
    if ((id = resolveToken(config, "unk_token")) >= 0) unk_token_id = id;
    if ((id = resolveToken(config, "pad_token")) >= 0) pad_token_id = id;
    if ((id = resolveToken(config, "cls_token")) >= 0) cls_token_id = id;
    if ((id = resolveToken(config, "sep_token")) >= 0) sep_token_id = id;
}

namespace {
// the chat template engine: a template is parsed once into nodes, and the nodes are rendered for the messages in a
// single forward pass, so a value inserted into the output (a message, a role or a token) is never read as template
// text

//! a variable of a chat template
enum class ChatVar {
    BOS,        //!< {{ bos_token }}
    EOS,        //!< {{ eos_token }}
    ROLE,       //!< {{ message['role'] }}
    CONTENT,    //!< {{ message['content'] }}
};

struct ChatNode;
typedef std::vector<ChatNode> ChatNodes;

//! a branch of an if block
struct ChatBranch {
    //! false for {% else %}
    bool has_condition = false;
    //! true if the condition is a comparison of the role of the message with a string, false if it is not supported
    bool role_condition = false;
    //! the role the condition compares with
    std::string role;
    //! the nodes rendered when the branch is taken
    ChatNodes body;
};

//! a node of a parsed chat template
struct ChatNode {
    enum class Kind {
        TEXT,   //!< literal text
        VAR,    //!< a variable
        IF,     //!< {% if %} ... {% elif %} ... {% else %} ... {% endif %}
        LOOP,   //!< {% for message in messages %} ... {% endfor %}
    };

    Kind kind;
    //! the text of a TEXT node
    std::string text;
    //! the variable of a VAR node
    ChatVar var = ChatVar::BOS;
    //! the branches of an IF node
    std::vector<ChatBranch> branches;
    //! the body of a LOOP node
    ChatNodes body;

    explicit ChatNode(Kind kind) : kind(kind) {
    }
};

//! the maximum nesting depth of the blocks of a chat template
constexpr size_t CHAT_TEMPLATE_MAX_DEPTH = 100;

constexpr std::string_view CHAT_VAR_OPEN = "{{";
constexpr std::string_view CHAT_VAR_CLOSE = "}}";
constexpr std::string_view CHAT_TAG_OPEN = "{%";
constexpr std::string_view CHAT_TAG_CLOSE = "%}";

//! returns the string without leading and trailing whitespace
std::string_view chatTrim(std::string_view s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

//! returns true if the string starts with the given keyword followed by whitespace
bool chatStartsWithKeyword(std::string_view s, std::string_view keyword) {
    return s.size() > keyword.size() && s.substr(0, keyword.size()) == keyword
        && (s[keyword.size()] == ' ' || s[keyword.size()] == '\t' || s[keyword.size()] == '\r'
            || s[keyword.size()] == '\n');
}

//! returns the variable named by the inside of a {{ }} tag, or false if it is not a known variable
bool chatParseVar(std::string_view name, ChatVar& var) {
    if (name == "bos_token") {
        var = ChatVar::BOS;
    } else if (name == "eos_token") {
        var = ChatVar::EOS;
    } else if (name == "message['role']" || name == "message[\"role\"]") {
        var = ChatVar::ROLE;
    } else if (name == "message['content']" || name == "message[\"content\"]") {
        var = ChatVar::CONTENT;
    } else {
        return false;
    }
    return true;
}

//! parses the condition of an if or elif tag: message['role'] == 'xxx' is supported; any other condition is false
void chatParseCondition(std::string_view condition, ChatBranch& branch) {
    branch.has_condition = true;
    size_t eq = condition.find("==");
    if (eq == std::string_view::npos) {
        return;
    }
    std::string_view lhs = chatTrim(condition.substr(0, eq));
    std::string_view rhs = chatTrim(condition.substr(eq + 2));
    if (lhs != "message['role']" && lhs != "message[\"role\"]") {
        return;
    }
    if (rhs.size() < 2 || (rhs.front() != '\'' && rhs.front() != '"') || rhs.back() != rhs.front()) {
        return;
    }
    std::string_view role = rhs.substr(1, rhs.size() - 2);
    if (role.find(rhs.front()) != std::string_view::npos) {
        return;
    }
    branch.role_condition = true;
    branch.role = std::string(role);
}

//! a block being parsed
struct ChatFrame {
    //! the IF or LOOP node of the block, nullptr for the top level
    ChatNode* node;
    //! the nodes the next node is added to
    ChatNodes* nodes;
    //! true if the IF block has an {% else %} branch
    bool has_else = false;
};

//! parses a chat template
/** @param tmpl the template
    @param nodes the parsed nodes
    @param has_loop set to true if the template has a {% for message in messages %} loop
    @param xsink a TOKENIZER-ERROR is raised here for a malformed template

    @return 0 for OK, -1 if an exception was raised
*/
int chatParseTemplate(const std::string& tmpl, ChatNodes& nodes, bool& has_loop, ExceptionSink* xsink) {
    has_loop = false;
    std::vector<ChatFrame> stack;
    stack.push_back({nullptr, &nodes});

    auto addText = [&stack](std::string_view text) {
        ChatNodes& cur = *stack.back().nodes;
        if (cur.empty() || cur.back().kind != ChatNode::Kind::TEXT) {
            cur.emplace_back(ChatNode::Kind::TEXT);
        }
        cur.back().text.append(text.data(), text.size());
    };
    auto malformed = [xsink](const char* desc) {
        xsink->raiseException("TOKENIZER-ERROR", "malformed chat_template: %s", desc);
        return -1;
    };

    std::string_view t(tmpl);
    size_t pos = 0;
    size_t count = 0;
    while (pos < t.size()) {
        if (++count % 100 == 0 && qore_check_cancel(xsink, "parsing chat template")) {
            return -1;
        }
        size_t var_pos = t.find(CHAT_VAR_OPEN, pos);
        size_t tag_pos = t.find(CHAT_TAG_OPEN, pos);
        size_t next = std::min(var_pos, tag_pos);
        if (next == std::string_view::npos) {
            addText(t.substr(pos));
            break;
        }
        if (next > pos) {
            addText(t.substr(pos, next - pos));
        }
        bool is_var = next == var_pos;
        std::string_view close = is_var ? CHAT_VAR_CLOSE : CHAT_TAG_CLOSE;
        size_t inner_start = next + CHAT_VAR_OPEN.size();
        size_t end = t.find(close, inner_start);
        if (end == std::string_view::npos) {
            // an unclosed tag is text
            addText(t.substr(next));
            break;
        }
        std::string_view whole = t.substr(next, end + close.size() - next);
        std::string_view inner = chatTrim(t.substr(inner_start, end - inner_start));
        pos = end + close.size();

        if (is_var) {
            ChatVar var;
            if (chatParseVar(inner, var)) {
                stack.back().nodes->emplace_back(ChatNode::Kind::VAR);
                stack.back().nodes->back().var = var;
            } else {
                // an unsupported expression is text
                addText(whole);
            }
            continue;
        }

        if (inner == "for message in messages") {
            for (const ChatFrame& frame : stack) {
                if (frame.node && frame.node->kind == ChatNode::Kind::LOOP) {
                    return malformed("a {% for message in messages %} loop cannot be in another one");
                }
            }
            if (stack.size() > CHAT_TEMPLATE_MAX_DEPTH) {
                return malformed("blocks are nested too deeply");
            }
            ChatNodes& cur = *stack.back().nodes;
            cur.emplace_back(ChatNode::Kind::LOOP);
            ChatNode* loop = &cur.back();
            stack.push_back({loop, &loop->body});
            has_loop = true;
        } else if (inner == "endfor") {
            if (!stack.back().node || stack.back().node->kind != ChatNode::Kind::LOOP) {
                return malformed(stack.back().node ? "missing {% endif %}" : "unexpected {% endfor %}");
            }
            stack.pop_back();
        } else if (chatStartsWithKeyword(inner, "if")) {
            if (stack.size() > CHAT_TEMPLATE_MAX_DEPTH) {
                return malformed("blocks are nested too deeply");
            }
            ChatNodes& cur = *stack.back().nodes;
            cur.emplace_back(ChatNode::Kind::IF);
            ChatNode* block = &cur.back();
            block->branches.emplace_back();
            chatParseCondition(inner.substr(2), block->branches.back());
            stack.push_back({block, &block->branches.back().body});
        } else if (chatStartsWithKeyword(inner, "elif") || inner == "else") {
            ChatFrame& frame = stack.back();
            if (!frame.node || frame.node->kind != ChatNode::Kind::IF) {
                return malformed(inner == "else" ? "unexpected {% else %}" : "unexpected {% elif %}");
            }
            if (frame.has_else) {
                return malformed(inner == "else" ? "more than one {% else %} in an if block"
                    : "{% elif %} after {% else %}");
            }
            frame.node->branches.emplace_back();
            if (inner == "else") {
                frame.has_else = true;
            } else {
                chatParseCondition(inner.substr(4), frame.node->branches.back());
            }
            frame.nodes = &frame.node->branches.back().body;
        } else if (inner == "endif") {
            if (!stack.back().node || stack.back().node->kind != ChatNode::Kind::IF) {
                return malformed("unexpected {% endif %}");
            }
            stack.pop_back();
        } else {
            // an unsupported statement is text
            addText(whole);
        }
    }

    if (stack.size() > 1) {
        return malformed(stack.back().node->kind == ChatNode::Kind::LOOP ? "missing {% endfor %}"
            : "missing {% endif %}");
    }
    return 0;
}

//! what a chat template is rendered with
struct ChatContext {
    const std::string& bos;
    const std::string& eos;
    const QoreListNode* messages;
    ExceptionSink* xsink;
};

//! renders the nodes of a chat template; a value is appended to the output and never read as template text
/** @param nodes the nodes to render
    @param ctx what the template is rendered with
    @param role the role of the current message; nullptr outside a loop
    @param content the content of the current message; nullptr outside a loop
    @param out the output

    @return 0 for OK, -1 if an exception was raised
*/
int chatRender(const ChatNodes& nodes, const ChatContext& ctx, const std::string* role, const std::string* content,
        std::string& out) {
    for (const ChatNode& node : nodes) {
        switch (node.kind) {
            case ChatNode::Kind::TEXT:
                out += node.text;
                break;
            case ChatNode::Kind::VAR:
                switch (node.var) {
                    case ChatVar::BOS:
                        out += ctx.bos;
                        break;
                    case ChatVar::EOS:
                        out += ctx.eos;
                        break;
                    case ChatVar::ROLE:
                        if (role) {
                            out += *role;
                        }
                        break;
                    case ChatVar::CONTENT:
                        if (content) {
                            out += *content;
                        }
                        break;
                }
                break;
            case ChatNode::Kind::IF:
                for (const ChatBranch& branch : node.branches) {
                    if (!branch.has_condition || (branch.role_condition && role && *role == branch.role)) {
                        if (chatRender(branch.body, ctx, role, content, out)) {
                            return -1;
                        }
                        break;
                    }
                }
                break;
            case ChatNode::Kind::LOOP: {
                // the parser does not allow a loop in a loop
                assert(!role);
                size_t n = ctx.messages ? ctx.messages->size() : 0;
                for (size_t i = 0; i < n; ++i) {
                    if (i && !(i % 100) && qore_check_cancel(ctx.xsink, "applying chat template")) {
                        return -1;
                    }
                    const QoreHashNode* msg = ctx.messages->retrieveEntry(i).get<const QoreHashNode>();
                    if (!msg) {
                        continue;
                    }
                    std::string msg_role = safeGetStdString(msg->getKeyValue("role"));
                    std::string msg_content = safeGetStdString(msg->getKeyValue("content"));
                    if (chatRender(node.body, ctx, &msg_role, &msg_content, out)) {
                        return -1;
                    }
                }
                break;
            }
        }
    }
    return 0;
}
}

QoreStringNode* QoreHFTokenizer::applyChatTemplate(const QoreListNode* messages,
        ExceptionSink* xsink) {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    if (chat_template.empty()) {
        xsink->raiseException("TOKENIZER-ERROR",
            "no chat_template configured; load tokenizer_config.json first");
        return nullptr;
    }

    // Supported template patterns:
    //     {% for message in messages %} ... {% endfor %}
    //     {% if message['role'] == 'xxx' %} ... {% elif message['role'] == 'yyy' %} ... {% else %} ... {% endif %}
    //     {{ bos_token }}, {{ eos_token }}, {{ message['role'] }}, {{ message['content'] }}
    // Any other tag is copied to the output as it is, and any other condition is false.
    ChatNodes nodes;
    bool has_loop;
    if (chatParseTemplate(chat_template, nodes, has_loop, xsink)) {
        return nullptr;
    }

    std::string result;
    if (!has_loop) {
        // No standard loop — try simple message concatenation
        size_t n = messages ? messages->size() : 0;
        for (size_t i = 0; i < n; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "applying chat template")) {
                return nullptr;
            }
            const QoreHashNode* msg = messages->retrieveEntry(i).get<const QoreHashNode>();
            if (!msg) {
                continue;
            }
            std::string content = safeGetStdString(msg->getKeyValue("content"));
            if (!content.empty()) {
                if (!result.empty()) {
                    result += "\n";
                }
                result += content;
            }
        }
        return new QoreStringNode(result);
    }

    // the BOS and EOS tokens may be added tokens
    std::string bos_str = bos_token_id < 0 ? std::string() : tokenForId(bos_token_id);
    std::string eos_str = eos_token_id < 0 ? std::string() : tokenForId(eos_token_id);
    ChatContext ctx{bos_str, eos_str, messages, xsink};
    if (chatRender(nodes, ctx, nullptr, nullptr, result)) {
        return nullptr;
    }
    return new QoreStringNode(result);
}

} // namespace QoreTokenizer
