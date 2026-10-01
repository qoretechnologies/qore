// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: Apache-2.0
#include <onnx/checker.h>
#include <onnx/defs/operator_sets.h>
#include <onnx/onnx_pb.h>
#include <initializer_list>
#include <string>

int main() {
    if (!qore_onnx::IsOnnxStaticRegistrationDisabled()) {
        return 1;
    }
    qore_onnx::RegisterOnnxOperatorSetSchema();
    qore_onnx::ModelProto model;
    model.set_ir_version(8);
    model.add_opset_import()->set_version(13);
    auto* graph = model.mutable_graph();
    graph->set_name("private-static-library");
    for (auto* value : {graph->add_input(), graph->add_output()}) {
        value->set_name("value");
        auto* tensor = value->mutable_type()->mutable_tensor_type();
        tensor->set_elem_type(qore_onnx::TensorProto::FLOAT);
        tensor->mutable_shape()->add_dim()->set_dim_value(1);
    }
    qore_onnx::checker::check_model(model);
    std::string data;
    if (!model.SerializeToString(&data)) {
        return 1;
    }
    qore_onnx::ModelProto copy;
    return !copy.ParseFromString(data) || copy.graph().name() != graph->name();
}
