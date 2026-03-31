/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TemplateProcessor.cpp

    Template-based post-processor and BertProcessing/RobertaProcessing implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "postprocessors/TemplateProcessor.h"
#include "utils/qore_helpers.h"

#include <cassert>

using namespace QoreTokenizer;

namespace QoreTokenizer {

// --- TemplateProcessor ---

std::vector<TemplatePiece> TemplateProcessor::parseTemplate(const QoreListNode* tmpl_list,
        ExceptionSink* xsink) {
    std::vector<TemplatePiece> pieces;

    if (!tmpl_list) {
        return pieces;
    }

    for (size_t i = 0; i < tmpl_list->size(); ++i) {
        const QoreHashNode* piece_hash = safeGetHash(tmpl_list->retrieveEntry(i));
        if (!piece_hash) {
            continue;
        }

        TemplatePiece piece;

        // Check if this is a Sequence piece (has "Sequence" key)
        QoreValue seq_val = piece_hash->getKeyValue("Sequence");
        if (seq_val.getType() == NT_HASH) {
            const QoreHashNode* seq_hash = safeGetHash(seq_val);
            piece.type = TemplatePiece::SEQUENCE;

            std::string id_str = safeGetStringKey(seq_hash, "id");
            if (!id_str.empty()) {
                piece.id = id_str;
            }

            QoreValue type_id_val = seq_hash->getKeyValue("type_id");
            if (type_id_val.getType() == NT_INT) {
                piece.type_id = (int)type_id_val.getAsBigInt();
            }

            pieces.push_back(piece);
            continue;
        }

        // Check if this is a SpecialToken piece (has "SpecialToken" key)
        QoreValue st_val = piece_hash->getKeyValue("SpecialToken");
        if (st_val.getType() == NT_HASH) {
            const QoreHashNode* st_hash = safeGetHash(st_val);
            piece.type = TemplatePiece::SPECIAL_TOKEN;

            std::string id_str = safeGetStringKey(st_hash, "id");
            if (!id_str.empty()) {
                piece.id = id_str;
            }

            QoreValue type_id_val = st_hash->getKeyValue("type_id");
            if (type_id_val.getType() == NT_INT) {
                piece.type_id = (int)type_id_val.getAsBigInt();
            }

            pieces.push_back(piece);
            continue;
        }
    }

    return pieces;
}

TemplateProcessor::TemplateProcessor(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    // Parse single template
    const QoreListNode* single_list = safeGetListKey(config, "single");
    if (single_list) {
        single_template = parseTemplate(single_list, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse pair template
    const QoreListNode* pair_list = safeGetListKey(config, "pair");
    if (pair_list) {
        pair_template = parseTemplate(pair_list, xsink);
        if (*xsink) {
            return;
        }
    }

    // Parse special_tokens: object { token_key: { id: string, ids: [int], tokens: [string] } }
    const QoreHashNode* st_hash = safeGetHashKey(config, "special_tokens");
    if (st_hash) {
        ConstHashIterator it(st_hash);
        while (it.next()) {
            std::string key = it.getKey();
            const QoreHashNode* st_info = safeGetHash(it.get());
            if (!st_info) {
                continue;
            }

            SpecialTokenInfo info;

            const QoreListNode* ids_list = safeGetListKey(st_info, "ids");
            if (ids_list) {
                for (size_t i = 0; i < ids_list->size(); ++i) {
                    info.ids.push_back((int)ids_list->retrieveEntry(i).getAsBigInt());
                }
            }

            const QoreListNode* tokens_list = safeGetListKey(st_info, "tokens");
            if (tokens_list) {
                for (size_t i = 0; i < tokens_list->size(); ++i) {
                    const QoreStringNode* tok = safeGetString(tokens_list->retrieveEntry(i));
                    if (tok) {
                        info.tokens.push_back(tok->c_str());
                    }
                }
            }

            special_tokens[key] = info;
        }
    }
}

EncodingResult TemplateProcessor::process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const {
    EncodingResult result;

    if (!add_special) {
        // No special tokens: just concatenate A and B
        result.ids = ids_a;
        result.type_ids.assign(ids_a.size(), 0);
        result.tokens = tokens_a;

        if (!ids_b.empty()) {
            result.ids.insert(result.ids.end(), ids_b.begin(), ids_b.end());
            std::vector<int> b_types(ids_b.size(), 1);
            result.type_ids.insert(result.type_ids.end(), b_types.begin(), b_types.end());
            result.tokens.insert(result.tokens.end(), tokens_b.begin(), tokens_b.end());
        }
        return result;
    }

    // Choose the appropriate template
    const std::vector<TemplatePiece>& tmpl =
        (!ids_b.empty() && !pair_template.empty()) ? pair_template : single_template;

    for (const auto& piece : tmpl) {
        if (piece.type == TemplatePiece::SEQUENCE) {
            const std::vector<int>* ids_ptr = nullptr;
            const std::vector<std::string>* tokens_ptr = nullptr;

            if (piece.id == "A") {
                ids_ptr = &ids_a;
                tokens_ptr = &tokens_a;
            } else if (piece.id == "B") {
                ids_ptr = &ids_b;
                tokens_ptr = &tokens_b;
            }

            if (ids_ptr) {
                for (size_t i = 0; i < ids_ptr->size(); ++i) {
                    result.ids.push_back((*ids_ptr)[i]);
                    result.type_ids.push_back(piece.type_id);
                    if (i < tokens_ptr->size()) {
                        result.tokens.push_back((*tokens_ptr)[i]);
                    }
                }
            }
        } else if (piece.type == TemplatePiece::SPECIAL_TOKEN) {
            auto it = special_tokens.find(piece.id);
            if (it != special_tokens.end()) {
                const SpecialTokenInfo& st = it->second;
                for (size_t i = 0; i < st.ids.size(); ++i) {
                    result.ids.push_back(st.ids[i]);
                    result.type_ids.push_back(piece.type_id);
                    if (i < st.tokens.size()) {
                        result.tokens.push_back(st.tokens[i]);
                    }
                }
            }
        }
    }

    return result;
}

EncodingResult TemplateProcessor::processWithOffsets(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        const std::vector<std::pair<size_t, size_t>>& offsets_a,
        const std::vector<std::pair<size_t, size_t>>& offsets_b,
        const std::vector<int>& word_ids_a,
        const std::vector<int>& word_ids_b,
        bool add_special) const {
    EncodingResult result;

    if (!add_special) {
        // No special tokens: concatenate A and B with their offsets
        result.ids = ids_a;
        result.type_ids.assign(ids_a.size(), 0);
        result.tokens = tokens_a;
        result.offsets.insert(result.offsets.end(), offsets_a.begin(), offsets_a.end());
        result.special_tokens_mask.assign(ids_a.size(), 0);
        result.word_ids = word_ids_a;

        if (!ids_b.empty()) {
            result.ids.insert(result.ids.end(), ids_b.begin(), ids_b.end());
            std::vector<int> b_types(ids_b.size(), 1);
            result.type_ids.insert(result.type_ids.end(), b_types.begin(), b_types.end());
            result.tokens.insert(result.tokens.end(), tokens_b.begin(), tokens_b.end());
            result.offsets.insert(result.offsets.end(), offsets_b.begin(), offsets_b.end());
            result.special_tokens_mask.resize(result.ids.size(), 0);
            result.word_ids.insert(result.word_ids.end(),
                word_ids_b.begin(), word_ids_b.end());
        }
        return result;
    }

    // Choose the appropriate template
    const std::vector<TemplatePiece>& tmpl =
        (!ids_b.empty() && !pair_template.empty()) ? pair_template : single_template;

    // Walk the template: SEQUENCE pieces get input offsets/word_ids,
    // SPECIAL_TOKEN pieces get (0,0) offset and word_id = -1
    size_t a_pos = 0;
    size_t b_pos = 0;

    for (const auto& piece : tmpl) {
        if (piece.type == TemplatePiece::SEQUENCE) {
            const std::vector<int>* ids_ptr = nullptr;
            const std::vector<std::string>* tokens_ptr = nullptr;
            const std::vector<std::pair<size_t, size_t>>* offsets_ptr = nullptr;
            const std::vector<int>* wids_ptr = nullptr;
            size_t* pos_ptr = nullptr;

            if (piece.id == "A") {
                ids_ptr = &ids_a;
                tokens_ptr = &tokens_a;
                offsets_ptr = &offsets_a;
                wids_ptr = &word_ids_a;
                pos_ptr = &a_pos;
            } else if (piece.id == "B") {
                ids_ptr = &ids_b;
                tokens_ptr = &tokens_b;
                offsets_ptr = &offsets_b;
                wids_ptr = &word_ids_b;
                pos_ptr = &b_pos;
            }

            if (ids_ptr) {
                for (size_t i = 0; i < ids_ptr->size(); ++i) {
                    result.ids.push_back((*ids_ptr)[i]);
                    result.type_ids.push_back(piece.type_id);
                    if (i < tokens_ptr->size()) {
                        result.tokens.push_back((*tokens_ptr)[i]);
                    }
                    if (*pos_ptr < offsets_ptr->size()) {
                        result.offsets.push_back((*offsets_ptr)[*pos_ptr]);
                    } else {
                        result.offsets.push_back({0, 0});
                    }
                    result.special_tokens_mask.push_back(0);
                    result.word_ids.push_back(
                        *pos_ptr < wids_ptr->size() ? (*wids_ptr)[*pos_ptr] : -1);
                    ++(*pos_ptr);
                }
            }
        } else if (piece.type == TemplatePiece::SPECIAL_TOKEN) {
            auto it = special_tokens.find(piece.id);
            if (it != special_tokens.end()) {
                const SpecialTokenInfo& st = it->second;
                for (size_t i = 0; i < st.ids.size(); ++i) {
                    result.ids.push_back(st.ids[i]);
                    result.type_ids.push_back(piece.type_id);
                    if (i < st.tokens.size()) {
                        result.tokens.push_back(st.tokens[i]);
                    }
                    result.offsets.push_back({0, 0});
                    result.special_tokens_mask.push_back(1);
                    result.word_ids.push_back(-1);
                }
            }
        }
    }

    return result;
}

// --- BertProcessor ---

static void readSpecialToken(const QoreHashNode* config, const char* key,
        const char* default_token, std::string& token_str, int& token_id) {
    token_str = default_token;
    token_id = -1;

    QoreValue val = config->getKeyValue(key);
    if (val.getType() == NT_HASH) {
        const QoreHashNode* h = safeGetHash(val);
        if (h) {
            std::string content = safeGetStringKey(h, "content");
            if (!content.empty()) {
                token_str = content;
            }
            QoreValue id_val = h->getKeyValue("id");
            if (id_val.getType() == NT_INT) {
                token_id = (int)id_val.getAsBigInt();
            }
        }
    } else if (val.getType() == NT_LIST) {
        // [token_string, id] tuple
        const QoreListNode* tuple = safeGetList(val);
        if (tuple && tuple->size() >= 2) {
            const QoreStringNode* tok = safeGetString(tuple->retrieveEntry(0));
            if (tok) {
                token_str = tok->c_str();
            }
            token_id = (int)tuple->retrieveEntry(1).getAsBigInt();
        }
    }
}

BertProcessor::BertProcessor(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    std::string cls_token, sep_token;
    int cls_id, sep_id;

    readSpecialToken(config, "cls", "[CLS]", cls_token, cls_id);
    readSpecialToken(config, "sep", "[SEP]", sep_token, sep_id);

    // Set up special tokens map
    if (cls_id >= 0) {
        special_tokens[cls_token] = {{cls_id}, {cls_token}};
    }
    if (sep_id >= 0) {
        special_tokens[sep_token] = {{sep_id}, {sep_token}};
    }

    // Single template: [CLS] A [SEP]
    single_template = {
        {TemplatePiece::SPECIAL_TOKEN, cls_token, 0},
        {TemplatePiece::SEQUENCE, "A", 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
    };

    // Pair template: [CLS] A [SEP] B [SEP]
    pair_template = {
        {TemplatePiece::SPECIAL_TOKEN, cls_token, 0},
        {TemplatePiece::SEQUENCE, "A", 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
        {TemplatePiece::SEQUENCE, "B", 1},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 1},
    };
}

// --- RobertaProcessor ---

RobertaProcessor::RobertaProcessor(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    std::string cls_token, sep_token;
    int cls_id, sep_id;

    readSpecialToken(config, "cls", "<s>", cls_token, cls_id);
    readSpecialToken(config, "sep", "</s>", sep_token, sep_id);

    // Set up special tokens map
    if (cls_id >= 0) {
        special_tokens[cls_token] = {{cls_id}, {cls_token}};
    }
    if (sep_id >= 0) {
        special_tokens[sep_token] = {{sep_id}, {sep_token}};
    }

    // Single template: <s> A </s>
    single_template = {
        {TemplatePiece::SPECIAL_TOKEN, cls_token, 0},
        {TemplatePiece::SEQUENCE, "A", 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
    };

    // Pair template: <s> A </s></s> B </s>
    pair_template = {
        {TemplatePiece::SPECIAL_TOKEN, cls_token, 0},
        {TemplatePiece::SEQUENCE, "A", 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
        {TemplatePiece::SEQUENCE, "B", 0},
        {TemplatePiece::SPECIAL_TOKEN, sep_token, 0},
    };
}

// --- ByteLevelPostProcessor ---

EncodingResult ByteLevelPostProcessor::process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool /* add_special */) const {
    // ByteLevel post-processor is a no-op passthrough
    EncodingResult result;

    result.ids = ids_a;
    result.type_ids.assign(ids_a.size(), 0);
    result.tokens = tokens_a;

    if (!ids_b.empty()) {
        result.ids.insert(result.ids.end(), ids_b.begin(), ids_b.end());
        std::vector<int> b_types(ids_b.size(), 1);
        result.type_ids.insert(result.type_ids.end(), b_types.begin(), b_types.end());
        result.tokens.insert(result.tokens.end(), tokens_b.begin(), tokens_b.end());
    }

    return result;
}

// --- SequencePostProcessor ---

SequencePostProcessor::SequencePostProcessor(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    const QoreListNode* proc_list = safeGetListKey(config, "processors");
    if (!proc_list) {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "Sequence post_processor config missing 'processors' list");
        return;
    }

    for (size_t i = 0; i < proc_list->size(); ++i) {
        const QoreHashNode* proc_config = safeGetHash(proc_list->retrieveEntry(i));
        if (!proc_config) {
            xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
                "Sequence post_processor: entry %zu is not a hash", i);
            return;
        }

        auto proc = AbstractPostProcessor::fromConfig(proc_config, xsink);
        if (*xsink) {
            return;
        }
        if (proc) {
            processors.push_back(std::move(proc));
        }
    }
}

EncodingResult SequencePostProcessor::process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const {
    if (processors.empty()) {
        // No processors: passthrough
        EncodingResult result;
        result.ids = ids_a;
        result.type_ids.assign(ids_a.size(), 0);
        result.tokens = tokens_a;
        if (!ids_b.empty()) {
            result.ids.insert(result.ids.end(), ids_b.begin(), ids_b.end());
            std::vector<int> b_types(ids_b.size(), 1);
            result.type_ids.insert(result.type_ids.end(), b_types.begin(), b_types.end());
            result.tokens.insert(result.tokens.end(), tokens_b.begin(), tokens_b.end());
        }
        return result;
    }

    // Apply the first processor
    EncodingResult result = processors[0]->process(ids_a, ids_b, tokens_a, tokens_b, add_special);

    // Apply subsequent processors: they receive the previous result as sequence A
    // and an empty sequence B
    for (size_t i = 1; i < processors.size(); ++i) {
        std::vector<int> empty_ids;
        std::vector<std::string> empty_tokens;
        result = processors[i]->process(result.ids, empty_ids, result.tokens, empty_tokens,
            add_special);
    }

    return result;
}

int TemplateProcessor::numAddedTokens(bool has_pair) const {
    const auto& tmpl = has_pair ? pair_template : single_template;
    int count = 0;
    for (const auto& piece : tmpl) {
        if (piece.type == TemplatePiece::SPECIAL_TOKEN) {
            auto it = special_tokens.find(piece.id);
            if (it != special_tokens.end()) {
                count += (int)it->second.ids.size();
            }
        }
    }
    return count;
}

} // namespace QoreTokenizer
