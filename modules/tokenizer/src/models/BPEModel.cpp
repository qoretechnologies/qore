/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BPEModel.cpp

    Byte-Pair Encoding tokenizer model implementation

    The BPE algorithm works by:
    1. Splitting the input into individual characters (UTF-8 aware)
    2. Iteratively merging the highest-priority (lowest rank) adjacent pair
    3. Continuing until no more merges can be applied
    4. Looking up the resulting tokens in the vocabulary

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "models/BPEModel.h"
#include "utils/qore_helpers.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cassert>

using namespace QoreTokenizer;

namespace QoreTokenizer {

BPEModel::BPEModel(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    // Read vocab: object { token: id }
    const QoreHashNode* vocab_hash = safeGetHashKey(config, "vocab");
    if (vocab_hash) {
        // Determine max ID to size id_to_token
        int max_id = -1;
        ConstHashIterator vi(vocab_hash);
        while (vi.next()) {
            int id = (int)vi.get().getAsBigInt();
            if (id > max_id) {
                max_id = id;
            }
        }
        if (max_id >= 0 && max_id < 1000000) {
            id_to_token.resize(max_id + 1);
        } else if (max_id >= 1000000) {
            xsink->raiseException("TOKENIZER-MODEL-ERROR",
                "vocab max ID %d exceeds safety limit of 1000000", max_id);
            return;
        }

        // Populate vocab maps
        ConstHashIterator vi2(vocab_hash);
        while (vi2.next()) {
            std::string token = vi2.getKey();
            int id = (int)vi2.get().getAsBigInt();
            vocab[token] = id;
            if (id >= 0 && id < (int)id_to_token.size()) {
                id_to_token[id] = token;
            }
        }
    }

    // Read merges: list of merge rules
    const QoreListNode* merges_list = safeGetListKey(config, "merges");
    if (merges_list) {
        for (size_t i = 0; i < merges_list->size(); ++i) {
            QoreValue entry = merges_list->retrieveEntry(i);
            std::string a, b;

            if (entry.getType() == NT_STRING) {
                // "a b" format
                const QoreStringNode* merge_str = safeGetString(entry);
                std::string s = merge_str->c_str();
                size_t space_pos = s.find(' ');
                if (space_pos != std::string::npos) {
                    a = s.substr(0, space_pos);
                    b = s.substr(space_pos + 1);
                }
            } else if (entry.getType() == NT_LIST) {
                // ["a", "b"] tuple format
                const QoreListNode* tuple = safeGetList(entry);
                if (tuple->size() >= 2) {
                    const QoreStringNode* a_node = safeGetString(tuple->retrieveEntry(0));
                    const QoreStringNode* b_node = safeGetString(tuple->retrieveEntry(1));
                    if (a_node && b_node) {
                        a = a_node->c_str();
                        b = b_node->c_str();
                    }
                }
            }

            if (!a.empty() && !b.empty()) {
                int rank = (int)merges.size();
                merges.push_back({a, b});
                std::string key = a + " " + b;
                merge_rank[key] = rank;
            }
        }
    }

    // Read unk_token - can be a string or an object with "content"
    QoreValue unk_val = config->getKeyValue("unk_token");
    if (unk_val.getType() == NT_STRING) {
        const QoreStringNode* unk_str = safeGetString(unk_val);
        if (unk_str) {
            unk_token = unk_str->c_str();
        }
    } else if (unk_val.getType() == NT_HASH) {
        const QoreHashNode* unk_hash = safeGetHash(unk_val);
        if (unk_hash) {
            std::string content = safeGetStringKey(unk_hash, "content");
            if (!content.empty()) {
                unk_token = content;
            }
        }
    }

    // Read optional fields
    std::string prefix_str = safeGetStringKey(config, "continuing_subword_prefix");
    if (!prefix_str.empty()) {
        continuing_subword_prefix = prefix_str;
    }

    std::string suffix_str = safeGetStringKey(config, "end_of_word_suffix");
    if (!suffix_str.empty()) {
        end_of_word_suffix = suffix_str;
    }

    byte_fallback = config->getKeyValue("byte_fallback").getAsBool();
    fuse_unk = config->getKeyValue("fuse_unk").getAsBool();
}

int BPEModel::getMergeRank(const std::string& a, const std::string& b) const {
    std::string key = a + " " + b;
    auto it = merge_rank.find(key);
    if (it != merge_rank.end()) {
        return it->second;
    }
    return -1;
}

std::vector<std::string> BPEModel::splitUtf8Chars(const std::string& input) {
    std::vector<std::string> chars;
    size_t i = 0;
    while (i < input.size()) {
        uint8_t c = input[i];
        int len = 1;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > input.size()) {
            len = 1;
        }
        chars.push_back(input.substr(i, len));
        i += len;
    }
    return chars;
}

std::string BPEModel::byteToken(uint8_t byte) {
    char buf[8];
    snprintf(buf, sizeof(buf), "<0x%02X>", byte);
    return std::string(buf);
}

std::vector<int> BPEModel::tokenize(const std::string& pre_token) const {
    if (pre_token.empty()) {
        return {};
    }

    // Step 1: split into UTF-8 characters
    std::vector<std::string> symbols = splitUtf8Chars(pre_token);

    // Apply end_of_word_suffix to the last symbol
    if (!end_of_word_suffix.empty() && !symbols.empty()) {
        symbols.back() += end_of_word_suffix;
    }

    // Apply continuing_subword_prefix to all symbols except the first
    if (!continuing_subword_prefix.empty()) {
        for (size_t i = 1; i < symbols.size(); ++i) {
            symbols[i] = continuing_subword_prefix + symbols[i];
        }
    }

    // Step 2: iteratively merge highest-priority pair
    while (symbols.size() > 1) {
        // Find the pair with the lowest rank (highest priority)
        int best_rank = INT_MAX;
        size_t best_pos = 0;
        bool found = false;

        for (size_t i = 0; i + 1 < symbols.size(); ++i) {
            int rank = getMergeRank(symbols[i], symbols[i + 1]);
            if (rank >= 0 && rank < best_rank) {
                best_rank = rank;
                best_pos = i;
                found = true;
            }
        }

        if (!found) {
            break;
        }

        // Merge the best pair
        std::string merged = symbols[best_pos] + symbols[best_pos + 1];
        symbols[best_pos] = merged;
        symbols.erase(symbols.begin() + best_pos + 1);
    }

    // Step 3: look up token IDs
    std::vector<int> ids;
    ids.reserve(symbols.size());

    int unk_id = -1;
    if (!unk_token.empty()) {
        auto it = vocab.find(unk_token);
        if (it != vocab.end()) {
            unk_id = it->second;
        }
    }

    bool prev_was_unk = false;
    for (const auto& sym : symbols) {
        auto it = vocab.find(sym);
        if (it != vocab.end()) {
            ids.push_back(it->second);
            prev_was_unk = false;
        } else if (byte_fallback) {
            // Decompose the symbol to byte tokens <0xHH>
            prev_was_unk = false;
            for (uint8_t byte : sym) {
                std::string bt = byteToken(byte);
                auto bt_it = vocab.find(bt);
                if (bt_it != vocab.end()) {
                    ids.push_back(bt_it->second);
                } else if (unk_id >= 0) {
                    ids.push_back(unk_id);
                }
            }
        } else {
            // Unknown token
            if (fuse_unk && prev_was_unk) {
                // Skip: fuse consecutive unknowns into one
            } else if (unk_id >= 0) {
                ids.push_back(unk_id);
            }
            prev_was_unk = true;
        }
    }

    return ids;
}

int BPEModel::vocabSize() const {
    return (int)vocab.size();
}

std::string BPEModel::idToToken(int id) const {
    if (id >= 0 && id < (int)id_to_token.size()) {
        return id_to_token[id];
    }
    return {};
}

int BPEModel::tokenToId(const std::string& token) const {
    auto it = vocab.find(token);
    if (it != vocab.end()) {
        return it->second;
    }
    return -1;
}

std::unordered_map<std::string, int> BPEModel::getVocab() const {
    return vocab;
}

} // namespace QoreTokenizer
