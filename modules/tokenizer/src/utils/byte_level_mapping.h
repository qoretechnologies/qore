/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    byte_level_mapping.h

    GPT-2 byte-level BPE byte <-> Unicode mapping

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BYTE_LEVEL_MAPPING_H
#define _QORE_TOKENIZER_BYTE_LEVEL_MAPPING_H

#include <string>
#include <unordered_map>
#include <cstdint>

namespace QoreTokenizer {

//! Returns the GPT-2 byte-to-unicode mapping (byte value → unicode char as UTF-8)
const std::unordered_map<uint8_t, std::string>& getByteToUnicode();

//! Returns the GPT-2 unicode-to-byte mapping (unicode char as UTF-8 → byte value)
const std::unordered_map<std::string, uint8_t>& getUnicodeToByte();

//! Encodes raw bytes to GPT-2 unicode representation
std::string bytesToUnicode(const std::string& input);

//! Decodes GPT-2 unicode representation back to raw bytes
std::string unicodeToBytes(const std::string& input);

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BYTE_LEVEL_MAPPING_H
