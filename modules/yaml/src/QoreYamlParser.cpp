/* indent-tabs-mode: nil -*- */
/*
    yaml Qore module

    Copyright (C) 2010 - 2026 Qore Technologies, s.r.o.

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

#include "yaml-module.h"
#include "yaml-scalar-util.h"

const char* QY_PARSE_ERR = "YAML-PARSER-ERROR";

QoreValue QoreYamlParser::parse() {
    ValueHolder rv(xsink);

    if (getCheckEvent(YAML_STREAM_START_EVENT))
        return QoreValue();

    if (getEvent())
        return QoreValue();

    if (event.type == YAML_DOCUMENT_START_EVENT) {
        if (getEvent())
            return QoreValue();

        if (event.type != YAML_DOCUMENT_END_EVENT) {
            rv = parseNode();
            if (*xsink)
                return QoreValue();

            if (getCheckEvent(YAML_DOCUMENT_END_EVENT))
                return QoreValue();

            if (getEvent())
                return QoreValue();
        }
    }

    if (checkEvent(YAML_STREAM_END_EVENT))
        return QoreValue();

    return rv.release();
}

QoreValue QoreYamlParser::parseNode(bool favor_string) {
    QoreValue rv;
    std::string anchor;
    if (event.data.scalar.anchor) {
        anchor = (const char*)event.data.scalar.anchor;
    }
    switch (event.type) {
        case YAML_SCALAR_EVENT:
            rv = parseScalar(favor_string);
            break;

        case YAML_SEQUENCE_START_EVENT:
            rv = parseSeq();
            break;

        case YAML_MAPPING_START_EVENT:
            rv = parseMap();
            break;

        case YAML_ALIAS_EVENT:
            return parseAlias();

        default:
            xsink->raiseException(QY_PARSE_ERR, "Unexpected event '%s' when parsing YAML document",
                get_event_name(event.type));
            return QoreValue();
    }

    if (!anchor.empty()) {
        alias_map_t::iterator i = alias_map.lower_bound(anchor);
        if (i != alias_map.end() && i->first == anchor) {
            // Replace existing anchor
            i->second.discard(xsink);
            i->second = rv.refSelf();
        } else {
            alias_map.insert(i, alias_map_t::value_type(anchor, rv.refSelf()));
        }
    }
    return rv;
}

QoreListNode* QoreYamlParser::parseSeq() {
    ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);

    unsigned iteration = 0;
    while (true) {
        // Check cancellation every 10 potentially recursive parsing steps
        if (++iteration % 10 == 0) {
            if (qore_check_cancel(xsink, "YAML sequence parsing")) {
                return nullptr;
            }
        }

        if (getEvent())
            return nullptr;

        if (event.type == YAML_SEQUENCE_END_EVENT)
            break;

        QoreValue rv = parseNode();
        if (*xsink) {
            return nullptr;
        }
        l->push(rv, nullptr);
    }

    return l.release();
}

QoreHashNode* QoreYamlParser::parseMap() {
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

        if (getEvent())
            return nullptr;

        if (event.type == YAML_MAPPING_END_EVENT)
            break;

        // get key node and convert to string
        ValueHolder key(parseNode(true), xsink);
        if (*xsink)
            return nullptr;

        //printd(5, "key=%p type=%s\n", *key, key->getTypeName());

        // convert to string in default encoding
        QoreStringValueHelper str(*key, QCS_DEFAULT, xsink);
        if (*xsink)
            return nullptr;

        // get value
        if (getEvent())
            return nullptr;

        QoreValue value = parseNode();
        if (*xsink)
            return nullptr;

        // Check for merge key
        if (!strcmp(str->c_str(), "<<")) {
            // Handle merge key - collect values to merge later
            if (!merge_values) {
                merge_values = new QoreListNode(autoTypeInfo);
            }
            merge_values->push(value, xsink);
            if (*xsink)
                return nullptr;
        } else {
            h->setKeyValue(str->c_str(), value, xsink);
            if (*xsink)
                return nullptr;
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
                        if (*xsink)
                            return nullptr;
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
                                if (*xsink)
                                    return nullptr;
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

QoreValue QoreYamlParser::parseAlias() {
    std::string anchor((const char*)event.data.alias.anchor);
    alias_map_t::iterator i = alias_map.find(anchor);
    if (i == alias_map.end()) {
        xsink->raiseException(QY_PARSE_ERR, "Reference to unknown anchor '%s' in alias", anchor.c_str());
        return QoreValue();
    }
    return i->second.refSelf();
}

QoreValue QoreYamlParser::parseScalar(bool favor_string) {
    // Use shared scalar parsing utilities
    const char* val = (const char*)event.data.scalar.value;
    size_t len = event.data.scalar.length;

    // Check for explicit tag - use shared tag parser
    if (event.data.scalar.tag) {
        const char* tag = (const char*)event.data.scalar.tag;
        return yaml_parse_tagged_scalar(val, len, tag, xsink);
    }

    if (core_schema) {
        return favor_string
            ? QoreValue(new QoreStringNode(val, len, QCS_UTF8))
            : yaml_parse_core_schema_scalar(val, len, event.data.scalar.style, xsink);
    }

    // Use shared implicit scalar parser for type inference
    return yaml_parse_implicit_scalar(val, len, event.data.scalar.style, favor_string, xsink);
}
