# -*- mode: qore; indent-tabs-mode: nil -*-

/*  QoreRagFactories.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreRagUtils {

#! Factory for @ref LocalOnnxEmbedder
public class LocalOnnxEmbedderFactory inherits AbstractEmbedderFactory {
    string getType() {
        return "local-onnx";
    }

    string getDisplayName() {
        return "Local ONNX embedder";
    }

    AbstractEmbedder create(hash<EmbedderConfig> config) {
        return new LocalOnnxEmbedder(config);
    }
}

#! Factory for @ref LocalOnnxReranker
public class LocalOnnxRerankerFactory inherits AbstractRerankerFactory {
    string getType() {
        return "local-onnx";
    }

    string getDisplayName() {
        return "Local ONNX cross-encoder reranker";
    }

    AbstractReranker create(hash<RerankerConfig> config) {
        return new LocalOnnxReranker(config);
    }
}

#! Factory for @ref HttpGatewayReranker
public class HttpGatewayRerankerFactory inherits AbstractRerankerFactory {
    string getType() {
        return "http-gateway";
    }

    string getDisplayName() {
        return "HTTP gateway reranker";
    }

    AbstractReranker create(hash<RerankerConfig> config) {
        return new HttpGatewayReranker(config);
    }
}

#! Factory for @ref SGLangReranker
public class SGLangRerankerFactory inherits AbstractRerankerFactory {
    string getType() {
        return "sglang";
    }

    string getDisplayName() {
        return "SGLang cross-encoder reranker";
    }

    AbstractReranker create(hash<RerankerConfig> config) {
        return new SGLangReranker(config);
    }
}

#! Factory for @ref StructuredChunker
public class StructuredChunkerFactory inherits AbstractChunkerFactory {
    string getStrategy() {
        return "structured";
    }

    string getDisplayName() {
        return "Paragraph-aware structured chunker";
    }

    AbstractChunker create(hash<ChunkerConfig> config) {
        return new StructuredChunker(config);
    }
}

#! Factory for @ref FixedChunker
public class FixedChunkerFactory inherits AbstractChunkerFactory {
    string getStrategy() {
        return "fixed";
    }

    string getDisplayName() {
        return "Fixed-size chunker with overlap";
    }

    AbstractChunker create(hash<ChunkerConfig> config) {
        return new FixedChunker(config);
    }
}

} # namespace QoreRagUtils
