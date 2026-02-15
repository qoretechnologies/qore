# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file EdifactDirectoryCache.qc EDIFACT directory cache utilities

/*  EdifactDirectoryCache.qc Copyright 2026 Qore Technologies, s.r.o.

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
    public const EdifactCacheEnvVar = "QORE_EDIFACT_DIRCACHE";
    public const EdifactCacheDefaultRoot = "~/.qore/edifact";
    public const EdifactCacheSchemaFile = "schema.qv";

    #! Cache helper for EDIFACT directory schemas
    public class EdifactDirectoryCache {
        private string joinPath(string base, string leaf) {
            if (!base.size()) {
                return leaf;
            }
            if (base[base.size() - 1] == "/") {
                return base + leaf;
            }
            return base + "/" + leaf;
        }

        private string expandUserPath(string path) {
            if (!path.size() || path[0] != "~") {
                return path;
            }
            string home = getenv("HOME") ?? "";
            if (!home.size()) {
                throw "EDIFACT-CACHE-ERROR", "HOME is not set; cannot expand '~' in cache path";
            }
            if (path == "~") {
                return home;
            }
            if (path.size() > 1 && path[1] == "/") {
                return home + path.substr(1);
            }
            return path;
        }

        string getCacheRoot() {
            string root = getenv(EdifactCacheEnvVar) ?? EdifactCacheDefaultRoot;
            return expandUserPath(root);
        }

        string getReleaseRoot(string release) {
            return joinPath(joinPath(getCacheRoot(), "UN/EDIFACT"), release);
        }

        string getSchemaPath(string release) {
            return joinPath(getReleaseRoot(release), EdifactCacheSchemaFile);
        }

        private ensureDir(string path) {
            mkdir(path, 0700, True);
        }

        private string serializeCacheString(string val) {
            string out;
            int len = val.size();
            for (int i = 0; i < len; ++i) {
                string ch = val[i];
                int code = ord(ch);
                if (ch == "\\") {
                    out += "\\\\";
                } else if (ch == "\"") {
                    out += "\\\"";
                } else if (code == 0x0a) {
                    out += "\\n";
                } else if (code == 0x0d) {
                    out += "\\r";
                } else if (code == 0x09) {
                    out += "\\t";
                } else if (code < 0x20 || code == 0x7f) {
                    out += sprintf("\\x%02x", code);
                } else {
                    out += ch;
                }
            }
            return "\"" + out + "\"";
        }

        private string serializeCacheValue(auto val) {
            if (val === NOTHING) {
                throw "SERIALIZATION-ERROR", "cannot serialize NOTHING value";
            }
            switch (val.typeCode()) {
                case NT_STRING:
                    return serializeCacheString(val);
                case NT_INT:
                    return sprintf("%d", val);
                case NT_FLOAT:
                    return sprintf("%.25g", val);
                case NT_BOOLEAN:
                    return val ? "true" : "false";
                case NT_DATE:
                    return serializeCacheString(val.format("IF"));
                case NT_LIST:
                    if (!val.size()) {
                        return "()";
                    }
                    string out = "(";
                    for (int i = 0; i < val.size(); ++i) {
                        if (i) {
                            out += ",";
                        }
                        out += serializeCacheValue(val[i]);
                    }
                    return out + ")";
                case NT_HASH:
                    if (!val.size()) {
                        return "{}";
                    }
                    string out = "{";
                    list<string> key_list = sort(keys val);
                    for (int i = 0; i < key_list.size(); ++i) {
                        string key = key_list[i];
                        if (i) {
                            out += ",";
                        }
                        out += serializeCacheString(key) + "=" + serializeCacheValue(val{key});
                    }
                    return out + "}";
            }
            throw "SERIALIZATION-ERROR", sprintf("cannot serialize values of type %y", val.type());
        }

        #! Loads a cached schema for a release if available
        *EdifactSchema loadSchema(string release, *hash opts = NOTHING) {
            string schema_path = getSchemaPath(release);
            try {
                hstat(schema_path);
            } catch (hash ex) {
                return NOTHING;
            }
            string cache_error = opts ? (opts.cache_error ?? "warn") : "warn";
            auto cache_warnings = opts ? opts.cache_warnings : NOTHING;
            if (cache_error != "ignore" && cache_error != "warn" && cache_error != "error") {
                throw "EDIFACT-CACHE-ERROR", sprintf("invalid cache_error value: %y", cache_error);
            }
            if (cache_warnings != NOTHING && cache_warnings.typeCode() != NT_LIST) {
                throw "EDIFACT-CACHE-ERROR", "cache warnings target must be a list";
            }
            try {
                string contents = FileLocationHandler::getTextFileFromLocation(schema_path);
                hash data = cast<hash>(Util::parse_to_qore_value(contents));
                return new EdifactSchema(data);
            } catch (hash ex) {
                if (ex.err == "LOCATION-ERROR" || ex.err == "FILE-ERROR"
                    || ex.err == "PARSE-ERROR" || ex.err == "PARSE-EXCEPTION"
                    || ex.err == "RUNTIME-TYPE-ERROR"
                    || ex.err == "SERIALIZATION-ERROR" || ex.err == "RUNTIME-CAST-ERROR") {
                    if (cache_error == "error") {
                        throw "EDIFACT-CACHE-ERROR",
                            sprintf("cache load failed for release %y: %y: %y", release, ex.err, ex.desc);
                    }
                    if (cache_error == "warn") {
                        string msg = sprintf("cache load failed for release %y: %y: %y",
                            release, ex.err, ex.desc);
                        if (cache_warnings != NOTHING) {
                            push cache_warnings, msg;
                        } else {
                            stderr.printf("%s\n", msg);
                        }
                    }
                    return NOTHING;
                }
                throw ex;
            }
        }

        #! Writes a schema to the cache
        writeSchema(EdifactSchema schema) {
            string release_root = getReleaseRoot(schema.release);
            ensureDir(release_root);
            string schema_path = getSchemaPath(schema.release);
            string contents = serializeCacheValue(schema.toHash());
            string tmp_path = sprintf("%s.%d.tmp", schema_path, getpid());
            FileLocationHandler::writeFileToLocation(tmp_path, contents);
            rename(tmp_path, schema_path);
        }
    }
}
