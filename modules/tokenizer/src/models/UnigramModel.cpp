/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    UnigramModel.cpp

    Unigram (SentencePiece) tokenizer model implementation

    The Unigram model uses the Viterbi algorithm:
    1. Build a lattice over the input string where each node i represents the
       state "processed i bytes of input"
    2. For each position i, find all vocabulary entries that match a substring
       starting at position i
    3. Use dynamic programming to find the segmentation that maximizes the
       total log probability
    4. Backtrack to recover the best tokenization

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "models/UnigramModel.h"
#include "utils/qore_helpers.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace QoreTokenizer;

namespace QoreTokenizer {

UnigramModel::UnigramModel(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    // Read vocab: array of [token, score] pairs
    const QoreListNode* vocab_list = safeGetListKey(config, "vocab");
    if (vocab_list) {
        vocab.reserve(vocab_list->size());
        for (size_t i = 0; i < vocab_list->size(); ++i) {
            const QoreListNode* pair = safeGetList(vocab_list->retrieveEntry(i));
            if (pair && pair->size() >= 2) {
                const QoreStringNode* token_node = safeGetString(pair->retrieveEntry(0));
                double score = pair->retrieveEntry(1).getAsFloat();
                std::string token;
                if (token_node) {
                    token = token_node->c_str();
                }
                int id = (int)vocab.size();
                vocab.push_back({token, score});
                if (!token.empty()) {
                    token_to_id[token] = id;
                    token_set.insert(token);
                    if (token.size() > max_token_len) {
                        max_token_len = token.size();
                    }
                }
            }
        }
    }

    // Read unk_id
    QoreValue unk_val = config->getKeyValue("unk_id");
    if (unk_val.getType() == NT_INT) {
        unk_id = (int)unk_val.getAsBigInt();
    }

    // Read byte_fallback
    byte_fallback = config->getKeyValue("byte_fallback").getAsBool();
}

std::string UnigramModel::byteToken(uint8_t byte) {
    char buf[8];
    snprintf(buf, sizeof(buf), "<0x%02X>", byte);
    return std::string(buf);
}

std::vector<int> UnigramModel::tokenize(const std::string& pre_token) const {
    if (pre_token.empty()) {
        return {};
    }

    const size_t n = pre_token.size();

    // Viterbi forward pass
    // best_score[i] = best log-probability score for segmenting pre_token[0..i)
    // best_prev[i] = the start position of the last token in the best segmentation ending at i
    const double NEG_INF = -std::numeric_limits<double>::infinity();
    std::vector<double> best_score(n + 1, NEG_INF);
    std::vector<size_t> best_prev(n + 1, 0);
    best_score[0] = 0.0;

    for (size_t i = 0; i < n; ++i) {
        if (best_score[i] == NEG_INF) {
            continue;
        }

        // Try all substrings starting at position i
        size_t max_len = std::min(max_token_len, n - i);
        bool found_any = false;

        for (size_t len = 1; len <= max_len; ++len) {
            // Only consider substrings that end at a UTF-8 character boundary
            if (i + len < n) {
                uint8_t next_byte = pre_token[i + len];
                // If next_byte is a UTF-8 continuation byte, this isn't a char boundary
                if ((next_byte & 0xC0) == 0x80) {
                    continue;
                }
            }

            std::string substr = pre_token.substr(i, len);
            auto it = token_to_id.find(substr);
            if (it != token_to_id.end()) {
                int id = it->second;
                double score = best_score[i] + vocab[id].second;
                if (score > best_score[i + len]) {
                    best_score[i + len] = score;
                    best_prev[i + len] = i;
                }
                found_any = true;
            }
        }

        // If no vocab entry matches starting at i, handle unknown character
        if (!found_any && best_score[i] > NEG_INF) {
            // Advance by one UTF-8 character
            uint8_t c = pre_token[i];
            size_t char_len = 1;
            if ((c & 0xE0) == 0xC0) {
                char_len = 2;
            } else if ((c & 0xF0) == 0xE0) {
                char_len = 3;
            } else if ((c & 0xF8) == 0xF0) {
                char_len = 4;
            }
            if (i + char_len > n) {
                char_len = 1;
            }

            // Use unk_token score; if unk_id not set, use a large penalty
            double unk_score = -10.0;
            if (unk_id >= 0 && unk_id < (int)vocab.size()) {
                unk_score = vocab[unk_id].second;
            }
            double score = best_score[i] + unk_score;
            if (score > best_score[i + char_len]) {
                best_score[i + char_len] = score;
                // Use SIZE_MAX as a sentinel to indicate unk
                best_prev[i + char_len] = i;
            }
        }
    }

    // Backtrack to recover the best segmentation
    if (best_score[n] == NEG_INF) {
        // Fallback: return unk for the entire input
        if (unk_id >= 0) {
            return {unk_id};
        }
        return {};
    }

    std::vector<std::pair<size_t, size_t>> segments; // (start, end) byte positions
    size_t pos = n;
    while (pos > 0) {
        size_t prev = best_prev[pos];
        segments.push_back({prev, pos});
        pos = prev;
    }
    // Reverse to get forward order
    std::reverse(segments.begin(), segments.end());

    // Convert segments to token IDs
    std::vector<int> ids;
    ids.reserve(segments.size());
    for (const auto& seg : segments) {
        std::string substr = pre_token.substr(seg.first, seg.second - seg.first);
        auto it = token_to_id.find(substr);
        if (it != token_to_id.end()) {
            ids.push_back(it->second);
        } else if (byte_fallback) {
            // Decompose unknown character to byte tokens
            for (size_t j = seg.first; j < seg.second; ++j) {
                std::string bt = byteToken((uint8_t)pre_token[j]);
                auto bt_it = token_to_id.find(bt);
                if (bt_it != token_to_id.end()) {
                    ids.push_back(bt_it->second);
                } else if (unk_id >= 0) {
                    ids.push_back(unk_id);
                }
            }
        } else if (unk_id >= 0) {
            ids.push_back(unk_id);
        }
    }

    return ids;
}

int UnigramModel::vocabSize() const {
    return (int)vocab.size();
}

std::string UnigramModel::idToToken(int id) const {
    if (id >= 0 && id < (int)vocab.size()) {
        return vocab[id].first;
    }
    return {};
}

int UnigramModel::tokenToId(const std::string& token) const {
    auto it = token_to_id.find(token);
    if (it != token_to_id.end()) {
        return it->second;
    }
    return -1;
}

} // namespace QoreTokenizer
