# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoCodecs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the KlaviyoDataProvider module
public namespace KlaviyoDataProvider {
#! The wire codecs this application registers
/** Klaviyo spells two of every listing's query arguments as compound strings in a syntax of its own:

    |!Argument|!Wire form|!What it means
    |\c filter|\c "any(name,[\"a\",\"b\"]),contains(x,\"y\")"|a comma-separated list of predicates
    |\c sort|\c "-created"|a field name, with a leading \c "-" for descending

    Both are correct and neither is usable as a form.  Asking somebody to type Klaviyo's filter syntax by
    hand - with its JSON-quoted values and per-field operators - is asking them to get it wrong, and the
    application this module replaces did not: it published a structured \c filter of named fields and a
    \c sort of \c "{field, direction}", and assembled the strings itself.

    So the same structures are published here and converted by a codec, which keeps the option names and
    types existing workflows were written against.

    @section klaviyocodecs_operators The Operator Belongs to the Field

    Klaviyo's filter predicates take an operator - \c equals, \c any, \c contains and others - and which one
    is right depends on the field, not on the caller: a name is matched by \c contains where a set of IDs is
    matched by \c any.  The application being replaced hard-coded that choice per action, and so does this:
    each filter codec carries its own field set with the operator each field uses, which is why there is one
    codec per listing rather than one shared codec.

    A field a codec does not declare cannot be filtered on through the action; \c make-api-call reaches the
    rest of the syntax.

    @section klaviyocodecs_campaign The Campaign Listing Is the Reason \c lift Exists

    \c "GET /api/campaigns" declares \c filter **required**, because Klaviyo will not list campaigns across
    channels: a request has to say \c "equals(messages.channel,\"email\")" or the SMS equivalent.  The
    superseded application therefore published \c channel and \c name rather than a filter, and folded them
    into the predicate itself.

    Publishing a \c filter option here would have renamed both.  Instead the campaign filter codec presents
    the argument as an object of \c channel and \c name, and the manifest marks it
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::lift "lift", which publishes those two as options of
    the action.  The codec then assembles the predicate from them.

    @section klaviyocodecs_registration Registration

    Codecs live in a process-wide registry, so they are registered before the action set that references
    them is built - see @ref KlaviyoDataProvider::KlaviyoSchema::getActionSet() "getActionSet()".
    Registering twice is harmless; the second registration replaces the first with the same value.
*/
public class KlaviyoCodecs {
    public {
        #! Codec name: a \c "{field, direction}" object presented for Klaviyo's signed sort token
        const Sort = "klaviyo-sort";

        #! Codec name: the list listing's filter
        const ListFilter = "klaviyo-list-filter";

        #! Codec name: the segment listing's filter
        const SegmentFilter = "klaviyo-segment-filter";

        #! Codec name: the profile listing's filter
        const ProfileFilter = "klaviyo-profile-filter";

        #! Codec name: the tag listing's filter
        const TagFilter = "klaviyo-tag-filter";

        #! Codec name: the campaign listing's filter, whose properties are published as options
        const CampaignFilter = "klaviyo-campaign-filter";

        #! The sort direction meaning "descending", which Klaviyo spells as a leading "-"
        const Descending = "desc";

        #! The sort direction meaning "ascending", which Klaviyo spells as no prefix
        const Ascending = "asc";

        #! The fields each filter codec publishes
        /** Each entry names the field as Klaviyo spells it on the wire, the operator its predicate uses,
            and whether the option takes a list of values or one.  The key is the option name, which is the
            name the superseded application used.
        */
        const FilterSpecs = {
            ListFilter: {
                "name": {
                    "wire_field": "name",
                    "operator": "any",
                    "list": True,
                    "display_name": "Name",
                    "short_desc": "List names to match",
                    "desc": "Return only lists whose name is one of these",
                },
                "id": {
                    "wire_field": "id",
                    "operator": "any",
                    "list": True,
                    "display_name": "ID",
                    "short_desc": "List IDs to match",
                    "desc": "Return only the lists with these IDs",
                },
            },
            SegmentFilter: {
                "name": {
                    "wire_field": "name",
                    "operator": "any",
                    "list": True,
                    "display_name": "Name",
                    "short_desc": "Segment names to match",
                    "desc": "Return only segments whose name is one of these",
                },
                "id": {
                    "wire_field": "id",
                    "operator": "any",
                    "list": True,
                    "display_name": "ID",
                    "short_desc": "Segment IDs to match",
                    "desc": "Return only the segments with these IDs",
                },
            },
            ProfileFilter: {
                "email": {
                    "wire_field": "email",
                    "operator": "any",
                    "list": True,
                    "display_name": "E-mail",
                    "short_desc": "E-mail addresses to match",
                    "desc": "Return only profiles with one of these e-mail addresses",
                },
                "phone_number": {
                    "wire_field": "phone_number",
                    "operator": "any",
                    "list": True,
                    "display_name": "Phone Number",
                    "short_desc": "Phone numbers to match",
                    "desc": "Return only profiles with one of these phone numbers, in E.164 format",
                },
                "external_id": {
                    "wire_field": "external_id",
                    "operator": "any",
                    "list": True,
                    "display_name": "External ID",
                    "short_desc": "Your own identifiers to match",
                    "desc": "Return only profiles carrying one of these external IDs",
                },
                "id": {
                    "wire_field": "id",
                    "operator": "any",
                    "list": True,
                    "display_name": "ID",
                    "short_desc": "Profile IDs to match",
                    "desc": "Return only the profiles with these IDs",
                },
            },
            TagFilter: {
                "name": {
                    "wire_field": "name",
                    "operator": "contains",
                    "list": False,
                    "display_name": "Name",
                    "short_desc": "Text the tag name must contain",
                    "desc": "Return only tags whose name contains this text",
                },
            },
            CampaignFilter: {
                "channel": {
                    "wire_field": "messages.channel",
                    "operator": "any",
                    "list": True,
                    "required": True,
                    "display_name": "Channel",
                    "short_desc": "The channel or channels whose campaigns to list",
                    "desc": "The channel or channels whose campaigns to list.\n\n"
                        "Klaviyo will not list campaigns without being told which channel to look at, "
                        "which is why this is required; naming both lists both.",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "email",
                            "display_name": "E-mail",
                            "desc": "E-mail campaigns",
                        },
                        <AllowedValueInfo>{
                            "value": "sms",
                            "display_name": "SMS",
                            "desc": "SMS campaigns",
                        },
                    ),
                },
                "name": {
                    "wire_field": "name",
                    "operator": "contains",
                    "list": False,
                    "display_name": "Name",
                    "short_desc": "Text the campaign name must contain",
                    "desc": "Return only campaigns whose name contains this text",
                },
            },
        };
    }

    private {
        #! Set once the codecs have been registered
        static bool registered = False;

        #! Serializes registration
        static Mutex lck();
    }

    #! Registers this application's codecs
    /** Called before the action set that references them is built.  Safe to call more than once.
    */
    static register() {
        AutoLock al(lck);
        if (registered) {
            return;
        }
        RestSchemaActionCodecs::registerCodec(KlaviyoCodecs::getSortCodec());
        map RestSchemaActionCodecs::registerCodec(KlaviyoCodecs::getFilterCodec($1.key, $1.value)),
            FilterSpecs.pairIterator();
        registered = True;
    }

    #! Returns the codec presenting Klaviyo's signed sort token as a field and a direction
    /** The permitted fields differ per listing and are set by each action's overlay, because the schema
        declares them per operation; the direction is the same everywhere.
    */
    private static hash<RestSchemaWireCodecInfo> getSortCodec() {
        HashDataType type(AutoHashOrNothingType, "klaviyo sort");
        type.addQoreFields({
            "field": {
                "name": "field",
                "display_name": "Field",
                "short_desc": "The field to sort by",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The field to sort by",
            },
            "direction": {
                "name": "direction",
                "display_name": "Direction",
                "short_desc": "Ascending or descending",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Whether to sort ascending or descending",
                "default_value": Descending,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": Ascending,
                        "display_name": "Ascending",
                        "desc": "Smallest or earliest first",
                    },
                    <AllowedValueInfo>{
                        "value": Descending,
                        "display_name": "Descending",
                        "desc": "Largest or latest first",
                    },
                ),
            },
        });
        return <RestSchemaWireCodecInfo>{
            "name": Sort,
            "type": type,
            "desc": "sent as Klaviyo's sort token: the field name, with a leading `-` for descending",
            "to_wire": auto sub (auto v) {
                if (!exists v || v.typeCode() != NT_HASH || !v.field.val()) {
                    return;
                }
                return (v.direction ?? Descending) == Ascending ? v.field : "-" + v.field;
            },
            "from_wire": auto sub (auto v) {
                if (!exists v) {
                    return v;
                }
                string s = v.toString();
                return s =~ /^-/
                    ? {"field": s.substr(1), "direction": Descending}
                    : {"field": s, "direction": Ascending};
            },
        };
    }

    #! Returns a codec presenting one listing's filter as an object of named fields
    /** @param name the codec name
        @param spec the fields the codec publishes, as declared in @ref FilterSpecs
    */
    private static hash<RestSchemaWireCodecInfo> getFilterCodec(string name, hash<auto> spec) {
        HashDataType type(AutoHashOrNothingType, "klaviyo " + name);
        hash<auto> fields = {};
        foreach hash<auto> i in (spec.pairIterator()) {
            hash<auto!> f = {
                "name": i.key,
                "display_name": i.value.display_name,
                "short_desc": i.value.short_desc,
                "desc": i.value.desc,
                "type": i.value."list"
                    ? AbstractDataProviderType::get(new Type(i.value.required
                        ? "softlist<string>" : "*softlist<string>"))
                    : AbstractDataProviderTypeMap{i.value.required ? "string" : "*string"},
            };
            if (i.value.allowed_values) {
                # a list-valued option constrains its elements, not the list
                if (i.value."list") {
                    f.element_allowed_values = i.value.allowed_values;
                } else {
                    f.allowed_values = i.value.allowed_values;
                }
            }
            if (i.value.required) {
                f.attr = {"required": True, "preselected": True};
            }
            fields{i.key} = f;
        }
        type.addQoreFields(fields);
        return <RestSchemaWireCodecInfo>{
            "name": name,
            "type": type,
            "desc": sprintf("sent as Klaviyo's filter syntax, `%s`",
                KlaviyoCodecs::describeFilter(spec)),
            "to_wire": auto sub (auto v) {
                return KlaviyoCodecs::buildFilter(v, spec);
            },
            "from_wire": auto sub (auto v) {
                return KlaviyoCodecs::parseFilter(v, spec);
            },
        };
    }

    #! Returns an example of a filter codec's wire form, for the generated field documentation
    private static string describeFilter(hash<auto> spec) {
        list<string> parts = ();
        foreach hash<auto> i in (spec.pairIterator()) {
            parts += sprintf("%s(%s,…)", i.value.operator, i.value.wire_field);
        }
        return parts.join(",");
    }

    #! Assembles Klaviyo's filter syntax from the fields the action published
    /** A field left unset contributes no predicate, and an empty list or an empty string is the same as
        unset: a predicate matching nothing is not what an unfilled form means.
    */
    static *string buildFilter(auto v, hash<auto> spec) {
        if (!exists v || v.typeCode() != NT_HASH) {
            return;
        }
        list<string> parts = ();
        foreach hash<auto> i in (spec.pairIterator()) {
            auto value = v{i.key};
            if (!exists value) {
                continue;
            }
            if (i.value."list") {
                list<auto> values = (map $1, (value.typeCode() == NT_LIST ? value : (value,)),
                    $1.toString().val()) ?? ();
                if (!values) {
                    continue;
                }
                # Klaviyo takes a single value bare and several as a JSON array, which is what the
                # application being replaced sent
                parts += sprintf("%s(%s,%s)", i.value.operator, i.value.wire_field,
                    make_json(values.size() == 1 ? values[0].toString() : (map $1.toString(), values)));
                continue;
            }
            if (!value.toString().val()) {
                continue;
            }
            parts += sprintf("%s(%s,%s)", i.value.operator, i.value.wire_field,
                make_json(value.toString()));
        }
        return parts ? parts.join(",") : NOTHING;
    }

    #! Reads Klaviyo's filter syntax back into the fields the action published
    /** The inverse of @ref buildFilter().  A predicate naming a field the codec does not publish is an
        error rather than being dropped, because dropping it would silently widen the filter.
    */
    static *hash<auto> parseFilter(auto v, hash<auto> spec) {
        if (!exists v) {
            return;
        }
        # index the spec by the field name as it appears on the wire
        hash<string, string> by_wire = {};
        map by_wire{$1.value.wire_field} = $1.key, spec.pairIterator();

        hash<auto!> rv = {};
        foreach string term in (KlaviyoCodecs::splitFilter(v.toString())) {
            *list<*string> m = (term =~ x/^\s*([A-Za-z-]+)\s*\((.*)\)\s*$/s);
            if (!m) {
                throw "KLAVIYO-FILTER-ERROR", sprintf("cannot read %y as a Klaviyo filter predicate; a "
                    "predicate is spelled \"operator(field,value)\"", term);
            }
            string body = m[1];
            int comma = body.find(",");
            if (comma < 0) {
                throw "KLAVIYO-FILTER-ERROR", sprintf("the filter predicate %y names no value; a predicate "
                    "is spelled \"operator(field,value)\"", term);
            }
            string field = trim(body.substr(0, comma));
            *string opt = by_wire{field};
            if (!opt) {
                throw "KLAVIYO-FILTER-ERROR", sprintf("the filter names field %y, which this action does "
                    "not publish; it publishes %y", field, sort(keys by_wire));
            }
            auto value = parse_json(trim(body.substr(comma + 1)));
            rv{opt} = spec{opt}."list" && value.typeCode() != NT_LIST ? (value,) : value;
        }
        return rv ? rv : NOTHING;
    }

    #! Splits Klaviyo's filter syntax into its predicates
    /** A predicate's value may itself be a JSON array containing commas, so the split has to track nesting
        and quoting rather than splitting on every comma.
    */
    private static list<string> splitFilter(string filter) {
        list<string> rv = ();
        int depth = 0;
        bool in_string = False;
        bool escaped = False;
        string current = "";
        foreach string c in (filter.split("")) {
            if (escaped) {
                escaped = False;
                current += c;
                continue;
            }
            if (in_string) {
                switch (c) {
                    case "\\": escaped = True; break;
                    case "\"": in_string = False; break;
                }
                current += c;
                continue;
            }
            switch (c) {
                case "\"":
                    in_string = True;
                    break;
                case "(":
                case "[":
                    ++depth;
                    break;
                case ")":
                case "]":
                    --depth;
                    break;
                case ",":
                    if (!depth) {
                        if (trim(current).val()) {
                            rv += current;
                        }
                        current = "";
                        continue;
                    }
                    break;
            }
            current += c;
        }
        if (trim(current).val()) {
            rv += current;
        }
        return rv;
    }
}
}
