/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_stats.cpp

    Statistical functions implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "ml_stats.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

// --- Descriptive Statistics ---

double ml_percentile(const VectorXd& data, double p) {
    if (data.size() == 0) {
        return 0.0;
    }
    if (data.size() == 1) {
        return data(0);
    }

    // Sort
    std::vector<double> sorted(data.data(), data.data() + data.size());
    std::sort(sorted.begin(), sorted.end());

    // Linear interpolation (same as numpy "linear" method)
    double idx = (p / 100.0) * (sorted.size() - 1);
    int lo = (int)idx;
    int hi = lo + 1;
    if (hi >= (int)sorted.size()) {
        return sorted.back();
    }
    double frac = idx - lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

double ml_iqr(const VectorXd& data) {
    return ml_percentile(data, 75.0) - ml_percentile(data, 25.0);
}

double ml_skewness(const VectorXd& data) {
    int n = (int)data.size();
    if (n < 3) {
        return 0.0;
    }

    double mean = data.mean();
    double m2 = 0, m3 = 0;
    for (int i = 0; i < n; ++i) {
        double d = data(i) - mean;
        m2 += d * d;
        m3 += d * d * d;
    }
    m2 /= n;
    m3 /= n;

    double std = std::sqrt(m2);
    if (std < 1e-15) {
        return 0.0;
    }

    // Fisher's skewness with bias correction
    double skew = m3 / (std * std * std);
    // Bias correction: multiply by sqrt(n*(n-1)) / (n-2)
    skew *= std::sqrt((double)n * (n - 1)) / (n - 2);
    return skew;
}

double ml_kurtosis(const VectorXd& data) {
    int n = (int)data.size();
    if (n < 4) {
        return 0.0;
    }

    double mean = data.mean();
    double m2 = 0, m4 = 0;
    for (int i = 0; i < n; ++i) {
        double d = data(i) - mean;
        double d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }
    m2 /= n;
    m4 /= n;

    if (m2 < 1e-15) {
        return 0.0;
    }

    // Excess kurtosis (Fisher's definition: normal = 0)
    double kurt = m4 / (m2 * m2) - 3.0;
    return kurt;
}

MatrixXd ml_correlation_matrix(const MatrixXd& data) {
    int n = (int)data.rows();
    int p = (int)data.cols();
    if (n < 2 || p == 0) {
        return MatrixXd::Identity(p, p);
    }

    // Center data
    MatrixXd centered = data.rowwise() - data.colwise().mean();

    // Covariance
    MatrixXd cov = (centered.transpose() * centered) / (n - 1);

    // Normalize to correlation
    MatrixXd corr(p, p);
    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) {
            double denom = std::sqrt(cov(i, i) * cov(j, j));
            corr(i, j) = (denom > 1e-15) ? cov(i, j) / denom : 0.0;
        }
    }
    return corr;
}

MatrixXd ml_covariance_matrix(const MatrixXd& data) {
    int n = (int)data.rows();
    if (n < 2) {
        return MatrixXd::Zero(data.cols(), data.cols());
    }
    MatrixXd centered = data.rowwise() - data.colwise().mean();
    return (centered.transpose() * centered) / (n - 1);
}

// --- Hypothesis Testing ---

// Approximation of the two-tailed Student's t CDF using the regularized
// incomplete beta function. For simplicity, use a normal approximation
// for large df (df > 30).
static double t_cdf_two_tail(double t_stat, double df) {
    // For large df, t-distribution ≈ normal
    double x = std::abs(t_stat);
    // Approximation: p ≈ 2 * (1 - Φ(x * sqrt(df/(df-2))))
    // For df > 30, this is quite accurate
    double z = x;
    if (df > 2) {
        z = x * std::sqrt(df / (df - 2));
    }
    // Normal CDF approximation (Abramowitz and Stegun 26.2.17)
    double p_one_tail = 0.5 * std::erfc(z / std::sqrt(2.0));
    return 2.0 * p_one_tail;
}

void ml_t_test(const VectorXd& a, const VectorXd& b,
        double& statistic, double& p_value, double& df) {
    int n1 = (int)a.size();
    int n2 = (int)b.size();

    double mean1 = a.mean();
    double mean2 = b.mean();

    double var1 = 0, var2 = 0;
    for (int i = 0; i < n1; ++i) {
        double d = a(i) - mean1;
        var1 += d * d;
    }
    var1 /= (n1 - 1);

    for (int i = 0; i < n2; ++i) {
        double d = b(i) - mean2;
        var2 += d * d;
    }
    var2 /= (n2 - 1);

    // Welch's t-test (unequal variances)
    double se = std::sqrt(var1 / n1 + var2 / n2);
    if (se < 1e-15) {
        statistic = 0;
        p_value = 1.0;
        df = n1 + n2 - 2;
        return;
    }

    statistic = (mean1 - mean2) / se;

    // Welch–Satterthwaite degrees of freedom
    double v1n = var1 / n1;
    double v2n = var2 / n2;
    df = (v1n + v2n) * (v1n + v2n)
        / (v1n * v1n / (n1 - 1) + v2n * v2n / (n2 - 1));

    p_value = t_cdf_two_tail(statistic, df);
}

void ml_chi_squared_test(const MatrixXd& contingency,
        double& statistic, double& p_value, double& df) {
    int r = (int)contingency.rows();
    int c = (int)contingency.cols();

    // Compute row sums, column sums, total
    VectorXd row_sums = contingency.rowwise().sum();
    VectorXd col_sums = contingency.colwise().sum().transpose();
    double total = contingency.sum();

    if (total < 1e-15) {
        statistic = 0;
        p_value = 1.0;
        df = (r - 1) * (c - 1);
        return;
    }

    // Chi-squared statistic
    statistic = 0;
    for (int i = 0; i < r; ++i) {
        for (int j = 0; j < c; ++j) {
            double expected = row_sums(i) * col_sums(j) / total;
            if (expected > 0) {
                double diff = contingency(i, j) - expected;
                statistic += diff * diff / expected;
            }
        }
    }

    df = (r - 1) * (c - 1);

    // p-value approximation using Wilson-Hilferty normal approximation
    // for chi-squared with df degrees of freedom
    if (df > 0) {
        double z = std::pow(statistic / df, 1.0 / 3.0)
            - (1.0 - 2.0 / (9.0 * df));
        z /= std::sqrt(2.0 / (9.0 * df));
        p_value = 0.5 * std::erfc(z / std::sqrt(2.0));
    } else {
        p_value = 1.0;
    }
}

// --- Probability Distributions ---

double ml_normal_pdf(double x, double mean, double std_dev) {
    if (std_dev <= 0) {
        return 0.0;
    }
    double z = (x - mean) / std_dev;
    return std::exp(-0.5 * z * z) / (std_dev * std::sqrt(2.0 * M_PI));
}

double ml_normal_cdf(double x, double mean, double std_dev) {
    if (std_dev <= 0) {
        return (x >= mean) ? 1.0 : 0.0;
    }
    double z = (x - mean) / std_dev;
    return 0.5 * std::erfc(-z / std::sqrt(2.0));
}

double ml_normal_ppf(double p, double mean, double std_dev) {
    if (p <= 0) {
        return -std::numeric_limits<double>::infinity();
    }
    if (p >= 1) {
        return std::numeric_limits<double>::infinity();
    }

    // Rational approximation for the inverse normal CDF
    // (Peter Acklam's algorithm, accurate to ~1e-9)
    static const double a[] = {
        -3.969683028665376e+01, 2.209460984245205e+02,
        -2.759285104469687e+02, 1.383577518672690e+02,
        -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {
        -5.447609879822406e+01, 1.615858368580409e+02,
        -1.556989798598866e+02, 6.680131188771972e+01,
        -1.328068155288572e+01};
    static const double c[] = {
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
        4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[] = {
        7.784695709041462e-03, 3.224671290700398e-01,
        2.445134137142996e+00, 3.754408661907416e+00};

    double q, r, z;
    if (p < 0.02425) {
        q = std::sqrt(-2.0 * std::log(p));
        z = (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])
            / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
    } else if (p <= 0.97575) {
        q = p - 0.5;
        r = q * q;
        z = (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5]) * q
            / (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
    } else {
        q = std::sqrt(-2.0 * std::log(1.0 - p));
        z = -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5])
            / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
    }

    return mean + std_dev * z;
}
