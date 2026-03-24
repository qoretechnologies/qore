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
#include <queue>
#include <functional>
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

std::vector<TokenWithOffset> BPEModel::bpeMerge(
        const std::string& pre_token) const {
    if (pre_token.empty()) {
        return {};
    }

    // Split into UTF-8 characters and build linked list
    std::vector<std::string> chars = splitUtf8Chars(pre_token);
    if (chars.empty()) {
        return {};
    }

    std::vector<BPENode> nodes;
    nodes.reserve(chars.size());

    // Build initial node list with byte offsets
    size_t byte_pos = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
        BPENode node;
        node.symbol = chars[i];
        node.orig_start = byte_pos;
        byte_pos += chars[i].size();
        node.orig_end = byte_pos;
        node.prev = (int)i - 1;
        node.next = (i + 1 < chars.size()) ? (int)(i + 1) : -1;
        nodes.push_back(std::move(node));
    }

    // Apply end_of_word_suffix to last node
    if (!end_of_word_suffix.empty()) {
        int last = (int)nodes.size() - 1;
        nodes[last].symbol += end_of_word_suffix;
    }

    // Apply continuing_subword_prefix to all nodes except first
    if (!continuing_subword_prefix.empty()) {
        for (int i = nodes[0].next; i >= 0; i = nodes[i].next) {
            nodes[i].symbol = continuing_subword_prefix + nodes[i].symbol;
        }
    }

    // Build priority queue of merge candidates
    std::priority_queue<MergeCandidate, std::vector<MergeCandidate>,
        std::greater<MergeCandidate>> pq;

    auto pushPair = [&](int left, int right) {
        if (left >= 0 && right >= 0 && nodes[left].active && nodes[right].active) {
            int rank = getMergeRank(nodes[left].symbol, nodes[right].symbol);
            if (rank >= 0) {
                pq.push({rank, left, right});
            }
        }
    };

    // Initialize: push all adjacent pairs
    for (int i = 0; i >= 0; i = nodes[i].next) {
        if (nodes[i].next >= 0) {
            pushPair(i, nodes[i].next);
        }
    }

    // Merge loop: O(n log n) amortized
    while (!pq.empty()) {
        auto mc = pq.top();
        pq.pop();

        // Stale check: both nodes must be active and still adjacent
        if (!nodes[mc.left_idx].active || !nodes[mc.right_idx].active
                || nodes[mc.left_idx].next != mc.right_idx) {
            continue;
        }

        // Verify rank still matches (symbol may have changed due to prior merge)
        int cur_rank = getMergeRank(nodes[mc.left_idx].symbol,
            nodes[mc.right_idx].symbol);
        if (cur_rank != mc.rank) {
            continue;
        }

        // Merge: left absorbs right
        nodes[mc.left_idx].symbol += nodes[mc.right_idx].symbol;
        nodes[mc.left_idx].orig_end = nodes[mc.right_idx].orig_end;
        nodes[mc.left_idx].next = nodes[mc.right_idx].next;
        if (nodes[mc.right_idx].next >= 0) {
            nodes[nodes[mc.right_idx].next].prev = mc.left_idx;
        }
        nodes[mc.right_idx].active = false;

        // Push new pairs formed by the merge
        pushPair(nodes[mc.left_idx].prev, mc.left_idx);
        pushPair(mc.left_idx, nodes[mc.left_idx].next);
    }

    // Walk linked list to collect final symbols with offsets
    return symbolsToIdsWithOffsets(nodes, 0);
}

std::vector<TokenWithOffset> BPEModel::symbolsToIdsWithOffsets(
        const std::vector<BPENode>& nodes, int head) const {
    std::vector<TokenWithOffset> result;

    int unk_id = -1;
    if (!unk_token.empty()) {
        auto it = vocab.find(unk_token);
        if (it != vocab.end()) {
            unk_id = it->second;
        }
    }

    bool prev_was_unk = false;
    for (int i = head; i >= 0; i = nodes[i].next) {
        if (!nodes[i].active) {
            continue;
        }
        const std::string& sym = nodes[i].symbol;
        size_t start = nodes[i].orig_start;
        size_t end = nodes[i].orig_end;

        auto it = vocab.find(sym);
        if (it != vocab.end()) {
            result.push_back({it->second, start, end});
            prev_was_unk = false;
        } else if (byte_fallback) {
            prev_was_unk = false;
            for (size_t j = 0; j < sym.size(); ++j) {
                std::string bt = byteToken((uint8_t)sym[j]);
                auto bt_it = vocab.find(bt);
                if (bt_it != vocab.end()) {
                    result.push_back({bt_it->second, start, end});
                } else if (unk_id >= 0) {
                    result.push_back({unk_id, start, end});
                }
            }
        } else {
            if (fuse_unk && prev_was_unk) {
                // Skip
            } else if (unk_id >= 0) {
                result.push_back({unk_id, start, end});
            }
            prev_was_unk = true;
        }
    }

    return result;
}

std::vector<TokenWithOffset> BPEModel::tokenizeWithOffsets(
        const std::string& pre_token) const {
    return bpeMerge(pre_token);
}

std::vector<int> BPEModel::tokenize(const std::string& pre_token) const {
    auto with_offsets = bpeMerge(pre_token);
    std::vector<int> ids;
    ids.reserve(with_offsets.size());
    for (const auto& t : with_offsets) {
        ids.push_back(t.id);
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
