/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TextFeatures.cpp

    TextFeatures implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_TextFeatures.h"
#include "ml_serialization.h"
#include "ml_tokenizer_bridge.h"

#include <algorithm>
#include <unordered_map>

QoreTextFeatures::QoreTextFeatures(const std::string& mode, int max_features, int min_df,
    const std::string& tokenizer_mode, void* tokenizer_handle)
    : mode(mode), tokenizer_mode(tokenizer_mode), tokenizer_handle(tokenizer_handle),
      max_features(max_features), min_df(min_df) {
}

QoreTextFeatures::~QoreTextFeatures() {
    if (tokenizer_handle) {
        ml_tokenizer_destroy(tokenizer_handle);
        tokenizer_handle = nullptr;
    }
}

std::vector<std::string> QoreTextFeatures::tokenizeWhitespace(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            token += std::tolower(static_cast<unsigned char>(c));
        } else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> QoreTextFeatures::tokenize(const std::string& text) const {
    if (tokenizer_mode == "subword" && tokenizer_handle) {
        int32_t count = 0;
        char** tok_result = ml_tokenizer_tokenize(tokenizer_handle,
            text.c_str(), text.size(), &count);
        if (tok_result && count > 0) {
            std::vector<std::string> tokens;
            tokens.reserve(count);
            for (int32_t i = 0; i < count; ++i) {
                tokens.push_back(tok_result[i]);
            }
            ml_tokenizer_free_tokens(tok_result, count);
            return tokens;
        }
    }
    return tokenizeWhitespace(text);
}

void QoreTextFeatures::fit(const QoreListNode* documents, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (!documents || documents->empty()) {
        xsink->raiseException("ML-TEXT-FEATURES-ERROR", "cannot fit on empty document list");
        return;
    }

    int64_t num_docs = documents->size();

    // Count document frequency for each term
    std::unordered_map<std::string, int> df_counts;

    for (int64_t i = 0; i < num_docs; ++i) {
        QoreValue v = documents->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            xsink->raiseException("ML-TEXT-FEATURES-ERROR",
                "document at index " QLLD " is NOTHING/NULL", i);
            return;
        }
        if (v.getType() != NT_STRING) {
            xsink->raiseException("ML-TEXT-FEATURES-ERROR",
                "document at index " QLLD " is not a string (type: %s)", i,
                v.getTypeName());
            return;
        }
        const QoreStringNode* str = v.get<const QoreStringNode>();
        std::vector<std::string> tokens = tokenize(str->c_str());

        // Count unique terms in this document
        std::unordered_map<std::string, bool> seen;
        for (const auto& tok : tokens) {
            if (!seen[tok]) {
                df_counts[tok]++;
                seen[tok] = true;
            }
        }
    }

    // Filter by min_df
    std::vector<std::pair<std::string, int>> filtered;
    for (const auto& entry : df_counts) {
        if (entry.second >= min_df) {
            filtered.push_back(entry);
        }
    }

    // Sort by document frequency descending, then alphabetically for determinism
    std::sort(filtered.begin(), filtered.end(),
        [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
            if (a.second != b.second) {
                return a.second > b.second;
            }
            return a.first < b.first;
        });

    // Apply max_features limit
    if (max_features > 0 && (int)filtered.size() > max_features) {
        filtered.resize(max_features);
    }

    // Build vocabulary map sorted alphabetically for deterministic output
    std::vector<std::string> terms;
    terms.reserve(filtered.size());
    for (const auto& entry : filtered) {
        terms.push_back(entry.first);
    }
    std::sort(terms.begin(), terms.end());

    vocabulary.clear();
    field_names.clear();
    for (int i = 0; i < (int)terms.size(); ++i) {
        vocabulary[terms[i]] = i;
        field_names.push_back(terms[i]);
    }

    n_docs = (int)num_docs;
    n_features = (int)vocabulary.size();

    // Compute IDF values if in tfidf mode
    if (mode == "tfidf") {
        idf_values.resize(n_features);
        for (const auto& entry : vocabulary) {
            // sklearn convention: idf = log((1 + n_docs) / (1 + df)) + 1
            int df = df_counts[entry.first];
            idf_values[entry.second] = std::log((1.0 + n_docs) / (1.0 + df)) + 1.0;
        }
    }

    fitted = true;
}

QoreListNode* QoreTextFeatures::transform(const QoreListNode* documents,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-TEXT-FEATURES-ERROR",
            "transformer has not been fitted; call fit() before transform()");
        return nullptr;
    }

    if (!documents || documents->empty()) {
        xsink->raiseException("ML-TEXT-FEATURES-ERROR", "cannot transform empty document list");
        return nullptr;
    }

    int64_t num_docs = documents->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    for (int64_t i = 0; i < num_docs; ++i) {
        QoreValue v = documents->retrieveEntry(i);
        if (v.isNullOrNothing()) {
            xsink->raiseException("ML-TEXT-FEATURES-ERROR",
                "document at index " QLLD " is NOTHING/NULL", i);
            return nullptr;
        }
        if (v.getType() != NT_STRING) {
            xsink->raiseException("ML-TEXT-FEATURES-ERROR",
                "document at index " QLLD " is not a string (type: %s)", i,
                v.getTypeName());
            return nullptr;
        }
        const QoreStringNode* str = v.get<const QoreStringNode>();
        std::vector<std::string> tokens = tokenize(str->c_str());

        // Count term frequencies
        std::unordered_map<std::string, int> tf_counts;
        for (const auto& tok : tokens) {
            tf_counts[tok]++;
        }

        // Build feature vector
        std::vector<double> vec(n_features, 0.0);

        if (mode == "count") {
            for (const auto& entry : tf_counts) {
                auto it = vocabulary.find(entry.first);
                if (it != vocabulary.end()) {
                    vec[it->second] = (double)entry.second;
                }
            }
        } else {
            // tfidf mode: tf * idf where tf = count / total_tokens
            double total_tokens = (double)tokens.size();
            if (total_tokens > 0.0) {
                for (const auto& entry : tf_counts) {
                    auto it = vocabulary.find(entry.first);
                    if (it != vocabulary.end()) {
                        double tf = (double)entry.second / total_tokens;
                        vec[it->second] = tf * idf_values[it->second];
                    }
                }
            }
        }

        // Convert vector to QoreListNode
        ReferenceHolder<QoreListNode> row(new QoreListNode(floatTypeInfo), xsink);
        for (int j = 0; j < n_features; ++j) {
            row->push(vec[j], xsink);
        }
        result->push(row.release(), xsink);
    }

    return result.release();
}

QoreListNode* QoreTextFeatures::fitTransform(const QoreListNode* documents,
        ExceptionSink* xsink) {
    fit(documents, xsink);
    if (*xsink) {
        return nullptr;
    }
    return transform(documents, xsink);
}

QoreListNode* QoreTextFeatures::getVocabulary(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-TEXT-FEATURES-ERROR",
            "transformer has not been fitted; call fit() before getVocabulary()");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    for (const auto& name : field_names) {
        names->push(new QoreStringNode(name), xsink);
    }
    return names.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreTextFeatures::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeString(buf, mode);
    MLSerialization::writeString(buf, tokenizer_mode);
    MLSerialization::writeInt32(buf, max_features);
    MLSerialization::writeInt32(buf, min_df);
    MLSerialization::writeInt32(buf, n_docs);
    MLSerialization::writeInt32(buf, n_features);

    // Write vocabulary as parallel string/int vectors
    MLSerialization::writeInt32(buf, (int32_t)vocabulary.size());
    for (const auto& entry : vocabulary) {
        MLSerialization::writeString(buf, entry.first);
        MLSerialization::writeInt32(buf, entry.second);
    }

    // Write IDF values
    MLSerialization::writeDoubleVector(buf, idf_values);

    // Write field names
    MLSerialization::writeStringVector(buf, field_names);

    return buf;
}

QoreTextFeatures* QoreTextFeatures::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    std::string mode = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string tok_mode = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int max_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int min_df = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_docs = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    // Read vocabulary
    int vocab_size = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::map<std::string, int> vocab;
    for (int i = 0; i < vocab_size; ++i) {
        std::string word = MLSerialization::readString(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        int idx = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        vocab[word] = idx;
    }

    // Read IDF values
    std::vector<double> idf = MLSerialization::readDoubleVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    // Read field names
    std::vector<std::string> fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* tf = new QoreTextFeatures(mode, max_features, min_df, tok_mode);
    tf->n_docs = n_docs;
    tf->n_features = n_features;
    tf->vocabulary = std::move(vocab);
    tf->idf_values = std::move(idf);
    tf->field_names = std::move(fnames);
    tf->fitted = true;
    return tf;
}
