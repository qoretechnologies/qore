/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPostProcessor.cpp

    Abstract base class for post-processors - factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "postprocessors/AbstractPostProcessor.h"
#include "postprocessors/TemplateProcessor.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

std::unique_ptr<AbstractPostProcessor> AbstractPostProcessor::fromConfig(
        const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "post_processor config is null");
        return nullptr;
    }

    std::string type = safeGetStringKey(config, "type");
    if (type.empty()) {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "post_processor config missing 'type' field");
        return nullptr;
    }

    if (type == "TemplateProcessing") {
        return std::make_unique<TemplateProcessor>(config, xsink);
    } else if (type == "BertProcessing") {
        return std::make_unique<BertProcessor>(config, xsink);
    } else if (type == "RobertaProcessing") {
        return std::make_unique<RobertaProcessor>(config, xsink);
    } else if (type == "ByteLevel") {
        return std::make_unique<ByteLevelPostProcessor>();
    } else if (type == "Sequence") {
        return std::make_unique<SequencePostProcessor>(config, xsink);
    } else {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "unknown post_processor type '%s'", type.c_str());
        return nullptr;
    }
}

} // namespace QoreTokenizer
