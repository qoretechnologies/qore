/*
  qore-json-module.h

  Qore Programming Language

  Copyright (C) 2006 - 2010 Qore Technologies

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_JSON_MODULE_H
#define _QORE_JSON_MODULE_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <qore/Qore.h>
#include <qore/InputStream.h>
#include <qore/OutputStream.h>
#include <qore/QoreSandboxManager.h>
#include "QoreJson.h"

//! Maximum file size for JsonSchema::load() (sandbox only) - 10MB
#define JSON_MAX_SCHEMA_FILE_SIZE (10 * 1024 * 1024)

// JSON_MAX_NESTING_DEPTH and JSON_INTERRUPT_CHECK_INTERVAL are defined in QoreJson.h;
// ql_cbor.qpp and ql_toon.qpp derive their own limits from them

#endif
