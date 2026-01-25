# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactDirectoryLoader.qc EDIFACT directory loader

/*  EdifactDirectoryLoader.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The EdifactUtil namespace contains all the definitions in the EdifactUtil module
public namespace EdifactUtil {
    #! Loader for EDIFACT directory flat files
    /** This loader expects UN/EDIFACT directory flat files to be present in a directory.
        Until a concrete format is provided, the loader can cache raw file contents
        for later parsing.
    */
    public class EdifactDirectoryLoader {
        private int parseMaxToken(string token) {
            if (token.lwr() == "n") {
                return -1;
            }
            return int(token);
        }

        private string joinPath(string base, string leaf) {
            if (!base.size()) {
                return leaf;
            }
            if (base[base.size() - 1] == "/") {
                return base + leaf;
            }
            return base + "/" + leaf;
        }

        private string basename(string path) {
            int idx = path.rfind("/");
            return idx == -1 ? path : path.substr(idx + 1);
        }

        private string resolvePath(string root, string path) {
            if (path.size() && path[0] == "/") {
                return path;
            }
            return joinPath(root, path);
        }

        private *string resolveExistingPath(string dir, string name) {
            string path = resolvePath(dir, name);
            try {
                hstat(path);
                return path;
            } catch (hash ex) {
                return NOTHING;
            }
        }

        private *string findFileByPrefix(string dir, string prefix) {
            Dir d();
            d.chdir(dir);
            foreach string entry in (d.listFiles()) {
                string name = basename(entry);
                if (name.size() > prefix.size() + 1
                        && name.substr(0, prefix.size()) == prefix
                        && name[prefix.size()] == ".") {
                    return resolvePath(dir, entry);
                }
            }
            return NOTHING;
        }

        private hash<string, string> loadRawFiles(string dir, list<string> files) {
            hash<string, string> out = {};
            foreach string name in (files) {
                *string path = resolveExistingPath(dir, name);
                if (!path) {
                    path = findFileByPrefix(dir, name);
                }
                if (path) {
                    string contents = FileLocationHandler::getTextFileFromLocation(path);
                    out{name} = contents;
                }
            }
            return out;
        }

        private string sanitizeLine(string line) {
            if (line.size() && line[line.size() - 1] == "\r") {
                return line.substr(0, line.size() - 1);
            }
            return line;
        }

        private hash<string, hash<EdifactMessageDef>> parseMessagesFromEdmdDir(string dir, string release,
                bool strict = False) {
            hash<string, hash<EdifactMessageDef>> msgs = new hash<string, hash<EdifactMessageDef>>();
            Dir d();
            d.chdir(dir);
            foreach string entry in (d.listFiles()) {
                string name = basename(entry);
                if (name.rfind("_D.") == -1 && name.rfind("_M.") == -1) {
                    continue;
                }
                string path = resolvePath(dir, entry);
                string contents = FileLocationHandler::getTextFileFromLocation(path);
                *hash<EdifactMessageDef> msg = parseMessageFromEdmd(contents, strict);
                if (msg) {
                    msgs{msg.type} = msg;
                } else if (strict) {
                    throw "EDIFACT-EDMD-ERROR", sprintf("invalid EDMD message file: %y", path);
                }
            }
            return msgs;
        }

        private list<hash<EdifactMessageNode>> applyMessageStatus(
                list<hash<EdifactMessageNode>> nodes, hash<string, hash<auto>> status_map) {
            for (int i = 0; i < nodes.size(); i++) {
                hash<EdifactMessageNode> node = nodes[i];
                if (node.pos && status_map{node.pos}) {
                    node.status = status_map{node.pos}.status;
                    node.min = status_map{node.pos}.min;
                    node.max = status_map{node.pos}.max;
                    if (!node.name && status_map{node.pos}.name) {
                        node.name = status_map{node.pos}.name;
                    }
                }
                if (node.children.size()) {
                    node.children = applyMessageStatus(node.children, status_map);
                }
                nodes[i] = node;
            }
            return nodes;
        }

        private int getNodeChildrenCount(list<hash<EdifactMessageNode>> nodes, list<int> path) {
            hash<EdifactMessageNode> node = nodes[path[0]];
            for (int i = 1; i < path.size(); i++) {
                node = node.children[path[i]];
            }
            return node.children.size();
        }

        private list<hash<EdifactMessageNode>> appendChild(
                list<hash<EdifactMessageNode>> nodes, list<int> path,
                hash<EdifactMessageNode> child, int offset = 0) {
            if (offset >= path.size()) {
                nodes += child;
                return nodes;
            }
            int idx = path[offset];
            hash<EdifactMessageNode> node = nodes[idx];
            node.children = appendChild(node.children, path, child, offset + 1);
            nodes[idx] = node;
            return nodes;
        }

        private list<hash<auto>> parseSegmentIndex(list<string> lines) {
            list<hash<auto>> index = ();
            bool in_index = False;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (line =~ /^\s*4\.2/) {
                    in_index = True;
                    continue;
                }
                if (line =~ /^\s*4\.[3-9]/ || line =~ /^\s*5\./) {
                    in_index = False;
                }
                if (!in_index) {
                    continue;
                }
                *list m = (line =~ x/^\s*([A-Z0-9]{3})\s+(.+?)\s*$/);
                if (m) {
                    index += {"tag": m[0], "name": trim(m[1])};
                }
            }
            return index;
        }

        private list<hash<EdifactMessageNode>> parseStructureFromSegmentTable(list<string> lines, bool strict = False) {
            list<hash<EdifactMessageNode>> root = ();
            list<hash<auto>> stack = ();
            bool in_table = False;
            bool header_found = False;

            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (line.find("Segment table") != -1) {
                    in_table = True;
                    continue;
                }
                if (line =~ /^\s*4\.[4-9]/ || line =~ /^\s*5\./) {
                    in_table = False;
                }
                if (!in_table) {
                    continue;
                }
                if (strict && line.find("\t") != -1) {
                    throw "EDIFACT-EDMD-ERROR", "tab indentation not allowed in segment table";
                }
                if (line.find("Pos") != -1 && line.find("Tag") != -1 && line.find("Name") != -1) {
                    header_found = True;
                    continue;
                }
                *list m = (line =~ x/^\s*(\d{4})(\s+)(.+)$/);
                if (!m) {
                    if (header_found && line =~ x/^\s+\S/) {
                        # ignore wrapped/continued lines in segment table
                        continue;
                    }
                    continue;
                }
                if (strict && !header_found) {
                    throw "EDIFACT-EDMD-ERROR", "missing segment table header";
                }
                int pos = int(m[0]);
                int indent = m[1].size();
                string rest = m[2];

                *hash<EdifactMessageNode> node;
                if (*list gm = (rest =~ x/^Segment group\s+(\d+)(?:\s*:\s*(.+?)|\s+([A-Z0-9].+?))?\s+[MC]\s+\d+\s*$/)) {
                    string gid = "SG" + gm[0];
                    *string gname = gm[1] ?? gm[2];
                    node = <EdifactMessageNode>{
                        "kind": "group",
                        "id": gid,
                        "pos": sprintf("%04d", pos),
                        "name": gname ? trim(gname) : NOTHING,
                        "status": "C",
                        "min": 0,
                        "max": 0,
                        "children": (),
                    };
                } else if (*list sm = (rest =~ x/^([A-Z0-9]{3})\s+(.+?)\s+[MC]\s+\d+\s*$/)) {
                    node = <EdifactMessageNode>{
                        "kind": "segment",
                        "id": sm[0],
                        "pos": sprintf("%04d", pos),
                        "name": trim(sm[1]),
                        "status": "C",
                        "min": 0,
                        "max": 0,
                        "children": (),
                    };
                } else {
                    continue;
                }

                while (stack && stack.last().indent >= indent) {
                    pop stack;
                }
                list<int> current_path;
                if (stack) {
                    list<int> parent_path = stack.last().path;
                    int child_idx = getNodeChildrenCount(root, parent_path);
                    root = appendChild(root, parent_path, node);
                    current_path = parent_path + (child_idx,);
                } else {
                    root += node;
                    current_path = (root.size() - 1,);
                }
                if (node.kind == "group") {
                    stack += {"indent": indent, "path": current_path};
                }
            }

            return root;
        }

        private *hash<EdifactMessageDef> parseMessageFromEdmd(string contents, bool strict = False) {
            list<string> lines = contents.split("\n");
            string msg_type;
            string version;
            string release;
            string name;
            *string function;
            *list<string> notes;

            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (*list m = (line =~ x/^\s*Message\s+Type\s*:\s*([A-Z0-9]+)\s*$/)) {
                    msg_type = m[0];
                } else if (*list m = (line =~ x/^\s*Version\s*:\s*([A-Z])\s*$/)) {
                    version = m[0];
                } else if (*list m = (line =~ x/^\s*Release\s*:\s*([0-9A-Z]+)\s*$/)) {
                    release = m[0];
                } else if (*list m = (line =~ x/^\s*Function:\s*(.+)\s*$/)) {
                    function = m[0];
                } else if (*list m = (line =~ x/^\s*Note:\s*(.+)\s*$/)) {
                    if (!notes) {
                        notes = ();
                    }
                    notes += m[0];
                } else if (!name && line.size() && line.find("message") != -1 && line.find("Message Type") == -1) {
                    name = trim(line);
                }
            }

            if (!msg_type) {
                if (strict) {
                    throw "EDIFACT-EDMD-ERROR", "missing Message Type header";
                }
                return NOTHING;
            }

            list<hash<EdifactMessageNode>> root = ();
            list<hash<auto>> stack = ();

            bool in_clarif = False;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (line =~ /^\s*4\.1/) {
                    in_clarif = True;
                    continue;
                }
                if (line =~ /^\s*4\.[2-9]/ || line =~ /^\s*5\./) {
                    in_clarif = False;
                }
                if (!in_clarif) {
                    continue;
                }
                *list m = (line =~ x/^\s*(\d{4})(\s+)(.+)$/);
                if (!m) {
                    continue;
                }
                int pos = int(m[0]);
                int indent = m[1].size();
                string rest = m[2];

                *hash<EdifactMessageNode> node;
                if (*list gm = (rest =~ x/^Segment group\s+(\d+)(?:\s*:\s*(.+)|\s+([A-Z0-9].+))?\s*$/)) {
                    string gid = "SG" + gm[0];
                    *string gname = gm[1] ?? gm[2];
                    node = <EdifactMessageNode>{
                        "kind": "group",
                        "id": gid,
                        "pos": sprintf("%04d", pos),
                        "name": gname ? trim(gname) : NOTHING,
                        "status": "C",
                        "min": 0,
                        "max": 0,
                        "children": (),
                    };
                } else if (*list sm = (rest =~ x/^([A-Z0-9]{3})\s*,\s*(.+)$/)) {
                    node = <EdifactMessageNode>{
                        "kind": "segment",
                        "id": sm[0],
                        "pos": sprintf("%04d", pos),
                        "name": trim(sm[1]),
                        "status": "C",
                        "min": 0,
                        "max": 0,
                        "children": (),
                    };
                } else {
                    continue;
                }

                while (stack && stack.last().indent >= indent) {
                    pop stack;
                }
                list<int> current_path;
                if (stack) {
                    list<int> parent_path = stack.last().path;
                    int child_idx = getNodeChildrenCount(root, parent_path);
                    root = appendChild(root, parent_path, node);
                    current_path = parent_path + (child_idx,);
                } else {
                    root += node;
                    current_path = (root.size() - 1,);
                }
                if (node.kind == "group") {
                    stack += {"indent": indent, "path": current_path};
                }
            }

            if (!root.size()) {
                root = parseStructureFromSegmentTable(lines, strict);
            }
            if (strict && !root.size()) {
                throw "EDIFACT-EDMD-ERROR", sprintf("no segment structure found for message %y", msg_type);
            }

            hash<string, hash<auto>> status_map = {};
            bool in_table = False;
            bool header_found = False;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (line.find("Segment table") != -1) {
                    in_table = True;
                    continue;
                }
                if (line =~ /^\s*4\.[4-9]/ || line =~ /^\s*5\./) {
                    in_table = False;
                }
                if (!in_table) {
                    continue;
                }
                if (strict && line.find("\t") != -1) {
                    throw "EDIFACT-EDMD-ERROR", "tab indentation not allowed in segment table";
                }
                if (line.find("Pos") != -1 && line.find("Tag") != -1 && line.find("Name") != -1) {
                    header_found = True;
                    continue;
                }
                if (*list gm = (line =~ x/^\s*(\d{4}).*Segment group\s+(\d+)(?:\s*:\s*(.+?)|\s+([A-Z0-9].+?))?\s+([MC])\s+(\d+|n)\s*$/i)) {
                    if (strict && !header_found) {
                        throw "EDIFACT-EDMD-ERROR", "missing segment table header";
                    }
                    string pos = gm[0];
                    *string gname = gm[2] ?? gm[3];
                    if (gname) {
                        gname = trim(gname);
                    }
                    status_map{pos} = {
                        "status": gm[4],
                        "min": gm[4] == "M" ? 1 : 0,
                        "max": parseMaxToken(gm[5]),
                        "name": gname ?? ("Segment group " + gm[1]),
                    };
                    continue;
                }
                if (*list sm = (line =~ x/^\s*(\d{4})\s+([A-Z0-9]{3})\s+(.+?)\s+([MC])\s+(\d+|n)\s*$/i)) {
                    if (strict && !header_found) {
                        throw "EDIFACT-EDMD-ERROR", "missing segment table header";
                    }
                    string pos = sm[0];
                    status_map{pos} = {
                        "status": sm[3],
                        "min": sm[3] == "M" ? 1 : 0,
                        "max": parseMaxToken(sm[4]),
                        "name": trim(sm[2]),
                    };
                }
            }

            if (status_map.size()) {
                root = applyMessageStatus(root, status_map);
            }
            if (strict && root.size()) {
                validateStructure(root);
            }

            return <EdifactMessageDef>{
                "type": msg_type,
                "name": name ?? msg_type,
                "version": version ?? "D",
                "release": release ?? "",
                "structure": root,
                "segment_index": parseSegmentIndex(lines),
                "function": function ?? "",
                "notes": notes ?? (),
            };
        }

        private validateStructure(list<hash<EdifactMessageNode>> nodes) {
            foreach hash<EdifactMessageNode> node in (nodes) {
                if (node.kind == "group") {
                    if (!node.children.size()) {
                        throw "EDIFACT-EDMD-ERROR", sprintf("group %y has no children", node.id);
                    }
                    validateStructure(node.children);
                } else if (node.kind == "segment") {
                    if (!node.name || !node.name.size()) {
                        throw "EDIFACT-EDMD-ERROR", sprintf("segment %y missing name", node.id);
                    }
                }
            }
        }

        private hash<auto> parseCodelistsFromUncl(string contents) {
            hash<auto> out = {};
            list<string> lines = contents.split("\n");
            *string current_element;
            *string current_code;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (!line.size()) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*(\d{4})\s+(.+?)(?:\s+\[[A-Z]\])?\s*$/)) {
                    current_element = m[0];
                    out{current_element} = {
                        "codes": {},
                        "name": trim(m[1]),
                    };
                    current_code = NOTHING;
                    continue;
                }
                if (!current_element) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*Desc:\s*(.+)$/)) {
                    out{current_element}.desc = m[0];
                    continue;
                }
                if (line =~ x/^\s*Repr:\s*/) {
                    continue;
                }
                if (*list m = (line =~ x/^\s{1,5}([A-Za-z0-9]{1,17})\s+(.+?)\s*$/)) {
                    current_code = m[0];
                    out{current_element}.codes{current_code} = m[1];
                    continue;
                }
                if (current_code && line =~ x/^\s{10,}\S/) {
                    out{current_element}.codes{current_code} += " " + trim(line);
                }
            }
            return out;
        }

        private hash<auto> parseCodelistsFromEdcl(string contents) {
            return parseCodelistsFromUncl(contents);
        }

        private *string findEdmdDir(string dir) {
            *string p = resolveExistingPath(dir, "EDMD");
            if (p) {
                try {
                    if (hstat(p).type == "DIRECTORY") {
                        return p;
                    }
                } catch (hash ex) {
                }
            }
            Dir d();
            d.chdir(dir);
            foreach string entry in (d.listFiles()) {
                string name = basename(entry);
                if (name.rfind("_D.") != -1 || name.rfind("_M.") != -1) {
                    return dir;
                }
            }
            return NOTHING;
        }

        private parseElementsFromEDED(EdifactSchema schema, string contents) {
            list<string> lines = contents.split("\n");
            *hash<EdifactElementDef> current;
            string desc;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (!line.size()) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*(\d{4})\s+(.+?)(?:\s+\[[A-Z]\])?\s*$/)) {
                    if (current) {
                        if (desc) {
                            current.desc = desc;
                        }
                        schema.element_defs{current.id} = current;
                    }
                    current = <EdifactElementDef>{
                        "id": m[0],
                        "name": trim(m[1]),
                        "type": "",
                        "min_length": 0,
                        "max_length": 0,
                    };
                    desc = "";
                    continue;
                }
                if (!current) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*Desc:\s*(.+)$/)) {
                    desc = m[0];
                    continue;
                }
                if (*list m = (line =~ x/^\s*Repr:\s*([a-z]+)\.\.([0-9]+)\s*$/i)) {
                    current.type = m[0];
                    current.min_length = 0;
                    current.max_length = int(m[1]);
                    continue;
                }
                if (desc && line =~ x/^\s{5,}\S/) {
                    desc += " " + trim(line);
                }
            }
            if (current) {
                if (desc) {
                    current.desc = desc;
                }
                schema.element_defs{current.id} = current;
            }
        }

        private parseCompositesFromEDCD(EdifactSchema schema, string contents) {
            list<string> lines = contents.split("\n");
            *hash<EdifactCompositeDef> current;
            string desc;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (!line.size()) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*(C\d{3})\s+(.+?)\s*$/)) {
                    if (current) {
                        if (desc) {
                            current.desc = desc;
                        }
                        schema.composite_defs{current.id} = current;
                    }
                    current = <EdifactCompositeDef>{
                        "id": m[0],
                        "name": trim(m[1]),
                        "components": (),
                    };
                    desc = "";
                    continue;
                }
                if (!current) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*Desc:\s*(.+)$/)) {
                    desc = m[0];
                    continue;
                }
                if (*list m = (line =~ x/^\s*(\d{3})\s+(\d{4})\s+(.+?)\s+([MC])\s+([a-z]+)\.\.([0-9]+)\s*$/i)) {
                    current.components += <EdifactComponentDef>{
                        "id": m[1],
                        "status": m[3],
                        "min": m[3] == "M" ? 1 : 0,
                        "max": 1,
                        "desc": trim(m[2]),
                    };
                    continue;
                }
                if (desc && line =~ x/^\s{5,}\S/) {
                    desc += " " + trim(line);
                }
            }
            if (current) {
                if (desc) {
                    current.desc = desc;
                }
                schema.composite_defs{current.id} = current;
            }
        }

        private parseSegmentsFromEDSD(EdifactSchema schema, string contents) {
            list<string> lines = contents.split("\n");
            *hash<EdifactSegmentDef> current;
            string desc;
            foreach string raw in (lines) {
                string line = sanitizeLine(raw);
                if (!line.size()) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*([A-Z]{3})\s+(.+?)\s*$/)) {
                    if (current) {
                        if (desc) {
                            current.desc = desc;
                        }
                        schema.segment_defs{current.tag} = current;
                    }
                    current = <EdifactSegmentDef>{
                        "tag": m[0],
                        "name": trim(m[1]),
                        "segment_elements": (),
                    };
                    desc = "";
                    continue;
                }
                if (!current) {
                    continue;
                }
                if (*list m = (line =~ x/^\s*Function:\s*(.+)$/)) {
                    desc = m[0];
                    continue;
                }
                if (*list m = (line =~ x/^\s*(\d{3})\s+([A-Z0-9]{4})\s+(.+?)\s+([MC])\s+(\d+)(?:\s+([a-z]+)\.\.([0-9]+))?\s*$/i)) {
                    current.segment_elements += <EdifactSegmentElementDef>{
                        "id": m[1],
                        "status": m[3],
                        "min": m[3] == "M" ? 1 : 0,
                        "max": int(m[4]),
                        "composite": m[1][0] == "C",
                        "desc": trim(m[2]),
                    };
                    continue;
                }
                if (desc && line =~ x/^\s{5,}\S/) {
                    desc += " " + trim(line);
                }
            }
            if (current) {
                if (desc) {
                    current.desc = desc;
                }
                schema.segment_defs{current.tag} = current;
            }
        }

        #! Loads a release from a directory of flat files
        /** @param dir directory path containing EDIFACT directory files
            @param release release identifier (ex: "D01B")
            @param opts options:
                - files: list of file names to read
                - raw_storage: "contents" (default) or "paths"
                - parse: parse elements/composites/segments (default: True)
        */
        EdifactSchema loadFromFlatDir(string dir, string release, *hash<auto> opts) {
            list<string> files = opts.files ?? ("EDMD", "EDSD", "EDCD", "EDED", "EDCL");
            string raw_storage = opts.raw_storage ?? "paths";
            bool do_parse = opts.parse ?? True;
            EdifactSchema schema(release);
            schema.meta.source = "flat-dir";
            schema.meta.release = release;
            schema.meta.source_dir = dir;
            schema.meta.raw_storage = raw_storage;
            schema.meta.parsed = False;

            if (raw_storage == "contents") {
                schema.raw.files = loadRawFiles(dir, files);
            } else if (raw_storage == "paths") {
                hash<string, string> paths = {};
                foreach string name in (files) {
                    *string path = resolveExistingPath(dir, name);
                    if (!path) {
                        path = findFileByPrefix(dir, name);
                    }
                    if (path) {
                        paths{name} = path;
                    }
                }
                schema.raw.files = paths;
            } else {
                throw "EDIFACT-DIR-ERROR", sprintf("invalid raw_storage option: %y", raw_storage);
            }

            if (do_parse) {
                *string eded_path = findFileByPrefix(dir, "EDED");
                *string edcd_path = findFileByPrefix(dir, "EDCD");
                *string edsd_path = findFileByPrefix(dir, "EDSD");
                *string edmd_dir = findEdmdDir(dir);
                *string edcl_path = findFileByPrefix(dir, "EDCL");
                *string uncl_path = findFileByPrefix(dir, "UNCL");
                if (!uncl_path) {
                    *string uncl_dir = resolveExistingPath(dir, "UNCL");
                    if (uncl_dir) {
                        uncl_path = findFileByPrefix(uncl_dir, "UNCL");
                    }
                }
                if (eded_path) {
                    parseElementsFromEDED(schema, FileLocationHandler::getTextFileFromLocation(eded_path));
                }
                if (edcd_path) {
                    parseCompositesFromEDCD(schema, FileLocationHandler::getTextFileFromLocation(edcd_path));
                }
                if (edsd_path) {
                    parseSegmentsFromEDSD(schema, FileLocationHandler::getTextFileFromLocation(edsd_path));
                }
                if (edmd_dir) {
                    bool strict_edmd = opts.strict ?? False;
                    schema.message_defs = parseMessagesFromEdmdDir(edmd_dir, release, strict_edmd);
                }
                if (edcl_path) {
                    schema.codelists.edcl = parseCodelistsFromEdcl(
                        FileLocationHandler::getTextFileFromLocation(edcl_path)
                    );
                }
                if (uncl_path) {
                    schema.codelists.uncl = parseCodelistsFromUncl(
                        FileLocationHandler::getTextFileFromLocation(uncl_path)
                    );
                }
                schema.meta.parsed = True;
                hash<string, string> parsed_files = {};
                if (eded_path) {
                    parsed_files{"EDED"} = eded_path;
                }
                if (edcd_path) {
                    parsed_files{"EDCD"} = edcd_path;
                }
                if (edsd_path) {
                    parsed_files{"EDSD"} = edsd_path;
                }
                if (edmd_dir) {
                    parsed_files{"EDMD"} = edmd_dir;
                }
                if (edcl_path) {
                    parsed_files{"EDCL"} = edcl_path;
                }
                if (uncl_path) {
                    parsed_files{"UNCL"} = uncl_path;
                }
                schema.meta.parsed_files = parsed_files;
            }

            return schema;
        }
    }
}
