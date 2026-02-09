# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CommentAttacher.qc Comment-to-node association logic

/*  CommentAttacher.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

#! Main QoreCodeFormat namespace
public namespace QoreCodeFormat {

#! Comment kind constants matching ASTCommentKind enum values
public const ACK_Line = 0;
public const ACK_Block = 1;
public const ACK_DocLine = 2;
public const ACK_DocBlock = 3;

#! Comment association with a node
public hashdecl CommentInfo {
    #! Comment kind (ACK_Line, ACK_Block, ACK_DocLine, ACK_DocBlock)
    int kind;
    #! Full comment text including delimiters
    string text;
    #! Location of the comment
    hash<auto> loc;
    #! Association type: "leading", "trailing", or "orphan"
    string association;
}

#! Associates comments with AST nodes based on source location
public class CommentAttacher {
    private {
        #! Comments list from AST tree
        list<hash<auto>> comments;

        #! Source lines for blank-line heuristics
        list<string> sourceLines;

        #! Map from node location key to leading comments
        hash<string, list<hash<CommentInfo>>> leadingComments;

        #! Map from node location key to trailing comments
        hash<string, list<hash<CommentInfo>>> trailingComments;

        #! Orphan comments (not attached to any node)
        list<hash<CommentInfo>> orphanComments;
    }

    #! Create a CommentAttacher
    /** @param comments list of comment hashes from AstTree::getComments()
        @param nodes list of node hashes from AstTree::getNodesInfo()
        @param sourceText original source text
    */
    constructor(list<hash<auto>> comments, list<hash<auto>> nodes, string sourceText) {
        self.comments = comments;
        self.sourceLines = sourceText.split("\n");
        attach(nodes);
    }

    #! Get leading comments for a node at the given location
    /** @param loc location hash with firstLine, firstCol
        @return list of leading comments, or empty list
    */
    list<hash<CommentInfo>> getLeadingComments(hash<auto> loc) {
        string key = makeKey(loc);
        return leadingComments{key} ?? ();
    }

    #! Get trailing comments for a node at the given location
    /** @param loc location hash with lastLine, lastCol
        @return list of trailing comments, or empty list
    */
    list<hash<CommentInfo>> getTrailingComments(hash<auto> loc) {
        string key = makeTrailingKey(loc);
        return trailingComments{key} ?? ();
    }

    #! Get all orphan comments (not attached to any node)
    list<hash<CommentInfo>> getOrphanComments() {
        return orphanComments;
    }

    #! Get all comments sorted by source location
    list<hash<CommentInfo>> getAllComments() {
        list<hash<CommentInfo>> result = ();
        foreach hash<auto> c in (comments) {
            result += <CommentInfo>{
                "kind": c.kind,
                "text": c.text,
                "loc": c.loc,
                "association": "all",
            };
        }
        return result;
    }

    private attach(*list<hash<auto>> nodes) {
        # Build a sorted list of node start locations
        list<hash<auto>> nodeLocs = ();
        collectNodeLocations(nodes, \nodeLocs);

        # Sort by line then column (use start_line/start_column with fallback to firstLine/firstCol)
        nodeLocs = sort(nodeLocs, int sub (hash<auto> a, hash<auto> b) {
            int aLine = exists a.firstLine ? a.firstLine : (a.start_line + 1);
            int bLine = exists b.firstLine ? b.firstLine : (b.start_line + 1);
            if (aLine != bLine) {
                return aLine <=> bLine;
            }
            int aCol = exists a.firstCol ? a.firstCol : (a.start_column + 1);
            int bCol = exists b.firstCol ? b.firstCol : (b.start_column + 1);
            return aCol <=> bCol;
        });

        foreach hash<auto> comment in (comments) {
            hash<CommentInfo> ci = <CommentInfo>{
                "kind": comment.kind,
                "text": comment.text,
                "loc": comment.loc,
                "association": "orphan",
            };

            int commentLine = comment.loc.firstLine;
            int commentEndLine = comment.loc.lastLine;

            # Find if this comment is on the same line as a node's last line (trailing)
            bool attached = False;
            foreach hash<auto> nloc in (nodeLocs) {
                # Get the end line/col, supporting both location formats
                int endLine = exists nloc.lastLine ? nloc.lastLine : (nloc.end_line + 1);
                int endCol = exists nloc.lastCol ? nloc.lastCol : (nloc.end_column + 1);
                # Trailing: comment on same line after node ends
                if (commentLine == endLine && comment.loc.firstCol > endCol) {
                    ci.association = "trailing";
                    string key = makeTrailingKey(nloc);
                    if (!trailingComments.hasKey(key)) {
                        trailingComments{key} = ();
                    }
                    trailingComments{key} += ci;
                    attached = True;
                    break;
                }
            }

            if (!attached) {
                # Leading: find the next node after this comment
                *hash<auto> nextNode;
                foreach hash<auto> nloc in (nodeLocs) {
                    int startLine = exists nloc.firstLine ? nloc.firstLine : (nloc.start_line + 1);
                    if (startLine > commentEndLine) {
                        nextNode = nloc;
                        break;
                    }
                }

                if (exists nextNode) {
                    ci.association = "leading";
                    string key = makeKey(nextNode);
                    if (!leadingComments.hasKey(key)) {
                        leadingComments{key} = ();
                    }
                    leadingComments{key} += ci;
                } else {
                    ci.association = "orphan";
                    orphanComments += ci;
                }
            }
        }
    }

    private collectNodeLocations(auto nodes, reference<list<hash<auto>>> locs) {
        if (!exists nodes || nodes.typeCode() != NT_LIST) {
            return;
        }
        foreach auto node in (nodes) {
            if (!exists node || node.typeCode() != NT_HASH) {
                continue;
            }
            if (exists node.loc) {
                locs += node.loc;
            }
            # Recurse into child nodes
            if (exists node.declarations) {
                collectNodeLocations(node.declarations, \locs);
            }
            if (exists node.body) {
                if (node.body.typeCode() == NT_LIST) {
                    collectNodeLocations(node.body, \locs);
                } else if (node.body.typeCode() == NT_HASH && exists node.body.loc) {
                    locs += node.body.loc;
                }
            }
            if (exists node.statements) {
                collectNodeLocations(node.statements, \locs);
            }
        }
    }

    private string makeKey(hash<auto> loc) {
        int line = exists loc.firstLine ? loc.firstLine : (loc.start_line + 1);
        int col = exists loc.firstCol ? loc.firstCol : (loc.start_column + 1);
        return sprintf("%d:%d", line, col);
    }

    private string makeTrailingKey(hash<auto> loc) {
        int line = exists loc.lastLine ? loc.lastLine : (loc.end_line + 1);
        int col = exists loc.lastCol ? loc.lastCol : (loc.end_column + 1);
        return sprintf("%d:%d:t", line, col);
    }
}

} # namespace QoreCodeFormat
