# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdFraming.qc @brief JSON-LD Framing Algorithm (W3C 4.7)

/*  JsonLdFraming.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! JSON-LD Framing Algorithm
/** Implements the Framing Algorithm (W3C Section 4.7).
    Frames a JSON-LD document using a query-by-example frame.

    @since json 1.12
*/
public class JsonLdFraming {
    private {
        #! Blank node generator
        JsonLdBlankNodeGenerator blankNodeGenerator;

        #! Framing options
        hash<JsonLdOptions> options;
    }

    #! Creates a new framing processor
    constructor(hash<JsonLdOptions> opts) {
        blankNodeGenerator = new JsonLdBlankNodeGenerator();
        options = opts;
    }

    #! Frames expanded JSON-LD using a frame
    /** @param expanded the expanded JSON-LD document
        @param frame the expanded frame
        @return the framed result

        @since json 1.12
    */
    hash<auto> frame(list<auto> expanded, hash<auto> frame) {
        # 1. Generate node map via flattening
        JsonLdFlattening flattener();
        hash<auto> flat = flattener.flatten(expanded);
        list<auto> graph_nodes = flat."@graph" ?? ();

        # 2. Build subject map
        hash<auto> subjects = {};
        foreach hash<auto> node in (graph_nodes) {
            if (exists node."@id") {
                subjects{node."@id"} = node;
            }
        }

        # 3. Frame matching
        string embed_mode = options.embed ?? "@once";
        bool explicit_mode = options.explicit_ ?? False;
        bool require_all = options.requireAll ?? False;
        bool omit_default = options.omitDefault ?? False;

        list<auto> result = ();
        hash<string, bool> embedded = {};

        frameNodes(subjects, frame, \result, embed_mode, explicit_mode,
            require_all, omit_default, \embedded);

        return {"@graph": result};
    }

    #! Frames nodes matching the frame pattern
    private frameNodes(hash<auto> subjects, hash<auto> frame, reference<list<auto>> result,
            string embed_mode, bool explicit_mode, bool require_all,
            bool omit_default, reference<hash<string, bool>> embedded) {
        # Find matching subjects
        list<string> matching = findMatchingSubjects(subjects, frame, require_all);

        foreach string id in (matching) {
            hash<auto> subject = subjects{id};

            # Check embed rules
            if (embed_mode == "@never") {
                push result, {"@id": id};
                continue;
            }

            if (embed_mode == "@once" && embedded{id}) {
                push result, {"@id": id};
                continue;
            }

            embedded{id} = True;

            # Build the framed output
            hash<auto> output = {};

            # Always include @id
            output."@id" = id;

            # Process @type
            if (exists subject."@type") {
                output."@type" = subject."@type";
            }

            # Process frame properties
            foreach string prop in (keys frame) {
                if (isJsonLdKeyword(prop)) {
                    continue;
                }

                if (exists subject{prop}) {
                    # Property exists in subject
                    auto frame_value = frame{prop};
                    auto subject_value = subject{prop};

                    if (frame_value.typeCode() == NT_LIST && frame_value.size() > 0
                            && frame_value[0].typeCode() == NT_HASH) {
                        # Sub-frame: recursively frame embedded nodes
                        hash<auto> sub_frame = frame_value[0];
                        list<auto> framed_values = ();

                        list<auto> sub_values = subject_value.typeCode() == NT_LIST
                            ? subject_value : (subject_value,);

                        foreach auto sv in (sub_values) {
                            if (sv.typeCode() == NT_HASH && exists sv."@id"
                                    && exists subjects{sv."@id"}) {
                                # Embedded node reference - frame it
                                list<auto> sub_result = ();
                                hash<auto> sub_subjects = {};
                                sub_subjects{sv."@id"} = subjects{sv."@id"};
                                frameNodes(sub_subjects, sub_frame, \sub_result,
                                    embed_mode, explicit_mode, require_all,
                                    omit_default, \embedded);
                                framed_values += sub_result;
                            } else {
                                push framed_values, sv;
                            }
                        }

                        output{prop} = framed_values;
                    } else {
                        output{prop} = subject_value;
                    }
                } else if (!explicit_mode) {
                    # In non-explicit mode, omit missing properties
                    # Unless @default is specified in frame
                    if (frame{prop}.typeCode() == NT_LIST && frame{prop}.size() > 0
                            && frame{prop}[0].typeCode() == NT_HASH
                            && exists ((frame{prop}[0]))."@default") {
                        output{prop} = ((frame{prop}[0]))."@default";
                    } else if (!omit_default) {
                        output{prop} = ({"@null": True},);
                    }
                }
            }

            # In non-explicit mode, include all subject properties not in frame
            if (!explicit_mode) {
                foreach string prop in (keys subject) {
                    if (!exists output{prop} && !isJsonLdKeyword(prop)) {
                        output{prop} = subject{prop};
                    }
                }
            }

            push result, output;
        }
    }

    #! Finds subjects matching the frame pattern
    private list<string> findMatchingSubjects(hash<auto> subjects, hash<auto> frame,
            bool require_all) {
        list<string> matching = ();

        foreach string id in (keys subjects) {
            hash<auto> subject = subjects{id};
            if (matchesFrame(subject, frame, require_all)) {
                push matching, id;
            }
        }

        return matching;
    }

    #! Checks if a subject matches a frame pattern
    private bool matchesFrame(hash<auto> subject, hash<auto> frame, bool require_all) {
        # Check @type match
        if (exists frame."@type") {
            list<auto> frame_types = frame."@type".typeCode() == NT_LIST
                ? frame."@type" : (frame."@type",);
            list<auto> subject_types = ();
            if (subject."@type".typeCode() == NT_LIST) {
                subject_types = subject."@type";
            } else if (exists subject."@type") {
                subject_types = (subject."@type",);
            }

            if (frame_types.size() == 0) {
                # Empty @type array: wildcard
            } else {
                bool type_match = False;
                foreach auto ft in (frame_types) {
                    foreach auto st in (subject_types) {
                        if (ft == st) {
                            type_match = True;
                            break;
                        }
                    }
                    if (type_match) {
                        break;
                    }
                }
                if (!type_match) {
                    return False;
                }
            }
        }

        # Check @id match
        if (exists frame."@id") {
            if (frame."@id".typeCode() == NT_STRING) {
                if (subject."@id" != frame."@id") {
                    return False;
                }
            } else if (frame."@id".typeCode() == NT_LIST) {
                if (!inlist(subject."@id", frame."@id")) {
                    return False;
                }
            }
        }

        # Check property matches
        int matches = 0;
        int required = 0;
        foreach string prop in (keys frame) {
            if (isJsonLdKeyword(prop)) {
                continue;
            }
            ++required;
            if (exists subject{prop}) {
                ++matches;
            }
        }

        if (require_all && matches < required) {
            return False;
        }

        return True;
    }
}

} # namespace JsonLd
