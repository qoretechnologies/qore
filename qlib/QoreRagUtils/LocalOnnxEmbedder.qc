# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRagUtils module — LocalOnnxEmbedder definition

/** LocalOnnxEmbedder.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Local ONNX embedder — wraps @ref QoreTokenizerUtils::EmbeddingModel "EmbeddingModel" in ONNX mode
/** Runs a local ONNX embedding model via the underlying
    @ref QoreTokenizerUtils::EmbeddingModel "EmbeddingModel" class in
    \c "onnx" mode.

    @par Configuration

    - \c model_path (required): filesystem path to the ONNX model file
    - \c options.tokenizer_config (required): tokenizer JSON hash as expected
      by @ref QoreTokenizerUtils::EmbeddingModel "EmbeddingModel"; pass the
      parsed contents of the model's \c tokenizer.json
    - \c options.max_tokens (optional): maximum input tokens; default 512
    - \c options.pooling_strategy (optional): \c "mean" (default), \c "cls", or \c "max"

    @par Laziness

    The underlying ONNX model is instantiated on first use; construction is
    cheap and validation of the model file is deferred until the first
    @ref encode() / @ref encodeBatch() call.

    @since QoreRagUtils 1.0
*/
public class LocalOnnxEmbedder inherits AbstractEmbedder {
    private {
        #! Captured configuration
        hash<EmbedderConfig> config;
        #! Lazily-created embedding model
        *QoreTokenizerUtils::EmbeddingModel model;
        #! Guard for lazy initialization
        Mutex init_mutex();
    }

    #! Creates the embedder
    /** @param config embedder configuration; \c model_path must be set

        @throw EMBEDDER-CONFIG-ERROR if required fields are missing
    */
    constructor(hash<EmbedderConfig> config) {
        if (!config.model_path.val()) {
            throw "EMBEDDER-CONFIG-ERROR",
                "LocalOnnxEmbedder requires 'model_path' in EmbedderConfig";
        }
        self.config = config;
    }

    #! Encodes a single piece of text into a vector
    list<float> encode(string text) {
        ensureModel();
        return model.embed(text);
    }

    #! Encodes a batch of texts into vectors
    list<list<float>> encodeBatch(list<string> texts) {
        ensureModel();
        return model.embedBatch(texts);
    }

    #! Returns the embedding dimension produced by this backend
    /** @note The underlying ONNX model returns -1 until the first successful
        embedding call; this method triggers a lightweight probe if the
        dimension is not yet known.
    */
    int getDimension() {
        ensureModel();
        int dim = model.getDimension();
        if (dim < 0) {
            # probe with a minimal text to populate the cached dimension
            model.embed(" ");
            dim = model.getDimension();
        }
        return dim;
    }

    #! Returns the model name (basename of \c model_path)
    *string getModel() {
        return basename(config.model_path);
    }

    #! Ensures the underlying EmbeddingModel is created
    private ensureModel() {
        if (!model) {
            init_mutex.lock();
            on_exit init_mutex.unlock();
            if (!model) {
                hash<QoreTokenizerUtils::EmbeddingConfig> emb_cfg = <QoreTokenizerUtils::EmbeddingConfig>{
                    "mode": "onnx",
                    "model_path": config.model_path,
                    "tokenizer_config": config.options.tokenizer_config,
                };
                if (config.options.max_tokens) {
                    emb_cfg.max_tokens = config.options.max_tokens;
                }
                if (config.options.pooling_strategy.val()) {
                    emb_cfg.pooling_strategy = config.options.pooling_strategy;
                }
                model = new QoreTokenizerUtils::EmbeddingModel(emb_cfg);
            }
        }
    }
}
}
