/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_stats.h

    Statistical functions for the ML module

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_ML_STATS_H
#define _QORE_MODULE_ML_ML_STATS_H

#include "ml_common.h"

//! Compute the p-th percentile of a vector (0-100 scale)
DLLLOCAL double ml_percentile(const VectorXd& data, double p);

//! Compute interquartile range (Q3 - Q1)
DLLLOCAL double ml_iqr(const VectorXd& data);

//! Compute skewness (Fisher's definition)
DLLLOCAL double ml_skewness(const VectorXd& data);

//! Compute excess kurtosis (Fisher's definition, normal = 0)
DLLLOCAL double ml_kurtosis(const VectorXd& data);

//! Compute Pearson correlation matrix
DLLLOCAL MatrixXd ml_correlation_matrix(const MatrixXd& data);

//! Compute covariance matrix
DLLLOCAL MatrixXd ml_covariance_matrix(const MatrixXd& data);

//! Two-sample t-test: returns {statistic, p_value, df}
DLLLOCAL void ml_t_test(const VectorXd& a, const VectorXd& b,
    double& statistic, double& p_value, double& df);

//! Chi-squared test on contingency table: returns {statistic, p_value, df}
DLLLOCAL void ml_chi_squared_test(const MatrixXd& contingency,
    double& statistic, double& p_value, double& df);

//! Normal distribution PDF
DLLLOCAL double ml_normal_pdf(double x, double mean, double std);

//! Normal distribution CDF (approximation)
DLLLOCAL double ml_normal_cdf(double x, double mean, double std);

//! Normal distribution inverse CDF / PPF (approximation)
DLLLOCAL double ml_normal_ppf(double p, double mean, double std);

//! Cosine similarity between two vectors (-1 to 1)
DLLLOCAL double ml_cosine_similarity(const VectorXd& a, const VectorXd& b);

//! Dot product between two vectors
DLLLOCAL double ml_dot_product(const VectorXd& a, const VectorXd& b);

//! Euclidean distance between two vectors
DLLLOCAL double ml_euclidean_distance(const VectorXd& a, const VectorXd& b);

//! Manhattan distance between two vectors
DLLLOCAL double ml_manhattan_distance(const VectorXd& a, const VectorXd& b);

#endif
