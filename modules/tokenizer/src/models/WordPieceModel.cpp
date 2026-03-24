/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WordPieceModel.cpp

    WordPiece tokenizer model implementation

    The WordPiece algorithm tokenizes by greedy longest-prefix matching:
    1. Try to find the longest prefix of the remaining input in the vocabulary
    2. If found, emit that token and continue with the remainder
    3. For subsequent pieces, prepend the continuing_subword_prefix (e.g., "##")
    4. If no prefix matches at any point, emit the unknown token for the entire word

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "models/WordPieceModel.h"
#include "utils/qore_helpers.h"

#include <cassert>

using namespace QoreTokenizer;

namespace QoreTokenizer {

WordPieceModel::WordPieceModel(const QoreHashNode* config, ExceptionSink* xsink)
        : continuing_subword_prefix("##"), max_input_chars_per_word(100) {
    assert(config);

    // Read vocab: object { token: id }
    const QoreHashNode* vocab_hash = safeGetHashKey(config, "vocab");
    if (vocab_hash) {
        // Determine max ID
        int max_id = -1;
        ConstHashIterator vi(vocab_hash);
        while (vi.next()) {
            int id = (int)vi.get().getAsBigInt();
            if (id > max_id) {
                max_id = id;
            }
        }
        // Reject unreasonably large vocab IDs (DoS protection)
        if (max_id >= 0 && max_id < 1000000) {
            id_to_token.resize(max_id + 1);
        } else if (max_id >= 1000000) {
            xsink->raiseException("TOKENIZER-MODEL-ERROR",
                "vocab max ID %d exceeds safety limit of 1000000", max_id);
            return;
        }

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

    // Read continuing_subword_prefix
    QoreValue prefix_val = config->getKeyValue("continuing_subword_prefix");
    if (prefix_val.getType() == NT_STRING) {
        const QoreStringNode* prefix_str = safeGetString(prefix_val);
        if (prefix_str) {
            continuing_subword_prefix = prefix_str->c_str();
        }
    }

    // Read max_input_chars_per_word
    QoreValue max_chars_val = config->getKeyValue("max_input_chars_per_word");
    if (max_chars_val.getType() == NT_INT) {
        max_input_chars_per_word = (int)max_chars_val.getAsBigInt();
    }
}

std::vector<int> WordPieceModel::tokenize(const std::string& pre_token) const {
    if (pre_token.empty()) {
        return {};
    }

    // Count UTF-8 characters
    size_t char_count = 0;
    for (size_t i = 0; i < pre_token.size(); ) {
        uint8_t c = pre_token[i];
        int len = 1;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > pre_token.size()) {
            len = 1;
        }
        ++char_count;
        i += len;
    }

    // If the word is too long, return unk_token
    if ((int)char_count > max_input_chars_per_word) {
        int unk_id = tokenToId(unk_token);
        if (unk_id >= 0) {
            return {unk_id};
        }
        return {};
    }

    // Build a list of UTF-8 character byte offsets for substring extraction
    std::vector<size_t> char_offsets;
    char_offsets.reserve(char_count + 1);
    for (size_t i = 0; i < pre_token.size(); ) {
        char_offsets.push_back(i);
        uint8_t c = pre_token[i];
        int len = 1;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > pre_token.size()) {
            len = 1;
        }
        i += len;
    }
    char_offsets.push_back(pre_token.size());

    std::vector<int> output_ids;
    size_t start_char = 0;
    bool is_bad = false;

    while (start_char < char_count) {
        size_t end_char = char_count;
        bool found = false;

        while (end_char > start_char) {
            size_t byte_start = char_offsets[start_char];
            size_t byte_end = char_offsets[end_char];
            std::string substr = pre_token.substr(byte_start, byte_end - byte_start);

            if (start_char > 0) {
                substr = continuing_subword_prefix + substr;
            }

            auto it = vocab.find(substr);
            if (it != vocab.end()) {
                output_ids.push_back(it->second);
                found = true;
                break;
            }
            --end_char;
        }

        if (!found) {
            is_bad = true;
            break;
        }
        start_char = end_char;
    }

    if (is_bad) {
        // Return unk_token for the entire word
        int unk_id = tokenToId(unk_token);
        if (unk_id >= 0) {
            return {unk_id};
        }
        return {};
    }

    return output_ids;
}

int WordPieceModel::vocabSize() const {
    return (int)vocab.size();
}

std::string WordPieceModel::idToToken(int id) const {
    if (id >= 0 && id < (int)id_to_token.size()) {
        return id_to_token[id];
    }
    return {};
}

int WordPieceModel::tokenToId(const std::string& token) const {
    auto it = vocab.find(token);
    if (it != vocab.end()) {
        return it->second;
    }
    return -1;
}

std::unordered_map<std::string, int> WordPieceModel::getVocab() const {
    return vocab;
}

} // namespace QoreTokenizer
