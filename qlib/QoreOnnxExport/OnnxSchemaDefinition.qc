# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxSchemaDefinition.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see OnnxSchema
public namespace QoreOnnxExport {

#! ONNX protobuf schema definition (minimal subset for model export)
/** This is a subset of the official ONNX proto schema containing only the
    message types needed to construct and serialize ONNX ModelProto graphs.

    @see https://github.com/onnx/onnx/blob/main/onnx/onnx.proto
*/
public const OnnxProtoDefinition =
    "syntax = \"proto3\";\n"
    "package onnx;\n"
    "\n"
    "message StringStringEntryProto {\n"
    "  string key = 1;\n"
    "  string value = 2;\n"
    "}\n"
    "\n"
    "message TensorShapeProto {\n"
    "  message Dimension {\n"
    "    oneof value {\n"
    "      int64 dim_value = 1;\n"
    "      string dim_param = 2;\n"
    "    }\n"
    "    string denotation = 3;\n"
    "  }\n"
    "  repeated Dimension dim = 1;\n"
    "}\n"
    "\n"
    "message TypeProto {\n"
    "  message Tensor {\n"
    "    int32 elem_type = 1;\n"
    "    TensorShapeProto shape = 2;\n"
    "  }\n"
    "  oneof value {\n"
    "    Tensor tensor_type = 1;\n"
    "  }\n"
    "  string denotation = 6;\n"
    "}\n"
    "\n"
    "message ValueInfoProto {\n"
    "  string name = 1;\n"
    "  TypeProto type = 2;\n"
    "  string doc_string = 3;\n"
    "}\n"
    "\n"
    "message TensorProto {\n"
    "  repeated int64 dims = 1;\n"
    "  int32 data_type = 2;\n"
    "  repeated float float_data = 4;\n"
    "  repeated int32 int32_data = 5;\n"
    "  repeated int64 int64_data = 7;\n"
    "  string name = 8;\n"
    "  bytes raw_data = 9;\n"
    "  repeated double double_data = 10;\n"
    "  repeated uint64 uint64_data = 11;\n"
    "  string doc_string = 12;\n"
    "}\n"
    "\n"
    "message AttributeProto {\n"
    "  string name = 1;\n"
    "  float f = 2;\n"
    "  int64 i = 3;\n"
    "  bytes s = 4;\n"
    "  TensorProto t = 5;\n"
    "  GraphProto g = 6;\n"
    "  repeated float floats = 7;\n"
    "  repeated int64 ints = 8;\n"
    "  repeated bytes strings = 9;\n"
    "  repeated TensorProto tensors = 10;\n"
    "  repeated GraphProto graphs = 11;\n"
    "  string doc_string = 13;\n"
    "  int32 type = 20;\n"
    "  string ref_attr_name = 21;\n"
    "}\n"
    "\n"
    "message NodeProto {\n"
    "  repeated string input = 1;\n"
    "  repeated string output = 2;\n"
    "  string name = 3;\n"
    "  string op_type = 4;\n"
    "  repeated AttributeProto attribute = 5;\n"
    "  string doc_string = 6;\n"
    "  string domain = 7;\n"
    "}\n"
    "\n"
    "message GraphProto {\n"
    "  repeated NodeProto node = 1;\n"
    "  string name = 2;\n"
    "  repeated TensorProto initializer = 5;\n"
    "  string doc_string = 10;\n"
    "  repeated ValueInfoProto input = 11;\n"
    "  repeated ValueInfoProto output = 12;\n"
    "  repeated ValueInfoProto value_info = 13;\n"
    "}\n"
    "\n"
    "message OperatorSetIdProto {\n"
    "  string domain = 1;\n"
    "  int64 version = 2;\n"
    "}\n"
    "\n"
    "message ModelProto {\n"
    "  int64 ir_version = 1;\n"
    "  repeated OperatorSetIdProto opset_import = 8;\n"
    "  string producer_name = 2;\n"
    "  string producer_version = 3;\n"
    "  string domain = 4;\n"
    "  int64 model_version = 5;\n"
    "  string doc_string = 6;\n"
    "  GraphProto graph = 7;\n"
    "  repeated StringStringEntryProto metadata_props = 14;\n"
    "}\n"
;

#! Cached ProtobufSchema for ONNX encoding
/** Lazily initialized on first use. Thread-safe via Qore's atomic reference
    counting — multiple threads can read concurrently after initialization.
*/
public Protobuf::ProtobufSchema sub getOnnxSchema() {
    our Protobuf::ProtobufSchema onnx_schema;
    if (!onnx_schema) {
        onnx_schema = new Protobuf::ProtobufSchema(OnnxProtoDefinition);
    }
    return onnx_schema;
}

#! ONNX TensorProto DataType constants
public const ONNX_FLOAT = 1;
public const ONNX_UINT8 = 2;
public const ONNX_INT8 = 3;
public const ONNX_INT16 = 5;
public const ONNX_INT32 = 6;
public const ONNX_INT64 = 7;
public const ONNX_STRING = 8;
public const ONNX_BOOL = 9;
public const ONNX_FLOAT16 = 10;
public const ONNX_DOUBLE = 11;
public const ONNX_UINT32 = 12;
public const ONNX_UINT64 = 13;

#! ONNX AttributeProto type constants
public const ONNX_ATTR_FLOAT = 1;
public const ONNX_ATTR_INT = 2;
public const ONNX_ATTR_STRING = 3;
public const ONNX_ATTR_TENSOR = 4;
public const ONNX_ATTR_GRAPH = 5;
public const ONNX_ATTR_FLOATS = 6;
public const ONNX_ATTR_INTS = 7;
public const ONNX_ATTR_STRINGS = 8;

} # namespace QoreOnnxExport
