/*
  QoreTransliteration.cpp

  regex-like transliteration class definition

  Qore Programming Language

  Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#include <qore/Qore.h>
#include "qore/intern/QoreTransliteration.h"

#include <cctype>
#include <cstdlib>
#include <strings.h>

// constructor used when parsing
QoreTransliteration::QoreTransliteration(const QoreProgramLocation* loc) : loc(loc) {
   //printd(5, "QoreTransliteration::QoreTransliteration() this: %p\n", this);
}

QoreTransliteration::~QoreTransliteration() {
   //printd(5, "QoreTransliteration::~QoreTransliteration() this: %p\n", this);
}

void QoreTransliteration::setTargetRange() {
   tr = true;
}

void QoreTransliteration::setSourceRange() {
   sr = true;
}

void QoreTransliteration::finishSource() {
   if (sr)
      parse_error(*loc, "no end character for character range at end of source pattern in transliteration");
}

void QoreTransliteration::finishTarget() {
   if (tr)
      parse_error(*loc, "no end character for character range at end of target pattern in transliteration");
}

void QoreTransliteration::doRange(QoreString& str, char last, ExceptionSink* xsink) {
    if (str.empty()) {
        if (xsink) {
            xsink->raiseException("TRANSLITERATION-ERROR", "no start character for range in transliteration");
        } else {
            parse_error(*loc, "no start character for character range in transliteration");
        }
        return;
    }
    unsigned start = static_cast<unsigned char>(str.c_str()[str.size() - 1]);
    unsigned end = static_cast<unsigned char>(last);
    if (start > end) {
        if (xsink) {
            xsink->raiseException("TRANSLITERATION-ERROR", "invalid descending range in transliteration");
        } else {
            parse_error(*loc, "invalid range '%c' - '%c' in transliteration operator", start, end);
        }
        return;
    }
    for (unsigned c = start + 1; c <= end; ++c) {
        str.concat(static_cast<char>(c));
    }
}

bool QoreTransliteration::compilePattern(QoreString& result, const QoreString& pattern, ExceptionSink* xsink) {
    TempEncodingHelper text(&pattern, QCS_DEFAULT, xsink);
    if (*xsink) {
        return false;
    }
    bool range = false;
    for (size_t i = 0; i < text->size(); ++i) {
        char c = text->c_str()[i];
        if (c == '-') {
            range = true;
            continue;
        }
        if (c == '\\') {
            if (++i == text->size()) {
                xsink->raiseException("TRANSLITERATION-ERROR", "unfinished escape in transliteration pattern");
                return false;
            }
            c = text->c_str()[i];
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
            }
        }
        if (range) {
            doRange(result, c, xsink);
            if (*xsink) {
                return false;
            }
            range = false;
        } else {
            result.concat(c);
        }
    }
    if (range) {
        xsink->raiseException("TRANSLITERATION-ERROR", "no end character for range in transliteration");
        return false;
    }
    return true;
}

QoreStringNode* QoreTransliteration::translate(const QoreString& input, const QoreString& from,
        const QoreString& to, ExceptionSink* xsink) {
    QoreTransliteration tr(nullptr);
    if (!tr.compilePattern(tr.source, from, xsink) || !tr.compilePattern(tr.target, to, xsink)) {
        return nullptr;
    }
    if (!tr.source.empty() && tr.target.empty()) {
        xsink->raiseException("TRANSLITERATION-ERROR", "a nonempty source pattern needs a target character");
        return nullptr;
    }
    return tr.exec(&input, xsink);
}

void QoreTransliteration::concatSource(char c) {
   if (sr) {
      doRange(source, c);
      sr = false;
   }
   else
      source.concat(c);
}

void QoreTransliteration::concatTarget(char c) {
   if (tr) {
      doRange(target, c);
      tr = false;
   }
   else
      target.concat(c);
}

QoreStringNode* QoreTransliteration::exec(const QoreString* str, ExceptionSink* xsink) const {
   //printd(5, "source='%s' target='%s' ('%s')\n", source->getBuffer(), target->getBuffer(), str->getBuffer());
   TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
   if (*xsink)
      return nullptr;

   SimpleRefHolder<QoreStringNode> ns(new QoreStringNode);
   for (size_t i = 0; i < tstr->strlen(); ++i) {
      char c = (**tstr)[i];
      const char* p = static_cast<const char*>(memchr(source.c_str(), c, source.size()));
      if (p) {
         size_t pos = p - source.c_str();
         if (target.strlen() <= pos)
            pos = target.strlen() - 1;
         ns->concat(target[pos]);
      }
      else
         ns->concat(c);
   }

   return ns.release();
}
