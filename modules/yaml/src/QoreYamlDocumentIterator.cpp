/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreYamlDocumentIterator.cpp

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "QC_YamlDocumentIterator.h"
#include "yaml-module.h"
#include "yaml-scalar-util.h"

QoreYamlDocumentIterator::QoreYamlDocumentIterator(const QoreStringNode* yaml_str,
                                                   bool skip_empty, int max_docs,
                                                   ExceptionSink* xsink)
    : parser_initialized(false), is_valid(false), skip_empty_docs(skip_empty),
      max_documents(max_docs), doc_number(0), has_event(false), from_stream(false),
      read_handler(nullptr) {

    TempEncodingHelper str(yaml_str, QCS_UTF8, xsink);
    if (*xsink) return;

    yaml_data = std::string(str->c_str(), str->strlen());
    initParser(xsink);
}

QoreYamlDocumentIterator::QoreYamlDocumentIterator(QoreObject* stream,
                                                   const QoreEncoding* encoding,
                                                   bool skip_empty, int max_docs,
                                                   ExceptionSink* xsink)
    : parser_initialized(false), is_valid(false), skip_empty_docs(skip_empty),
      max_documents(max_docs), doc_number(0), has_event(false), from_stream(true),
      read_handler(nullptr) {

    // Use shared streaming read handler for true streaming input
    read_handler.reset(new YamlStreamReadHandler(stream, encoding));
    initParser(xsink);
}

QoreYamlDocumentIterator::~QoreYamlDocumentIterator() {
    ExceptionSink xsink;
    current_value.discard(&xsink);
    clearAliasMap(&xsink);
    cleanupParser();
}

void QoreYamlDocumentIterator::clearAliasMap(ExceptionSink* xsink) {
    for (alias_map_t::iterator i = alias_map.begin(); i != alias_map.end(); ++i) {
        i->second.discard(xsink);
    }
    alias_map.clear();
}

void QoreYamlDocumentIterator::storeAnchor(const std::string& anchor, QoreValue val, ExceptionSink* xsink) {
    alias_map_t::iterator i = alias_map.lower_bound(anchor);
    if (i != alias_map.end() && i->first == anchor) {
        // Replace existing anchor
        i->second.discard(xsink);
        i->second = val.refSelf();
    } else {
        alias_map.insert(i, alias_map_t::value_type(anchor, val.refSelf()));
    }
}

int QoreYamlDocumentIterator::initParser(ExceptionSink* xsink) {
    if (!yaml_parser_initialize(&parser)) {
        xsink->raiseException(QY_PARSE_ERR, "failed to initialize YAML parser");
        return -1;
    }

    if (read_handler) {
        // Use shared streaming read handler for stream input
        read_handler->setupParser(&parser);
    } else {
        // Use string input for string-based iterator
        yaml_parser_set_input_string(&parser, (const unsigned char*)yaml_data.c_str(), yaml_data.size());
    }

    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    parser_initialized = true;
    return 0;
}

void QoreYamlDocumentIterator::cleanupParser() {
    discardEvent();
    if (parser_initialized) {
        yaml_parser_delete(&parser);
        parser_initialized = false;
    }
}

void QoreYamlDocumentIterator::discardEvent() {
    if (has_event) {
        yaml_event_delete(&event);
        has_event = false;
    }
}

int QoreYamlDocumentIterator::getEvent(ExceptionSink* xsink) {
    discardEvent();

    if (!yaml_parser_parse(&parser, &event)) {
        // Check for stream read error
        if (read_handler && read_handler->hasError()) {
            xsink->raiseException(QY_STREAM_ERR, "stream read error: %s",
                read_handler->getErrorMessage().c_str());
        } else {
            xsink->raiseException(QY_PARSE_ERR, "YAML parse error at line %d column %d: %s",
                (int)parser.problem_mark.line + 1, (int)parser.problem_mark.column + 1,
                parser.problem ? parser.problem : "unknown error");
        }
        return -1;
    }

    has_event = true;
    return 0;
}

bool QoreYamlDocumentIterator::next(ExceptionSink* xsink) {
    current_value.discard(xsink);
    current_value = QoreValue();
    is_valid = false;
    if (*xsink || qore_check_cancel(xsink, "YAML document iteration")) {
        return false;
    }

    // Check max documents limit
    if (max_documents > 0 && doc_number >= max_documents) {
        return false;
    }

    unsigned iteration = 0;
    while (true) {
        // Parsing a document can recursively process a large data structure.
        if (++iteration % 10 == 0) {
            if (qore_check_cancel(xsink, "YAML document iteration")) {
                return false;
            }
        }

        if (getEvent(xsink)) return false;

        switch (event.type) {
            case YAML_NO_EVENT:
            case YAML_STREAM_END_EVENT:
                // No more documents
                return false;

            case YAML_STREAM_START_EVENT:
                // Skip stream start
                continue;

            case YAML_DOCUMENT_START_EVENT: {
                // Clear alias map for new document
                clearAliasMap(xsink);

                // Parse the document
                doc_number++;
                QoreValue doc_value = parseDocument(xsink);
                if (*xsink) return false;

                // Skip empty documents if configured
                if (skip_empty_docs && doc_value.isNothing()) {
                    continue;
                }

                current_value = doc_value;
                is_valid = true;
                return true;
            }

            default:
                xsink->raiseException(QY_PARSE_ERR, "unexpected YAML event %d while looking for document",
                    (int)event.type);
                return false;
        }
    }
}

QoreValue QoreYamlDocumentIterator::getValue(ExceptionSink* xsink) {
    if (!is_valid) {
        xsink->raiseException("ITERATOR-ERROR", "iterator is not in a valid position");
        return QoreValue();
    }
    return current_value.refSelf();
}

int QoreYamlDocumentIterator::reset(ExceptionSink* xsink) {
    if (from_stream) {
        xsink->raiseException("ITERATOR-ERROR", "cannot reset stream-based iterator");
        return -1;
    }

    cleanupParser();
    current_value.discard(xsink);
    current_value = QoreValue();
    is_valid = false;
    doc_number = 0;
    return *xsink ? -1 : initParser(xsink);
}

QoreValue QoreYamlDocumentIterator::parseDocument(ExceptionSink* xsink) {
    if (getEvent(xsink)) return QoreValue();

    // Check for empty document
    if (event.type == YAML_DOCUMENT_END_EVENT) {
        return QoreValue();
    }

    QoreValue rv = parseNode(xsink);
    if (*xsink) return QoreValue();

    // Expect document end
    if (getEvent(xsink)) {
        rv.discard(xsink);
        return QoreValue();
    }

    if (event.type != YAML_DOCUMENT_END_EVENT) {
        rv.discard(xsink);
        xsink->raiseException(QY_PARSE_ERR, "expected document end event, got %d", (int)event.type);
        return QoreValue();
    }

    return rv;
}

QoreValue QoreYamlDocumentIterator::parseNode(ExceptionSink* xsink) {
    QoreValue rv;
    std::string anchor;

    switch (event.type) {
        case YAML_SCALAR_EVENT:
            if (event.data.scalar.anchor) {
                anchor = (const char*)event.data.scalar.anchor;
            }
            rv = parseScalar(xsink);
            break;

        case YAML_SEQUENCE_START_EVENT:
            if (event.data.sequence_start.anchor) {
                anchor = (const char*)event.data.sequence_start.anchor;
            }
            rv = parseSequence(xsink);
            break;

        case YAML_MAPPING_START_EVENT:
            if (event.data.mapping_start.anchor) {
                anchor = (const char*)event.data.mapping_start.anchor;
            }
            rv = parseMapping(xsink);
            break;

        case YAML_ALIAS_EVENT: {
            std::string alias_anchor((const char*)event.data.alias.anchor);
            alias_map_t::iterator i = alias_map.find(alias_anchor);
            if (i == alias_map.end()) {
                xsink->raiseException(QY_PARSE_ERR,
                    "reference to unknown anchor '%s' in alias", alias_anchor.c_str());
                return QoreValue();
            }
            return i->second.refSelf();
        }

        default:
            xsink->raiseException(QY_PARSE_ERR, "unexpected event type %d when parsing node",
                (int)event.type);
            return QoreValue();
    }

    if (*xsink) {
        return QoreValue();
    }

    // Store anchor if present
    if (!anchor.empty()) {
        storeAnchor(anchor, rv, xsink);
    }

    return rv;
}

QoreListNode* QoreYamlDocumentIterator::parseSequence(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);

    unsigned iteration = 0;
    while (true) {
        // Check cancellation every 10 potentially recursive parsing steps
        if (++iteration % 10 == 0) {
            if (qore_check_cancel(xsink, "YAML sequence parsing")) {
                return nullptr;
            }
        }

        if (getEvent(xsink)) return nullptr;

        if (event.type == YAML_SEQUENCE_END_EVENT) {
            break;
        }

        QoreValue val = parseNode(xsink);
        if (*xsink) return nullptr;

        l->push(val, xsink);
    }

    return l.release();
}

QoreHashNode* QoreYamlDocumentIterator::parseMapping(ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);

    // Collect merge key values to apply at the end (so explicit keys take precedence)
    ReferenceHolder<QoreListNode> merge_values(xsink);

    unsigned iteration = 0;
    while (true) {
        // Check cancellation every 10 potentially recursive parsing steps
        if (++iteration % 10 == 0) {
            if (qore_check_cancel(xsink, "YAML mapping parsing")) {
                return nullptr;
            }
        }

        if (getEvent(xsink)) return nullptr;

        if (event.type == YAML_MAPPING_END_EVENT) {
            break;
        }

        // Parse key (must be scalar for hash key)
        if (event.type != YAML_SCALAR_EVENT) {
            xsink->raiseException(QY_PARSE_ERR, "expected scalar key in mapping, got event type %d",
                (int)event.type);
            return nullptr;
        }

        std::string key((const char*)event.data.scalar.value, event.data.scalar.length);

        // Parse value
        if (getEvent(xsink)) return nullptr;

        QoreValue val = parseNode(xsink);
        if (*xsink) return nullptr;

        // Check for merge key
        if (key == "<<") {
            // Handle merge key - collect values to merge later
            if (!merge_values) {
                merge_values = new QoreListNode(autoTypeInfo);
            }
            merge_values->push(val, xsink);
            if (*xsink) return nullptr;
        } else {
            h->setKeyValue(key.c_str(), val, xsink);
            if (*xsink) return nullptr;
        }
    }

    // Apply merge values (explicit keys already in h take precedence)
    // Process in reverse order so later merge keys override earlier ones
    if (merge_values) {
        for (size_t mi = merge_values->size(); mi > 0; --mi) {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML merge expansion")) {
                return nullptr;
            }
            QoreValue mv = merge_values->retrieveEntry(mi - 1);
            if (mv.getType() == NT_HASH) {
                // Merge single mapping
                const QoreHashNode* mh = mv.get<const QoreHashNode>();
                ConstHashIterator hi(mh);
                while (hi.next()) {
                    if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML merge expansion")) {
                        return nullptr;
                    }
                    // Only set if key doesn't already exist
                    if (!h->existsKey(hi.getKey())) {
                        h->setKeyValue(hi.getKey(), hi.get().refSelf(), xsink);
                        if (*xsink) return nullptr;
                    }
                }
            } else if (mv.getType() == NT_LIST) {
                // Merge sequence of mappings (process in forward order - first in sequence takes precedence)
                const QoreListNode* ml = mv.get<const QoreListNode>();
                for (size_t i = 0; i < ml->size(); ++i) {
                    if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML merge expansion")) {
                        return nullptr;
                    }
                    QoreValue item = ml->retrieveEntry(i);
                    if (item.getType() == NT_HASH) {
                        const QoreHashNode* mh = item.get<const QoreHashNode>();
                        ConstHashIterator hi(mh);
                        while (hi.next()) {
                            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML merge expansion")) {
                                return nullptr;
                            }
                            if (!h->existsKey(hi.getKey())) {
                                h->setKeyValue(hi.getKey(), hi.get().refSelf(), xsink);
                                if (*xsink) return nullptr;
                            }
                        }
                    }
                }
            }
            // Silently ignore non-hash/non-list merge values (as per YAML spec behavior)
        }
    }

    return h.release();
}

QoreValue QoreYamlDocumentIterator::parseScalar(ExceptionSink* xsink) {
    // Use shared scalar parsing utilities for full type support
    const char* val = (const char*)event.data.scalar.value;
    size_t len = event.data.scalar.length;

    // Check for explicit tag - use shared tag parser
    if (event.data.scalar.tag) {
        const char* tag = (const char*)event.data.scalar.tag;
        return yaml_parse_tagged_scalar(val, len, tag, xsink);
    }

    // Use shared implicit scalar parser for type inference
    // This handles: booleans, null, sqlnull, dates, durations, numbers, strings
    return yaml_parse_implicit_scalar(val, len, event.data.scalar.style, false, xsink);
}

// Helper function to parse all documents
QoreListNode* parse_yaml_documents_intern(const QoreStringNode* yaml, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    QoreYamlDocumentIterator iter(yaml, false, 0, xsink);
    if (*xsink) return nullptr;

    unsigned iteration = 0;
    while (iter.next(xsink)) {
        if (++iteration % 10 == 0 && qore_check_cancel(xsink, "YAML document collection")) {
            return nullptr;
        }
        if (*xsink) return nullptr;

        QoreValue val = iter.getValue(xsink);
        if (*xsink) return nullptr;

        result->push(val, xsink);
        if (*xsink) return nullptr;
    }

    if (*xsink) return nullptr;
    return result.release();
}
