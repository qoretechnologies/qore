# -*- mode: qore; indent-tabs-mode: nil -*-

/*  OnnxGraphBuilder.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see OnnxGraphBuilder
public namespace QoreOnnxExport {

#! Helper class for building ONNX graph structures as Qore hashes
/** Provides factory methods for creating ONNX protobuf message hashes
    that can be serialized via the ProtobufSchema encode API.
*/
public class OnnxGraphBuilder {
    #! Default ONNX opset version
    public const DefaultOpsetVersion = 18;

    #! Default ONNX IR version
    public const DefaultIrVersion = 9;

    #! Creates a TensorProto hash for float data
    /** @param name the tensor name
        @param dims the tensor dimensions (list of int)
        @param data the float data values
        @return a TensorProto-compatible hash
    */
    static hash<auto> makeTensorFloat(string name, list<int> dims, list<auto> data) {
        return {
            "dims": dims,
            "data_type": ONNX_FLOAT,
            "float_data": data,
            "name": name,
        };
    }

    #! Creates a TensorProto hash for int64 data
    static hash<auto> makeTensorInt64(string name, list<int> dims, list<auto> data) {
        return {
            "dims": dims,
            "data_type": ONNX_INT64,
            "int64_data": data,
            "name": name,
        };
    }

    #! Creates a ValueInfoProto hash for a graph input
    /** @param name the input tensor name
        @param n_features the number of features (columns)
        @param elem_type the element data type (default: ONNX_FLOAT)
        @return a ValueInfoProto-compatible hash
    */
    static hash<auto> makeInput(string name, int n_features, int elem_type = ONNX_FLOAT) {
        return {
            "name": name,
            "type": {
                "tensor_type": {
                    "elem_type": elem_type,
                    "shape": {
                        "dim": (
                            {"dim_param": "N"},
                            {"dim_value": n_features},
                        ),
                    },
                },
            },
        };
    }

    #! Creates a ValueInfoProto hash for a graph output
    /** @param name the output tensor name
        @param n_outputs the number of output values per sample
        @param elem_type the element data type (default: ONNX_FLOAT)
        @return a ValueInfoProto-compatible hash
    */
    static hash<auto> makeOutput(string name, int n_outputs, int elem_type = ONNX_FLOAT) {
        return {
            "name": name,
            "type": {
                "tensor_type": {
                    "elem_type": elem_type,
                    "shape": {
                        "dim": (
                            {"dim_param": "N"},
                            {"dim_value": n_outputs},
                        ),
                    },
                },
            },
        };
    }

    #! Creates a scalar output (single value per sample)
    static hash<auto> makeScalarOutput(string name, int elem_type = ONNX_INT64) {
        return {
            "name": name,
            "type": {
                "tensor_type": {
                    "elem_type": elem_type,
                    "shape": {
                        "dim": (
                            {"dim_param": "N"},
                        ),
                    },
                },
            },
        };
    }

    #! Creates a NodeProto hash
    /** @param op_type the ONNX operator type (e.g., "MatMul", "Add", "Sub")
        @param inputs list of input tensor names
        @param outputs list of output tensor names
        @param attrs optional list of AttributeProto hashes
        @param name optional node name
        @param domain optional operator domain (e.g., "ai.onnx.ml")
        @return a NodeProto-compatible hash
    */
    static hash<auto> makeNode(string op_type, list<string> inputs, list<string> outputs,
            *list<hash<auto>> attrs, *string name, *string domain) {
        hash<auto> node = {
            "op_type": op_type,
            "input": inputs,
            "output": outputs,
        };
        if (attrs) {
            node.attribute = attrs;
        }
        if (name) {
            node.name = name;
        }
        if (domain) {
            node.domain = domain;
        }
        return node;
    }

    #! Creates a float attribute
    static hash<auto> makeAttrFloat(string name, float value) {
        return {"name": name, "type": ONNX_ATTR_FLOAT, "f": value};
    }

    #! Creates an int attribute
    static hash<auto> makeAttrInt(string name, int value) {
        return {"name": name, "type": ONNX_ATTR_INT, "i": value};
    }

    #! Creates a string attribute
    static hash<auto> makeAttrString(string name, string value) {
        return {"name": name, "type": ONNX_ATTR_STRING, "s": binary(value)};
    }

    #! Creates a repeated float attribute
    static hash<auto> makeAttrFloats(string name, list<auto> values) {
        return {"name": name, "type": ONNX_ATTR_FLOATS, "floats": values};
    }

    #! Creates a repeated int attribute
    static hash<auto> makeAttrInts(string name, list<auto> values) {
        return {"name": name, "type": ONNX_ATTR_INTS, "ints": values};
    }

    #! Creates a repeated string attribute
    static hash<auto> makeAttrStrings(string name, list<auto> values) {
        list<binary> bvalues();
        foreach auto v in (values) {
            push bvalues, binary(string(v));
        }
        return {"name": name, "type": ONNX_ATTR_STRINGS, "strings": bvalues};
    }

    #! Builds a complete ONNX ModelProto hash
    /** @param graph_name the computation graph name
        @param nodes list of NodeProto hashes
        @param inputs list of ValueInfoProto hashes for graph inputs
        @param outputs list of ValueInfoProto hashes for graph outputs
        @param initializers list of TensorProto hashes for model weights/parameters
        @param opset_imports optional list of OperatorSetIdProto hashes
        @param producer_name optional producer name (default: "qore-ml")
        @return a ModelProto-compatible hash
    */
    static hash<auto> buildModel(string graph_name, list<hash<auto>> nodes,
            list<hash<auto>> inputs, list<hash<auto>> outputs,
            list<hash<auto>> initializers,
            *list<hash<auto>> opset_imports,
            string producer_name = "qore-ml") {
        if (!opset_imports) {
            opset_imports = ({"domain": "", "version": DefaultOpsetVersion},);
        }
        return {
            "ir_version": DefaultIrVersion,
            "opset_import": opset_imports,
            "producer_name": producer_name,
            "producer_version": "1.0",
            "graph": {
                "name": graph_name,
                "node": nodes,
                "input": inputs,
                "output": outputs,
                "initializer": initializers,
            },
        };
    }

    #! Serializes a ModelProto hash to ONNX binary format
    /** @param model_hash the ModelProto hash
        @return the ONNX binary data
    */
    static binary serialize(hash<auto> model_hash) {
        return getOnnxSchema().encode("onnx.ModelProto", model_hash);
    }
}

} # namespace QoreOnnxExport
