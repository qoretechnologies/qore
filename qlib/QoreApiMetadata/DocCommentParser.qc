# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DocCommentParser.qc Doxygen-style doc comment parser

/*  DocCommentParser.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @cond nodoc
public namespace QoreApiMetadata {
#! @endcond

#! Parses doxygen-style doc comments into structured fields
public class DocCommentParser {
    #! Parse a doc comment string into structured fields
    /** @param comment the raw doc comment string (with delimiters)
        @return a hash with parsed fields:
        - \c description: main description text
        - \c brief: brief description (from \@brief tag)
        - \c params: hash<string, string> mapping param name to description
        - \c return_description: return value description
        - \c throws: list of ThrowInfo hashes
        - \c since: version string
        - \c see_also: list of see-also references
        - \c deprecated: deprecation text or True if just marked deprecated
        - \c notes: list of note strings
        - \c warnings: list of warning strings
        - \c examples: list of code example strings
    */
    static hash<auto> parse(string comment) {
        # strip delimiters and normalize lines
        list<string> lines = DocCommentParser::stripDelimiters(comment);

        hash<auto> result = {
            "description": "",
            "brief": NOTHING,
            "params": {},
            "return_description": NOTHING,
            "throws": new list<hash<ThrowInfo>>(),
            "since": NOTHING,
            "see_also": new list<string>(),
            "deprecated": NOTHING,
            "notes": new list<string>(),
            "warnings": new list<string>(),
            "examples": new list<string>(),
        };

        # current tag being accumulated
        *string current_tag;
        # current tag argument (e.g., param name, throw name)
        *string current_arg;
        # accumulated text for current tag
        string current_text = "";
        # whether we are inside a @code block
        bool in_code = False;
        string code_text = "";

        foreach string line in (lines) {
            # handle @code/@endcode blocks
            if (in_code) {
                if (line =~ /@endcode/) {
                    result.examples += code_text;
                    in_code = False;
                    code_text = "";
                    continue;
                }
                if (code_text.length()) {
                    code_text += "\n";
                }
                code_text += line;
                continue;
            }

            # blank line flushes current tag and returns to description mode
            if (!line.length()) {
                if (exists current_tag) {
                    DocCommentParser::flushTag(\result, current_tag, current_arg, current_text);
                    current_tag = NOTHING;
                    current_arg = NOTHING;
                    current_text = "";
                }
                continue;
            }

            if (line =~ /@code\b/) {
                # flush current tag first
                DocCommentParser::flushTag(\result, current_tag, current_arg, current_text);
                current_tag = NOTHING;
                current_arg = NOTHING;
                current_text = "";
                in_code = True;
                # check if there's content after @code on the same line
                *string after = (line =~ x/@code\s*\{[^}]*\}\s*(.*)/)[0];
                if (!exists after) {
                    after = (line =~ x/@code\s+(.*)/)[0];
                }
                if (exists after && after.length()) {
                    code_text = after;
                }
                continue;
            }

            # check for tag
            *list<string> tag_match = (line =~ x/^@(\w+)\s*(.*)/);
            if (exists tag_match) {
                # flush previous tag
                DocCommentParser::flushTag(\result, current_tag, current_arg, current_text);
                current_tag = NOTHING;
                current_arg = NOTHING;
                current_text = "";

                string tag_name = tag_match[0];
                string tag_rest = tag_match[1];

                switch (tag_name) {
                    case "param": {
                        current_tag = "param";
                        # extract param name and description
                        *list<string> pm = (tag_rest =~ x/^(\S+)\s*(.*)/);
                        if (exists pm) {
                            current_arg = pm[0];
                            current_text = pm[1];
                        } else {
                            current_arg = tag_rest;
                            current_text = "";
                        }
                        break;
                    }
                    case "return":
                    case "returns":
                    case "retval": {
                        current_tag = "return";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "throw":
                    case "throws":
                    case "exception": {
                        current_tag = "throw";
                        *list<string> tm = (tag_rest =~ x/^(\S+)\s*(.*)/);
                        if (exists tm) {
                            current_arg = tm[0];
                            current_text = tm[1];
                        } else {
                            current_arg = tag_rest;
                            current_text = "";
                        }
                        break;
                    }
                    case "since": {
                        current_tag = "since";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "see": {
                        current_tag = "see";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "deprecated": {
                        current_tag = "deprecated";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "note": {
                        current_tag = "note";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "warning": {
                        current_tag = "warning";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "brief": {
                        current_tag = "brief";
                        current_arg = NOTHING;
                        current_text = tag_rest;
                        break;
                    }
                    case "par": {
                        # @par Section: — treat as description text with @par prefix
                        if (current_text.length()) {
                            current_text += " ";
                        }
                        current_text += "@par";
                        if (tag_rest.length()) {
                            current_text += " " + tag_rest;
                        }
                        break;
                    }
                    default: {
                        # unknown tag — treat as description text
                        if (current_text.length()) {
                            current_text += " ";
                        }
                        current_text += "@" + tag_name;
                        if (tag_rest.length()) {
                            current_text += " " + tag_rest;
                        }
                        break;
                    }
                }
            } else {
                # continuation line — append to current accumulator
                if (current_text.length() && line.length()) {
                    current_text += " ";
                }
                current_text += line;
            }
        }

        # flush final tag
        DocCommentParser::flushTag(\result, current_tag, current_arg, current_text);

        # trim all string fields
        result.description = trim(result.description);
        if (exists result.brief) {
            result.brief = trim(result.brief);
        }
        if (exists result.return_description) {
            result.return_description = trim(result.return_description);
        }
        if (exists result.since) {
            result.since = trim(result.since);
        }
        if (exists result.deprecated && result.deprecated.typeCode() == NT_STRING) {
            result.deprecated = trim(result.deprecated);
            if (!result.deprecated.length()) {
                result.deprecated = True;
            }
        }

        # convert doxygen inline formatting to Markdown in all text fields
        result.description = DoxygenToMarkdown::convert(result.description);
        if (exists result.brief) {
            result.brief = DoxygenToMarkdown::convert(result.brief);
        }
        if (exists result.return_description) {
            result.return_description = DoxygenToMarkdown::convert(result.return_description);
        }
        foreach string key in (keys result.params) {
            result.params{key} = DoxygenToMarkdown::convert(result.params{key});
        }
        for (int i = 0; i < result.throws.size(); ++i) {
            if (exists result.throws[i].description) {
                result.throws[i].description =
                    DoxygenToMarkdown::convert(result.throws[i].description);
            }
        }
        for (int i = 0; i < result.notes.size(); ++i) {
            result.notes[i] = DoxygenToMarkdown::convert(result.notes[i]);
        }
        for (int i = 0; i < result.warnings.size(); ++i) {
            result.warnings[i] = DoxygenToMarkdown::convert(result.warnings[i]);
        }
        if (exists result.deprecated && result.deprecated.typeCode() == NT_STRING) {
            result.deprecated = DoxygenToMarkdown::convert(result.deprecated);
        }
        for (int i = 0; i < result.see_also.size(); ++i) {
            result.see_also[i] = DoxygenToMarkdown::convert(result.see_also[i]);
        }

        return result;
    }

    #! Strip comment delimiters and normalize to clean lines
    private static list<string> stripDelimiters(string comment) {
        string text = comment;

        # strip block comment delimiters: /** ... */ or /* ... */
        if (text =~ /^\/\*/) {
            text = regex_subst(text, "^\\/\\*\\*?\\s*", "");
            text = regex_subst(text, "\\s*\\*\\/\\s*$", "");
        }

        # split into lines
        list<string> raw_lines = text.split("\n");
        list<string> result = ();
        bool in_block = False;

        foreach string line in (raw_lines) {
            string cleaned = trim(line);

            # handle inline /** block start (e.g., after #! lines)
            if (!in_block && cleaned =~ /^\/\*\*/) {
                cleaned = regex_subst(cleaned, "^\\/\\*\\*\\s?", "");
                in_block = True;
                # check if block also ends on same line
                if (cleaned =~ /\*\/\s*$/) {
                    cleaned = regex_subst(cleaned, "\\s*\\*\\/\\s*$", "");
                    in_block = False;
                }
            } else if (in_block && cleaned =~ /\*\/\s*$/) {
                # end of block comment
                cleaned = regex_subst(cleaned, "\\s*\\*\\/\\s*$", "");
                in_block = False;
            }

            # strip leading * (common in block comments)
            if (cleaned =~ /^\*/) {
                cleaned = regex_subst(cleaned, "^\\*\\s?", "");
            }
            # strip //! or #! prefix
            if (cleaned =~ /^\/\/!/) {
                cleaned = regex_subst(cleaned, "^\\/\\/!\\s?", "");
            } else if (cleaned =~ /^#!/) {
                cleaned = regex_subst(cleaned, "^#!\\s?", "");
            }
            result += cleaned;
        }

        return result;
    }

    #! Flush the current tag accumulator into the result hash
    private static flushTag(reference<hash<auto>> result, *string tag, *string arg,
            string text) {
        string trimmed = trim(text);

        if (!exists tag) {
            # description text
            if (trimmed.length()) {
                if (result.description.length()) {
                    result.description += " ";
                }
                result.description += trimmed;
            }
            return;
        }

        switch (tag) {
            case "param": {
                if (exists arg && arg.length()) {
                    result.params{arg} = trimmed;
                }
                break;
            }
            case "return": {
                result.return_description = trimmed;
                break;
            }
            case "throw": {
                if (exists arg && arg.length()) {
                    result.throws += <ThrowInfo>{
                        "name": arg,
                        "description": trimmed.length() ? trimmed : NOTHING,
                    };
                }
                break;
            }
            case "since": {
                result.since = trimmed;
                break;
            }
            case "see": {
                if (trimmed.length()) {
                    result.see_also += trimmed;
                }
                break;
            }
            case "deprecated": {
                if (trimmed.length()) {
                    result.deprecated = trimmed;
                } else {
                    result.deprecated = True;
                }
                break;
            }
            case "note": {
                if (trimmed.length()) {
                    result.notes += trimmed;
                }
                break;
            }
            case "warning": {
                if (trimmed.length()) {
                    result.warnings += trimmed;
                }
                break;
            }
            case "brief": {
                result.brief = trimmed;
                break;
            }
        }
    }
}

#! @cond nodoc
} # namespace QoreApiMetadata
#! @endcond
