# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectTypeHelper.qc Type serialization/deserialization for Connect reflection

/*  ConnectTypeHelper.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace Connect {

#! Static helper for bidirectional conversion between DataProvider types and wire-safe hash descriptors
/** Used by the Connect reflection service to serialize type information for discovery,
    and by ConnectDataProvider to reconstruct types from reflection responses.

    Descriptor format:
    - Scalar: \c {"kind": "scalar", "qore_type": "string"}
    - Hash with fields: \c {"kind": "hash", "name": "MyType", "fields": {...}}
    - List: \c {"kind": "list", "element_type": {...}}
    - Soft list: \c {"kind": "soft_list", "element_type": {...}}
*/
public class ConnectTypeHelper {
    #! Serializes an AbstractDataProviderType to a wire-safe hash descriptor
    /** @param type the DataProvider type to serialize
        @param visited set of already-visited type names for cycle detection

        @return a hash describing the type in a codec-neutral format
    */
    static hash<auto> typeToDescriptor(AbstractDataProviderType type,
            *hash<string, bool> visited) {
        string type_name = type.getName();

        # Check for HashDataType (hashdecl-like with named fields)
        if (type instanceof HashDataType) {
            HashDataType ht = cast<HashDataType>(type);
            string name = ht.getName();

            # Cycle detection
            if (visited{name}) {
                return {"kind": "scalar", "qore_type": "auto"};
            }
            visited{name} = True;

            hash<auto> fields();
            *hash<auto> field_info = ht.getFields();
            if (field_info) {
                map (fields{$1.key} = {
                    "type": ConnectTypeHelper::typeToDescriptor($1.value.getType(), visited),
                    "desc": $1.value.getDescription(),
                }), field_info.pairIterator();
            }
            return {
                "kind": "hash",
                "name": name,
                "fields": fields,
            };
        }

        # Check for SoftListDataType
        if (type instanceof SoftListDataType) {
            SoftListDataType slt = cast<SoftListDataType>(type);
            *AbstractDataProviderType elem = slt.getElementType();
            return {
                "kind": "soft_list",
                "element_type": elem
                    ? ConnectTypeHelper::typeToDescriptor(elem, visited)
                    : {"kind": "scalar", "qore_type": "auto"},
            };
        }

        # Check for ListDataType
        if (type instanceof ListDataType) {
            ListDataType lt = cast<ListDataType>(type);
            *AbstractDataProviderType elem = lt.getElementType();
            return {
                "kind": "list",
                "element_type": elem
                    ? ConnectTypeHelper::typeToDescriptor(elem, visited)
                    : {"kind": "scalar", "qore_type": "auto"},
            };
        }

        # Scalar type — map to qore_type name
        return {"kind": "scalar", "qore_type": type_name};
    }

    #! Reconstructs an AbstractDataProviderType from a wire-safe hash descriptor
    /** @param desc the descriptor hash (as returned by typeToDescriptor())

        @return the reconstructed DataProvider type

        @throw CONNECT-TYPE-ERROR if the descriptor is invalid
    */
    static AbstractDataProviderType descriptorToType(hash<auto> desc) {
        switch (desc.kind) {
            case "scalar":
                return ConnectTypeHelper::scalarNameToType(desc.qore_type);

            case "hash": {
                HashDataType ht(desc.name ?? "anonymous");
                if (desc.fields) {
                    map ht.addField(new QoreDataField(
                        $1.key,
                        $1.value.desc ?? "",
                        ConnectTypeHelper::descriptorToType($1.value.type),
                    )), desc.fields.pairIterator();
                }
                return ht;
            }

            case "list": {
                AbstractDataProviderType elem = desc.element_type
                    ? ConnectTypeHelper::descriptorToType(desc.element_type)
                    : AbstractDataProviderType::get(AutoType);
                return new ListDataType(desc.name ?? "list", elem);
            }

            case "soft_list": {
                AbstractDataProviderType elem = desc.element_type
                    ? ConnectTypeHelper::descriptorToType(desc.element_type)
                    : AbstractDataProviderType::get(AutoType);
                return new SoftListDataType(desc.name ?? "softlist", elem);
            }

            default:
                throw "CONNECT-TYPE-ERROR",
                    sprintf("unknown type descriptor kind: %y", desc.kind);
        }
    }

    #! Maps a Qore type name to a DataProvider type
    /** @param name the type name (e.g., \c "string", \c "int", \c "auto")

        @return the DataProvider type
    */
    static AbstractDataProviderType scalarNameToType(string name) {
        switch (name) {
            case "string":
                return AbstractDataProviderType::get(StringType);
            case "int":
                return AbstractDataProviderType::get(IntType);
            case "float":
                return AbstractDataProviderType::get(FloatType);
            case "number":
                return AbstractDataProviderType::get(NumberType);
            case "bool":
                return AbstractDataProviderType::get(BoolType);
            case "date":
                return AbstractDataProviderType::get(DateType);
            case "binary":
            case "data":
                return AbstractDataProviderType::get(DataType);
            case "softstring":
                return AbstractDataProviderType::get(SoftStringType);
            case "softint":
                return AbstractDataProviderType::get(SoftIntType);
            case "softfloat":
                return AbstractDataProviderType::get(SoftFloatType);
            case "softbool":
                return AbstractDataProviderType::get(SoftBoolType);
            case "softdate":
                return AbstractDataProviderType::get(SoftDateType);
            case "softnumber":
                return AbstractDataProviderType::get(SoftNumberType);
            case "hash":
            case "hash<auto>":
                return AbstractDataProviderType::get(AutoHashType);
            case "list":
            case "list<auto>":
                return AbstractDataProviderType::get(AutoListType);
            default:
                return AbstractDataProviderType::get(AutoType);
        }
    }

    #! Maps an RPC type integer to a human-readable name
    /** @param rpc_type the RPC type constant
        @return the human-readable name (e.g., \c "unary", \c "datagram_fire_and_forget")
    */
    static string rpcTypeName(int rpc_type) {
        switch (rpc_type) {
            case CONNECT_RPC_UNARY:
                return "unary";
            case CONNECT_RPC_SERVER_STREAM:
                return "server_stream";
            case CONNECT_RPC_CLIENT_STREAM:
                return "client_stream";
            case CONNECT_RPC_BIDI:
                return "bidi";
            case CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET:
                return "datagram_fire_and_forget";
            case CONNECT_RPC_DATAGRAM_BIDI:
                return "datagram_bidi";
            default:
                return sprintf("unknown_%d", rpc_type);
        }
    }

    #! Converts a ConnectServerMethodInfo to a wire-safe descriptor hash
    /** @param info the method info

        @return a hash describing the method including type descriptors
    */
    static hash<auto> methodInfoToDescriptor(hash<auto> info) {
        hash<auto> desc = {
            "service": info.service,
            "method": info.method,
            "path": info.path,
            "rpc_type": info.rpc_type,
            "rpc_type_name": ConnectTypeHelper::rpcTypeName(info.rpc_type),
        };
        if (info.request_type) {
            desc.request_type = ConnectTypeHelper::typeToDescriptor(info.request_type);
        }
        if (info.response_type) {
            desc.response_type = ConnectTypeHelper::typeToDescriptor(info.response_type);
        }
        return desc;
    }
}

} # namespace Connect
