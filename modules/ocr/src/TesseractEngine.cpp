/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TesseractEngine.cpp

    Qore OCR module — Tesseract engine implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "TesseractEngine.h"

#include <qore/QoreSandboxManager.h>

#include <tesseract/resultiterator.h>

#include <sys/stat.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <unordered_set>
#include <vector>

namespace QoreOcr {

//! Returns true if \a c may appear in a language code
static bool ocr_is_language_code_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

//! Checks a language specification
/** A language specification is one or more language codes joined by \c "+"; each code names the file
    <tt>\<code\>.traineddata</tt> in the data directory, so it may only contain ASCII letters, digits, \c "_" and
    \c "-": a path character would name a file in another directory.

    @return true if the specification is valid, false if an \c OCR-INIT-ERROR exception was raised
*/
static bool ocr_check_language(const QoreStringNode& language, ExceptionSink* xsink) {
    size_t code_len = 0;
    // the length is used, not strlen(), so that a NUL character is refused like any other path character
    for (size_t i = 0, len = language.size(); i <= len; ++i) {
        char c = i < len ? language.c_str()[i] : '+';
        if (c == '+') {
            if (!code_len) {
                xsink->raiseException("OCR-INIT-ERROR", "invalid language specification '%s': each language code "
                    "joined by '+' must be non-empty", language.c_str());
                return false;
            }
            code_len = 0;
            continue;
        }
        if (!ocr_is_language_code_char(c)) {
            xsink->raiseException("OCR-INIT-ERROR", "invalid language specification '%s': a language code may only "
                "contain ASCII letters, digits, '_' and '-'", language.c_str());
            return false;
        }
        ++code_len;
    }
    return true;
}

//! The parameters that the language configuration of trained data loaded by a sandboxed program may set
/** These are the parameters that the language configurations of the official trained data (the \c tessdata,
    \c tessdata_fast and \c tessdata_best repositories) set; none of them names a file or produces output, except
    \c tessedit_load_sublangs, whose languages are loaded through ocr_sandbox_file_reader() like the requested ones
    and are checked like them.  A parameter that names a file, such as \c user_words_file or
    \c tessedit_write_params_to_file, would let the trained data read or write a file the sandbox does not allow.
*/
static const std::unordered_set<std::string> ocr_sandbox_config_params = {
    "allow_blob_division",
    "assume_fixed_pitch_char_segment",
    "chop_enable",
    "classify_cp_cutoff_strength",
    "classify_integer_matcher_multiplier",
    "classify_misfit_junk_penalty",
    "disable_character_fragments",
    "edges_children_count_limit",
    "edges_children_fix",
    "edges_max_children_layers",
    "edges_max_children_per_outline",
    "edges_use_new_outline_complexity",
    "language_model_ngram_nonmatch_score",
    "language_model_ngram_on",
    "language_model_ngram_scale_factor",
    "language_model_ngram_space_delimited_language",
    "language_model_use_sigmoidal_certainty",
    "merge_fragments_in_matrix",
    "ocr_devanagari_split_strategy",
    "pageseg_devanagari_split_strategy",
    "preserve_interword_spaces",
    "prioritize_division",
    "segment_nonalphabetic_script",
    "segment_penalty_dict_nonword",
    "segment_penalty_garbage",
    "segsearch_max_char_wh_ratio",
    "segsearch_max_futile_classifications",
    "stopper_nondict_certainty_base",
    "tessedit_char_blacklist",
    "tessedit_load_sublangs",
    "tessedit_ocr_engine_mode",
    "tessedit_tess_adaption_mode",
    "tessedit_use_primary_params_model",
    "textord_force_make_prop_words",
    "textord_max_noise_size",
    "textord_no_rejects",
    "textord_noise_normratio",
    "textord_noise_rejwords",
    "textord_noise_rowratio",
    "textord_noise_sizelimit",
    "textord_noise_syfract",
    "textord_single_height_mode",
    "textord_space_size_is_variable",
    "textord_tabfind_aligned_gap_fraction",
    "textord_tabfind_vertical_text_ratio",
    "textord_tabvector_vertical_box_ratio",
    "textord_tabvector_vertical_gap_fraction",
    "textord_use_cjk_fp_model",
    "tosp_force_wordbreak_on_punct",
    "tosp_old_sp_kn_th_factor",
    "tosp_old_to_constrain_sp_kn",
    "tosp_old_to_method",
    "tosp_only_small_gaps_for_kern",
    "tosp_use_pre_chopping",
};

//! The longest language configuration line that trained data loaded by a sandboxed program may have
/** Tesseract reads a language configuration in pieces of at most \c MAX_PATH - 1 bytes (at least 259 bytes on
    every platform) and treats each piece as a line, so a longer line would be read as several lines; with shorter
    lines, the lines checked here are exactly the lines Tesseract reads.  The official trained data has no line
    longer than 96 bytes.
*/
static constexpr size_t OCR_MAX_CONFIG_LINE = 255;

//! Checks the language configuration of trained data loaded on behalf of a sandboxed program
/** The lines are parsed as Tesseract's \c ParamUtils::ReadParamsFromFp() parses them: a line starting with
    \c "#", \c "\r" or \c "\n" is ignored, trailing line ends are removed, the parameter name runs to the first
    space or tab, and the value starts after the following spaces and tabs.

    @return true if every parameter the configuration sets is allowed, false if an \c OCR-INIT-ERROR exception was
    raised
*/
static bool ocr_check_lang_config(const char* filename, const char* cfg, size_t len, ExceptionSink* xsink) {
    // Tesseract would stop reading a line at a NUL character
    if (memchr(cfg, '\0', len)) {
        xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' has a language configuration with a NUL "
            "character, which a sandboxed program may not load", filename);
        return false;
    }
    size_t pos = 0;
    for (size_t line_no = 0; pos < len; ++line_no) {
        // trained data can have a configuration of millions of lines
        if (line_no && !(line_no % 100) && qore_check_cancel(xsink, "checking OCR trained data")) {
            return false;
        }
        const char* line = cfg + pos;
        const char* nl = static_cast<const char*>(memchr(line, '\n', len - pos));
        size_t line_len = nl ? static_cast<size_t>(nl - line) + 1 : len - pos;
        pos += line_len;
        if (line_len > OCR_MAX_CONFIG_LINE) {
            xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' has a language configuration line longer "
                "than %zu bytes, which a sandboxed program may not load", filename, OCR_MAX_CONFIG_LINE);
            return false;
        }
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') {
            continue;
        }
        while (line_len && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            --line_len;
        }
        size_t name_len = 0;
        while (name_len < line_len && line[name_len] != ' ' && line[name_len] != '\t') {
            ++name_len;
        }
        // an empty name cannot name a parameter
        if (!name_len) {
            continue;
        }
        std::string name(line, name_len);
        if (ocr_sandbox_config_params.find(name) == ocr_sandbox_config_params.end()) {
            xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' sets the parameter '%s' in its language "
                "configuration, which a sandboxed program may not load", filename, name.c_str());
            return false;
        }
        if (name == "tessedit_load_sublangs") {
            size_t vpos = name_len;
            while (vpos < line_len && (line[vpos] == ' ' || line[vpos] == '\t')) {
                ++vpos;
            }
            // the languages to load are joined by "+", and a "~" prefix names a language not to load
            for (size_t i = vpos; i < line_len; ++i) {
                if (!ocr_is_language_code_char(line[i]) && line[i] != '+' && line[i] != '~') {
                    xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' names the sub-languages '%s' in its "
                        "language configuration; a language code may only contain ASCII letters, digits, '_' and "
                        "'-'", filename, std::string(line + vpos, line_len - vpos).c_str());
                    return false;
                }
            }
        }
    }
    return true;
}

//! Reverses the byte order of a value read from trained data written on a host with the other byte order
template <typename T>
static T ocr_reverse_bytes(T v) {
    unsigned char b[sizeof(T)];
    memcpy(b, &v, sizeof(T));
    std::reverse(b, b + sizeof(T));
    memcpy(&v, b, sizeof(T));
    return v;
}

//! Checks trained data loaded on behalf of a sandboxed program
/** The header is parsed as Tesseract's \c TessdataManager::LoadMemBuffer() parses it: a 32-bit entry count,
    byte-swapped when it is larger than 1000, and a table of 64-bit offsets.  The components follow the table in
    order, and each offset only determines the size of its component, so the language configuration, the first
    component, is the data right after the table.

    @return true if the trained data may be loaded, false if an \c OCR-INIT-ERROR exception was raised
*/
static bool ocr_check_trained_data(const char* filename, const std::vector<char>& data, ExceptionSink* xsink) {
    static constexpr uint32_t max_entries = 1000;
    uint32_t num_entries = 0;
    if (data.size() >= sizeof(num_entries)) {
        memcpy(&num_entries, data.data(), sizeof(num_entries));
    }
    bool swap = num_entries > max_entries;
    if (swap) {
        num_entries = ocr_reverse_bytes(num_entries);
    }
    size_t header_size = sizeof(num_entries) + sizeof(int64_t) * static_cast<size_t>(num_entries);
    if (data.size() < sizeof(num_entries) || num_entries > max_entries || data.size() < header_size) {
        xsink->raiseException("OCR-INIT-ERROR", "'%s' is not valid trained data", filename);
        return false;
    }
    auto offset = [&data, swap](size_t i) -> int64_t {
        int64_t v;
        memcpy(&v, data.data() + sizeof(uint32_t) + sizeof(int64_t) * i, sizeof(v));
        return swap ? ocr_reverse_bytes(v) : v;
    };
    if (!num_entries || offset(0) < 0) {
        // no language configuration
        return true;
    }
    int64_t config_size = static_cast<int64_t>(data.size()) - offset(0);
    size_t next = 1;
    while (next < num_entries && offset(next) == -1) {
        ++next;
    }
    if (next < num_entries) {
        config_size = offset(next) - offset(0);
    }
    if (config_size < 0 || static_cast<uint64_t>(config_size) > data.size() - header_size) {
        xsink->raiseException("OCR-INIT-ERROR", "'%s' is not valid trained data", filename);
        return false;
    }
    return ocr_check_lang_config(filename, data.data() + header_size, static_cast<size_t>(config_size), xsink);
}

//! Reads a trained data file on behalf of a sandboxed program
/** @return true if the file was read, false if an exception was raised
*/
static bool ocr_read_trained_data(const char* filename, std::vector<char>& data, ExceptionSink* xsink) {
    // QoreFile::open2() checks that the sandbox of the calling program allows the file to be read
    QoreFile f;
    if (f.open2(xsink, filename, O_RDONLY)) {
        return false;
    }
    struct stat sb;
    if (fstat(f.getFD(), &sb)) {
        xsink->raiseErrnoException("OCR-INIT-ERROR", errno, "cannot get the status of trained data '%s'", filename);
        return false;
    }
    // a file that is not a regular file, such as a FIFO, could block the read forever
    if (!S_ISREG(sb.st_mode)) {
        xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' is not a regular file", filename);
        return false;
    }
    // Tesseract takes the size of trained data as an int
    if (sb.st_size > INT_MAX) {
        xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' is too large (%lld bytes)", filename,
            static_cast<long long>(sb.st_size));
        return false;
    }
    data.clear();
    data.reserve(static_cast<size_t>(sb.st_size));
    std::vector<char> buf(1024 * 1024);
    while (true) {
        size_t n = f.read(buf.data(), buf.size(), -1, xsink);
        if (*xsink) {
            return false;
        }
        if (!n) {
            break;
        }
        data.insert(data.end(), buf.begin(), buf.begin() + n);
        // the file may have grown since its size was checked
        if (data.size() > static_cast<size_t>(INT_MAX)) {
            xsink->raiseException("OCR-INIT-ERROR", "trained data '%s' is too large", filename);
            return false;
        }
        if (qore_check_cancel(xsink, "reading OCR trained data")) {
            return false;
        }
    }
    return true;
}

//! The exception sink of the sandboxed Tesseract initialization running on this thread
/** Tesseract's \c FileReader callback has no user data argument; Tesseract calls it on the thread that initializes
    the engine.
*/
static thread_local ExceptionSink* ocr_reader_xsink = nullptr;

//! Sets the exception sink of ocr_sandbox_file_reader() for a sandboxed Tesseract initialization
class OcrReaderScope {
public:
    DLLLOCAL OcrReaderScope(ExceptionSink* xsink) : prev(ocr_reader_xsink) {
        ocr_reader_xsink = xsink;
    }

    DLLLOCAL ~OcrReaderScope() {
        ocr_reader_xsink = prev;
    }

    OcrReaderScope(const OcrReaderScope&) = delete;
    OcrReaderScope& operator=(const OcrReaderScope&) = delete;

private:
    ExceptionSink* prev;
};

//! Reads trained data for Tesseract on behalf of a sandboxed program
/** Tesseract loads every trained data file through this reader when a sandboxed program gives an explicit data
    directory, including the sub-languages named by a language configuration: the sandbox of the calling program
    must allow the file, its language configuration may only set the parameters in ocr_sandbox_config_params, and
    Tesseract is given exactly the bytes that were checked, so the file cannot be changed between the check and the
    load.
*/
static bool ocr_sandbox_file_reader(const char* filename, std::vector<char>* data) {
    // a read outside an initialization, which this module does not make, fails without an exception
    ExceptionSink local_xsink;
    ExceptionSink* xsink = ocr_reader_xsink ? ocr_reader_xsink : &local_xsink;
    bool ok = false;
    // an exception must not reach Tesseract
    try {
        ok = !*xsink && ocr_read_trained_data(filename, *data, xsink) && ocr_check_trained_data(filename, *data, xsink);
    } catch (const std::exception& e) {
        xsink->raiseException("OCR-INIT-ERROR", "cannot read trained data '%s': %s", filename, e.what());
    }
    if (!ok) {
        data->clear();
    }
    local_xsink.clear();
    return ok;
}

QoreTesseractEngine::QoreTesseractEngine(const QoreStringNode* datapath,
        const QoreStringNode* language, ExceptionSink* xsink) {
    if (language && language->size() && !ocr_check_language(*language, xsink)) {
        return;
    }
    // a path with a NUL character would name the directory before the NUL character
    if (datapath && q_check_path(*datapath, "OCR-INIT-ERROR", xsink)) {
        return;
    }

    const char* dp = datapath && datapath->strlen() ? datapath->c_str() : nullptr;
    const char* lang = language && language->strlen() ? language->c_str() : "eng";

    // the trained data in an explicit data directory is read on behalf of the calling program; the system data
    // directory is a system resource
    bool sandboxed = false;
    if (dp) {
        QoreProgram* pgm = getProgram();
        if (pgm && (pgm->getParseOptions() & PO_NO_FILESYSTEM)) {
            xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the trained data in data directory '%s' "
                "when sandboxing restriction PO_NO_FILESYSTEM is set; use the system data directory instead", dp);
            return;
        }
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
        sandboxed = static_cast<bool>(smh);
    }

    api = std::make_unique<tesseract::TessBaseAPI>();

    int rv;
    try {
        if (sandboxed) {
            // every trained data file is read and checked by ocr_sandbox_file_reader(); a data size of 0 makes the
            // first argument the data directory
            OcrReaderScope reader_scope(xsink);
            rv = api->Init(dp, 0, lang, tesseract::OEM_DEFAULT, nullptr, 0, nullptr, nullptr, false,
                ocr_sandbox_file_reader);
        } else {
            rv = api->Init(dp, lang);
        }
    } catch (const std::exception& e) {
        xsink->raiseException("OCR-INIT-ERROR", "failed to initialize Tesseract with datapath=%s language=%s: %s",
            dp ? dp : "(default)", lang, e.what());
        api.reset();
        return;
    }
    // a trained data file was refused or could not be read
    if (*xsink) {
        api.reset();
        return;
    }
    if (rv != 0) {
        xsink->raiseException("OCR-INIT-ERROR",
            "failed to initialize Tesseract with datapath=%s language=%s",
            dp ? dp : "(default)", lang);
        api.reset();
        return;
    }
    initialized = true;
}

QoreTesseractEngine::~QoreTesseractEngine() {
    if (api) {
        api->End();
    }
}

Pix* QoreTesseractEngine::loadImage(const BinaryNode* data, ExceptionSink* xsink) {
    if (!data || !data->size()) {
        xsink->raiseException("OCR-ERROR", "empty image data");
        return nullptr;
    }

    Pix* pix = pixReadMem(
        static_cast<const l_uint8*>(data->getPtr()),
        static_cast<size_t>(data->size())
    );

    if (!pix) {
        xsink->raiseException("OCR-ERROR",
            "failed to decode image data (%lld bytes); "
            "supported formats: PNG, JPEG, TIFF, BMP, GIF",
            static_cast<long long>(data->size()));
        return nullptr;
    }

    return pix;
}

QoreStringNode* QoreTesseractEngine::extractText(const BinaryNode* data,
        ExceptionSink* xsink) {
    if (!initialized) {
        xsink->raiseException("OCR-ERROR", "TesseractEngine not initialized");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(api_mutex);

    Pix* pix = loadImage(data, xsink);
    if (!pix) {
        return nullptr;
    }

    api->SetImage(pix);

    char* text = api->GetUTF8Text();
    if (!text) {
        pixDestroy(&pix);
        xsink->raiseException("OCR-ERROR", "failed to extract text from image");
        return nullptr;
    }

    QoreStringNode* result = new QoreStringNode(text);
    delete[] text;
    pixDestroy(&pix);
    api->Clear();

    return result;
}

QoreHashNode* QoreTesseractEngine::extractTextWithMetadata(const BinaryNode* data,
        ExceptionSink* xsink) {
    if (!initialized) {
        xsink->raiseException("OCR-ERROR", "TesseractEngine not initialized");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(api_mutex);

    Pix* pix = loadImage(data, xsink);
    if (!pix) {
        return nullptr;
    }

    api->SetImage(pix);

    char* text = api->GetUTF8Text();
    if (!text) {
        pixDestroy(&pix);
        xsink->raiseException("OCR-ERROR", "failed to extract text from image");
        return nullptr;
    }

    int confidence = api->MeanTextConf();

    // Build block-level results
    ReferenceHolder<QoreListNode> blocks(new QoreListNode(autoTypeInfo), xsink);

    tesseract::ResultIterator* ri = api->GetIterator();
    if (ri) {
        tesseract::PageIteratorLevel level = tesseract::RIL_BLOCK;
        while (!ri->Empty(level)) {
            const char* block_text = ri->GetUTF8Text(level);
            if (block_text) {
                ReferenceHolder<QoreHashNode> block(new QoreHashNode(autoTypeInfo), xsink);
                block->setKeyValue("text", new QoreStringNode(block_text), xsink);
                block->setKeyValue("confidence",
                    (double)(ri->Confidence(level) / 100.0), xsink);

                int x1, y1, x2, y2;
                if (ri->BoundingBox(level, &x1, &y1, &x2, &y2)) {
                    ReferenceHolder<QoreHashNode> bbox(new QoreHashNode(autoTypeInfo), xsink);
                    bbox->setKeyValue("x1", x1, xsink);
                    bbox->setKeyValue("y1", y1, xsink);
                    bbox->setKeyValue("x2", x2, xsink);
                    bbox->setKeyValue("y2", y2, xsink);
                    block->setKeyValue("bbox", bbox.release(), xsink);
                }

                blocks->push(block.release(), xsink);
                delete[] block_text;
            }
            ri->Next(level);
        }
        delete ri;
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("text", new QoreStringNode(text), xsink);
    result->setKeyValue("confidence", (double)(confidence / 100.0), xsink);
    result->setKeyValue("blocks", blocks.release(), xsink);

    delete[] text;
    pixDestroy(&pix);
    api->Clear();

    return result.release();
}

QoreHashNode* QoreTesseractEngine::getCapabilities() const {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), &xsink);

    result->setKeyValue("has_tesseract", true, &xsink);

    const char* version = tesseract::TessBaseAPI::Version();
    result->setKeyValue("version", new QoreStringNode(version ? version : "unknown"), &xsink);

    // Get available languages if initialized
    if (initialized && api) {
        std::vector<std::string> langs;
        api->GetAvailableLanguagesAsVector(&langs);
        ReferenceHolder<QoreListNode> lang_list(new QoreListNode(stringTypeInfo), &xsink);
        for (const auto& lang : langs) {
            lang_list->push(new QoreStringNode(lang), &xsink);
        }
        result->setKeyValue("languages", lang_list.release(), &xsink);
    }

    return result.release();
}

} // namespace QoreOcr
