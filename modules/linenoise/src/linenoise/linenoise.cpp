/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Switch to gets() if $TERM is something we can't support.
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - Completion?
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n (1 based)
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (Cursor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * The following are used to clear the screen: ESC [ H ESC [ 2 J
 * This is actually composed of two sequences:
 *
 * cursorhome
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED2 (Clear entire screen)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#ifdef _WIN32

#include <conio.h>
#include <windows.h>
#include <io.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf  // Microsoft headers use underscores in some names
#endif

#if !defined GNUC
#define strcasecmp _stricmp
#endif

#define strdup _strdup
#define isatty _isatty
#define write _write
#define STDIN_FILENO 0

#else /* _WIN32 */

#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <poll.h>
#include <cctype>
#include <wctype.h>

#endif /* _WIN32 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "linenoise.h"
#include "ConvertUTF.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

using std::string;
using std::vector;
using std::unique_ptr;
using namespace linenoise_ng;

typedef unsigned char char8_t;

static ConversionResult copyString8to32(char32_t* dst, size_t dstSize,
                                        size_t& dstCount, const char* src) {
  const UTF8* sourceStart = reinterpret_cast<const UTF8*>(src);
  const UTF8* sourceEnd = sourceStart + strlen(src);
  UTF32* targetStart = reinterpret_cast<UTF32*>(dst);
  UTF32* targetEnd = targetStart + dstSize;

  ConversionResult res = ConvertUTF8toUTF32(
      &sourceStart, sourceEnd, &targetStart, targetEnd, lenientConversion);

  if (res == conversionOK) {
    dstCount = targetStart - reinterpret_cast<UTF32*>(dst);

    if (dstCount < dstSize) {
      *targetStart = 0;
    }
  }

  return res;
}

static ConversionResult copyString8to32(char32_t* dst, size_t dstSize,
                                        size_t& dstCount, const char8_t* src) {
  return copyString8to32(dst, dstSize, dstCount,
                         reinterpret_cast<const char*>(src));
}

static size_t strlen32(const char32_t* str) {
  const char32_t* ptr = str;

  while (*ptr) {
    ++ptr;
  }

  return ptr - str;
}

static size_t strlen8(const char8_t* str) {
  return strlen(reinterpret_cast<const char*>(str));
}

static char8_t* strdup8(const char* src) {
  return reinterpret_cast<char8_t*>(strdup(src));
}

#ifdef _WIN32
static const int FOREGROUND_WHITE =
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
static const int BACKGROUND_WHITE =
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
static const int INTENSITY = FOREGROUND_INTENSITY | BACKGROUND_INTENSITY;

class WinAttributes {
 public:
  WinAttributes() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    _defaultAttribute = info.wAttributes & INTENSITY;
    _defaultColor = info.wAttributes & FOREGROUND_WHITE;
    _defaultBackground = info.wAttributes & BACKGROUND_WHITE;

    _consoleAttribute = _defaultAttribute;
    _consoleColor = _defaultColor | _defaultBackground;
  }

 public:
  int _defaultAttribute;
  int _defaultColor;
  int _defaultBackground;

  int _consoleAttribute;
  int _consoleColor;
};

static WinAttributes WIN_ATTR;

static void copyString32to16(char16_t* dst, size_t dstSize, size_t* dstCount,
                             const char32_t* src, size_t srcSize) {
  const UTF32* sourceStart = reinterpret_cast<const UTF32*>(src);
  const UTF32* sourceEnd = sourceStart + srcSize;
  char16_t* targetStart = reinterpret_cast<char16_t*>(dst);
  char16_t* targetEnd = targetStart + dstSize;

  ConversionResult res = ConvertUTF32toUTF16(
      &sourceStart, sourceEnd, &targetStart, targetEnd, lenientConversion);

  if (res == conversionOK) {
    *dstCount = targetStart - reinterpret_cast<char16_t*>(dst);

    if (*dstCount < dstSize) {
      *targetStart = 0;
    }
  }
}
#endif

static void copyString32to8(char* dst, size_t dstSize, size_t* dstCount,
                            const char32_t* src, size_t srcSize) {
  const UTF32* sourceStart = reinterpret_cast<const UTF32*>(src);
  const UTF32* sourceEnd = sourceStart + srcSize;
  UTF8* targetStart = reinterpret_cast<UTF8*>(dst);
  UTF8* targetEnd = targetStart + dstSize;

  ConversionResult res = ConvertUTF32toUTF8(
      &sourceStart, sourceEnd, &targetStart, targetEnd, lenientConversion);

  if (res == conversionOK) {
    *dstCount = targetStart - reinterpret_cast<UTF8*>(dst);

    if (*dstCount < dstSize) {
      *targetStart = 0;
    }
  }
}

static void copyString32to8(char* dst, size_t dstLen, const char32_t* src) {
  size_t dstCount = 0;
  copyString32to8(dst, dstLen, &dstCount, src, strlen32(src));
}

static void copyString32(char32_t* dst, const char32_t* src, size_t len) {
  while (0 < len && *src) {
    *dst++ = *src++;
    --len;
  }

  *dst = 0;
}

static int strncmp32(const char32_t* left, const char32_t* right, size_t len) {
  while (0 < len && *left) {
    if (*left != *right) {
      return *left - *right;
    }

    ++left;
    ++right;
    --len;
  }

  return 0;
}

#ifdef _WIN32
#include <iostream>

static size_t OutputWin(char16_t* text16, char32_t* text32, size_t len32) {
  size_t count16 = 0;

  copyString32to16(text16, len32, &count16, text32, len32);
  WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text16,
                static_cast<DWORD>(count16), nullptr, nullptr);

  return count16;
}

static char32_t* HandleEsc(char32_t* p, char32_t* end) {
  if (*p == '[') {
    int code = 0;

    for (++p; p < end; ++p) {
      char32_t c = *p;

      if ('0' <= c && c <= '9') {
        code = code * 10 + (c - '0');
      } else if (c == 'm' || c == ';') {
        switch (code) {
          case 0:
            WIN_ATTR._consoleAttribute = WIN_ATTR._defaultAttribute;
            WIN_ATTR._consoleColor =
                WIN_ATTR._defaultColor | WIN_ATTR._defaultBackground;
            break;

          case 1:  // BOLD
          case 5:  // BLINK
            WIN_ATTR._consoleAttribute =
                (WIN_ATTR._defaultAttribute ^ FOREGROUND_INTENSITY) & INTENSITY;
            break;

          case 30:
            WIN_ATTR._consoleColor = BACKGROUND_WHITE;
            break;

          case 31:
            WIN_ATTR._consoleColor =
                FOREGROUND_RED | WIN_ATTR._defaultBackground;
            break;

          case 32:
            WIN_ATTR._consoleColor =
                FOREGROUND_GREEN | WIN_ATTR._defaultBackground;
            break;

          case 33:
            WIN_ATTR._consoleColor =
                FOREGROUND_RED | FOREGROUND_GREEN | WIN_ATTR._defaultBackground;
            break;

          case 34:
            WIN_ATTR._consoleColor =
                FOREGROUND_BLUE | WIN_ATTR._defaultBackground;
            break;

          case 35:
            WIN_ATTR._consoleColor =
                FOREGROUND_BLUE | FOREGROUND_RED | WIN_ATTR._defaultBackground;
            break;

          case 36:
            WIN_ATTR._consoleColor = FOREGROUND_BLUE | FOREGROUND_GREEN |
                                     WIN_ATTR._defaultBackground;
            break;

          case 37:
            WIN_ATTR._consoleColor = FOREGROUND_GREEN | FOREGROUND_RED |
                                     FOREGROUND_BLUE |
                                     WIN_ATTR._defaultBackground;
            break;
        }

        code = 0;
      }

      if (*p == 'm') {
        ++p;
        break;
      }
    }
  } else {
    ++p;
  }

  auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(handle,
                          WIN_ATTR._consoleAttribute | WIN_ATTR._consoleColor);

  return p;
}

static size_t WinWrite32(char16_t* text16, char32_t* text32, size_t len32) {
  char32_t* p = text32;
  char32_t* q = p;
  char32_t* e = text32 + len32;
  size_t count16 = 0;

  while (p < e) {
    if (*p == 27) {
      if (q < p) {
        count16 += OutputWin(text16, q, p - q);
      }

      q = p = HandleEsc(p + 1, e);
    } else {
      ++p;
    }
  }

  if (q < p) {
    count16 += OutputWin(text16, q, p - q);
  }

  return count16;
}
#endif

static int write32(int fd, char32_t* text32, int len32) {
#ifdef _WIN32
  if (isatty(fd)) {
    size_t len16 = 2 * len32 + 1;
    unique_ptr<char16_t[]> text16(new char16_t[len16]);
    size_t count16 = WinWrite32(text16.get(), text32, len32);

    return static_cast<int>(count16);
  } else {
    size_t len8 = 4 * len32 + 1;
    unique_ptr<char[]> text8(new char[len8]);
    size_t count8 = 0;

    copyString32to8(text8.get(), len8, &count8, text32, len32);

    return write(fd, text8.get(), static_cast<unsigned int>(count8));
  }
#else
  size_t len8 = 4 * len32 + 1;
  unique_ptr<char[]> text8(new char[len8]);
  size_t count8 = 0;

  copyString32to8(text8.get(), len8, &count8, text32, len32);

  return write(fd, text8.get(), count8);
#endif
}

class Utf32String {
 public:
  Utf32String() : _length(0), _data(nullptr) {
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[1]();
  }

  explicit Utf32String(const char* src) : _length(0), _data(nullptr) {
    size_t len = strlen(src);
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[len + 1]();
    copyString8to32(_data, len + 1, _length, src);
  }

  explicit Utf32String(const char8_t* src) : _length(0), _data(nullptr) {
    size_t len = strlen(reinterpret_cast<const char*>(src));
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[len + 1]();
    copyString8to32(_data, len + 1, _length, src);
  }

  explicit Utf32String(const char32_t* src) : _length(0), _data(nullptr) {
    for (_length = 0; src[_length] != 0; ++_length) {
    }

    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[_length + 1]();
    memcpy(_data, src, _length * sizeof(char32_t));
  }

  explicit Utf32String(const char32_t* src, int len) : _length(len), _data(nullptr) {
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[len + 1]();
    memcpy(_data, src, len * sizeof(char32_t));
  }

  explicit Utf32String(int len) : _length(0), _data(nullptr) {
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[len]();
  }

  explicit Utf32String(const Utf32String& that) : _length(that._length), _data(nullptr) {
    // note: parens intentional, _data must be properly initialized
    _data = new char32_t[_length + 1]();
    memcpy(_data, that._data, sizeof(char32_t) * _length);
  }

  Utf32String& operator=(const Utf32String& that) {
    if (this != &that) {
      delete[] _data;
      _data = new char32_t[that._length]();
      _length = that._length;
      memcpy(_data, that._data, sizeof(char32_t) * _length);
    }

    return *this;
  }

  ~Utf32String() { delete[] _data; }

 public:
  char32_t* get() const { return _data; }

  size_t length() const { return _length; }

  size_t chars() const { return _length; }

  void initFromBuffer() {
    for (_length = 0; _data[_length] != 0; ++_length) {
    }
  }

  const char32_t& operator[](size_t pos) const { return _data[pos]; }

  char32_t& operator[](size_t pos) { return _data[pos]; }

 private:
  size_t _length;
  char32_t* _data;
};

class Utf8String {
  Utf8String(const Utf8String&) = delete;
  Utf8String& operator=(const Utf8String&) = delete;

 public:
  explicit Utf8String(const Utf32String& src) {
    size_t len = src.length() * 4 + 1;
    _data = new char[len];
    copyString32to8(_data, len, src.get());
  }

  ~Utf8String() { delete[] _data; }

 public:
  char* get() const { return _data; }

 private:
  char* _data;
};

struct linenoiseCompletions {
  vector<Utf32String> completionStrings;
};

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096

// make control-characters more readable
#define ctrlChar(upperCaseASCII) (upperCaseASCII - 0x40)

/**
 * Recompute widths of all characters in a char32_t buffer
 * @param text          input buffer of Unicode characters
 * @param widths        output buffer of character widths
 * @param charCount     number of characters in buffer
 */
namespace linenoise_ng {
int mk_wcwidth(char32_t ucs);
}

static void recomputeCharacterWidths(const char32_t* text, char* widths,
                                     int charCount) {
  for (int i = 0; i < charCount; ++i) {
    widths[i] = mk_wcwidth(text[i]);
  }
}

/**
 * Calculate a new screen position given a starting position, screen width and
 * character count
 * @param x             initial x position (zero-based)
 * @param y             initial y position (zero-based)
 * @param screenColumns screen column count
 * @param charCount     character positions to advance
 * @param xOut          returned x position (zero-based)
 * @param yOut          returned y position (zero-based)
 */
static void calculateScreenPosition(int x, int y, int screenColumns,
                                    int charCount, int& xOut, int& yOut) {
  xOut = x;
  yOut = y;
  int charsRemaining = charCount;
  while (charsRemaining > 0) {
    int charsThisRow = (x + charsRemaining < screenColumns) ? charsRemaining
                                                            : screenColumns - x;
    xOut = x + charsThisRow;
    yOut = y;
    charsRemaining -= charsThisRow;
    x = 0;
    ++y;
  }
  if (xOut == screenColumns) {  // we have to special-case line wrap
    xOut = 0;
    ++yOut;
  }
}

/**
 * Calculate a column width using mk_wcswidth()
 * @param buf32  text to calculate
 * @param len    length of text to calculate
 */
namespace linenoise_ng {
int mk_wcswidth(const char32_t* pwcs, size_t n);
}

static int calculateColumnPosition(char32_t* buf32, int len) {
  int width = mk_wcswidth(reinterpret_cast<const char32_t*>(buf32), len);
  if (width == -1)
    return len;
  else
    return width;
}

static bool isControlChar(char32_t testChar) {
  return (testChar < ' ') ||                      // C0 controls
         (testChar >= 0x7F && testChar <= 0x9F);  // DEL and C1 controls
}

/**
 * Check if a char32_t character is one of the specified ASCII characters
 * Unlike strchr, this properly handles char32_t and won't match Unicode
 * characters whose low byte happens to match
 * @param chars  ASCII characters to check against (null-terminated)
 * @param c      character to test
 * @return       true if c is one of the ASCII characters in chars
 */
static bool isCharInString(const char* chars, char32_t c) {
  // Only match ASCII characters (< 128)
  if (c >= 128) return false;
  while (*chars) {
    if (static_cast<char32_t>(*chars) == c) return true;
    ++chars;
  }
  return false;
}

struct PromptBase {            // a convenience struct for grouping prompt info
  Utf32String promptText;      // our copy of the prompt text, edited
  char* promptCharWidths;      // character widths from mk_wcwidth()
  int promptChars;             // chars in promptText
  int promptBytes;             // bytes in promptText
  int promptExtraLines;        // extra lines (beyond 1) occupied by prompt
  int promptIndentation;       // column offset to end of prompt
  int promptLastLinePosition;  // index into promptText where last line begins
  int promptPreviousInputLen;  // promptChars of previous input line, for
                               // clearing
  int promptCursorRowOffset;   // where the cursor is relative to the start of
                               // the prompt
  int promptScreenColumns;     // width of screen in columns
  int promptPreviousLen;       // help erasing
  int promptErrorCode;         // error code (invalid UTF-8) or zero

  PromptBase() : promptPreviousInputLen(0) {}

  bool write() {
    if (write32(1, promptText.get(), promptBytes) == -1) return false;

    return true;
  }
};

struct PromptInfo : public PromptBase {
  PromptInfo(const char* textPtr, int columns) {
    promptExtraLines = 0;
    promptLastLinePosition = 0;
    promptPreviousLen = 0;
    promptScreenColumns = columns;
    Utf32String tempUnicode(textPtr);

    // strip control characters from the prompt -- we do allow newline
    char32_t* pIn = tempUnicode.get();
    char32_t* pOut = pIn;

    int len = 0;
    int x = 0;

    bool const strip = (isatty(1) == 0);

    while (*pIn) {
      char32_t c = *pIn;
      if ('\n' == c || !isControlChar(c)) {
        *pOut = c;
        ++pOut;
        ++pIn;
        ++len;
        if ('\n' == c || ++x >= promptScreenColumns) {
          x = 0;
          ++promptExtraLines;
          promptLastLinePosition = len;
        }
      } else if (c == '\x1b') {
        if (strip) {
          // jump over control chars
          ++pIn;
          if (*pIn == '[') {
            ++pIn;
            while (*pIn && ((*pIn == ';') || ((*pIn >= '0' && *pIn <= '9')))) {
              ++pIn;
            }
            if (*pIn == 'm') {
              ++pIn;
            }
          }
        } else {
          // copy control chars
          *pOut = *pIn;
          ++pOut;
          ++pIn;
          if (*pIn == '[') {
            *pOut = *pIn;
            ++pOut;
            ++pIn;
            while (*pIn && ((*pIn == ';') || ((*pIn >= '0' && *pIn <= '9')))) {
              *pOut = *pIn;
              ++pOut;
              ++pIn;
            }
            if (*pIn == 'm') {
              *pOut = *pIn;
              ++pOut;
              ++pIn;
            }
          }
        }
      } else {
        ++pIn;
      }
    }
    *pOut = 0;
    promptChars = len;
    promptBytes = static_cast<int>(pOut - tempUnicode.get());
    promptText = tempUnicode;

    promptIndentation = len - promptLastLinePosition;
    promptCursorRowOffset = promptExtraLines;
  }
};

// Used with DynamicPrompt (history search)
//
static const Utf32String forwardSearchBasePrompt("(i-search)`");
static const Utf32String reverseSearchBasePrompt("(reverse-i-search)`");
static const Utf32String endSearchBasePrompt("': ");
static Utf32String
    previousSearchText;  // remembered across invocations of linenoise()

// changing prompt for "(reverse-i-search)`text':" etc.
//
struct DynamicPrompt : public PromptBase {
  Utf32String searchText;  // text we are searching for
  char* searchCharWidths;  // character widths from mk_wcwidth()
  int searchTextLen;       // chars in searchText
  int direction;           // current search direction, 1=forward, -1=reverse

  DynamicPrompt(PromptBase& pi, int initialDirection)
      : searchTextLen(0), direction(initialDirection) {
    promptScreenColumns = pi.promptScreenColumns;
    promptCursorRowOffset = 0;
    Utf32String emptyString(1);
    searchText = emptyString;
    const Utf32String* basePrompt =
        (direction > 0) ? &forwardSearchBasePrompt : &reverseSearchBasePrompt;
    size_t promptStartLength = basePrompt->length();
    promptChars =
        static_cast<int>(promptStartLength + endSearchBasePrompt.length());
    promptBytes = promptChars;
    promptLastLinePosition = promptChars;  // TODO fix this, we are asssuming
                                           // that the history prompt won't wrap
                                           // (!)
    promptPreviousLen = promptChars;
    Utf32String tempUnicode(promptChars + 1);
    memcpy(tempUnicode.get(), basePrompt->get(),
           sizeof(char32_t) * promptStartLength);
    memcpy(&tempUnicode[promptStartLength], endSearchBasePrompt.get(),
           sizeof(char32_t) * (endSearchBasePrompt.length() + 1));
    tempUnicode.initFromBuffer();
    promptText = tempUnicode;
    calculateScreenPosition(0, 0, pi.promptScreenColumns, promptChars,
                            promptIndentation, promptExtraLines);
  }

  void updateSearchPrompt(void) {
    const Utf32String* basePrompt =
        (direction > 0) ? &forwardSearchBasePrompt : &reverseSearchBasePrompt;
    size_t promptStartLength = basePrompt->length();
    promptChars = static_cast<int>(promptStartLength + searchTextLen +
                                   endSearchBasePrompt.length());
    promptBytes = promptChars;
    Utf32String tempUnicode(promptChars + 1);
    memcpy(tempUnicode.get(), basePrompt->get(),
           sizeof(char32_t) * promptStartLength);
    memcpy(&tempUnicode[promptStartLength], searchText.get(),
           sizeof(char32_t) * searchTextLen);
    size_t endIndex = promptStartLength + searchTextLen;
    memcpy(&tempUnicode[endIndex], endSearchBasePrompt.get(),
           sizeof(char32_t) * (endSearchBasePrompt.length() + 1));
    tempUnicode.initFromBuffer();
    promptText = tempUnicode;
  }

  void updateSearchText(const char32_t* textPtr) {
    Utf32String tempUnicode(textPtr);
    searchTextLen = static_cast<int>(tempUnicode.chars());
    searchText = tempUnicode;
    updateSearchPrompt();
  }
};

class KillRing {
  static const int capacity = 10;
  int size;
  int index;
  char indexToSlot[10];
  vector<Utf32String> theRing;

 public:
  enum action { actionOther, actionKill, actionYank };
  action lastAction;
  size_t lastYankSize;

  KillRing() : size(0), index(0), lastAction(actionOther) {
    theRing.reserve(capacity);
  }

  void kill(const char32_t* text, int textLen, bool forward) {
    if (textLen == 0) {
      return;
    }
    Utf32String killedText(text, textLen);
    if (lastAction == actionKill && size > 0) {
      int slot = indexToSlot[0];
      int currentLen = static_cast<int>(theRing[slot].length());
      int resultLen = currentLen + textLen;
      Utf32String temp(resultLen + 1);
      if (forward) {
        memcpy(temp.get(), theRing[slot].get(), currentLen * sizeof(char32_t));
        memcpy(&temp[currentLen], killedText.get(), textLen * sizeof(char32_t));
      } else {
        memcpy(temp.get(), killedText.get(), textLen * sizeof(char32_t));
        memcpy(&temp[textLen], theRing[slot].get(),
               currentLen * sizeof(char32_t));
      }
      temp[resultLen] = 0;
      temp.initFromBuffer();
      theRing[slot] = temp;
    } else {
      if (size < capacity) {
        if (size > 0) {
          memmove(&indexToSlot[1], &indexToSlot[0], size);
        }
        indexToSlot[0] = size;
        size++;
        theRing.push_back(killedText);
      } else {
        int slot = indexToSlot[capacity - 1];
        theRing[slot] = killedText;
        memmove(&indexToSlot[1], &indexToSlot[0], capacity - 1);
        indexToSlot[0] = slot;
      }
      index = 0;
    }
  }

  Utf32String* yank() { return (size > 0) ? &theRing[indexToSlot[index]] : 0; }

  Utf32String* yankPop() {
    if (size == 0) {
      return 0;
    }
    ++index;
    if (index == size) {
      index = 0;
    }
    return &theRing[indexToSlot[index]];
  }
};

// Undo/redo system: saves buffer state before destructive operations
struct UndoState {
  Utf32String text;
  int pos;
  UndoState() : pos(0) {}
  UndoState(const char32_t* buf, int len, int p) : text(buf, len), pos(p) {}
};

class UndoStack {
  std::vector<UndoState> undoEntries;
  std::vector<UndoState> redoEntries;
  static const size_t MAX_ENTRIES = 100;
  int lastSaveLen;  // track last saved length for coalescing consecutive inserts
  int lastSavePos;
  bool lastWasInsert;

 public:
  UndoStack() : lastSaveLen(0), lastSavePos(0), lastWasInsert(false) {}

  void save(const char32_t* buf, int len, int pos, bool isInsert = false) {
    // Coalesce consecutive character insertions
    if (isInsert && lastWasInsert && !undoEntries.empty()) {
      lastSaveLen = len;
      lastSavePos = pos;
      return;
    }
    lastWasInsert = isInsert;
    lastSaveLen = len;
    lastSavePos = pos;
    if (undoEntries.size() >= MAX_ENTRIES) {
      undoEntries.erase(undoEntries.begin());
    }
    undoEntries.push_back(UndoState(buf, len, pos));
    redoEntries.clear();
  }

  void breakCoalescing() {
    lastWasInsert = false;
  }

  bool undo(char32_t* buf, int buflen, int& len, int& pos) {
    if (undoEntries.empty()) return false;
    // Save current state to redo stack
    redoEntries.push_back(UndoState(buf, len, pos));
    // Restore from undo stack
    UndoState& state = undoEntries.back();
    int restoreLen = static_cast<int>(state.text.length());
    if (restoreLen > buflen) restoreLen = buflen;
    memcpy(buf, state.text.get(), sizeof(char32_t) * restoreLen);
    buf[restoreLen] = '\0';
    len = restoreLen;
    pos = state.pos;
    if (pos > len) pos = len;
    undoEntries.pop_back();
    lastWasInsert = false;
    return true;
  }

  bool redo(char32_t* buf, int buflen, int& len, int& pos) {
    if (redoEntries.empty()) return false;
    // Save current state to undo stack
    undoEntries.push_back(UndoState(buf, len, pos));
    // Restore from redo stack
    UndoState& state = redoEntries.back();
    int restoreLen = static_cast<int>(state.text.length());
    if (restoreLen > buflen) restoreLen = buflen;
    memcpy(buf, state.text.get(), sizeof(char32_t) * restoreLen);
    buf[restoreLen] = '\0';
    len = restoreLen;
    pos = state.pos;
    if (pos > len) pos = len;
    redoEntries.pop_back();
    lastWasInsert = false;
    return true;
  }

  void clear() {
    undoEntries.clear();
    redoEntries.clear();
    lastSaveLen = 0;
    lastSavePos = 0;
    lastWasInsert = false;
  }
};

static UndoStack undoStack;

class InputBuffer {
  char32_t* buf32;   // input buffer
  char* charWidths;  // character widths from mk_wcwidth()
  int buflen;        // buffer size in characters
  int len;           // length of text in input buffer
  int pos;           // character position in buffer ( 0 <= pos <= len )
  int terminatingKeystroke;  // used by history search to pass back terminating key

  void clearScreen(PromptBase& pi);
  int incrementalHistorySearch(PromptBase& pi, int startChar);
  int completeLine(PromptBase& pi);
  void refreshLine(PromptBase& pi);

  // Key handler methods for dispatch table
  // Return: 0 = continue editing, 1 = accept line, -1 = abort/error
  int handleTimeout(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveToStart(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveLeft(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveWordLeft(PromptBase& pi, KillRing& killRing, int c);
  int handleAbort(PromptBase& pi, KillRing& killRing, int c);
  int handleCapitalizeWord(PromptBase& pi, KillRing& killRing, int c);
  int handleDeleteOrExit(PromptBase& pi, KillRing& killRing, int c);
  int handleKillWordRight(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveToEnd(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveRight(PromptBase& pi, KillRing& killRing, int c);
  int handleMoveWordRight(PromptBase& pi, KillRing& killRing, int c);
  int handleBackspace(PromptBase& pi, KillRing& killRing, int c);
  int handleKillWordLeft(PromptBase& pi, KillRing& killRing, int c);
  int handleAcceptLine(PromptBase& pi, KillRing& killRing, int c);
  int handleKillToEnd(PromptBase& pi, KillRing& killRing, int c);
  int handleClearScreenCmd(PromptBase& pi, KillRing& killRing, int c);
  int handleLowercaseWord(PromptBase& pi, KillRing& killRing, int c);
  int handleHistoryNavigate(PromptBase& pi, KillRing& killRing, int c);
  int handleHistorySearch(PromptBase& pi, KillRing& killRing, int c);
  int handleTranspose(PromptBase& pi, KillRing& killRing, int c);
  int handleKillToStart(PromptBase& pi, KillRing& killRing, int c);
  int handleUppercaseWord(PromptBase& pi, KillRing& killRing, int c);
  int handleKillToWhitespace(PromptBase& pi, KillRing& killRing, int c);
  int handleYank(PromptBase& pi, KillRing& killRing, int c);
  int handleYankPop(PromptBase& pi, KillRing& killRing, int c);
  int handleUndo(PromptBase& pi, KillRing& killRing, int c);
  int handleSuspend(PromptBase& pi, KillRing& killRing, int c);
  int handleDelete(PromptBase& pi, KillRing& killRing, int c);
  int handleInsertToggle(PromptBase& pi, KillRing& killRing, int c);
  int handleBracketedPaste(PromptBase& pi, KillRing& killRing, int c);
  int handleHistoryJump(PromptBase& pi, KillRing& killRing, int c);
  int handleMacro(PromptBase& pi, KillRing& killRing, int c);

 public:
  InputBuffer(char32_t* buffer, char* widthArray, int bufferLen)
      : buf32(buffer),
        charWidths(widthArray),
        buflen(bufferLen - 1),
        len(0),
        pos(0),
        terminatingKeystroke(-1) {
    buf32[0] = 0;
  }
  void preloadBuffer(const char* preloadText) {
    size_t ucharCount = 0;
    copyString8to32(buf32, buflen + 1, ucharCount, preloadText);
    recomputeCharacterWidths(buf32, charWidths, static_cast<int>(ucharCount));
    len = static_cast<int>(ucharCount);
    pos = static_cast<int>(ucharCount);
  }
  bool tryAutoDedent(char32_t c);
  int getInputLine(PromptBase& pi);
  int length(void) const { return len; }

  friend void initDefaultBindings();
};

// Special codes for keyboard input:
//
// Between Windows and the various Linux "terminal" programs, there is some
// pretty diverse behavior in the "scan codes" and escape sequences we are
// presented with.  So ... we'll translate them all into our own pidgin
// pseudocode, trying to stay out of the way of UTF-8 and international
// characters.  Here's the general plan.
//
// "User input keystrokes" (key chords, whatever) will be encoded as a single
// value.
// The low 21 bits are reserved for Unicode characters.  Popular function-type
// keys
// get their own codes in the range 0x10200000 to (if needed) 0x1FE00000,
// currently
// just arrow keys, Home, End and Delete.  Keypresses with Ctrl get ORed with
// 0x20000000, with Alt get ORed with 0x40000000.  So, Ctrl+Alt+Home is encoded
// as 0x20000000 + 0x40000000 + 0x10A00000 == 0x70A00000.  To keep things
// complicated,
// the Alt key is equivalent to prefixing the keystroke with ESC, so ESC
// followed by
// D is treated the same as Alt + D ... we'll just use Emacs terminology and
// call
// this "Meta".  So, we will encode both ESC followed by D and Alt held down
// while D
// is pressed the same, as Meta-D, encoded as 0x40000064.
//
// Here are the definitions of our component constants:
//
// Maximum unsigned 32-bit value    = 0xFFFFFFFF;   // For reference, max 32-bit
// value
// Highest allocated Unicode char   = 0x001FFFFF;   // For reference, max
// Unicode value
static const int META = 0x40000000;  // Meta key combination
static const int CTRL = 0x20000000;  // Ctrl key combination
// static const int SPECIAL_KEY = 0x10000000;   // Common bit for all special
// keys
static const int UP_ARROW_KEY = 0x10200000;  // Special keys
static const int DOWN_ARROW_KEY = 0x10400000;
static const int RIGHT_ARROW_KEY = 0x10600000;
static const int LEFT_ARROW_KEY = 0x10800000;
static const int HOME_KEY = 0x10A00000;
static const int END_KEY = 0x10C00000;
static const int DELETE_KEY = 0x10E00000;
static const int PAGE_UP_KEY = 0x11000000;
static const int PAGE_DOWN_KEY = 0x11200000;
static const int INSERT_KEY = 0x11400000;
static const int BRACKETED_PASTE_START = 0x11600000;
static const int TIMEOUT_KEY = 0x11800000;

static const char* unsupported_term[] = {"dumb", "cons25", "emacs", NULL};
static linenoiseCompletionCallback* completionCallback = NULL;
static linenoiseHintsCallback* hintsCallback = NULL;
static linenoiseFreeHintsCallback* freeHintsCallback = NULL;
static linenoiseSyntaxCallback* syntaxCallback = NULL;
static std::string rightPromptText;
static bool maskMode = false;
static bool insertMode = true;
static bool menuCompleteEnabled = false;
static bool completionCaseInsensitive = false;
static bool filenameCompletionEnabled = false;
static int readTimeoutMs = 0;

// Keyboard macro recorder
class MacroRecorder {
  std::vector<int> recordBuffer;
  bool recording;

 public:
  MacroRecorder() : recording(false) {}

  void startRecording() {
    recording = true;
    recordBuffer.clear();
  }

  void stopRecording() {
    recording = false;
  }

  void addKeystroke(int c) {
    if (recording) {
      recordBuffer.push_back(c);
    }
  }

  bool isRecording() const { return recording; }

  const std::vector<int>& getMacro() const { return recordBuffer; }
};

static MacroRecorder macroRecorder;
static std::vector<int> pendingKeystrokes;

// Menu-complete state
static bool menuCompleteActive = false;
static int menuCompleteIndex = 0;
static int menuCompleteStartIndex = 0;
static int menuCompleteOrigLen = 0;
static int menuCompleteOrigPos = 0;
static Utf32String menuCompleteOrigText;  // saved original text for cycling

#ifdef _WIN32
static HANDLE console_in, console_out;
static DWORD oldMode;
static WORD oldDisplayAttribute;
#else
static struct termios orig_termios; /* in order to restore at exit */
#endif

static KillRing killRing;

static int rawmode = 0; /* for atexit() function to check if restore is needed*/
static int rawModeRefCount = 0; /* async session reference count for raw mode */
static int atexit_registered = 0; /* register atexit just 1 time */
static int historyMaxLen = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int historyLen = 0;
static int historyIndex = 0;
static char8_t** history = NULL;

// used to emulate Windows command prompt on down-arrow after a recall
// we use -2 as our "not set" value because we add 1 to the previous index on
// down-arrow,
// and zero is a valid index (so -1 is a valid "previous index")
static int historyPreviousIndex = -2;
static bool historyRecallMostRecent = false;

// History provider callback state
static linenoiseHistoryProviderCallback historyPrevCallback = NULL;
static linenoiseHistoryProviderCallback historyNextCallback = NULL;
static linenoiseHistoryResetCallback historyResetCallback = NULL;
static linenoiseHistorySearchCallback historySearchCallback = NULL;
static void* historyProviderUserData = NULL;
static std::string providerSavedLine;
static bool providerNavigating = false;

static inline bool hasHistoryProvider() {
    return historyPrevCallback != NULL && historyNextCallback != NULL;
}

static void linenoiseAtExit(void);

static bool isUnsupportedTerm(void) {
  char* term = getenv("TERM");
  if (term == NULL) return false;
  for (int j = 0; unsupported_term[j]; ++j)
    if (!strcasecmp(term, unsupported_term[j])) {
      return true;
    }
  return false;
}

static void beep() {
  fprintf(stderr, "\x7");  // ctrl-G == bell/beep
  fflush(stderr);
}

void linenoiseHistoryFree(void) {
  if (history) {
    for (int j = 0; j < historyLen; ++j) free(history[j]);
    historyLen = 0;
    free(history);
    history = 0;
  }
}

static int enableRawMode(void) {
#ifdef _WIN32
  if (!console_in) {
    console_in = GetStdHandle(STD_INPUT_HANDLE);
    console_out = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(console_in, &oldMode);
    SetConsoleMode(console_in, oldMode &
                                   ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                     ENABLE_PROCESSED_INPUT));
  }
  return 0;
#else
  struct termios raw;

  if (!isatty(STDIN_FILENO)) goto fatal;
  if (!atexit_registered) {
    atexit(linenoiseAtExit);
    atexit_registered = 1;
  }
  if (tcgetattr(0, &orig_termios) == -1) goto fatal;

  raw = orig_termios; /* modify the original mode */
  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes - disable post processing */
  // this is wrong, we don't want raw output, it turns newlines into straight
  // linefeeds
  // raw.c_oflag &= ~(OPOST);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - echoing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer.
   * We want read to return every single byte, without timeout. */
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

  /* put terminal in raw mode after flushing */
  if (tcsetattr(0, TCSADRAIN, &raw) < 0) goto fatal;
  rawmode = 1;
  // enable bracketed paste mode
  if (write(1, "\x1b[?2004h", 8) == -1) { /* ignore */ }
  return 0;

fatal:
  errno = ENOTTY;
  return -1;
#endif
}

static void disableRawMode(void) {
#ifdef _WIN32
  SetConsoleMode(console_in, oldMode);
  console_in = 0;
  console_out = 0;
#else
  if (rawmode) {
    // disable bracketed paste mode and reset cursor shape before restoring terminal
    if (write(1, "\x1b[?2004l\x1b[0 q", 13) == -1) { /* ignore */ }
    if (tcsetattr(0, TCSADRAIN, &orig_termios) != -1) rawmode = 0;
  }
#endif
}

// At exit we'll try to fix the terminal to the initial conditions
static void linenoiseAtExit(void) { disableRawMode(); }

static int getScreenColumns(void) {
  int cols;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO inf;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &inf);
  cols = inf.dwSize.X;
#else
  struct winsize ws;
  cols = (ioctl(1, TIOCGWINSZ, &ws) == -1) ? 80 : ws.ws_col;
#endif
  // cols is 0 in certain circumstances like inside debugger, which creates
  // further issues
  return (cols > 0) ? cols : 80;
}

static int getScreenRows(void) {
  int rows;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO inf;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &inf);
  rows = 1 + inf.srWindow.Bottom - inf.srWindow.Top;
#else
  struct winsize ws;
  rows = (ioctl(1, TIOCGWINSZ, &ws) == -1) ? 24 : ws.ws_row;
#endif
  return (rows > 0) ? rows : 24;
}

static void setDisplayAttribute(bool enhancedDisplay, bool error) {
#ifdef _WIN32
  if (enhancedDisplay) {
    CONSOLE_SCREEN_BUFFER_INFO inf;
    GetConsoleScreenBufferInfo(console_out, &inf);
    oldDisplayAttribute = inf.wAttributes;
    BYTE oldLowByte = oldDisplayAttribute & 0xFF;
    BYTE newLowByte;
    switch (oldLowByte) {
      case 0x07:
        // newLowByte = FOREGROUND_BLUE | FOREGROUND_INTENSITY;  // too dim
        // newLowByte = FOREGROUND_BLUE;                         // even dimmer
        newLowByte = FOREGROUND_BLUE |
                     FOREGROUND_GREEN;  // most similar to xterm appearance
        break;
      case 0x70:
        newLowByte = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        break;
      default:
        newLowByte = oldLowByte ^ 0xFF;  // default to inverse video
        break;
    }
    inf.wAttributes = (inf.wAttributes & 0xFF00) | newLowByte;
    SetConsoleTextAttribute(console_out, inf.wAttributes);
  } else {
    SetConsoleTextAttribute(console_out, oldDisplayAttribute);
  }
#else
  if (enhancedDisplay) {
    char const* p = (error ? "\x1b[1;31m" : "\x1b[1;34m");
    if (write(1, p, 7) == -1)
      return; /* bright blue (visible with both B&W bg) */
  } else {
    if (write(1, "\x1b[0m", 4) == -1) return; /* reset */
  }
#endif
}

/**
 * Display the dynamic incremental search prompt and the current user input
 * line.
 * @param pi   PromptBase struct holding information about the prompt and our
 * screen position
 * @param buf32  input buffer to be displayed
 * @param len  count of characters in the buffer
 * @param pos  current cursor position within the buffer (0 <= pos <= len)
 */
static void dynamicRefresh(PromptBase& pi, char32_t* buf32, int len, int pos) {
  // calculate the position of the end of the prompt
  int xEndOfPrompt, yEndOfPrompt;
  calculateScreenPosition(0, 0, pi.promptScreenColumns, pi.promptChars,
                          xEndOfPrompt, yEndOfPrompt);
  pi.promptIndentation = xEndOfPrompt;

  // calculate the position of the end of the input line
  int xEndOfInput, yEndOfInput;
  calculateScreenPosition(xEndOfPrompt, yEndOfPrompt, pi.promptScreenColumns,
                          calculateColumnPosition(buf32, len), xEndOfInput,
                          yEndOfInput);

  // calculate the desired position of the cursor
  int xCursorPos, yCursorPos;
  calculateScreenPosition(xEndOfPrompt, yEndOfPrompt, pi.promptScreenColumns,
                          calculateColumnPosition(buf32, pos), xCursorPos,
                          yCursorPos);

#ifdef _WIN32
  // position at the start of the prompt, clear to end of previous input
  CONSOLE_SCREEN_BUFFER_INFO inf;
  GetConsoleScreenBufferInfo(console_out, &inf);
  inf.dwCursorPosition.X = 0;
  inf.dwCursorPosition.Y -= pi.promptCursorRowOffset /*- pi.promptExtraLines*/;
  SetConsoleCursorPosition(console_out, inf.dwCursorPosition);
  DWORD count;
  FillConsoleOutputCharacterA(console_out, ' ',
                              pi.promptPreviousLen + pi.promptPreviousInputLen,
                              inf.dwCursorPosition, &count);
  pi.promptPreviousLen = pi.promptIndentation;
  pi.promptPreviousInputLen = len;

  // display the prompt
  if (!pi.write()) return;

  // display the input line
  if (write32(1, buf32, len) == -1) return;

  // position the cursor
  GetConsoleScreenBufferInfo(console_out, &inf);
  inf.dwCursorPosition.X = xCursorPos;  // 0-based on Win32
  inf.dwCursorPosition.Y -= yEndOfInput - yCursorPos;
  SetConsoleCursorPosition(console_out, inf.dwCursorPosition);
#else  // _WIN32
  char seq[64];
  int cursorRowMovement = pi.promptCursorRowOffset - pi.promptExtraLines;
  if (cursorRowMovement > 0) {  // move the cursor up as required
    snprintf(seq, sizeof seq, "\x1b[%dA", cursorRowMovement);
    if (write(1, seq, strlen(seq)) == -1) return;
  }
  // position at the start of the prompt, clear to end of screen
  snprintf(seq, sizeof seq, "\x1b[1G\x1b[J");  // 1-based on VT100
  if (write(1, seq, strlen(seq)) == -1) return;

  // display the prompt
  if (!pi.write()) return;

  // display the input line
  if (write32(1, buf32, len) == -1) return;

  // we have to generate our own newline on line wrap
  if (xEndOfInput == 0 && yEndOfInput > 0)
    if (write(1, "\n", 1) == -1) return;

  // position the cursor
  cursorRowMovement = yEndOfInput - yCursorPos;
  if (cursorRowMovement > 0) {  // move the cursor up as required
    snprintf(seq, sizeof seq, "\x1b[%dA", cursorRowMovement);
    if (write(1, seq, strlen(seq)) == -1) return;
  }
  // position the cursor within the line
  snprintf(seq, sizeof seq, "\x1b[%dG", xCursorPos + 1);  // 1-based on VT100
  if (write(1, seq, strlen(seq)) == -1) return;
#endif

  pi.promptCursorRowOffset =
      pi.promptExtraLines + yCursorPos;  // remember row for next pass
}

/**
 * Refresh the user's input line: the prompt is already onscreen and is not
 * redrawn here
 * @param pi   PromptBase struct holding information about the prompt and our
 * screen position
 */
void InputBuffer::refreshLine(PromptBase& pi) {
  // check for a matching brace/bracket/paren, remember its position if found
  int highlight = -1;
  bool indicateError = false;
  if (!maskMode && pos < len) {
    /* this scans for a brace matching buf32[pos] to highlight */
    unsigned char part1, part2;
    int scanDirection = 0;
    if (isCharInString("}])", buf32[pos])) {
      scanDirection = -1; /* backwards */
      if (buf32[pos] == '}') {
        part1 = '}'; part2 = '{';
      } else if (buf32[pos] == ']') {
        part1 = ']'; part2 = '[';
      } else {
        part1 = ')'; part2 = '(';
      }
    }
    else if (isCharInString("{[(", buf32[pos])) {
      scanDirection = 1; /* forwards */
      if (buf32[pos] == '{') {
        //part1 = '{'; part2 = '}';
        part1 = '}'; part2 = '{';
      } else if (buf32[pos] == '[') {
        //part1 = '['; part2 = ']';
        part1 = ']'; part2 = '[';
      } else {
        //part1 = '('; part2 = ')';
        part1 = ')'; part2 = '(';
      }
    }

    if (scanDirection) {
      int unmatched = scanDirection;
      int unmatchedOther = 0;
      for (int i = pos + scanDirection; i >= 0 && i < len; i += scanDirection) {
        /* TODO: the right thing when inside a string */
        if (isCharInString("}])", buf32[i])) {
          if (buf32[i] == part1) {
            --unmatched;
          } else {
            --unmatchedOther;
          }
        } else if (isCharInString("{[(", buf32[i])) {
          if (buf32[i] == part2) {
            ++unmatched;
          } else {
            ++unmatchedOther;
          }
        }
        if (unmatched == 0) {
          highlight = i;
          indicateError = (unmatchedOther != 0);
          break;
        }
      }
    }
  }

  // calculate the position of the end of the input line
  int xEndOfInput, yEndOfInput;
  calculateScreenPosition(pi.promptIndentation, 0, pi.promptScreenColumns,
                          calculateColumnPosition(buf32, len), xEndOfInput,
                          yEndOfInput);

  // calculate the desired position of the cursor
  int xCursorPos, yCursorPos;
  calculateScreenPosition(pi.promptIndentation, 0, pi.promptScreenColumns,
                          calculateColumnPosition(buf32, pos), xCursorPos,
                          yCursorPos);

#ifdef _WIN32
  // position at the end of the prompt, clear to end of previous input
  CONSOLE_SCREEN_BUFFER_INFO inf;
  GetConsoleScreenBufferInfo(console_out, &inf);
  inf.dwCursorPosition.X = pi.promptIndentation;  // 0-based on Win32
  inf.dwCursorPosition.Y -= pi.promptCursorRowOffset - pi.promptExtraLines;
  SetConsoleCursorPosition(console_out, inf.dwCursorPosition);
  DWORD count;
  if (len < pi.promptPreviousInputLen)
    FillConsoleOutputCharacterA(console_out, ' ', pi.promptPreviousInputLen,
                                inf.dwCursorPosition, &count);
  pi.promptPreviousInputLen = len;

  // display the input line
  if (maskMode) {
    for (int i = 0; i < len; ++i) {
      if (write(1, "*", 1) == -1) return;
    }
  } else if (highlight == -1) {
    if (write32(1, buf32, len) == -1) return;
  } else {
    if (write32(1, buf32, highlight) == -1) return;
    setDisplayAttribute(true, indicateError); /* bright blue (visible with both B&W bg) */
    if (write32(1, &buf32[highlight], 1) == -1) return;
    setDisplayAttribute(false, indicateError);
    if (write32(1, buf32 + highlight + 1, len - highlight - 1) == -1) return;
  }

  // position the cursor
  GetConsoleScreenBufferInfo(console_out, &inf);
  inf.dwCursorPosition.X = xCursorPos;  // 0-based on Win32
  inf.dwCursorPosition.Y -= yEndOfInput - yCursorPos;
  SetConsoleCursorPosition(console_out, inf.dwCursorPosition);
#else  // _WIN32
  char seq[64];
  int cursorRowMovement = pi.promptCursorRowOffset - pi.promptExtraLines;
  if (cursorRowMovement > 0) {  // move the cursor up as required
    snprintf(seq, sizeof seq, "\x1b[%dA", cursorRowMovement);
    if (write(1, seq, strlen(seq)) == -1) return;
  }
  // position at the end of the prompt, clear to end of screen
  snprintf(seq, sizeof seq, "\x1b[%dG\x1b[J",
           pi.promptIndentation + 1);  // 1-based on VT100
  if (write(1, seq, strlen(seq)) == -1) return;

  if (maskMode) {  // write mask characters instead of actual content
    for (int i = 0; i < len; ++i) {
      if (write(1, "*", 1) == -1) return;
    }
  } else if (syntaxCallback && highlight == -1) {
    // syntax highlighting: get colorized output from callback
    size_t buf8Size = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> buf8(new char[buf8Size]);
    copyString32to8(buf8.get(), buf8Size, buf32);
    char* colorized = syntaxCallback(buf8.get());
    if (colorized) {
      if (write(1, colorized, strlen(colorized)) == -1) { free(colorized); return; }
      free(colorized);
      // reset attributes after syntax highlighted output
      if (write(1, "\x1b[0m", 4) == -1) return;
    } else {
      if (write32(1, buf32, len) == -1) return;
    }
  } else if (syntaxCallback && highlight != -1) {
    // syntax highlighting with brace match: write highlighted char with brace color override
    size_t buf8Size = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> buf8(new char[buf8Size]);
    copyString32to8(buf8.get(), buf8Size, buf32);
    char* colorized = syntaxCallback(buf8.get());
    if (colorized) {
      // Write the colorized text but we can't easily extract the brace position
      // from the ANSI-colored output, so write the full colorized text and then
      // overlay the brace highlight by repositioning cursor
      if (write(1, colorized, strlen(colorized)) == -1) { free(colorized); return; }
      free(colorized);
      if (write(1, "\x1b[0m", 4) == -1) return;
      // Now overlay the brace highlight character
      int highlightCol = pi.promptIndentation + calculateColumnPosition(buf32, highlight);
      snprintf(seq, sizeof seq, "\x1b[%dG", highlightCol + 1);
      if (write(1, seq, strlen(seq)) == -1) return;
      setDisplayAttribute(true, indicateError);
      if (write32(1, &buf32[highlight], 1) == -1) return;
      setDisplayAttribute(false, indicateError);
      // Move cursor to end of line for subsequent output (hints/right prompt)
      int endCol = pi.promptIndentation + calculateColumnPosition(buf32, len);
      snprintf(seq, sizeof seq, "\x1b[%dG", endCol + 1);
      if (write(1, seq, strlen(seq)) == -1) return;
    } else {
      if (write32(1, buf32, highlight) == -1) return;
      setDisplayAttribute(true, indicateError);
      if (write32(1, &buf32[highlight], 1) == -1) return;
      setDisplayAttribute(false, indicateError);
      if (write32(1, buf32 + highlight + 1, len - highlight - 1) == -1) return;
    }
  } else if (highlight == -1) {  // write unhighlighted text
    if (write32(1, buf32, len) == -1) return;
  } else {  // highlight the matching brace/bracket/parenthesis
    if (write32(1, buf32, highlight) == -1) return;
    setDisplayAttribute(true, indicateError);
    if (write32(1, &buf32[highlight], 1) == -1) return;
    setDisplayAttribute(false, indicateError);
    if (write32(1, buf32 + highlight + 1, len - highlight - 1) == -1) return;
  }

  // display hints after the input text (display-only, does not affect cursor)
  if (!maskMode && hintsCallback) {
    // convert buf32 to UTF-8 for the callback
    size_t buf8Size = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> buf8(new char[buf8Size]);
    copyString32to8(buf8.get(), buf8Size, buf32);
    int color = 0;
    int bold = 0;
    char* hint = hintsCallback(buf8.get(), &color, &bold);
    if (hint) {
      auto hintDeleter = [](char* p) {
        if (freeHintsCallback) {
          freeHintsCallback(p);
        } else {
          free(p);
        }
      };
      unique_ptr<char, decltype(hintDeleter)> hintGuard(hint, hintDeleter);
      char hintSeq[64];
      snprintf(hintSeq, sizeof hintSeq, "\x1b[%d;%dm", bold ? 1 : 2, color);
      if (write(1, hintSeq, strlen(hintSeq)) == -1) return;
      if (write(1, hint, strlen(hint)) == -1) return;
      if (write(1, "\x1b[0m", 4) == -1) return;
    }
  }

  // display right prompt at right edge of first line (when input fits on one line)
  if (!maskMode && !rightPromptText.empty() && yEndOfInput == 0) {
    // convert right prompt to char32_t to measure column width
    size_t rpBufSize = rightPromptText.size() + 1;
    unique_ptr<char32_t[]> rpBuf32(new char32_t[rpBufSize]);
    size_t rpLen32 = 0;
    copyString8to32(rpBuf32.get(), rpBufSize, rpLen32, rightPromptText.c_str());
    int rpColWidth = calculateColumnPosition(rpBuf32.get(), static_cast<int>(rpLen32));
    int rpStartCol = pi.promptScreenColumns - rpColWidth;
    // only display if there are at least 2 chars of gap between content and right prompt
    if (rpStartCol > xEndOfInput + 2) {
      snprintf(seq, sizeof seq, "\x1b[%dG", rpStartCol + 1);  // 1-based
      if (write(1, seq, strlen(seq)) == -1) return;
      if (write(1, "\x1b[2m", 4) == -1) return;  // dim attribute
      if (write(1, rightPromptText.c_str(), rightPromptText.size()) == -1) return;
      if (write(1, "\x1b[0m", 4) == -1) return;  // reset attributes
    }
  }

  // we have to generate our own newline on line wrap
  if (xEndOfInput == 0 && yEndOfInput > 0)
    if (write(1, "\n", 1) == -1) return;

  // position the cursor
  cursorRowMovement = yEndOfInput - yCursorPos;
  if (cursorRowMovement > 0) {  // move the cursor up as required
    snprintf(seq, sizeof seq, "\x1b[%dA", cursorRowMovement);
    if (write(1, seq, strlen(seq)) == -1) return;
  }
  // position the cursor within the line
  snprintf(seq, sizeof seq, "\x1b[%dG", xCursorPos + 1);  // 1-based on VT100
  if (write(1, seq, strlen(seq)) == -1) return;
#endif

  pi.promptCursorRowOffset =
      pi.promptExtraLines + yCursorPos;  // remember row for next pass
}

#ifndef _WIN32

/**
 * Read a UTF-8 sequence from the non-Windows keyboard and return the Unicode
 * (char32_t) character it
 * encodes
 *
 * @return  char32_t Unicode character
 */
static char32_t readUnicodeCharacter(void) {
  static char8_t utf8String[5];
  static size_t utf8Count = 0;
  while (true) {
    char8_t c;

#ifndef _WIN32
    // Apply read timeout if configured (only on first byte of multi-byte sequence)
    if (readTimeoutMs > 0 && utf8Count == 0) {
      struct pollfd pfd;
      pfd.fd = 0;
      pfd.events = POLLIN;
      pfd.revents = 0;
      int ret = poll(&pfd, 1, readTimeoutMs);
      if (ret == 0) return TIMEOUT_KEY;  // timeout
      if (ret < 0 && errno != EINTR) return 0;  // error
    }
#endif

    /* Continue reading if interrupted by signal. */
    ssize_t nread;
    do {
      nread = read(0, &c, 1);
    } while ((nread == -1) && (errno == EINTR));

    if (nread <= 0) return 0;
    if (c <= 0x7F) {  // short circuit ASCII
      utf8Count = 0;
      return c;
    } else if (utf8Count < sizeof(utf8String) - 1) {
      utf8String[utf8Count++] = c;
      utf8String[utf8Count] = 0;
      char32_t unicodeChar[2];
      size_t ucharCount;
      ConversionResult res =
          copyString8to32(unicodeChar, 2, ucharCount, utf8String);
      if (res == conversionOK && ucharCount) {
        utf8Count = 0;
        return unicodeChar[0];
      }
    } else {
      utf8Count =
          0;  // this shouldn't happen: got four bytes but no UTF-8 character
    }
  }
}

namespace EscapeSequenceProcessing {  // move these out of global namespace

// This chunk of code does parsing of the escape sequences sent by various Linux
// terminals.
//
// It handles arrow keys, Home, End and Delete keys by interpreting the
// sequences sent by
// gnome terminal, xterm, rxvt, konsole, aterm and yakuake including the Alt and
// Ctrl key
// combinations that are understood by linenoise.
//
// The parsing uses tables, a bunch of intermediate dispatch routines and a
// doDispatch
// loop that reads the tables and sends control to "deeper" routines to continue
// the
// parsing.  The starting call to doDispatch( c, initialDispatch ) will
// eventually return
// either a character (with optional CTRL and META bits set), or -1 if parsing
// fails, or
// zero if an attempt to read from the keyboard fails.
//
// This is rather sloppy escape sequence processing, since we're not paying
// attention to what the
// actual TERM is set to and are processing all key sequences for all terminals,
// but it works with
// the most common keystrokes on the most common terminals.  It's intricate, but
// the nested 'if'
// statements required to do it directly would be worse.  This way has the
// advantage of allowing
// changes and extensions without having to touch a lot of code.

// This is a typedef for the routine called by doDispatch().  It takes the
// current character
// as input, does any required processing including reading more characters and
// calling other
// dispatch routines, then eventually returns the final (possibly extended or
// special) character.
//
typedef char32_t (*CharacterDispatchRoutine)(char32_t);

// This structure is used by doDispatch() to hold a list of characters to test
// for and
// a list of routines to call if the character matches.  The dispatch routine
// list is one
// longer than the character list; the final entry is used if no character
// matches.
//
struct CharacterDispatch {
  unsigned int len;                    // length of the chars list
  const char* chars;                   // chars to test
  CharacterDispatchRoutine* dispatch;  // array of routines to call
};

// This dispatch routine is given a dispatch table and then farms work out to
// routines
// listed in the table based on the character it is called with.  The dispatch
// routines can
// read more input characters to decide what should eventually be returned.
// Eventually,
// a called routine returns either a character or -1 to indicate parsing
// failure.
//
static char32_t doDispatch(char32_t c, CharacterDispatch& dispatchTable) {
  for (unsigned int i = 0; i < dispatchTable.len; ++i) {
    if (static_cast<unsigned char>(dispatchTable.chars[i]) == c) {
      return dispatchTable.dispatch[i](c);
    }
  }
  return dispatchTable.dispatch[dispatchTable.len](c);
}

static char32_t thisKeyMetaCtrl =
    0;  // holds pre-set Meta and/or Ctrl modifiers

// Final dispatch routines -- return something
//
static char32_t normalKeyRoutine(char32_t c) { return thisKeyMetaCtrl | c; }
static char32_t upArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | UP_ARROW_KEY;
}
static char32_t downArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | DOWN_ARROW_KEY;
}
static char32_t rightArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | RIGHT_ARROW_KEY;
}
static char32_t leftArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | LEFT_ARROW_KEY;
}
static char32_t homeKeyRoutine(char32_t) { return thisKeyMetaCtrl | HOME_KEY; }
static char32_t endKeyRoutine(char32_t) { return thisKeyMetaCtrl | END_KEY; }
static char32_t pageUpKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | PAGE_UP_KEY;
}
static char32_t pageDownKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | PAGE_DOWN_KEY;
}
static char32_t deleteCharRoutine(char32_t) {
  return thisKeyMetaCtrl | ctrlChar('H');
}  // key labeled Backspace
static char32_t deleteKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | DELETE_KEY;
}  // key labeled Delete
static char32_t insertKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | INSERT_KEY;
}  // key labeled Insert
static char32_t bracketedPasteStartRoutine(char32_t) {
  return BRACKETED_PASTE_START;
}
static char32_t ctrlUpArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | CTRL | UP_ARROW_KEY;
}
static char32_t ctrlDownArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | CTRL | DOWN_ARROW_KEY;
}
static char32_t ctrlRightArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | CTRL | RIGHT_ARROW_KEY;
}
static char32_t ctrlLeftArrowKeyRoutine(char32_t) {
  return thisKeyMetaCtrl | CTRL | LEFT_ARROW_KEY;
}
static char32_t escFailureRoutine(char32_t) {
  beep();
  return -1;
}

// Handle ESC [ 1 ; 3 (or 5) <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket1Semicolon3or5Routines[] = {
    upArrowKeyRoutine, downArrowKeyRoutine, rightArrowKeyRoutine,
    leftArrowKeyRoutine, escFailureRoutine};
static CharacterDispatch escLeftBracket1Semicolon3or5Dispatch = {
    4, "ABCD", escLeftBracket1Semicolon3or5Routines};

// Handle ESC [ 1 ; <more stuff> escape sequences
//
static char32_t escLeftBracket1Semicolon3Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  thisKeyMetaCtrl |= META;
  return doDispatch(c, escLeftBracket1Semicolon3or5Dispatch);
}
static char32_t escLeftBracket1Semicolon5Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  thisKeyMetaCtrl |= CTRL;
  return doDispatch(c, escLeftBracket1Semicolon3or5Dispatch);
}
static CharacterDispatchRoutine escLeftBracket1SemicolonRoutines[] = {
    escLeftBracket1Semicolon3Routine, escLeftBracket1Semicolon5Routine,
    escFailureRoutine};
static CharacterDispatch escLeftBracket1SemicolonDispatch = {
    2, "35", escLeftBracket1SemicolonRoutines};

// Handle ESC [ 1 <more stuff> escape sequences
//
static char32_t escLeftBracket1SemicolonRoutine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket1SemicolonDispatch);
}
static CharacterDispatchRoutine escLeftBracket1Routines[] = {
    homeKeyRoutine, escLeftBracket1SemicolonRoutine, escFailureRoutine};
static CharacterDispatch escLeftBracket1Dispatch = {2, "~;",
                                                    escLeftBracket1Routines};

// Handle ESC [ 3 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket3Routines[] = {deleteKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket3Dispatch = {1, "~",
                                                    escLeftBracket3Routines};

// Handle ESC [ 4 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket4Routines[] = {endKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket4Dispatch = {1, "~",
                                                    escLeftBracket4Routines};

// Handle ESC [ 5 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket5Routines[] = {pageUpKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket5Dispatch = {1, "~",
                                                    escLeftBracket5Routines};

// Handle ESC [ 6 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket6Routines[] = {pageDownKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket6Dispatch = {1, "~",
                                                    escLeftBracket6Routines};

// Handle ESC [ 7 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket7Routines[] = {homeKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket7Dispatch = {1, "~",
                                                    escLeftBracket7Routines};

// Handle ESC [ 8 <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracket8Routines[] = {endKeyRoutine,
                                                             escFailureRoutine};
static CharacterDispatch escLeftBracket8Dispatch = {1, "~",
                                                    escLeftBracket8Routines};

// Handle ESC [ <digit> escape sequences
//
static char32_t escLeftBracket0Routine(char32_t c) {
  return escFailureRoutine(c);
}
static char32_t escLeftBracket1Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket1Dispatch);
}
// Handle ESC [ 2 <more stuff> escape sequences
// ESC [ 2 ~ = Insert key
// ESC [ 2 0 0 ~ = Bracketed paste start
static char32_t escLeftBracket2Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  if (c == '~') {
    return insertKeyRoutine(c);
  }
  if (c == '0') {
    // could be ESC [ 2 0 0 ~ (bracketed paste start) or ESC [ 2 0 1 ~ (paste end)
    c = readUnicodeCharacter();
    if (c == 0) return 0;
    if (c == '0') {
      c = readUnicodeCharacter();
      if (c == 0) return 0;
      if (c == '~') {
        return bracketedPasteStartRoutine(c);
      }
    }
    // ESC [ 2 0 1 ~ (paste end) should not arrive outside paste mode, ignore
  }
  return escFailureRoutine(c);
}
static char32_t escLeftBracket3Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket3Dispatch);
}
static char32_t escLeftBracket4Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket4Dispatch);
}
static char32_t escLeftBracket5Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket5Dispatch);
}
static char32_t escLeftBracket6Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket6Dispatch);
}
static char32_t escLeftBracket7Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket7Dispatch);
}
static char32_t escLeftBracket8Routine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracket8Dispatch);
}
static char32_t escLeftBracket9Routine(char32_t c) {
  return escFailureRoutine(c);
}

// Handle ESC [ <more stuff> escape sequences
//
static CharacterDispatchRoutine escLeftBracketRoutines[] = {
    upArrowKeyRoutine,      downArrowKeyRoutine,    rightArrowKeyRoutine,
    leftArrowKeyRoutine,    homeKeyRoutine,         endKeyRoutine,
    escLeftBracket0Routine, escLeftBracket1Routine, escLeftBracket2Routine,
    escLeftBracket3Routine, escLeftBracket4Routine, escLeftBracket5Routine,
    escLeftBracket6Routine, escLeftBracket7Routine, escLeftBracket8Routine,
    escLeftBracket9Routine, escFailureRoutine};
static CharacterDispatch escLeftBracketDispatch = {16, "ABCDHF0123456789",
                                                   escLeftBracketRoutines};

// Handle ESC O <char> escape sequences
//
static CharacterDispatchRoutine escORoutines[] = {
    upArrowKeyRoutine,       downArrowKeyRoutine,     rightArrowKeyRoutine,
    leftArrowKeyRoutine,     homeKeyRoutine,          endKeyRoutine,
    ctrlUpArrowKeyRoutine,   ctrlDownArrowKeyRoutine, ctrlRightArrowKeyRoutine,
    ctrlLeftArrowKeyRoutine, escFailureRoutine};
static CharacterDispatch escODispatch = {10, "ABCDHFabcd", escORoutines};

// Initial ESC dispatch -- could be a Meta prefix or the start of an escape
// sequence
//
static char32_t escLeftBracketRoutine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escLeftBracketDispatch);
}
static char32_t escORoutine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escODispatch);
}
static char32_t setMetaRoutine(char32_t c);  // need forward reference
static CharacterDispatchRoutine escRoutines[] = {escLeftBracketRoutine,
                                                 escORoutine, setMetaRoutine};
static CharacterDispatch escDispatch = {2, "[O", escRoutines};

// Initial dispatch -- we are not in the middle of anything yet
//
static char32_t escRoutine(char32_t c) {
  c = readUnicodeCharacter();
  if (c == 0) return 0;
  return doDispatch(c, escDispatch);
}
static CharacterDispatchRoutine initialRoutines[] = {
    escRoutine, deleteCharRoutine, normalKeyRoutine};
static CharacterDispatch initialDispatch = {2, "\x1B\x7F", initialRoutines};

// Special handling for the ESC key because it does double duty
//
static char32_t setMetaRoutine(char32_t c) {
  thisKeyMetaCtrl = META;
  if (c == 0x1B) {  // another ESC, stay in ESC processing mode
    c = readUnicodeCharacter();
    if (c == 0) return 0;
    return doDispatch(c, escDispatch);
  }
  return doDispatch(c, initialDispatch);
}

}  // namespace EscapeSequenceProcessing // move these out of global namespace

#endif  // #ifndef _WIN32

// linenoiseReadChar -- read a keystroke or keychord from the keyboard, and
// translate it
// into an encoded "keystroke".  When convenient, extended keys are translated
// into their
// simpler Emacs keystrokes, so an unmodified "left arrow" becomes Ctrl-B.
//
// A return value of zero means "no input available", and a return value of -1
// means "invalid key".
//
static char32_t linenoiseReadChar(void) {
#ifdef _WIN32

  INPUT_RECORD rec;
  DWORD count;
  int modifierKeys = 0;
  bool escSeen = false;
  while (true) {
    ReadConsoleInputW(console_in, &rec, 1, &count);
#if 0  // helper for debugging keystrokes, display info in the debug "Output"
       // window in the debugger
        {
            if ( rec.EventType == KEY_EVENT ) {
                //if ( rec.Event.KeyEvent.uChar.UnicodeChar ) {
                    char buf[1024];
                    sprintf(
                            buf,
                            "Unicode character 0x%04X, repeat count %d, virtual keycode 0x%04X, "
                            "virtual scancode 0x%04X, key %s%s%s%s%s\n",
                            rec.Event.KeyEvent.uChar.UnicodeChar,
                            rec.Event.KeyEvent.wRepeatCount,
                            rec.Event.KeyEvent.wVirtualKeyCode,
                            rec.Event.KeyEvent.wVirtualScanCode,
                            rec.Event.KeyEvent.bKeyDown ? "down" : "up",
                                (rec.Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED)  ?
                                    " L-Ctrl" : "",
                                (rec.Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED) ?
                                    " R-Ctrl" : "",
                                (rec.Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED)   ?
                                    " L-Alt"  : "",
                                (rec.Event.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED)  ?
                                    " R-Alt"  : ""
                           );
                    OutputDebugStringA( buf );
                //}
            }
        }
#endif
    if (rec.EventType != KEY_EVENT) {
      continue;
    }
    // Windows provides for entry of characters that are not on your keyboard by
    // sending the
    // Unicode characters as a "key up" with virtual keycode 0x12 (VK_MENU ==
    // Alt key) ...
    // accept these characters, otherwise only process characters on "key down"
    if (!rec.Event.KeyEvent.bKeyDown &&
        rec.Event.KeyEvent.wVirtualKeyCode != VK_MENU) {
      continue;
    }
    modifierKeys = 0;
    // AltGr is encoded as ( LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED ), so don't
    // treat this
    // combination as either CTRL or META we just turn off those two bits, so it
    // is still
    // possible to combine CTRL and/or META with an AltGr key by using
    // right-Ctrl and/or
    // left-Alt
    if ((rec.Event.KeyEvent.dwControlKeyState &
         (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) ==
        (LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)) {
      rec.Event.KeyEvent.dwControlKeyState &=
          ~(LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED);
    }
    if (rec.Event.KeyEvent.dwControlKeyState &
        (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
      modifierKeys |= CTRL;
    }
    if (rec.Event.KeyEvent.dwControlKeyState &
        (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) {
      modifierKeys |= META;
    }
    if (escSeen) {
      modifierKeys |= META;
    }
    if (rec.Event.KeyEvent.uChar.UnicodeChar == 0) {
      switch (rec.Event.KeyEvent.wVirtualKeyCode) {
        case VK_LEFT:
          return modifierKeys | LEFT_ARROW_KEY;
        case VK_RIGHT:
          return modifierKeys | RIGHT_ARROW_KEY;
        case VK_UP:
          return modifierKeys | UP_ARROW_KEY;
        case VK_DOWN:
          return modifierKeys | DOWN_ARROW_KEY;
        case VK_DELETE:
          return modifierKeys | DELETE_KEY;
        case VK_HOME:
          return modifierKeys | HOME_KEY;
        case VK_END:
          return modifierKeys | END_KEY;
        case VK_PRIOR:
          return modifierKeys | PAGE_UP_KEY;
        case VK_NEXT:
          return modifierKeys | PAGE_DOWN_KEY;
        default:
          continue;  // in raw mode, ReadConsoleInput shows shift, ctrl ...
      }              //  ... ignore them
    } else if (rec.Event.KeyEvent.uChar.UnicodeChar ==
               ctrlChar('[')) {  // ESC, set flag for later
      escSeen = true;
      continue;
    } else {
      // we got a real character, return it
      return modifierKeys | rec.Event.KeyEvent.uChar.UnicodeChar;
    }
  }

#else
  char32_t c;
  c = readUnicodeCharacter();
  if (c == 0) return 0;

// If _DEBUG_LINUX_KEYBOARD is set, then ctrl-^ puts us into a keyboard
// debugging mode
// where we print out decimal and decoded values for whatever the "terminal"
// program
// gives us on different keystrokes.  Hit ctrl-C to exit this mode.
//
#define _DEBUG_LINUX_KEYBOARD
#if defined(_DEBUG_LINUX_KEYBOARD)
  if (c == ctrlChar('^')) {  // ctrl-^, special debug mode, prints all keys hit,
                             // ctrl-C to get out
    printf(
        "\nEntering keyboard debugging mode (on ctrl-^), press ctrl-C to exit "
        "this mode\n");
    while (true) {
      unsigned char keys[10];
      int ret = read(0, keys, 10);

      if (ret <= 0) {
        printf("\nret: %d\n", ret);
      }
      for (int i = 0; i < ret; ++i) {
        char32_t key = static_cast<char32_t>(keys[i]);
        char* friendlyTextPtr;
        char friendlyTextBuf[10];
        const char* prefixText = (key < 0x80) ? "" : "0x80+";
        char32_t keyCopy = (key < 0x80) ? key : key - 0x80;
        if (keyCopy >= '!' && keyCopy <= '~') {  // printable
          friendlyTextBuf[0] = '\'';
          friendlyTextBuf[1] = keyCopy;
          friendlyTextBuf[2] = '\'';
          friendlyTextBuf[3] = 0;
          friendlyTextPtr = friendlyTextBuf;
        } else if (keyCopy == ' ') {
          friendlyTextPtr = const_cast<char*>("space");
        } else if (keyCopy == 27) {
          friendlyTextPtr = const_cast<char*>("ESC");
        } else if (keyCopy == 0) {
          friendlyTextPtr = const_cast<char*>("NUL");
        } else if (keyCopy == 127) {
          friendlyTextPtr = const_cast<char*>("DEL");
        } else {
          friendlyTextBuf[0] = '^';
          friendlyTextBuf[1] = keyCopy + 0x40;
          friendlyTextBuf[2] = 0;
          friendlyTextPtr = friendlyTextBuf;
        }
        printf("%d x%02X (%s%s)  ", key, key, prefixText, friendlyTextPtr);
      }
      printf("\x1b[1G\n");  // go to first column of new line

      // drop out of this loop on ctrl-C
      if (keys[0] == ctrlChar('C')) {
        printf("Leaving keyboard debugging mode (on ctrl-C)\n");
        fflush(stdout);
        return -2;
      }
    }
  }
#endif  // _DEBUG_LINUX_KEYBOARD

  EscapeSequenceProcessing::thisKeyMetaCtrl =
      0;  // no modifiers yet at initialDispatch
  return EscapeSequenceProcessing::doDispatch(
      c, EscapeSequenceProcessing::initialDispatch);
#endif  // #_WIN32
}

/**
 * Free memory used in a recent command completion session
 *
 * @param lc pointer to a linenoiseCompletions struct
 */
static void freeCompletions(linenoiseCompletions* lc) {
  lc->completionStrings.clear();
}

/**
 * convert {CTRL + 'A'}, {CTRL + 'a'} and {CTRL + ctrlChar( 'A' )} into
 * ctrlChar( 'A' )
 * leave META alone
 *
 * @param c character to clean up
 * @return cleaned-up character
 */
static int cleanupCtrl(int c) {
  if (c & CTRL) {
    int d = c & 0x1FF;
    if (d >= 'a' && d <= 'z') {
      c = (c + ('a' - ctrlChar('A'))) & ~CTRL;
    }
    if (d >= 'A' && d <= 'Z') {
      c = (c + ('A' - ctrlChar('A'))) & ~CTRL;
    }
    if (d >= ctrlChar('A') && d <= ctrlChar('Z')) {
      c = c & ~CTRL;
    }
  }
  return c;
}

// break characters that may precede items to be completed
static const char breakChars[] = " =+-/\\*?\"'`&<>:;|@{([])}";

// maximum number of completions to display without asking
static const size_t completionCountCutoff = 100;

#ifndef _WIN32
// Built-in filename completion callback
static void filenameCompletionCallback(const char* input, linenoiseCompletions* lc) {
  // Find the word being completed by scanning back for break characters
  const char* wordStart = input + strlen(input);
  while (wordStart > input) {
    --wordStart;
    if (*wordStart == ' ' || *wordStart == '=' || *wordStart == ';' ||
        *wordStart == '|' || *wordStart == '&' || *wordStart == '(' ||
        *wordStart == ')') {
      ++wordStart;
      break;
    }
  }

  // Split into directory and prefix
  std::string word(wordStart);
  std::string dirPath;
  std::string prefix;

  size_t lastSlash = word.rfind('/');
  if (lastSlash != std::string::npos) {
    dirPath = word.substr(0, lastSlash + 1);
    prefix = word.substr(lastSlash + 1);
  } else {
    dirPath = "";
    prefix = word;
  }

  std::string openDir = dirPath.empty() ? "." : dirPath;
  DIR* dir = opendir(openDir.c_str());
  if (!dir) return;

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string name(entry->d_name);
    // Skip . and ..
    if (name == "." || name == "..") continue;

    // Check prefix match
    bool matches;
    if (completionCaseInsensitive) {
      matches = (name.length() >= prefix.length());
      if (matches) {
        for (size_t i = 0; i < prefix.length(); ++i) {
          if (tolower(name[i]) != tolower(prefix[i])) {
            matches = false;
            break;
          }
        }
      }
    } else {
      matches = (name.compare(0, prefix.length(), prefix) == 0);
    }

    if (matches) {
      std::string completion = dirPath + name;
      // Check if directory to append /
      std::string fullPath = openDir + "/" + name;
      struct stat st;
      if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        completion += "/";
      }
      linenoiseAddCompletion(lc, completion.c_str());
    }
  }
  closedir(dir);
}
#endif

/**
 * Handle command completion, using a completionCallback() routine to provide
 * possible substitutions
 * This routine handles the mechanics of updating the user's input buffer with
 * possible replacement
 * of text as the user selects a proposed completion string, or cancels the
 * completion attempt.
 * @param pi     PromptBase struct holding information about the prompt and our
 * screen position
 */
int InputBuffer::completeLine(PromptBase& pi) {
  linenoiseCompletions lc;
  char32_t c = 0;

  // Find the previous break character to determine the word being completed.
  // We also handle the case where tab is hit while not at end-of-line.
  int startIndex = pos;
  while (--startIndex >= 0) {
    if (isCharInString(breakChars, buf32[startIndex])) {
      break;
    }
  }
  ++startIndex;
  int itemLength = pos - startIndex;

  // Pass the full line up to cursor position to the callback for context
  // detection (e.g., "new " before the current word). The callback returns
  // word-level completions (replacements for the text from startIndex to pos).
  Utf32String fullLineCopy(buf32, pos);
  Utf8String fullLine(fullLineCopy);

  // get a list of completions
  if (completionCallback) {
    completionCallback(fullLine.get(), &lc);
  }
#ifndef _WIN32
  else if (filenameCompletionEnabled) {
    filenameCompletionCallback(fullLine.get(), &lc);
  }
#endif

  // if no completions, we are done
  if (lc.completionStrings.size() == 0) {
    beep();
    freeCompletions(&lc);
    return 0;
  }

  // at least one completion
  int longestCommonPrefix = 0;
  int displayLength = 0;
  if (lc.completionStrings.size() == 1) {
    longestCommonPrefix = static_cast<int>(lc.completionStrings[0].length());
  } else {
    bool keepGoing = true;
    while (keepGoing) {
      for (size_t j = 0; j < lc.completionStrings.size() - 1; ++j) {
        char32_t c1 = lc.completionStrings[j][longestCommonPrefix];
        char32_t c2 = lc.completionStrings[j + 1][longestCommonPrefix];
        if (completionCaseInsensitive) {
          c1 = towlower(c1);
          c2 = towlower(c2);
        }
        if ((0 == c1) || (0 == c2) || (c1 != c2)) {
          keepGoing = false;
          break;
        }
      }
      if (keepGoing) {
        ++longestCommonPrefix;
      }
    }
  }
  if (lc.completionStrings.size() != 1) {  // beep if ambiguous
    beep();
  }

  // if we can extend the item, extend it and return to main loop
  if (longestCommonPrefix > itemLength) {
    displayLength = len + longestCommonPrefix - itemLength;
    if (displayLength > buflen) {
      longestCommonPrefix -= displayLength - buflen;  // don't overflow buffer
      displayLength = buflen;                         // truncate the insertion
      beep();                                         // and make a noise
    }
    Utf32String displayText(displayLength + 1);
    memcpy(displayText.get(), buf32, sizeof(char32_t) * startIndex);
    memcpy(&displayText[startIndex], &lc.completionStrings[0][0],
           sizeof(char32_t) * longestCommonPrefix);
    int tailIndex = startIndex + longestCommonPrefix;
    memcpy(&displayText[tailIndex], &buf32[pos],
           sizeof(char32_t) * (displayLength - tailIndex + 1));
    copyString32(buf32, displayText.get(), displayLength);
    pos = startIndex + longestCommonPrefix;
    len = displayLength;
    refreshLine(pi);
    freeCompletions(&lc);
    return 0;
  }

  // Menu-complete mode: cycle through completions on each Tab press
  if (menuCompleteEnabled && lc.completionStrings.size() > 1) {
    // Save original buffer state for cycling
    menuCompleteOrigText = Utf32String(buf32, len);
    menuCompleteOrigLen = len;
    menuCompleteOrigPos = pos;
    menuCompleteStartIndex = startIndex;
    menuCompleteIndex = 0;
    menuCompleteActive = true;

    // Replace with first completion
    while (menuCompleteActive) {
      int compLen = static_cast<int>(lc.completionStrings[menuCompleteIndex].length());
      int newLen = menuCompleteStartIndex + compLen +
                   (menuCompleteOrigLen - menuCompleteOrigPos);
      if (newLen > buflen) {
        newLen = buflen;
        compLen = buflen - menuCompleteStartIndex -
                  (menuCompleteOrigLen - menuCompleteOrigPos);
        if (compLen < 0) {
          compLen = 0;
        }
      }
      // Build new buffer: prefix + completion + suffix
      memcpy(buf32, menuCompleteOrigText.get(),
             sizeof(char32_t) * menuCompleteStartIndex);
      memcpy(buf32 + menuCompleteStartIndex,
             lc.completionStrings[menuCompleteIndex].get(),
             sizeof(char32_t) * compLen);
      int tailStart = menuCompleteStartIndex + compLen;
      int tailLen = menuCompleteOrigLen - menuCompleteOrigPos;
      if (tailStart + tailLen > buflen) {
        tailLen = buflen - tailStart;
      }
      memcpy(buf32 + tailStart,
             menuCompleteOrigText.get() + menuCompleteOrigPos,
             sizeof(char32_t) * tailLen);
      len = tailStart + tailLen;
      pos = menuCompleteStartIndex + compLen;
      buf32[len] = '\0';
      refreshLine(pi);

      // Read next keystroke
      do {
        c = linenoiseReadChar();
        c = cleanupCtrl(c);
      } while (c == static_cast<char32_t>(-1));

      if (c == ctrlChar('I')) {  // another Tab: cycle to next
        menuCompleteIndex =
            (menuCompleteIndex + 1) % static_cast<int>(lc.completionStrings.size());
      } else {
        // Accept current completion, pass this keystroke to main loop
        menuCompleteActive = false;
        freeCompletions(&lc);
        return c;
      }
    }
  }

  // we can't complete any further, wait for second tab
  do {
    c = linenoiseReadChar();
    c = cleanupCtrl(c);
  } while (c == static_cast<char32_t>(-1));

  // if any character other than tab, pass it to the main loop
  if (c != ctrlChar('I')) {
    freeCompletions(&lc);
    return c;
  }

  // we got a second tab, maybe show list of possible completions
  bool showCompletions = true;
  bool onNewLine = false;
  if (lc.completionStrings.size() > completionCountCutoff) {
    int savePos =
        pos;  // move cursor to EOL to avoid overwriting the command line
    pos = len;
    refreshLine(pi);
    pos = savePos;
    printf("\nDisplay all %u possibilities? (y or n)",
           static_cast<unsigned int>(lc.completionStrings.size()));
    fflush(stdout);
    onNewLine = true;
    while (c != 'y' && c != 'Y' && c != 'n' && c != 'N' && c != ctrlChar('C')) {
      do {
        c = linenoiseReadChar();
        c = cleanupCtrl(c);
      } while (c == static_cast<char32_t>(-1));
    }
    switch (c) {
      case 'n':
      case 'N':
        showCompletions = false;
        freeCompletions(&lc);
        break;
      case ctrlChar('C'):
        showCompletions = false;
        freeCompletions(&lc);
        if (write(1, "^C", 2) == -1) return -1;  // Display the ^C we got
        c = 0;
        break;
    }
  }

  // if showing the list, do it the way readline does it
  bool stopList = false;
  if (showCompletions) {
    int longestCompletion = 0;
    for (size_t j = 0; j < lc.completionStrings.size(); ++j) {
      itemLength = static_cast<int>(lc.completionStrings[j].length());
      if (itemLength > longestCompletion) {
        longestCompletion = itemLength;
      }
    }
    longestCompletion += 2;
    int columnCount = pi.promptScreenColumns / longestCompletion;
    if (columnCount < 1) {
      columnCount = 1;
    }
    if (!onNewLine) {  // skip this if we showed "Display all %d possibilities?"
      int savePos =
          pos;  // move cursor to EOL to avoid overwriting the command line
      pos = len;
      refreshLine(pi);
      pos = savePos;
    }
    size_t pauseRow = getScreenRows() - 1;
    size_t rowCount =
        (lc.completionStrings.size() + columnCount - 1) / columnCount;
    for (size_t row = 0; row < rowCount; ++row) {
      if (row == pauseRow) {
        printf("\n--More--");
        fflush(stdout);
        c = 0;
        bool doBeep = false;
        while (c != ' ' && c != '\r' && c != '\n' && c != 'y' && c != 'Y' &&
               c != 'n' && c != 'N' && c != 'q' && c != 'Q' &&
               c != ctrlChar('C')) {
          if (doBeep) {
            beep();
          }
          doBeep = true;
          do {
            c = linenoiseReadChar();
            c = cleanupCtrl(c);
          } while (c == static_cast<char32_t>(-1));
        }
        switch (c) {
          case ' ':
          case 'y':
          case 'Y':
            printf("\r        \r");
            pauseRow += getScreenRows() - 1;
            break;
          case '\r':
          case '\n':
            printf("\r        \r");
            ++pauseRow;
            break;
          case 'n':
          case 'N':
          case 'q':
          case 'Q':
            printf("\r        \r");
            stopList = true;
            break;
          case ctrlChar('C'):
            if (write(1, "^C", 2) == -1) return -1;  // Display the ^C we got
            stopList = true;
            break;
        }
      } else {
        printf("\n");
      }
      if (stopList) {
        break;
      }
      for (int column = 0; column < columnCount; ++column) {
        size_t index = (column * rowCount) + row;
        if (index < lc.completionStrings.size()) {
          itemLength = static_cast<int>(lc.completionStrings[index].length());
          fflush(stdout);
          if (write32(1, lc.completionStrings[index].get(), itemLength) == -1)
            return -1;
          if (((column + 1) * rowCount) + row < lc.completionStrings.size()) {
            for (int k = itemLength; k < longestCompletion; ++k) {
              printf(" ");
            }
          }
        }
      }
    }
    fflush(stdout);
    freeCompletions(&lc);
  }

  // display the prompt on a new line, then redisplay the input buffer
  if (!stopList || c == ctrlChar('C')) {
    if (write(1, "\n", 1) == -1) return 0;
  }
  if (!pi.write()) return 0;
#ifndef _WIN32
  // we have to generate our own newline on line wrap on Linux
  if (pi.promptIndentation == 0 && pi.promptExtraLines > 0)
    if (write(1, "\n", 1) == -1) return 0;
#endif
  pi.promptCursorRowOffset = pi.promptExtraLines;
  refreshLine(pi);
  return 0;
}

/**
 * Clear the screen ONLY (no redisplay of anything)
 */
void linenoiseMaskModeEnable(void) {
  maskMode = true;
}

void linenoiseMaskModeDisable(void) {
  maskMode = false;
}

void linenoiseSetMenuComplete(int enable) {
  menuCompleteEnabled = enable != 0;
}

void linenoiseSetCompletionCaseInsensitive(int ci) {
  completionCaseInsensitive = ci != 0;
}

void linenoiseSetFilenameCompletion(int enable) {
  filenameCompletionEnabled = enable != 0;
}

void linenoiseSetReadTimeout(int ms) {
  readTimeoutMs = ms;
}

void linenoiseClearScreen(void) {
#ifdef _WIN32
  COORD coord = {0, 0};
  CONSOLE_SCREEN_BUFFER_INFO inf;
  HANDLE screenHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  GetConsoleScreenBufferInfo(screenHandle, &inf);
  SetConsoleCursorPosition(screenHandle, coord);
  DWORD count;
  FillConsoleOutputCharacterA(screenHandle, ' ', inf.dwSize.X * inf.dwSize.Y,
                              coord, &count);
#else
  if (write(1, "\x1b[H\x1b[2J", 7) <= 0) return;
#endif
}

void InputBuffer::clearScreen(PromptBase& pi) {
  linenoiseClearScreen();
  if (!pi.write()) return;
#ifndef _WIN32
  // we have to generate our own newline on line wrap on Linux
  if (pi.promptIndentation == 0 && pi.promptExtraLines > 0)
    if (write(1, "\n", 1) == -1) return;
#endif
  pi.promptCursorRowOffset = pi.promptExtraLines;
  refreshLine(pi);
}

/**
 * Incremental history search -- take over the prompt and keyboard as the user
 * types a search
 * string, deletes characters from it, changes direction, and either accepts the
 * found line (for
 * execution orediting) or cancels.
 * @param pi        PromptBase struct holding information about the (old,
 * static) prompt and our
 *                  screen position
 * @param startChar the character that began the search, used to set the initial
 * direction
 */
int InputBuffer::incrementalHistorySearch(PromptBase& pi, int startChar) {
  size_t bufferSize;
  size_t ucharCount = 0;

  // When using a history provider, we keep a local UTF-8 string for the
  // "current line" instead of using the internal history array.
  bool useProvider = hasHistoryProvider() && historySearchCallback;
  std::string providerLine;

  // if not already recalling, add the current line to the history list so we
  // don't have to
  // special case it
  if (useProvider) {
    bufferSize = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> tempBuffer(new char[bufferSize]);
    copyString32to8(tempBuffer.get(), bufferSize, buf32);
    providerLine = tempBuffer.get();
  } else if (historyIndex == historyLen - 1) {
    free(history[historyLen - 1]);
    bufferSize = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> tempBuffer(new char[bufferSize]);
    copyString32to8(tempBuffer.get(), bufferSize, buf32);
    history[historyLen - 1] = strdup8(tempBuffer.get());
  }
  int historyLineLength = len;
  int historyLinePosition = pos;
  char32_t emptyBuffer[1];
  char emptyWidths[1];
  InputBuffer empty(emptyBuffer, emptyWidths, 1);
  empty.refreshLine(pi);  // erase the old input first
  DynamicPrompt dp(pi, (startChar == ctrlChar('R')) ? -1 : 1);

  dp.promptPreviousLen = pi.promptPreviousLen;
  dp.promptPreviousInputLen = pi.promptPreviousInputLen;
  dynamicRefresh(dp, buf32, historyLineLength,
                 historyLinePosition);  // draw user's text with our prompt

  // loop until we get an exit character
  int c = 0;
  bool keepLooping = true;
  bool useSearchedLine = true;
  bool searchAgain = false;
  char32_t* activeHistoryLine = 0;
  while (keepLooping) {
    c = linenoiseReadChar();
    c = cleanupCtrl(c);  // convert CTRL + <char> into normal ctrl

    switch (c) {
      // these characters keep the selected text but do not execute it
      case ctrlChar('A'):  // ctrl-A, move cursor to start of line
      case HOME_KEY:
      case ctrlChar('B'):  // ctrl-B, move cursor left by one character
      case LEFT_ARROW_KEY:
      case META + 'b':  // meta-B, move cursor left by one word
      case META + 'B':
      case CTRL + LEFT_ARROW_KEY:
      case META + LEFT_ARROW_KEY:  // Emacs allows Meta, bash & readline don't
      case ctrlChar('D'):
      case META + 'd':  // meta-D, kill word to right of cursor
      case META + 'D':
      case ctrlChar('E'):  // ctrl-E, move cursor to end of line
      case END_KEY:
      case ctrlChar('F'):  // ctrl-F, move cursor right by one character
      case RIGHT_ARROW_KEY:
      case META + 'f':  // meta-F, move cursor right by one word
      case META + 'F':
      case CTRL + RIGHT_ARROW_KEY:
      case META + RIGHT_ARROW_KEY:  // Emacs allows Meta, bash & readline don't
      case META + ctrlChar('H'):
      case ctrlChar('J'):
      case ctrlChar('K'):  // ctrl-K, kill from cursor to end of line
      case ctrlChar('M'):
      case ctrlChar('N'):  // ctrl-N, recall next line in history
      case ctrlChar('P'):  // ctrl-P, recall previous line in history
      case DOWN_ARROW_KEY:
      case UP_ARROW_KEY:
      case ctrlChar('T'):  // ctrl-T, transpose characters
      case ctrlChar(
          'U'):  // ctrl-U, kill all characters to the left of the cursor
      case ctrlChar('W'):
      case META + 'y':  // meta-Y, "yank-pop", rotate popped text
      case META + 'Y':
      case 127:
      case DELETE_KEY:
      case META + '<':  // start of history
      case PAGE_UP_KEY:
      case META + '>':  // end of history
      case PAGE_DOWN_KEY:
        keepLooping = false;
        break;

      // these characters revert the input line to its previous state
      case ctrlChar('C'):  // ctrl-C, abort this line
      case ctrlChar('G'):
      case ctrlChar('L'):  // ctrl-L, clear screen and redisplay line
        keepLooping = false;
        useSearchedLine = false;
        if (c != ctrlChar('L')) {
          c = -1;  // ctrl-C and ctrl-G just abort the search and do nothing
                   // else
        }
        break;

      // these characters stay in search mode and update the display
      case ctrlChar('S'):
      case ctrlChar('R'):
        if (dp.searchTextLen ==
            0) {  // if no current search text, recall previous text
          if (previousSearchText.length()) {
            dp.updateSearchText(previousSearchText.get());
          }
        }
        if ((dp.direction == 1 && c == ctrlChar('R')) ||
            (dp.direction == -1 && c == ctrlChar('S'))) {
          dp.direction = 0 - dp.direction;  // reverse direction
          dp.updateSearchPrompt();          // change the prompt
        } else {
          searchAgain = true;  // same direction, search again
        }
        break;

// job control is its own thing
#ifndef _WIN32
      case ctrlChar('Z'):  // ctrl-Z, job control
        disableRawMode();  // Returning to Linux (whatever) shell, leave raw
                           // mode
        raise(SIGSTOP);    // Break out in mid-line
        enableRawMode();   // Back from Linux shell, re-enter raw mode
        {
          bufferSize = historyLineLength + 1;
          unique_ptr<char32_t[]> tempUnicode(new char32_t[bufferSize]);
          if (useProvider) {
            copyString8to32(tempUnicode.get(), bufferSize, ucharCount,
                            providerLine.c_str());
          } else {
            copyString8to32(tempUnicode.get(), bufferSize, ucharCount,
                            history[historyIndex]);
          }
          dynamicRefresh(dp, tempUnicode.get(), historyLineLength,
                         historyLinePosition);
        }
        continue;
        break;
#endif

      // these characters update the search string, and hence the selected input
      // line
      case ctrlChar('H'):  // backspace/ctrl-H, delete char to left of cursor
        if (dp.searchTextLen > 0) {
          unique_ptr<char32_t[]> tempUnicode(new char32_t[dp.searchTextLen]);
          --dp.searchTextLen;
          dp.searchText[dp.searchTextLen] = 0;
          copyString32(tempUnicode.get(), dp.searchText.get(),
                       dp.searchTextLen);
          dp.updateSearchText(tempUnicode.get());
        } else {
          beep();
        }
        break;

      case ctrlChar('Y'):  // ctrl-Y, yank killed text
        break;

      default:
        if (!isControlChar(c) && c <= 0x0010FFFF) {  // not an action character
          unique_ptr<char32_t[]> tempUnicode(
              new char32_t[dp.searchTextLen + 2]);
          copyString32(tempUnicode.get(), dp.searchText.get(),
                       dp.searchTextLen);
          tempUnicode[dp.searchTextLen] = c;
          tempUnicode[dp.searchTextLen + 1] = 0;
          dp.updateSearchText(tempUnicode.get());
        } else {
          beep();
        }
    }  // switch

    // if we are staying in search mode, search now
    if (keepLooping) {
      if (useProvider) {
        // Provider search mode: delegate search to callback
        bool matchFound = false;
        if (dp.searchTextLen > 0) {
          // Convert search text from char32_t to UTF-8
          size_t searchBufSize = sizeof(char32_t) * dp.searchTextLen + 1;
          unique_ptr<char[]> searchUtf8(new char[searchBufSize]);
          copyString32to8(searchUtf8.get(), searchBufSize, dp.searchText.get());

          char* match = historySearchCallback(
              searchUtf8.get(), dp.direction, historyProviderUserData);
          if (match) {
            providerLine = match;
            free(match);
            matchFound = true;
          } else {
            beep();
          }
        }
        searchAgain = false;

        // Single allocation for display
        if (activeHistoryLine) {
          delete[] activeHistoryLine;
          activeHistoryLine = nullptr;
        }
        bufferSize = providerLine.size() + 1;
        activeHistoryLine = new char32_t[bufferSize];
        copyString8to32(activeHistoryLine, bufferSize, ucharCount,
                        providerLine.c_str());
        historyLineLength = static_cast<int>(ucharCount);

        // Find match position for cursor placement
        if (matchFound) {
          historyLinePosition = 0;
          for (int i = 0; i <= historyLineLength - static_cast<int>(dp.searchTextLen); ++i) {
            if (strncmp32(dp.searchText.get(), &activeHistoryLine[i],
                          dp.searchTextLen) == 0) {
              historyLinePosition = i;
              break;
            }
          }
        }

        dynamicRefresh(dp, activeHistoryLine, historyLineLength,
                       historyLinePosition);
      } else {
        // Internal history search mode
        bufferSize = historyLineLength + 1;
        if (activeHistoryLine) {
          delete[] activeHistoryLine;
          activeHistoryLine = nullptr;
        }
        activeHistoryLine = new char32_t[bufferSize];
        copyString8to32(activeHistoryLine, bufferSize, ucharCount,
                        history[historyIndex]);
        if (dp.searchTextLen > 0) {
          bool found = false;
          int historySearchIndex = historyIndex;
          int lineLength = static_cast<int>(ucharCount);
          int lineSearchPos = historyLinePosition;
          if (searchAgain) {
            lineSearchPos += dp.direction;
          }
          searchAgain = false;
          while (true) {
            while ((dp.direction > 0) ? (lineSearchPos < lineLength)
                                      : (lineSearchPos >= 0)) {
              if (strncmp32(dp.searchText.get(),
                            &activeHistoryLine[lineSearchPos],
                            dp.searchTextLen) == 0) {
                found = true;
                break;
              }
              lineSearchPos += dp.direction;
            }
            if (found) {
              historyIndex = historySearchIndex;
              historyLineLength = lineLength;
              historyLinePosition = lineSearchPos;
              break;
            } else if ((dp.direction > 0) ? (historySearchIndex < historyLen - 1)
                                          : (historySearchIndex > 0)) {
              historySearchIndex += dp.direction;
              bufferSize = strlen8(history[historySearchIndex]) + 1;
              delete[] activeHistoryLine;
              activeHistoryLine = nullptr;
              activeHistoryLine = new char32_t[bufferSize];
              copyString8to32(activeHistoryLine, bufferSize, ucharCount,
                              history[historySearchIndex]);
              lineLength = static_cast<int>(ucharCount);
              lineSearchPos =
                  (dp.direction > 0) ? 0 : (lineLength - dp.searchTextLen);
            } else {
              beep();
              break;
            }
          };  // while
        }
        if (activeHistoryLine) {
          delete[] activeHistoryLine;
          activeHistoryLine = nullptr;
        }
        bufferSize = historyLineLength + 1;
        activeHistoryLine = new char32_t[bufferSize];
        copyString8to32(activeHistoryLine, bufferSize, ucharCount,
                        history[historyIndex]);
        dynamicRefresh(dp, activeHistoryLine, historyLineLength,
                       historyLinePosition);  // draw user's text with our prompt
      }
    }
  }  // while

  // leaving history search, restore previous prompt, maybe make searched line
  // current
  PromptBase pb;
  pb.promptChars = pi.promptIndentation;
  pb.promptBytes = pi.promptBytes;
  Utf32String tempUnicode(pb.promptBytes + 1);

  copyString32(tempUnicode.get(), &pi.promptText[pi.promptLastLinePosition],
               pb.promptBytes - pi.promptLastLinePosition);
  tempUnicode.initFromBuffer();
  pb.promptText = tempUnicode;
  pb.promptExtraLines = 0;
  pb.promptIndentation = pi.promptIndentation;
  pb.promptLastLinePosition = 0;
  pb.promptPreviousInputLen = historyLineLength;
  pb.promptCursorRowOffset = dp.promptCursorRowOffset;
  pb.promptScreenColumns = pi.promptScreenColumns;
  pb.promptPreviousLen = dp.promptChars;
  if (useSearchedLine && activeHistoryLine) {
    historyRecallMostRecent = true;
    copyString32(buf32, activeHistoryLine, buflen + 1);
    len = historyLineLength;
    pos = historyLinePosition;
  }
  if (activeHistoryLine) {
    delete[] activeHistoryLine;
    activeHistoryLine = nullptr;
  }
  // Reset provider navigation state after search
  if (useProvider) {
    providerNavigating = false;
    providerSavedLine.clear();
    if (historyResetCallback) {
      historyResetCallback(historyProviderUserData);
    }
  }
  dynamicRefresh(pb, buf32, len,
                 pos);  // redraw the original prompt with current input
  pi.promptPreviousInputLen = len;
  pi.promptCursorRowOffset = pi.promptExtraLines + pb.promptCursorRowOffset;
  previousSearchText =
      dp.searchText;  // save search text for possible reuse on ctrl-R ctrl-R
  return c;           // pass a character or -1 back to main loop
}

static bool isCharacterAlphanumeric(char32_t testChar) {
#ifdef _WIN32
  return (iswalnum((wint_t)testChar) != 0 ? true : false);
#else
  return (iswalnum(testChar) != 0 ? true : false);
#endif
}

#ifndef _WIN32
static bool gotResize = false;
#endif
static int keyType = 0;

// Key handler dispatch table types and initialization
using KeyHandler = int (InputBuffer::*)(PromptBase&, KillRing&, int);
static std::unordered_map<int, KeyHandler> defaultKeyBindings;
static bool bindingsInitialized = false;

// User key binding support
struct UserKeyBinding {
  linenoiseUserKeyCallback callback;
  void* userData;
};
static std::unordered_map<int, UserKeyBinding> userKeyBindings;
static linenoiseUserKeyCallback globalUserKeyCallback = NULL;

void initDefaultBindings() {
  if (bindingsInitialized) return;
  // Movement
  defaultKeyBindings[ctrlChar('A')] = &InputBuffer::handleMoveToStart;
  defaultKeyBindings[HOME_KEY] = &InputBuffer::handleMoveToStart;
  defaultKeyBindings[ctrlChar('B')] = &InputBuffer::handleMoveLeft;
  defaultKeyBindings[LEFT_ARROW_KEY] = &InputBuffer::handleMoveLeft;
  defaultKeyBindings[META + 'b'] = &InputBuffer::handleMoveWordLeft;
  defaultKeyBindings[META + 'B'] = &InputBuffer::handleMoveWordLeft;
  defaultKeyBindings[CTRL + LEFT_ARROW_KEY] = &InputBuffer::handleMoveWordLeft;
  defaultKeyBindings[META + LEFT_ARROW_KEY] = &InputBuffer::handleMoveWordLeft;
  defaultKeyBindings[ctrlChar('E')] = &InputBuffer::handleMoveToEnd;
  defaultKeyBindings[END_KEY] = &InputBuffer::handleMoveToEnd;
  defaultKeyBindings[ctrlChar('F')] = &InputBuffer::handleMoveRight;
  defaultKeyBindings[RIGHT_ARROW_KEY] = &InputBuffer::handleMoveRight;
  defaultKeyBindings[META + 'f'] = &InputBuffer::handleMoveWordRight;
  defaultKeyBindings[META + 'F'] = &InputBuffer::handleMoveWordRight;
  defaultKeyBindings[CTRL + RIGHT_ARROW_KEY] = &InputBuffer::handleMoveWordRight;
  defaultKeyBindings[META + RIGHT_ARROW_KEY] = &InputBuffer::handleMoveWordRight;
  // Editing
  defaultKeyBindings[ctrlChar('C')] = &InputBuffer::handleAbort;
  defaultKeyBindings[META + 'c'] = &InputBuffer::handleCapitalizeWord;
  defaultKeyBindings[META + 'C'] = &InputBuffer::handleCapitalizeWord;
  defaultKeyBindings[ctrlChar('D')] = &InputBuffer::handleDeleteOrExit;
  defaultKeyBindings[ctrlChar('H')] = &InputBuffer::handleBackspace;
  defaultKeyBindings[ctrlChar('J')] = &InputBuffer::handleAcceptLine;
  defaultKeyBindings[ctrlChar('M')] = &InputBuffer::handleAcceptLine;
  defaultKeyBindings[ctrlChar('L')] = &InputBuffer::handleClearScreenCmd;
  defaultKeyBindings[ctrlChar('T')] = &InputBuffer::handleTranspose;
  defaultKeyBindings[127] = &InputBuffer::handleDelete;
  defaultKeyBindings[DELETE_KEY] = &InputBuffer::handleDelete;
  defaultKeyBindings[INSERT_KEY] = &InputBuffer::handleInsertToggle;
  defaultKeyBindings[BRACKETED_PASTE_START] = &InputBuffer::handleBracketedPaste;
  // Kill and yank
  defaultKeyBindings[META + 'd'] = &InputBuffer::handleKillWordRight;
  defaultKeyBindings[META + 'D'] = &InputBuffer::handleKillWordRight;
  defaultKeyBindings[META + ctrlChar('H')] = &InputBuffer::handleKillWordLeft;
  defaultKeyBindings[ctrlChar('K')] = &InputBuffer::handleKillToEnd;
  defaultKeyBindings[ctrlChar('U')] = &InputBuffer::handleKillToStart;
  defaultKeyBindings[ctrlChar('W')] = &InputBuffer::handleKillToWhitespace;
  defaultKeyBindings[ctrlChar('Y')] = &InputBuffer::handleYank;
  defaultKeyBindings[META + 'y'] = &InputBuffer::handleYankPop;
  defaultKeyBindings[META + 'Y'] = &InputBuffer::handleYankPop;
  // Word case
  defaultKeyBindings[META + 'l'] = &InputBuffer::handleLowercaseWord;
  defaultKeyBindings[META + 'L'] = &InputBuffer::handleLowercaseWord;
  defaultKeyBindings[META + 'u'] = &InputBuffer::handleUppercaseWord;
  defaultKeyBindings[META + 'U'] = &InputBuffer::handleUppercaseWord;
  // History
  defaultKeyBindings[ctrlChar('N')] = &InputBuffer::handleHistoryNavigate;
  defaultKeyBindings[ctrlChar('P')] = &InputBuffer::handleHistoryNavigate;
  defaultKeyBindings[DOWN_ARROW_KEY] = &InputBuffer::handleHistoryNavigate;
  defaultKeyBindings[UP_ARROW_KEY] = &InputBuffer::handleHistoryNavigate;
  defaultKeyBindings[ctrlChar('R')] = &InputBuffer::handleHistorySearch;
  defaultKeyBindings[ctrlChar('S')] = &InputBuffer::handleHistorySearch;
  defaultKeyBindings[META + '<'] = &InputBuffer::handleHistoryJump;
  defaultKeyBindings[PAGE_UP_KEY] = &InputBuffer::handleHistoryJump;
  defaultKeyBindings[META + '>'] = &InputBuffer::handleHistoryJump;
  defaultKeyBindings[PAGE_DOWN_KEY] = &InputBuffer::handleHistoryJump;
  // Undo
  defaultKeyBindings[ctrlChar('_')] = &InputBuffer::handleUndo;
  // Macros
  defaultKeyBindings[ctrlChar('X')] = &InputBuffer::handleMacro;
  // Special
  defaultKeyBindings[TIMEOUT_KEY] = &InputBuffer::handleTimeout;
#ifndef _WIN32
  defaultKeyBindings[ctrlChar('Z')] = &InputBuffer::handleSuspend;
#endif
  bindingsInitialized = true;
}

// --- Key handler implementations ---

int InputBuffer::handleTimeout(PromptBase& pi, KillRing& killRing, int c) {
  if (!hasHistoryProvider()) {
    --historyLen;
    free(history[historyLen]);
  }
  return -1;
}

int InputBuffer::handleMoveToStart(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  pos = 0;
  refreshLine(pi);
  return 0;
}

int InputBuffer::handleMoveLeft(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos > 0) {
    --pos;
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleMoveWordLeft(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos > 0) {
    while (pos > 0 && !isCharacterAlphanumeric(buf32[pos - 1])) {
      --pos;
    }
    while (pos > 0 && isCharacterAlphanumeric(buf32[pos - 1])) {
      --pos;
    }
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleAbort(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  historyRecallMostRecent = false;
  errno = EAGAIN;
  if (!hasHistoryProvider()) {
    --historyLen;
    free(history[historyLen]);
  }
  // we need one last refresh with the cursor at the end of the line
  // so we don't display the next prompt over the previous input line
  pos = len;  // pass len as pos for EOL
  refreshLine(pi);
  if (write(1, "^C", 2) == -1) return -1;  // Display the ^C we got
  return -1;
}

int InputBuffer::handleCapitalizeWord(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  historyRecallMostRecent = false;
  undoStack.save(buf32, len, pos);
  if (pos < len) {
    while (pos < len && !isCharacterAlphanumeric(buf32[pos])) {
      ++pos;
    }
    if (pos < len && isCharacterAlphanumeric(buf32[pos])) {
      if (buf32[pos] >= 'a' && buf32[pos] <= 'z') {
        buf32[pos] += 'A' - 'a';
      }
      ++pos;
    }
    while (pos < len && isCharacterAlphanumeric(buf32[pos])) {
      if (buf32[pos] >= 'A' && buf32[pos] <= 'Z') {
        buf32[pos] += 'a' - 'A';
      }
      ++pos;
    }
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleDeleteOrExit(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (len > 0 && pos < len) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    memmove(buf32 + pos, buf32 + pos + 1, sizeof(char32_t) * (len - pos));
    --len;
    refreshLine(pi);
  } else if (len == 0) {
    if (!hasHistoryProvider()) {
      --historyLen;
      free(history[historyLen]);
    }
    return -1;
  }
  return 0;
}

int InputBuffer::handleKillWordRight(PromptBase& pi, KillRing& killRing, int c) {
  if (pos < len) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    int endingPos = pos;
    while (endingPos < len && !isCharacterAlphanumeric(buf32[endingPos])) {
      ++endingPos;
    }
    while (endingPos < len && isCharacterAlphanumeric(buf32[endingPos])) {
      ++endingPos;
    }
    killRing.kill(&buf32[pos], endingPos - pos, true);
    memmove(buf32 + pos, buf32 + endingPos,
            sizeof(char32_t) * (len - endingPos + 1));
    len -= endingPos - pos;
    refreshLine(pi);
  }
  killRing.lastAction = KillRing::actionKill;
  return 0;
}

int InputBuffer::handleMoveToEnd(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  pos = len;
  refreshLine(pi);
  return 0;
}

int InputBuffer::handleMoveRight(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos < len) {
    ++pos;
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleMoveWordRight(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos < len) {
    while (pos < len && !isCharacterAlphanumeric(buf32[pos])) {
      ++pos;
    }
    while (pos < len && isCharacterAlphanumeric(buf32[pos])) {
      ++pos;
    }
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleBackspace(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos > 0) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    memmove(buf32 + pos - 1, buf32 + pos,
            sizeof(char32_t) * (1 + len - pos));
    --pos;
    --len;
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleKillWordLeft(PromptBase& pi, KillRing& killRing, int c) {
  if (pos > 0) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    int startingPos = pos;
    while (pos > 0 && !isCharacterAlphanumeric(buf32[pos - 1])) {
      --pos;
    }
    while (pos > 0 && isCharacterAlphanumeric(buf32[pos - 1])) {
      --pos;
    }
    killRing.kill(&buf32[pos], startingPos - pos, false);
    memmove(buf32 + pos, buf32 + startingPos,
            sizeof(char32_t) * (len - startingPos + 1));
    len -= startingPos - pos;
    refreshLine(pi);
  }
  killRing.lastAction = KillRing::actionKill;
  return 0;
}

int InputBuffer::handleAcceptLine(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  // we need one last refresh with the cursor at the end of the line
  // so we don't display the next prompt over the previous input line
  pos = len;  // pass len as pos for EOL
  refreshLine(pi);
  if (hasHistoryProvider()) {
    providerNavigating = false;
    providerSavedLine.clear();
  } else {
    historyPreviousIndex = historyRecallMostRecent ? historyIndex : -2;
    --historyLen;
    free(history[historyLen]);
  }
  return 1;  // accept line
}

int InputBuffer::handleKillToEnd(PromptBase& pi, KillRing& killRing, int c) {
  undoStack.save(buf32, len, pos);
  killRing.kill(&buf32[pos], len - pos, true);
  buf32[pos] = '\0';
  len = pos;
  refreshLine(pi);
  killRing.lastAction = KillRing::actionKill;
  historyRecallMostRecent = false;
  return 0;
}

int InputBuffer::handleClearScreenCmd(PromptBase& pi, KillRing& killRing, int c) {
  clearScreen(pi);
  return 0;
}

int InputBuffer::handleLowercaseWord(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos < len) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    while (pos < len && !isCharacterAlphanumeric(buf32[pos])) {
      ++pos;
    }
    while (pos < len && isCharacterAlphanumeric(buf32[pos])) {
      if (buf32[pos] >= 'A' && buf32[pos] <= 'Z') {
        buf32[pos] += 'a' - 'A';
      }
      ++pos;
    }
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleHistoryNavigate(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;

  if (hasHistoryProvider()) {
    // History provider mode
    bool goingUp = (c == UP_ARROW_KEY || c == ctrlChar('P'));

    if (goingUp) {
      if (!providerNavigating) {
        // Save current line before first navigation
        size_t tempBufferSize = sizeof(char32_t) * len + 1;
        unique_ptr<char[]> tempBuffer(new char[tempBufferSize]);
        copyString32to8(tempBuffer.get(), tempBufferSize, buf32);
        providerSavedLine = tempBuffer.get();
        providerNavigating = true;
      }
      char* entry = historyPrevCallback(providerSavedLine.c_str(), historyProviderUserData);
      if (entry) {
        size_t ucharCount = 0;
        copyString8to32(buf32, buflen, ucharCount, entry);
        free(entry);
        len = pos = static_cast<int>(ucharCount);
        historyRecallMostRecent = true;
        refreshLine(pi);
      }
      // If NULL, at oldest — do nothing
    } else {
      // Going down (newer)
      if (providerNavigating) {
        char* entry = historyNextCallback(providerSavedLine.c_str(), historyProviderUserData);
        if (entry) {
          size_t ucharCount = 0;
          copyString8to32(buf32, buflen, ucharCount, entry);
          free(entry);
          len = pos = static_cast<int>(ucharCount);
          historyRecallMostRecent = true;
          refreshLine(pi);
        } else {
          // Back to current line
          size_t ucharCount = 0;
          copyString8to32(buf32, buflen, ucharCount, providerSavedLine.c_str());
          len = pos = static_cast<int>(ucharCount);
          providerNavigating = false;
          historyRecallMostRecent = false;
          refreshLine(pi);
        }
      }
    }
    return 0;
  }

  // Internal history mode
  // if not already recalling, add the current line to the history list so
  // we don't have to special case it
  if (historyIndex == historyLen - 1) {
    free(history[historyLen - 1]);
    size_t tempBufferSize = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> tempBuffer(new char[tempBufferSize]);
    copyString32to8(tempBuffer.get(), tempBufferSize, buf32);
    history[historyLen - 1] = strdup8(tempBuffer.get());
  }
  if (historyLen > 1) {
    if (c == UP_ARROW_KEY) {
      c = ctrlChar('P');
    }
    if (historyPreviousIndex != -2 && c != ctrlChar('P')) {
      historyIndex =
          1 + historyPreviousIndex;  // emulate Windows down-arrow
    } else {
      historyIndex += (c == ctrlChar('P')) ? -1 : 1;
    }
    historyPreviousIndex = -2;
    if (historyIndex < 0) {
      historyIndex = 0;
      return 0;
    } else if (historyIndex >= historyLen) {
      historyIndex = historyLen - 1;
      return 0;
    }
    historyRecallMostRecent = true;
    size_t ucharCount = 0;
    copyString8to32(buf32, buflen, ucharCount, history[historyIndex]);
    len = pos = static_cast<int>(ucharCount);
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleHistorySearch(PromptBase& pi, KillRing& killRing, int c) {
  if (hasHistoryProvider() && !historySearchCallback) {
    // Provider set but no search callback
    beep();
    return 0;
  }
  terminatingKeystroke = incrementalHistorySearch(pi, c);
  return 0;
}

int InputBuffer::handleTranspose(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos > 0 && len > 1) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    size_t leftCharPos = (pos == len) ? pos - 2 : pos - 1;
    char32_t aux = buf32[leftCharPos];
    buf32[leftCharPos] = buf32[leftCharPos + 1];
    buf32[leftCharPos + 1] = aux;
    if (pos != len) ++pos;
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleKillToStart(PromptBase& pi, KillRing& killRing, int c) {
  if (pos > 0) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    killRing.kill(&buf32[0], pos, false);
    len -= pos;
    memmove(buf32, buf32 + pos, sizeof(char32_t) * (len + 1));
    pos = 0;
    refreshLine(pi);
  }
  killRing.lastAction = KillRing::actionKill;
  return 0;
}

int InputBuffer::handleUppercaseWord(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (pos < len) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    while (pos < len && !isCharacterAlphanumeric(buf32[pos])) {
      ++pos;
    }
    while (pos < len && isCharacterAlphanumeric(buf32[pos])) {
      if (buf32[pos] >= 'a' && buf32[pos] <= 'z') {
        buf32[pos] += 'A' - 'a';
      }
      ++pos;
    }
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleKillToWhitespace(PromptBase& pi, KillRing& killRing, int c) {
  if (pos > 0) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    int startingPos = pos;
    while (pos > 0 && buf32[pos - 1] == ' ') {
      --pos;
    }
    while (pos > 0 && buf32[pos - 1] != ' ') {
      --pos;
    }
    killRing.kill(&buf32[pos], startingPos - pos, false);
    memmove(buf32 + pos, buf32 + startingPos,
            sizeof(char32_t) * (len - startingPos + 1));
    len -= startingPos - pos;
    refreshLine(pi);
  }
  killRing.lastAction = KillRing::actionKill;
  return 0;
}

int InputBuffer::handleYank(PromptBase& pi, KillRing& killRing, int c) {
  historyRecallMostRecent = false;
  undoStack.save(buf32, len, pos);
  {
    Utf32String* restoredText = killRing.yank();
    if (restoredText) {
      bool truncated = false;
      size_t ucharCount = restoredText->length();
      if (ucharCount > static_cast<size_t>(buflen - len)) {
        ucharCount = buflen - len;
        truncated = true;
      }
      memmove(buf32 + pos + ucharCount, buf32 + pos,
              sizeof(char32_t) * (len - pos + 1));
      memmove(buf32 + pos, restoredText->get(),
              sizeof(char32_t) * ucharCount);
      pos += static_cast<int>(ucharCount);
      len += static_cast<int>(ucharCount);
      refreshLine(pi);
      killRing.lastAction = KillRing::actionYank;
      killRing.lastYankSize = ucharCount;
      if (truncated) {
        beep();
      }
    } else {
      beep();
    }
  }
  return 0;
}

int InputBuffer::handleYankPop(PromptBase& pi, KillRing& killRing, int c) {
  if (killRing.lastAction == KillRing::actionYank) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    Utf32String* restoredText = killRing.yankPop();
    if (restoredText) {
      bool truncated = false;
      size_t ucharCount = restoredText->length();
      if (ucharCount >
          static_cast<size_t>(killRing.lastYankSize + buflen - len)) {
        ucharCount = killRing.lastYankSize + buflen - len;
        truncated = true;
      }
      if (ucharCount > killRing.lastYankSize) {
        memmove(buf32 + pos + ucharCount - killRing.lastYankSize,
                buf32 + pos, sizeof(char32_t) * (len - pos + 1));
        memmove(buf32 + pos - killRing.lastYankSize, restoredText->get(),
                sizeof(char32_t) * ucharCount);
      } else {
        memmove(buf32 + pos - killRing.lastYankSize, restoredText->get(),
                sizeof(char32_t) * ucharCount);
        memmove(buf32 + pos + ucharCount - killRing.lastYankSize,
                buf32 + pos, sizeof(char32_t) * (len - pos + 1));
      }
      pos += static_cast<int>(ucharCount - killRing.lastYankSize);
      len += static_cast<int>(ucharCount - killRing.lastYankSize);
      killRing.lastYankSize = ucharCount;
      refreshLine(pi);
      if (truncated) {
        beep();
      }
      return 0;
    }
  }
  beep();
  return 0;
}

int InputBuffer::handleUndo(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  undoStack.breakCoalescing();
  if (undoStack.undo(buf32, buflen, len, pos)) {
    refreshLine(pi);
  } else {
    beep();
  }
  return 0;
}

int InputBuffer::handleSuspend(PromptBase& pi, KillRing& killRing, int c) {
#ifndef _WIN32
  disableRawMode();  // Returning to Linux (whatever) shell, leave raw mode
  raise(SIGSTOP);    // Break out in mid-line
  enableRawMode();   // Back from Linux shell, re-enter raw mode
  if (!pi.write()) return -1;  // Redraw prompt failed
  refreshLine(pi);              // Refresh the line
#endif
  return 0;
}

int InputBuffer::handleDelete(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  if (len > 0 && pos < len) {
    historyRecallMostRecent = false;
    undoStack.save(buf32, len, pos);
    memmove(buf32 + pos, buf32 + pos + 1, sizeof(char32_t) * (len - pos));
    --len;
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleInsertToggle(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  insertMode = !insertMode;
#ifndef _WIN32
  // change cursor shape: bar for insert, block for overwrite
  if (insertMode) {
    if (write(1, "\x1b[5 q", 5) == -1) return -1;  // blinking bar
  } else {
    if (write(1, "\x1b[2 q", 5) == -1) return -1;  // steady block
  }
#endif
  return 0;
}

int InputBuffer::handleBracketedPaste(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  historyRecallMostRecent = false;
  undoStack.save(buf32, len, pos);
  // Read and insert characters until paste end (ESC [ 2 0 1 ~)
  while (true) {
    char32_t pc = readUnicodeCharacter();
    if (pc == 0) break;
    if (pc == 0x1B) {  // ESC - check for paste end sequence: ESC [ 2 0 1 ~
      // Buffer consumed chars so we can insert them if it's not paste-end
      char32_t consumed[5];
      int nConsumed = 0;
      bool pasteEnd = false;
      char32_t p1 = readUnicodeCharacter();
      if (p1 == 0) break;
      consumed[nConsumed++] = p1;
      if (p1 == '[') {
        char32_t p2 = readUnicodeCharacter();
        if (p2 == 0) break;
        consumed[nConsumed++] = p2;
        if (p2 == '2') {
          char32_t p3 = readUnicodeCharacter();
          if (p3 == 0) break;
          consumed[nConsumed++] = p3;
          if (p3 == '0') {
            char32_t p4 = readUnicodeCharacter();
            if (p4 == 0) break;
            consumed[nConsumed++] = p4;
            if (p4 == '1') {
              char32_t p5 = readUnicodeCharacter();
              if (p5 == 0) break;
              consumed[nConsumed++] = p5;
              if (p5 == '~') {
                pasteEnd = true;
              }
            }
          }
        }
      }
      if (pasteEnd) {
        break;
      }
      // Not a paste end sequence; insert the consumed printable chars
      for (int ci = 0; ci < nConsumed; ++ci) {
        char32_t ch = consumed[ci];
        if (!isControlChar(ch) && len < buflen) {
          if (len == pos) {
            buf32[pos] = ch;
            ++pos;
            ++len;
            buf32[len] = '\0';
          } else {
            memmove(buf32 + pos + 1, buf32 + pos,
                    sizeof(char32_t) * (len - pos));
            buf32[pos] = ch;
            ++len;
            ++pos;
            buf32[len] = '\0';
          }
        }
      }
      continue;
    }
    // Insert character into buffer (skip control chars except tab)
    if (isControlChar(pc) && pc != '\t') {
      continue;
    }
    if (len < buflen) {
      if (len == pos) {
        buf32[pos] = pc;
        ++pos;
        ++len;
        buf32[len] = '\0';
      } else {
        memmove(buf32 + pos + 1, buf32 + pos,
                sizeof(char32_t) * (len - pos));
        buf32[pos] = pc;
        ++len;
        ++pos;
        buf32[len] = '\0';
      }
    }
  }
  recomputeCharacterWidths(buf32, charWidths, len);
  refreshLine(pi);
  return 0;
}

int InputBuffer::handleHistoryJump(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;

  if (hasHistoryProvider()) {
    // No jump-to-start/end concept in provider API
    beep();
    return 0;
  }

  // if not already recalling, add the current line to the history list so
  // we don't have to special case it
  if (historyIndex == historyLen - 1) {
    free(history[historyLen - 1]);
    size_t tempBufferSize = sizeof(char32_t) * len + 1;
    unique_ptr<char[]> tempBuffer(new char[tempBufferSize]);
    copyString32to8(tempBuffer.get(), tempBufferSize, buf32);
    history[historyLen - 1] = strdup8(tempBuffer.get());
  }
  if (historyLen > 1) {
    historyIndex =
        (c == META + '<' || c == PAGE_UP_KEY) ? 0 : historyLen - 1;
    historyPreviousIndex = -2;
    historyRecallMostRecent = true;
    size_t ucharCount = 0;
    copyString8to32(buf32, buflen, ucharCount, history[historyIndex]);
    len = pos = static_cast<int>(ucharCount);
    refreshLine(pi);
  }
  return 0;
}

int InputBuffer::handleMacro(PromptBase& pi, KillRing& killRing, int c) {
  killRing.lastAction = KillRing::actionOther;
  // Read the next character to determine macro sub-command
  c = linenoiseReadChar();
  if (c <= 0) {
    return 0;
  }
  if (c == '(' || c == ')' || c == 'e' || c == 'E') {
    // These are macro control keys — not recorded
  } else {
    beep();
    return 0;
  }
  if (c == '(') {
    // Start recording
    macroRecorder.startRecording();
  } else if (c == ')') {
    // Stop recording
    macroRecorder.stopRecording();
  } else if (c == 'e' || c == 'E') {
    // Execute macro: push recorded keystrokes onto pending queue
    const std::vector<int>& macro = macroRecorder.getMacro();
    if (!macro.empty()) {
      pendingKeystrokes.insert(pendingKeystrokes.end(), macro.begin(), macro.end());
    }
  }
  return 0;
}

int InputBuffer::getInputLine(PromptBase& pi) {
  keyType = 0;
  insertMode = true;  // always start in insert mode
  undoStack.clear();

  // The latest history entry is always our current buffer
  if (hasHistoryProvider()) {
    if (historyResetCallback) {
      historyResetCallback(historyProviderUserData);
    }
    providerNavigating = false;
    providerSavedLine.clear();
  } else {
    if (len > 0) {
      size_t bufferSize = sizeof(char32_t) * len + 1;
      unique_ptr<char[]> tempBuffer(new char[bufferSize]);
      copyString32to8(tempBuffer.get(), bufferSize, buf32);
      linenoiseHistoryAdd(tempBuffer.get());
    } else {
      linenoiseHistoryAdd("");
    }
    historyIndex = historyLen - 1;
  }
  historyRecallMostRecent = false;

  // display the prompt
  if (!pi.write()) return -1;

#ifndef _WIN32
  // we have to generate our own newline on line wrap on Linux
  if (pi.promptIndentation == 0 && pi.promptExtraLines > 0)
    if (write(1, "\n", 1) == -1) return -1;
#endif

  // the cursor starts out at the end of the prompt
  pi.promptCursorRowOffset = pi.promptExtraLines;

  // kill and yank start in "other" mode
  killRing.lastAction = KillRing::actionOther;

  // when history search returns control to us, we execute its terminating
  // keystroke
  terminatingKeystroke = -1;

  // initialize dispatch table on first use
  initDefaultBindings();

  // if there is already text in the buffer, display it first
  if (len > 0) {
    refreshLine(pi);
  }

  // loop collecting characters, respond to line editing characters
  while (true) {
    int c;
    if (terminatingKeystroke == -1) {
      // Check pending keystrokes from macro replay first
      if (!pendingKeystrokes.empty()) {
        c = pendingKeystrokes.front();
        pendingKeystrokes.erase(pendingKeystrokes.begin());
      } else {
        c = linenoiseReadChar();  // get a new keystroke
      }

      keyType = 0;
      if (c != 0) {
        // set flag that we got some input
        if (c == TIMEOUT_KEY) {
          keyType = 3;
        } else if (c == ctrlChar('C')) {
          keyType = 1;
        } else if (c == ctrlChar('D')) {
          keyType = 2;
        }
      }

#ifndef _WIN32
      if (c == 0 && gotResize) {
        // caught a window resize event
        // now redraw the prompt and line
        gotResize = false;
        pi.promptScreenColumns = getScreenColumns();
        dynamicRefresh(pi, buf32, len,
                       pos);  // redraw the original prompt with current input
        continue;
      }
#endif
    } else {
      c = terminatingKeystroke;   // use the terminating keystroke from search
      terminatingKeystroke = -1;  // clear it once we've used it
    }

    c = cleanupCtrl(c);  // convert CTRL + <char> into normal ctrl

    if (c == 0) {
      return len;
    }

    if (c == -1) {
      refreshLine(pi);
      continue;
    }

    if (c == -2) {
      if (!pi.write()) return -1;
      refreshLine(pi);
      continue;
    }

    // Record keystroke for macro (skip the macro chord key itself)
    if (macroRecorder.isRecording() && c != ctrlChar('X')) {
      macroRecorder.addKeystroke(c);
    }

    // ctrl-I/tab, command completion, needs to be before switch statement
    if (c == ctrlChar('I') && (completionCallback || filenameCompletionEnabled)) {
      if (pos == 0)  // SERVER-4967 -- in earlier versions, you could paste
                     // previous output
        continue;    //  back into the shell ... this output may have leading
                     //  tabs.
      // This hack (i.e. what the old code did) prevents command completion
      //  on an empty line but lets users paste text with leading tabs.

      killRing.lastAction = KillRing::actionOther;
      historyRecallMostRecent = false;

      // completeLine does the actual completion and replacement
      c = completeLine(pi);

      if (c < 0)  // return on error
        return len;

      if (c == 0)  // read next character when 0
        continue;

      // deliberate fall-through here, so we use the terminating character
    }

    // Check user key bindings first (they take priority)
    auto uit = userKeyBindings.find(c);
    if (uit != userKeyBindings.end()) {
      killRing.lastAction = KillRing::actionOther;
      historyRecallMostRecent = false;
      // Convert current buffer to UTF-8 for the callback
      size_t cbBuf8Size = sizeof(char32_t) * len + 1;
      unique_ptr<char[]> cbBuf8(new char[cbBuf8Size]);
      copyString32to8(cbBuf8.get(), cbBuf8Size, buf32);
      char* newLine = NULL;
      int newPos = pos;
      uit->second.callback(cbBuf8.get(), pos, &newLine, &newPos, uit->second.userData);
      if (newLine) {
        // Convert UTF-8 result back to char32_t
        size_t ucharCount = 0;
        copyString8to32(buf32, buflen + 1, ucharCount, newLine);
        len = static_cast<int>(ucharCount);
        free(newLine);
        recomputeCharacterWidths(buf32, charWidths, len);
      }
      if (newPos >= 0 && newPos <= len) {
        pos = newPos;
      }
      refreshLine(pi);
    } else {
      // Look up handler in default dispatch table
      auto it = defaultKeyBindings.find(c);
      if (it != defaultKeyBindings.end()) {
        int rv = (this->*(it->second))(pi, killRing, c);
        if (rv != 0) {
          return rv > 0 ? len : -1;
        }
      } else {
        // default: self-insert printable characters
        killRing.lastAction = KillRing::actionOther;
        historyRecallMostRecent = false;
        if (providerNavigating) {
          providerNavigating = false;
          providerSavedLine.clear();
          if (historyResetCallback) {
            historyResetCallback(historyProviderUserData);
          }
        }
        if (c & (META | CTRL)) {  // beep on unknown Ctrl and/or Meta keys
          beep();
          continue;
        }
        undoStack.save(buf32, len, pos, true);  // coalesce consecutive inserts
        bool didDedent = tryAutoDedent(c);
        if (len < buflen) {
          if (isControlChar(c)) {  // don't insert control characters
            beep();
            continue;
          }
          if (len == pos) {  // at end of buffer (insert in both modes)
            buf32[pos] = c;
            ++pos;
            ++len;
            buf32[len] = '\0';
            if (maskMode || didDedent) {
              // mask mode or dedent changed the buffer; must redraw entire line
              refreshLine(pi);
            } else {
              int inputLen = calculateColumnPosition(buf32, len);
              if (pi.promptIndentation + inputLen < pi.promptScreenColumns) {
                if (inputLen > pi.promptPreviousInputLen)
                  pi.promptPreviousInputLen = inputLen;
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (write32(1, reinterpret_cast<char32_t*>(&c), 1) == -1)
                  return -1;
              } else {
                refreshLine(pi);
              }
            }
          } else if (!insertMode) {  // overwrite mode: replace char in place
            buf32[pos] = c;
            ++pos;
            refreshLine(pi);
          } else {  // insert mode, not at end of buffer: move chars to our right
            memmove(buf32 + pos + 1, buf32 + pos,
                    sizeof(char32_t) * (len - pos));
            buf32[pos] = c;
            ++len;
            ++pos;
            buf32[len] = '\0';
            refreshLine(pi);
          }
        } else {
          beep();  // buffer is full, beep on new characters
        }
      }
    }
  }
  return len;
}

static string preloadedBufferContents;  // used with linenoisePreloadBuffer
static string preloadErrorMessage;

// Auto-dedent configuration — set once at init via linenoiseSetAutoDedent(),
// then only read during input.  Same thread-safety model as preloadedBufferContents:
// the Qore binding serialises writes; reads happen on the single input thread.
static string autoDedentIndentStr;
static char32_t autoDedentChar = 0;

bool InputBuffer::tryAutoDedent(char32_t c) {
  if (autoDedentChar == 0 || c != autoDedentChar || autoDedentIndentStr.empty()) {
    return false;
  }
  // Check: buffer must contain only whitespace up to current position
  for (int i = 0; i < pos; i++) {
    if (buf32[i] != ' ' && buf32[i] != '\t') {
      return false;
    }
  }
  // Check: nothing after cursor (or only whitespace)
  for (int i = pos; i < len; i++) {
    if (buf32[i] != ' ' && buf32[i] != '\t') {
      return false;
    }
  }
  // Check buffer starts with at least one indent string
  size_t indentLen = autoDedentIndentStr.length();
  if (static_cast<size_t>(len) < indentLen) {
    return false;
  }
  for (size_t i = 0; i < indentLen; i++) {
    if (buf32[i] != static_cast<char32_t>(autoDedentIndentStr[i])) {
      return false;
    }
  }
  // Remove one indent from start of buffer
  int removeCount = static_cast<int>(indentLen);
  memmove(buf32, buf32 + removeCount, sizeof(char32_t) * (len - removeCount));
  memmove(charWidths, charWidths + removeCount, sizeof(char) * (len - removeCount));
  len -= removeCount;
  pos -= removeCount;
  buf32[len] = '\0';
  return true;
}

/**
 * linenoisePreloadBuffer provides text to be inserted into the command buffer
 *
 * the provided text will be processed to be usable and will be used to preload
 * the input buffer on the next call to linenoise()
 *
 * @param preloadText text to begin with on the next call to linenoise()
 */
void linenoisePreloadBuffer(const char* preloadText) {
  if (!preloadText) {
    return;
  }
  int bufferSize = static_cast<int>(strlen(preloadText) + 1);
  unique_ptr<char[]> tempBuffer(new char[bufferSize]);
  strncpy(&tempBuffer[0], preloadText, bufferSize);

  // remove characters that won't display correctly
  char* pIn = &tempBuffer[0];
  char* pOut = pIn;
  bool controlsStripped = false;
  bool whitespaceSeen = false;
  while (*pIn) {
    unsigned char c =
        *pIn++;       // we need unsigned so chars 0x80 and above are allowed
    if ('\r' == c) {  // silently skip CR
      continue;
    }
    if ('\n' == c || '\t' == c) {  // note newline or tab
      whitespaceSeen = true;
      continue;
    }
    if (isControlChar(
            c)) {  // remove other control characters, flag for message
      controlsStripped = true;
      *pOut++ = ' ';
      continue;
    }
    if (whitespaceSeen) {  // convert whitespace to a single space
      *pOut++ = ' ';
      whitespaceSeen = false;
    }
    *pOut++ = c;
  }
  *pOut = 0;
  int processedLength = static_cast<int>(pOut - tempBuffer.get());
  bool lineTruncated = false;
  if (processedLength > (LINENOISE_MAX_LINE - 1)) {
    lineTruncated = true;
    tempBuffer[LINENOISE_MAX_LINE - 1] = 0;
  }
  preloadedBufferContents = tempBuffer.get();
  if (controlsStripped) {
    preloadErrorMessage +=
        " [Edited line: control characters were converted to spaces]\n";
  }
  if (lineTruncated) {
    preloadErrorMessage += " [Edited line: the line length was reduced from ";
    char buf[128];
    snprintf(buf, sizeof(buf), "%d to %d]\n", processedLength,
             (LINENOISE_MAX_LINE - 1));
    preloadErrorMessage += buf;
  }
}

/**
 * linenoiseSetAutoDedent configures auto-dedent behavior
 *
 * When the dedent character is typed and the edit buffer contains only
 * whitespace (i.e., only auto-indentation), one indent level is removed
 * before the character is inserted.
 *
 * @param indentStr the indentation string (e.g. "    " for 4 spaces)
 * @param dedentChar the character that triggers dedent (e.g. '}')
 */
void linenoiseSetAutoDedent(const char* indentStr, char32_t dedentChar) {
  if (indentStr) {
    autoDedentIndentStr = indentStr;
  } else {
    autoDedentIndentStr.clear();
  }
  autoDedentChar = dedentChar;
}

/**
 * linenoise is a readline replacement.
 *
 * call it with a prompt to display and it will return a line of input from the
 * user
 *
 * @param prompt text of prompt to display to the user
 * @return       the returned string belongs to the caller on return and must be
 * freed to prevent
 *               memory leaks
 */
char* linenoise(const char* prompt) {
#ifndef _WIN32
  gotResize = false;
#endif
  if (isatty(STDIN_FILENO)) {  // input is from a terminal
    char32_t buf32[LINENOISE_MAX_LINE];
    char charWidths[LINENOISE_MAX_LINE];
    if (!preloadErrorMessage.empty()) {
      printf("%s", preloadErrorMessage.c_str());
      fflush(stdout);
      preloadErrorMessage.clear();
    }
    PromptInfo pi(prompt, getScreenColumns());
    if (isUnsupportedTerm()) {
      if (!pi.write()) return 0;
      fflush(stdout);
      if (preloadedBufferContents.empty()) {
        unique_ptr<char[]> buf8(new char[LINENOISE_MAX_LINE]);
        if (fgets(buf8.get(), LINENOISE_MAX_LINE, stdin) == NULL) {
          return NULL;
        }
        size_t len = strlen(buf8.get());
        while (len && (buf8[len - 1] == '\n' || buf8[len - 1] == '\r')) {
          --len;
          buf8[len] = '\0';
        }
        return strdup(buf8.get());  // caller must free buffer
      } else {
        char* buf8 = strdup(preloadedBufferContents.c_str());
        preloadedBufferContents.clear();
        return buf8;  // caller must free buffer
      }
    } else {
      if (enableRawMode() == -1) {
        return NULL;
      }
      InputBuffer ib(buf32, charWidths, LINENOISE_MAX_LINE);
      if (!preloadedBufferContents.empty()) {
        ib.preloadBuffer(preloadedBufferContents.c_str());
        preloadedBufferContents.clear();
      }
      int count = ib.getInputLine(pi);
      disableRawMode();
      printf("\n");
      if (count == -1) {
        return NULL;
      }
      size_t bufferSize = sizeof(char32_t) * ib.length() + 1;
      unique_ptr<char[]> buf8(new char[bufferSize]);
      copyString32to8(buf8.get(), bufferSize, buf32);
      return strdup(buf8.get());  // caller must free buffer
    }
  } else {  // input not from a terminal, we should work with piped input, i.e.
            // redirected stdin
    unique_ptr<char[]> buf8(new char[LINENOISE_MAX_LINE]);
    if (fgets(buf8.get(), LINENOISE_MAX_LINE, stdin) == NULL) {
      return NULL;
    }

    // if fgets() gave us the newline, remove it
    int count = static_cast<int>(strlen(buf8.get()));
    if (count > 0 && buf8[count - 1] == '\n') {
      --count;
      buf8[count] = '\0';
    }
    return strdup(buf8.get());  // caller must free buffer
  }
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback* fn) {
  completionCallback = fn;
}

void linenoiseSetHintsCallback(linenoiseHintsCallback* fn) {
  hintsCallback = fn;
}

void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback* fn) {
  freeHintsCallback = fn;
}

void linenoiseSetSyntaxCallback(linenoiseSyntaxCallback* fn) {
  syntaxCallback = fn;
}

void linenoiseSetRightPrompt(const char* prompt) {
  if (prompt) {
    rightPromptText = prompt;
  } else {
    rightPromptText.clear();
  }
}

void linenoiseSetHistoryProvider(
    linenoiseHistoryProviderCallback prevCb,
    linenoiseHistoryProviderCallback nextCb,
    linenoiseHistoryResetCallback resetCb,
    linenoiseHistorySearchCallback searchCb,
    void* userData
) {
  historyPrevCallback = prevCb;
  historyNextCallback = nextCb;
  historyResetCallback = resetCb;
  historySearchCallback = searchCb;
  historyProviderUserData = userData;
  providerNavigating = false;
  providerSavedLine.clear();
}

void linenoiseSetUserKeyCallback(linenoiseUserKeyCallback cb) {
  globalUserKeyCallback = cb;
}

void linenoiseBindKey(int keyCode, void* userData) {
  if (globalUserKeyCallback) {
    UserKeyBinding binding;
    binding.callback = globalUserKeyCallback;
    binding.userData = userData;
    userKeyBindings[keyCode] = binding;
  }
}

void linenoiseUnbindKey(int keyCode) {
  userKeyBindings.erase(keyCode);
}

void linenoiseAddCompletion(linenoiseCompletions* lc, const char* str) {
  lc->completionStrings.push_back(Utf32String(str));
}

int linenoiseHistoryAdd(const char* line) {
  if (historyMaxLen == 0) {
    return 0;
  }
  if (history == NULL) {
    history =
        reinterpret_cast<char8_t**>(malloc(sizeof(char8_t*) * historyMaxLen));
    if (history == NULL) {
      return 0;
    }
    memset(history, 0, (sizeof(char*) * historyMaxLen));
  }
  char8_t* linecopy = strdup8(line);
  if (!linecopy) {
    return 0;
  }

  // convert newlines in multi-line code to spaces before storing
  char8_t* p = linecopy;
  while (*p) {
    if (*p == '\n') {
      *p = ' ';
    }
    ++p;
  }

  // prevent duplicate history entries
  if (historyLen > 0 && history[historyLen - 1] != nullptr &&
      strcmp(reinterpret_cast<char const*>(history[historyLen - 1]),
             reinterpret_cast<char const*>(linecopy)) == 0) {
    free(linecopy);
    return 0;
  }

  if (historyLen == historyMaxLen) {
    free(history[0]);
    memmove(history, history + 1, sizeof(char*) * (historyMaxLen - 1));
    --historyLen;
    if (--historyPreviousIndex < -1) {
      historyPreviousIndex = -2;
    }
  }

  history[historyLen] = linecopy;
  ++historyLen;
  return 1;
}

int linenoiseHistorySetMaxLen(int len) {
  if (len < 1) {
    return 0;
  }
  if (history) {
    int tocopy = historyLen;
    char8_t** newHistory =
        reinterpret_cast<char8_t**>(malloc(sizeof(char8_t*) * len));
    if (newHistory == NULL) {
      return 0;
    }
    if (len < tocopy) {
      // Free entries that will be dropped (the oldest ones)
      for (int i = 0; i < historyLen - len; ++i) {
        free(history[i]);
      }
      tocopy = len;
    }
    memcpy(newHistory, history + historyLen - tocopy,
           sizeof(char8_t*) * tocopy);
    free(history);
    history = newHistory;
  }
  historyMaxLen = len;
  if (historyLen > historyMaxLen) {
    historyLen = historyMaxLen;
  }
  return 1;
}

int linenoiseHistoryGetMaxLen() {
    return historyMaxLen;
}

/* Fetch a line of the history by (zero-based) index.  If the requested
 * line does not exist, NULL is returned.  The return value is a heap-allocated
 * copy of the line, and the caller is responsible for de-allocating it. */
char* linenoiseHistoryLine(int index) {
  if (index < 0 || index >= historyLen) return NULL;

  return strdup(reinterpret_cast<char const*>(history[index]));
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char* filename) {
#if _WIN32
  FILE* fp = fopen(filename, "wt");
#else
  int fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    return -1;
  }

  FILE* fp = fdopen(fd, "wt");
#endif

  if (fp == NULL) {
    return -1;
  }

  for (int j = 0; j < historyLen; ++j) {
    if (history[j][0] != '\0') {
      fprintf(fp, "%s\n", history[j]);
    }
  }

  fclose(fp);

  return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const char* filename) {
  FILE* fp = fopen(filename, "rt");
  if (fp == NULL) {
    return -1;
  }

  char buf[LINENOISE_MAX_LINE];
  while (fgets(buf, LINENOISE_MAX_LINE, fp) != NULL) {
    char* p = strchr(buf, '\r');
    if (!p) {
      p = strchr(buf, '\n');
    }
    if (p) {
      *p = '\0';
    }
    if (p != buf) {
      linenoiseHistoryAdd(buf);
    }
  }
  fclose(fp);
  return 0;
}

/* Set if to use or not the multi line mode. */
/* note that this is a stub only, as linenoise-ng always multi-line */
void linenoiseSetMultiLine(int) {}

/* This special mode is used by linenoise in order to print scan codes
 * on screen for debugging / development purposes. It is implemented
 * by the linenoise_example program using the --keycodes option. */
void linenoisePrintKeyCodes(void) {
  char quit[4];

  printf(
      "Linenoise key codes debugging mode.\n"
      "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
  if (enableRawMode() == -1) return;
  memset(quit, ' ', 4);
  while (1) {
    char c;
    int nread;

#if _WIN32
    nread = _read(STDIN_FILENO, &c, 1);
#else
    nread = read(STDIN_FILENO, &c, 1);
#endif
    if (nread <= 0) continue;
    memmove(quit, quit + 1, sizeof(quit) - 1); /* shift string to left. */
    quit[sizeof(quit) - 1] = c; /* Insert current char on the right. */
    if (memcmp(quit, "quit", sizeof(quit)) == 0) break;

    printf("'%c' %02x (%d) (type quit to exit)\n", isprint(c) ? c : '?', (int)c,
           (int)c);
    printf("\r"); /* Go left edge manually, we are in raw mode. */
    fflush(stdout);
  }
  disableRawMode();
}

#ifndef _WIN32
static void WindowSizeChanged(int) {
  // do nothing here but setting this flag
  gotResize = true;
}
#endif

int linenoiseInstallWindowChangeHandler(void) {
#ifndef _WIN32
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = &WindowSizeChanged;

  if (sigaction(SIGWINCH, &sa, nullptr) == -1) {
    return errno;
  }
#endif
  return 0;
}

int linenoiseKeyType(void) {
  return keyType;
}

int linenoiseColumns(void) {
  return getScreenColumns();
}

int linenoiseRows(void) {
  return getScreenRows();
}

// ============================================================================
// Async (non-blocking) line editing API
// ============================================================================

// Input state machine for async byte-by-byte processing
enum AsyncInputMode {
  ASYNC_NORMAL,    // Normal input
  ASYNC_ESC,       // ESC received, waiting for next byte
  ASYNC_ESC_SEQ,   // In escape sequence (escBuf being filled)
  ASYNC_UTF8,      // Accumulating UTF-8 bytes
};

struct linenoiseState {
  // Editing buffer
  char32_t buf32[LINENOISE_MAX_LINE];
  char charWidths[LINENOISE_MAX_LINE];
  int len;
  int pos;
  bool insertMode;

  // Terminal state
  bool rawModeEnabled;
  bool silent;

  // Result tracking
  int result;      // LN_FEED_MORE until resolved
  int keyType;

  // Input state machine
  AsyncInputMode inputMode;

  // UTF-8 byte accumulator
  unsigned char utf8Buf[4];
  int utf8Len;       // bytes accumulated so far
  int utf8Expected;  // total bytes expected for current char

  // Escape sequence accumulator
  unsigned char escBuf[16];
  int escLen;

  // Prompt
  std::string prompt;

  // UTF-8 buffer cache for linenoiseEditGetBuffer
  mutable std::string getBufferCache;
  mutable bool getBufferCacheDirty;
};

// Determine how many bytes a UTF-8 leading byte expects
static int utf8ExpectedBytes(unsigned char c) {
  if ((c & 0x80) == 0) return 1;       // 0xxxxxxx — ASCII
  if ((c & 0xE0) == 0xC0) return 2;    // 110xxxxx
  if ((c & 0xF0) == 0xE0) return 3;    // 1110xxxx
  if ((c & 0xF8) == 0xF0) return 4;    // 11110xxx
  return 1;  // invalid leading byte — treat as single byte
}

// Decode accumulated UTF-8 bytes into a char32_t
static char32_t utf8Decode(const unsigned char* buf, int len) {
  if (len == 1) return buf[0];
  if (len == 2) {
    return ((buf[0] & 0x1F) << 6) | (buf[1] & 0x3F);
  }
  if (len == 3) {
    return ((buf[0] & 0x0F) << 12) | ((buf[1] & 0x3F) << 6) | (buf[2] & 0x3F);
  }
  if (len == 4) {
    return ((buf[0] & 0x07) << 18) | ((buf[1] & 0x3F) << 12) |
           ((buf[2] & 0x3F) << 6) | (buf[3] & 0x3F);
  }
  return 0;
}

// Simple terminal refresh for async state
static void asyncRefreshLine(linenoiseState* state) {
  if (state->silent) return;
  // \r moves to start of line, ESC[K clears to end
  std::string output = "\r\x1b[K";
  output += state->prompt;
  // Convert buffer to UTF-8 for display
  size_t bufSize = sizeof(char32_t) * state->len + 1;
  unique_ptr<char[]> utf8(new char[bufSize]);
  copyString32to8(utf8.get(), bufSize, state->buf32);
  output += utf8.get();
  // Position cursor: move back (len - pos) columns
  if (state->pos < state->len) {
    // Calculate column distance from pos to end
    int cols = 0;
    for (int i = state->pos; i < state->len; ++i) {
      cols += state->charWidths[i] ? state->charWidths[i] : 1;
    }
    if (cols > 0) {
      char moveBuf[32];
      snprintf(moveBuf, sizeof(moveBuf), "\x1b[%dD", cols);
      output += moveBuf;
    }
  }
  if (write(1, output.c_str(), output.size()) == -1) {
    // write error — ignore in async mode
  }
}

// Process a decoded character in the async editing state
static int asyncProcessChar(linenoiseState* state, char32_t c) {
  // Handle Enter
  if (c == '\r' || c == '\n') {
    // Move cursor to end for clean display
    state->pos = state->len;
    asyncRefreshLine(state);
    state->result = LN_FEED_DONE;
    state->keyType = 0;
    return LN_FEED_DONE;
  }

  // Handle Ctrl+C — abort
  if (c == 3) {
    state->pos = state->len;
    asyncRefreshLine(state);
    if (!state->silent) {
      if (write(1, "^C", 2) == -1) {
        // ignore
      }
    }
    state->result = LN_FEED_ABORT;
    state->keyType = 1;
    return LN_FEED_ABORT;
  }

  // Handle Ctrl+D — exit on empty, delete otherwise
  if (c == 4) {
    if (state->len == 0) {
      state->result = LN_FEED_EXIT;
      state->keyType = 2;
      return LN_FEED_EXIT;
    }
    // Delete char under cursor
    if (state->pos < state->len) {
      memmove(state->buf32 + state->pos, state->buf32 + state->pos + 1,
              sizeof(char32_t) * (state->len - state->pos));
      --state->len;
      state->buf32[state->len] = '\0';
      recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
      state->getBufferCacheDirty = true;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Backspace (Ctrl+H or 127)
  if (c == 8 || c == 127) {
    if (state->pos > 0) {
      memmove(state->buf32 + state->pos - 1, state->buf32 + state->pos,
              sizeof(char32_t) * (state->len - state->pos + 1));
      --state->pos;
      --state->len;
      recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
      state->getBufferCacheDirty = true;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+A — move to start
  if (c == 1) {
    if (state->pos > 0) {
      state->pos = 0;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+E — move to end
  if (c == 5) {
    if (state->pos < state->len) {
      state->pos = state->len;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+B — move left
  if (c == 2) {
    if (state->pos > 0) {
      --state->pos;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+F — move right
  if (c == 6) {
    if (state->pos < state->len) {
      ++state->pos;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+K — kill to end of line
  if (c == 11) {
    state->buf32[state->pos] = '\0';
    state->len = state->pos;
    recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
    state->getBufferCacheDirty = true;
    asyncRefreshLine(state);
    return LN_FEED_MORE;
  }

  // Handle Ctrl+U — kill to start of line
  if (c == 21) {
    if (state->pos > 0) {
      memmove(state->buf32, state->buf32 + state->pos,
              sizeof(char32_t) * (state->len - state->pos + 1));
      state->len -= state->pos;
      state->pos = 0;
      recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
      state->getBufferCacheDirty = true;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+L — clear screen and redraw
  if (c == 12) {
    if (!state->silent) {
      if (write(1, "\x1b[H\x1b[2J", 7) == -1) {
        // ignore
      }
    }
    asyncRefreshLine(state);
    return LN_FEED_MORE;
  }

  // Handle Ctrl+T — transpose
  if (c == 20) {
    if (state->pos > 0 && state->len > 1) {
      int swapPos = (state->pos == state->len) ? state->pos - 2 : state->pos - 1;
      char32_t tmp = state->buf32[swapPos];
      state->buf32[swapPos] = state->buf32[swapPos + 1];
      state->buf32[swapPos + 1] = tmp;
      if (state->pos != state->len) {
        ++state->pos;
      }
      recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
      state->getBufferCacheDirty = true;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Handle Ctrl+W — kill word left
  if (c == 23) {
    if (state->pos > 0) {
      int startingPos = state->pos;
      while (state->pos > 0 && state->buf32[state->pos - 1] == ' ') {
        --state->pos;
      }
      while (state->pos > 0 && state->buf32[state->pos - 1] != ' ') {
        --state->pos;
      }
      memmove(state->buf32 + state->pos, state->buf32 + startingPos,
              sizeof(char32_t) * (state->len - startingPos + 1));
      state->len -= startingPos - state->pos;
      recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
      state->getBufferCacheDirty = true;
      asyncRefreshLine(state);
    }
    return LN_FEED_MORE;
  }

  // Skip other control characters
  if (c < 32 || c == 0x7F) {
    return LN_FEED_MORE;
  }

  // Self-insert printable character
  if (state->len < LINENOISE_MAX_LINE - 1) {
    if (state->len == state->pos) {
      // Append at end
      state->buf32[state->pos] = c;
      ++state->pos;
      ++state->len;
      state->buf32[state->len] = '\0';
    } else if (state->insertMode) {
      // Insert in middle
      memmove(state->buf32 + state->pos + 1, state->buf32 + state->pos,
              sizeof(char32_t) * (state->len - state->pos));
      state->buf32[state->pos] = c;
      ++state->len;
      ++state->pos;
      state->buf32[state->len] = '\0';
    } else {
      // Overwrite
      state->buf32[state->pos] = c;
      ++state->pos;
    }
    recomputeCharacterWidths(state->buf32, state->charWidths, state->len);
    state->getBufferCacheDirty = true;
    asyncRefreshLine(state);
  }

  return LN_FEED_MORE;
}

// Process an escape sequence from accumulated bytes
static int asyncProcessEscape(linenoiseState* state) {
  if (state->escLen < 2) return LN_FEED_MORE;

  // ESC [ sequences
  if (state->escBuf[0] == '[') {
    if (state->escLen == 2) {
      switch (state->escBuf[1]) {
        case 'A':  // Up arrow — no history in async mode
          beep();
          return LN_FEED_MORE;
        case 'B':  // Down arrow — no history in async mode
          beep();
          return LN_FEED_MORE;
        case 'C':  // Right arrow
          return asyncProcessChar(state, 6);  // same as Ctrl+F
        case 'D':  // Left arrow
          return asyncProcessChar(state, 2);  // same as Ctrl+B
        case 'H':  // Home
          return asyncProcessChar(state, 1);  // same as Ctrl+A
        case 'F':  // End
          return asyncProcessChar(state, 5);  // same as Ctrl+E
        default:
          break;
      }
    }
    // ESC [ 3 ~ — Delete
    if (state->escLen == 3 && state->escBuf[1] == '3' && state->escBuf[2] == '~') {
      return asyncProcessChar(state, 4);  // same as Ctrl+D (delete)
    }
    // ESC [ 1 ; 5 C/D — Ctrl+Right/Left (word movement)
    if (state->escLen == 5 && state->escBuf[1] == '1' && state->escBuf[2] == ';' &&
        state->escBuf[3] == '5') {
      if (state->escBuf[4] == 'C') {
        // Ctrl+Right — move word right
        while (state->pos < state->len && state->buf32[state->pos] == ' ') {
          ++state->pos;
        }
        while (state->pos < state->len && state->buf32[state->pos] != ' ') {
          ++state->pos;
        }
        asyncRefreshLine(state);
        return LN_FEED_MORE;
      }
      if (state->escBuf[4] == 'D') {
        // Ctrl+Left — move word left
        while (state->pos > 0 && state->buf32[state->pos - 1] == ' ') {
          --state->pos;
        }
        while (state->pos > 0 && state->buf32[state->pos - 1] != ' ') {
          --state->pos;
        }
        asyncRefreshLine(state);
        return LN_FEED_MORE;
      }
    }
  }
  // ESC O sequences (alternative Home/End)
  else if (state->escBuf[0] == 'O') {
    if (state->escLen == 2) {
      switch (state->escBuf[1]) {
        case 'H':  // Home
          return asyncProcessChar(state, 1);
        case 'F':  // End
          return asyncProcessChar(state, 5);
        default:
          break;
      }
    }
  }

  // Unknown sequence — discard
  state->escLen = 0;
  return LN_FEED_MORE;
}

linenoiseState* linenoiseEditStart(const char* prompt) {
  linenoiseState* state = new linenoiseState();
  // Initialize editing buffer (only first element matters; len=0)
  state->buf32[0] = 0;
  state->charWidths[0] = 0;
  state->len = 0;
  state->pos = 0;
  state->insertMode = true;
  state->rawModeEnabled = false;
  state->silent = false;
  state->result = LN_FEED_MORE;
  state->keyType = 0;
  state->inputMode = ASYNC_NORMAL;
  state->utf8Len = 0;
  state->utf8Expected = 0;
  state->escLen = 0;
  state->getBufferCacheDirty = true;

  if (prompt) {
    state->prompt = prompt;
  }

  if (rawModeRefCount == 0) {
    if (enableRawMode() == 0) {
      state->rawModeEnabled = true;
    }
  } else {
    // Raw mode already active from another session
    state->rawModeEnabled = true;
  }
  if (state->rawModeEnabled) {
    ++rawModeRefCount;
  }

  // Display prompt
  if (!state->silent && !state->prompt.empty()) {
    if (write(1, state->prompt.c_str(), state->prompt.size()) == -1) {
      // ignore write error
    }
  }

  return state;
}

int linenoiseEditFeed(linenoiseState* state, char c) {
  if (!state) return LN_FEED_ABORT;
  if (state->result != LN_FEED_MORE) return state->result;

  unsigned char uc = static_cast<unsigned char>(c);

  switch (state->inputMode) {
    case ASYNC_ESC:
      // First byte after ESC — determines sequence type
      state->escBuf[0] = uc;
      state->escLen = 1;
      if (uc == '[' || uc == 'O') {
        state->inputMode = ASYNC_ESC_SEQ;
        return LN_FEED_MORE;
      }
      // Unknown ESC + char — discard
      state->escLen = 0;
      state->inputMode = ASYNC_NORMAL;
      return LN_FEED_MORE;

    case ASYNC_ESC_SEQ: {
      // Accumulating escape sequence bytes
      if (state->escLen < 15) {
        state->escBuf[state->escLen++] = uc;
      } else {
        // Overflow — discard
        state->escLen = 0;
        state->inputMode = ASYNC_NORMAL;
        return LN_FEED_MORE;
      }
      // Check if sequence is complete (ends with letter or ~)
      if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || uc == '~') {
        int rv = asyncProcessEscape(state);
        state->escLen = 0;
        state->inputMode = ASYNC_NORMAL;
        return rv;
      }
      // Still accumulating (digits, semicolons)
      return LN_FEED_MORE;
    }

    case ASYNC_UTF8:
      // Accumulating UTF-8 continuation bytes
      if ((uc & 0xC0) == 0x80) {
        state->utf8Buf[state->utf8Len++] = uc;
        if (state->utf8Len == state->utf8Expected) {
          char32_t decoded = utf8Decode(state->utf8Buf, state->utf8Len);
          state->utf8Len = 0;
          state->utf8Expected = 0;
          state->inputMode = ASYNC_NORMAL;
          return asyncProcessChar(state, decoded);
        }
        return LN_FEED_MORE;
      }
      // Invalid continuation — reset and fall through
      state->utf8Len = 0;
      state->utf8Expected = 0;
      state->inputMode = ASYNC_NORMAL;
      break;  // fall through to ASYNC_NORMAL processing

    case ASYNC_NORMAL:
      break;  // handled below
  }

  // ASYNC_NORMAL: process a new byte

  // ESC starts an escape sequence
  if (uc == 0x1B) {
    state->inputMode = ASYNC_ESC;
    return LN_FEED_MORE;
  }

  // Check for multi-byte UTF-8 sequence start
  int expected = utf8ExpectedBytes(uc);
  if (expected > 1) {
    state->utf8Buf[0] = uc;
    state->utf8Len = 1;
    state->utf8Expected = expected;
    state->inputMode = ASYNC_UTF8;
    return LN_FEED_MORE;
  }

  // Single-byte character
  return asyncProcessChar(state, static_cast<char32_t>(uc));
}

char* linenoiseEditGetLine(linenoiseState* state) {
  if (!state || state->result != LN_FEED_DONE) return NULL;
  size_t bufferSize = sizeof(char32_t) * state->len + 1;
  unique_ptr<char[]> buf8(new char[bufferSize]);
  copyString32to8(buf8.get(), bufferSize, state->buf32);
  return strdup(buf8.get());
}

void linenoiseEditStop(linenoiseState* state) {
  if (!state) return;
  if (state->rawModeEnabled) {
    --rawModeRefCount;
    if (rawModeRefCount <= 0) {
      rawModeRefCount = 0;
      disableRawMode();
    }
  }
  if (!state->silent) {
    if (write(1, "\n", 1) == -1) {
      // ignore
    }
  }
  delete state;
}

int linenoiseEditFd(linenoiseState*) {
  return STDIN_FILENO;
}

const char* linenoiseEditGetBuffer(linenoiseState* state, int* cursor_pos, int* len) {
  if (!state) {
    if (cursor_pos) *cursor_pos = 0;
    if (len) *len = 0;
    return "";
  }
  if (cursor_pos) *cursor_pos = state->pos;
  if (len) *len = state->len;
  if (state->getBufferCacheDirty || state->getBufferCache.empty()) {
    size_t bufferSize = sizeof(char32_t) * state->len + 1;
    unique_ptr<char[]> buf8(new char[bufferSize]);
    copyString32to8(buf8.get(), bufferSize, state->buf32);
    state->getBufferCache = buf8.get();
    state->getBufferCacheDirty = false;
  }
  return state->getBufferCache.c_str();
}

void linenoiseEditSetSilent(linenoiseState* state, int silent) {
  if (state) {
    state->silent = (silent != 0);
  }
}

int linenoiseEditKeyType(linenoiseState* state) {
  if (!state) return 0;
  return state->keyType;
}
