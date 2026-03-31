/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_csv.h

    CSV reader/writer for DataFrame

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_DATAFRAME_DF_CSV_H
#define _QORE_DATAFRAME_DF_CSV_H

#include "QC_DataFrame.h"

namespace QoreDataFrameNS {

//! Read a CSV file into a DataFrame
/** @param path file path
    @param options optional CsvOptions hash
    @param xsink exception sink
    @return new DataFrame, or nullptr on error
*/
DLLLOCAL QoreDataFrame* readCSV(const std::string& path, const QoreHashNode* options,
    ExceptionSink* xsink);

//! Write a DataFrame to a CSV file
/** @param df the DataFrame to write
    @param path file path
    @param options optional CsvOptions hash
    @param xsink exception sink
*/
DLLLOCAL void writeCSV(const QoreDataFrame* df, const std::string& path,
    const QoreHashNode* options, ExceptionSink* xsink);

} // namespace QoreDataFrameNS

#endif // _QORE_DATAFRAME_DF_CSV_H
