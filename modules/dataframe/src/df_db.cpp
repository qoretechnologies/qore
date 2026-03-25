/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_db.cpp

    DataFrame database I/O — stub for QPP-level implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

// Database I/O is implemented at the QPP level in QC_DataFrame.qpp
// because it needs to call Qore DBI object methods (selectRows, exec)
// which are most naturally invoked through the Qore method call API.
// This file is kept as a placeholder for future C++ DBI integration.
