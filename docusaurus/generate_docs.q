#!/usr/bin/env qore

%new-style
%strict-args
%require-types
%enable-all-warnings

%requires astparser

public namespace QoreDocGenerator {

    #! Converts Qore AST to Docusaurus MDX
    public class MarkdownGenerator {
        private {
            string outputDir;
        }

        public constructor(string outDir) {
            outputDir = outDir;
        }

        public processFile(string filePath, string relPath) {
            printf("Processing %s...\n", filePath);
            try {
                astparser::AstParser parser();
                *astparser::AstTree tree = parser.parseFile(filePath);
                if (!tree) return;
                list<auto> nodes = tree.getNodesInfo();
                
                foreach hash<auto> node in (nodes) {
                    processNode(node, outputDir + "/" + relPath);
                }
            } catch (hash<ExceptionInfo> ex) {
                printf("ERROR processing %s: %s\n", filePath, ex.desc);
            }
        }

        private processNode(hash<auto> node, string currentDir) {
            if (node.nodetype != astparser::ANT_Declaration) return;

            switch (node.kind) {
                case astparser::ADK_Namespace:
                    processNamespace(node, currentDir);
                    break;
                case astparser::ADK_Class:
                    processClass(node, currentDir);
                    break;
                case astparser::ADK_Hash:
                    processHashDecl(node, currentDir);
                    break;
            }
        }

        private processNamespace(hash<auto> ns, string currentDir) {
            if (!ns.name.name) return;
            string name = string(ns.name.name);
            
            string nsDir = currentDir + "/" + name;
            mkdir(nsDir, 0755);

            string indexFile = nsDir + "/index.md";
            
            hash<auto> doc = parseDocComment(ns.docComment);
            
            string content = generateFrontmatter(name, name, "index");
            content += sprintf("# Namespace %s\n\n", name);
            if (doc.description) content += doc.description + "\n\n";
            
            writeFile(indexFile, content);

            if (ns.declarations) {
                foreach hash<auto> decl in (ns.declarations) {
                    processNode(decl, nsDir);
                }
            }
        }

        private processClass(hash<auto> cls, string currentDir) {
            if (!cls.name.name) return;
            string name = string(cls.name.name);
            string classFile = currentDir + "/" + name + ".md";
            
            hash<auto> doc = parseDocComment(cls.docComment);
            
            string content = generateFrontmatter(name, name, name);
            content += sprintf("# Class %s\n\n", name);
            
            if (cls.modifiers) {
                content += "**Modifiers:** `" + cls.modifiers + "`\n\n";
            }

            if (cls.inherits) {
                content += "**Inherits:** ";
                list<string> parents = ();
                foreach hash<auto> parent in (cls.inherits) {
                    parents += sprintf("[%s](%s)", parent.name.name ?? "Unknown", parent.name.name ?? "Unknown");
                }
                content += join(", ", parents) + "\n\n";
            }

            if (doc.description) content += doc.description + "\n\n";

            content += "## Method Summary\n\n";
            content += "| Modifiers | Name | Description |\n";
            content += "| --- | --- | --- |\n";
            
            if (cls.declarations) {
                foreach hash<auto> decl in (cls.declarations) {
                    if (decl.kind == astparser::ADK_Function) {
                        if (!decl.name.name) continue;
                        hash<auto> mDoc = parseDocComment(decl.docComment);
                        string summary = mDoc.summary ?? "No description";
                        content += sprintf("| %s | [`%s()`](#%s) | %s |\n", 
                            decl.modifiers ?? "", 
                            decl.name.name, 
                            decl.name.name, 
                            summary
                        );
                    }
                }
            }
            content += "\n";

            content += "## Method Details\n\n";
            if (cls.declarations) {
                foreach hash<auto> decl in (cls.declarations) {
                    content += processClassMember(decl);
                }
            }

            writeFile(classFile, content);
        }

        private string processClassMember(hash<auto> decl) {
            string out = "";
            switch (decl.kind) {
                case astparser::ADK_Function:
                    out += renderMethod(decl);
                    break;
                case astparser::ADK_Variable:
                    out += renderProperty(decl);
                    break;
                case astparser::ADK_Constant:
                    out += renderConstant(decl);
                    break;
            }
            return out;
        }

        private string renderMethod(hash<auto> method) {
            if (!method.name.name) return "";
            string name = string(method.name.name);
            
            hash<auto> doc = parseDocComment(method.docComment);
            
            string out = sprintf("### %s\n\n", name);
            
            string sig = "";
            if (method.modifiers) sig += method.modifiers + " ";
            if (method.returnType) sig += formatType(method.returnType) + " ";
            sig += name + "(";
            
            list<string> paramStrs = ();
            list<hash<auto>> params = collectParams(method.params);
            
            foreach hash<auto> p in (params) {
                string pStr = "";
                if (p.typeName) pStr += formatType(p.typeName) + " ";
                pStr += (p.name.name ?? "arg");
                paramStrs += pStr;
            }
            
            sig += join(", ", paramStrs);
            sig += ")";
            
            out += "```qore\n" + sig + "\n```\n\n";
            
            if (doc.deprecated) {
                out += ":::warning Deprecated\n" + doc.deprecated + "\n:::\n\n";
            }

            if (doc.description) out += doc.description + "\n\n";

            if (doc.params) {
                out += "**Parameters:**\n\n";
                out += "| Name | Description |\n";
                out += "| --- | --- |\n";
                foreach hash<auto> p in (doc.params) {
                    out += sprintf("| `%s` | %s |\n", p.name, p.desc);
                }
                out += "\n";
            }

            if (doc.returns) {
                out += "**Returns:**\n\n" + doc.returns + "\n\n";
            }

            if (doc.throws) {
                out += "**Throws:**\n\n";
                out += "| Exception | Description |\n";
                out += "| --- | --- |\n";
                foreach hash<auto> t in (doc.throws) {
                    out += sprintf("| `%s` | %s |\n", t.name, t.desc);
                }
                out += "\n";
            }
            
            if (doc.examples) {
                foreach string ex in (doc.examples) {
                    out += "**Example:**\n" + ex + "\n\n";
                }
            }
            
            if (doc.since) {
                out += "*Since: " + doc.since + "*\n\n";
            }

            out += "---\n";
            return out;
        }

        private string renderProperty(hash<auto> prop) {
            if (!prop.name.name) return "";
            string name = string(prop.name.name);
            hash<auto> doc = parseDocComment(prop.docComment);
            
            string out = sprintf("### %s\n\n", name);
            out += "`" + (prop.typeName.name ?? "auto") + "`\n\n";
            if (doc.description) out += doc.description + "\n\n";
            out += "---\n";
            return out;
        }
        
        private string renderConstant(hash<auto> cons) {
             if (!cons.name.name) return "";
             string name = string(cons.name.name);
             hash<auto> doc = parseDocComment(cons.docComment);
             
             string out = sprintf("### %s\n\n", name);
             if (doc.description) out += doc.description + "\n\n";
             out += "---\n";
             return out;
        }

        private processHashDecl(hash<auto> hd, string currentDir) {
            if (!hd.name.name) return;
            string name = string(hd.name.name);
            string file = currentDir + "/" + name + ".md";
            hash<auto> doc = parseDocComment(hd.docComment);
            
            string content = generateFrontmatter(name, name, name);
            content += sprintf("# HashDecl %s\n\n", name);
            if (doc.description) content += doc.description + "\n\n";

            content += "## Members\n\n";
            content += "| Member | Type | Description |\n";
            content += "| --- | --- | --- |\n";

            if (hd.declarations) {
                foreach hash<auto> decl in (hd.declarations) {
                    if (decl.kind == astparser::ADK_HashMember) {
                        hash<auto> mDoc = parseDocComment(decl.docComment);
                        content += sprintf("| `%s` | `%s` | %s |\n", 
                            decl.name.name,
                            decl.typeName.name,
                            mDoc.summary
                        );
                    }
                }
            }

            writeFile(file, content);
        }

        private hash<auto> parseDocComment(*string comment) {
            hash<auto> info = {
                "description": "",
                "summary": "",
                "params": (),
                "throws": (),
                "returns": "",
                "examples": (),
                "since": "",
                "deprecated": "",
            };

            if (!comment) return info;

            string text = "";
            foreach string line in (split("\n", comment)) {
                string l = line;
                trim l;
                
                # Check for markers
                if (l =~ /^#!\s?(.*)$/) {
                    l = ($1 ?? "");
                } else if (l =~ /^\/\*\*?\s?(.*)$/) {
                    l = ($1 ?? "");
                } else if (l =~ /^\*\s?(.*)$/) {
                    l = ($1 ?? "");
                }
                
                # Check for trailing */
                if (l =~ /^(.*)\*\/$/) {
                    l = ($1 ?? "");
                }
                
                text += l + "\n";
            }

            list<string> lines = split("\n", text);
            string buffer = "";
            string currentTag = "desc";
            
            code commit = sub () {
                if (buffer.empty()) return;
                trim buffer;
                
                buffer = formatMarkdown(buffer);

                switch (currentTag) {
                    case "desc":
                        info.description += buffer + "\n";
                        if (info.summary.empty()) {
                            info.summary = (split("\n", buffer))[0];
                        }
                        break;
                    case "return":
                        info.returns = buffer;
                        break;
                    case "since":
                        info.since = buffer;
                        break;
                    case "deprecated":
                        info.deprecated = buffer;
                        break;
                }
                buffer = "";
            };

            foreach string line in (lines) {
                if (line =~ /^\s*@(\w+)(.*)$/) {
                    string tag = ($1 ?? "");
                    string content = ($2 ?? "");
                    commit();
                    
                    trim content;
                    
                    if (tag == "param") {
                        if (content =~ /^(\w+)\s+(.*)$/) {
                            info.params += {"name": ($1 ?? ""), "desc": formatMarkdown(($2 ?? ""))};
                        }
                        currentTag = "param_continuation"; 
                    } else if (tag == "throw" || tag == "throws") {
                        if (content =~ /^([\w\-]+)\s+(.*)$/) {
                            info.throws += {"name": ($1 ?? ""), "desc": formatMarkdown(($2 ?? ""))};
                        }
                        currentTag = "throw_continuation";
                    } else if (tag == "return" || tag == "returns") {
                        currentTag = "return";
                        buffer = content + "\n";
                    } else if (tag == "par" && content =~ /^Example/) {
                        currentTag = "example";
                        buffer = "";
                    } else if (tag == "code") {
                        buffer += "```qore\n";
                        currentTag = "code";
                    } else if (tag == "endcode") {
                        buffer += "```\n";
                        if (currentTag == "code") {
                             info.examples += buffer;
                             buffer = "";
                             currentTag = "desc";
                        }
                    } else if (tag == "since") {
                        currentTag = "since";
                        buffer = content;
                    } else if (tag == "deprecated") {
                        currentTag = "deprecated";
                        buffer = content;
                    } else if (tag == "see" || tag == "note") {
                        currentTag = "desc";
                        buffer = "**" + (tag == "note" ? "Note" : "See also") + ":** " + content + "\n";
                    } else {
                        buffer += line + "\n";
                    }
                } else {
                    if (currentTag == "param_continuation") {
                        if (info.params) {
                            info.params[info.params.size()-1].desc += " " + formatMarkdown(line);
                        }
                    } else if (currentTag == "throw_continuation") {
                        if (info.throws) {
                            info.throws[info.throws.size()-1].desc += " " + formatMarkdown(line);
                        }
                    } else if (currentTag == "example") {
                        if (line =~ /@code/) {
                            buffer += "```qore\n";
                            currentTag = "code";
                        } else if (line =~ /@endcode/) {
                            buffer += "```\n";
                            info.examples += buffer;
                            buffer = "";
                            currentTag = "desc";
                        } else {
                            buffer += line + "\n";
                        }
                    } else if (currentTag == "code") {
                         if (line =~ /@endcode/) {
                            buffer += "```\n";
                            info.examples += buffer;
                            buffer = "";
                            currentTag = "desc";
                         } else {
                            buffer += line + "\n";
                         }
                    } else {
                        buffer += line + "\n";
                    }
                }
            }
            commit();
            
            return info;
        }

        private string formatMarkdown(string text) {
            string out = text;
            out =~ s/\\c\s+([^\s\.,;:\)]+)/`$1`/g;
            out =~ s/\\a\s+([^\s\.,;:\)]+)/_$1_/g;
            out =~ s/@ref\s+(\S+)\s*"([^"]+)"/[$2](#$1)/g; 
            out =~ s/@ref\s+(\S+)/`$1`/g; 
            out =~ s/^\s*-\s/* /mg; 
            return out;
        }

        private string generateFrontmatter(string id, string title, string label) {
            return sprintf("---\nid: %s\ntitle: %s\nsidebar_label: %s\n---\n\n", id, title, label);
        }

        private string formatType(hash<auto> typeNode) {
            if (typeNode.nodetype == astparser::ANT_Name) {
                return typeNode.name ?? "auto";
            }
            return "auto";
        }
        
        private mkdirs(string path) {
            if (is_dir(path)) {
                return;
            }
            string parent = dirname(path);
            if (parent != "." && parent != "/" && !is_dir(parent)) {
                mkdirs(parent);
            }
            mkdir(path, 0755);
        }

        private writeFile(string path, string content) {
            string dir = dirname(path);
            string filename = basename(path);
            
            if (filename =~ /::/) {
                list<string> parts = split("::", filename);
                filename = parts[parts.size() - 1];
                splice parts, parts.size() - 1, 1;
                foreach string part in (parts) {
                    dir += "/" + part;
                }
                path = dir + "/" + filename;
            }

            try {
                mkdirs(dir);
                File f();
                f.open2(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                f.write(content);
                f.close();
            } catch (hash<ExceptionInfo> ex) {
                printf("Failed to write %s: %s\n", path, ex.desc);
            }
        }
        
        private list<hash<auto>> collectParams(auto paramsNode) {
            list<hash<auto>> out = ();
            if (!paramsNode) return out;
            
            if (paramsNode.typeCode() != NT_HASH) return out;
            
            hash<auto> p = paramsNode;

            if (p.kind == 19 && p.elements) { 
                foreach hash<auto> el in (p.elements) {
                    out += collectParams(el);
                }
                return out;
            }
            
            if (p.kind == 2 && p.left) { 
                out += collectParams(p.left);
                return out;
            }
            
            if (p.kind == 12 && p.declaration) {
                out += collectParams(p.declaration);
                return out;
            }

            if (p.kind == 10 && p.name) {
                out += p;
                return out;
            }
            
            return out;
        }
    }
}

sub usage() {
    printf("usage: %s --src <dir> --out <dir> --prefix <name>\n", get_script_name());
    exit(1);
}

string src_dir;
string out_dir;
string prefix;

for (int i = 0; i < ARGV.size(); ++i) {
    if (ARGV[i] == "--src" && ARGV.size() > i + 1) {
        src_dir = ARGV[++i];
    } else if (ARGV[i] == "--out" && ARGV.size() > i + 1) {
        out_dir = ARGV[++i];
    } else if (ARGV[i] == "--prefix" && ARGV.size() > i + 1) {
        prefix = ARGV[++i];
    }
}

if (!src_dir || !out_dir || !prefix) {
    usage();
}

mkdir(out_dir, 0755);

QoreDocGenerator::MarkdownGenerator gen(out_dir);

printf("Scanning %s (Output prefix: %s)...\n", src_dir, prefix);

try {
    string cmd = sprintf("find '%s' -type f -name '*.*' \\( -name '*.q' -o -name '*.qm' -o -name '*.qc' \\)", src_dir);
    string output = backquote(cmd);
    list<string> files = split("\n", output);
    
    foreach string fullPath in (files) {
        trim fullPath;
        if (fullPath == "") continue;
        
        if (src_dir !~ /\/$/) {
            src_dir += "/";
        }
        
        int rootLen = src_dir.length();
        if (fullPath.substr(0, rootLen) == src_dir) {
            string relPath = fullPath.substr(rootLen);
            string fileDir = dirname(relPath);
            
            string targetDir = prefix;
            if (fileDir != ".") {
                targetDir += "/" + fileDir;
            }
            
            gen.processFile(fullPath, targetDir);
        }
    }
} catch (hash<ExceptionInfo> ex) {
    printf("Error scanning %s: %s\n", src_dir, ex.desc);
}

printf("Done.\n");
