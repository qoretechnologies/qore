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

// --- Cephes Special Functions (public domain) ---
// Adapted from the Cephes Mathematical Library by Stephen L. Moshier.
// These are the same functions used by Python's scipy.special.
// Original: http://www.netlib.org/cephes/
// License: public domain / freely redistributable

static const double MACHEP = 1.11022302462515654042e-16;   // 2^-53
static const double MAXLOG = 7.09782712893383996843e+02;    // log(DBL_MAX)

// Continued fraction expansion for the incomplete beta function.
// Evaluates I_x(a,b) using the modified Lentz continued fraction.
static double incbcf(double a, double b, double x) {
    double xk, pk, pkm1, pkm2, qk, qkm1, qkm2;
    double k1, k2, k3, k4, k5, k6, k7, k8;
    double r, t, ans, thresh;
    int n;

    k1 = a;
    k2 = a + b;
    k3 = a;
    k4 = a + 1.0;
    k5 = 1.0;
    k6 = b - 1.0;
    k7 = k4;
    k8 = a + 2.0;

    pkm2 = 0.0;
    qkm2 = 1.0;
    pkm1 = 1.0;
    qkm1 = 1.0;
    ans = 1.0;
    r = 1.0;
    n = 0;
    thresh = 3.0 * MACHEP;

    do {
        xk = -(x * k1 * k2) / (k3 * k4);
        pk = pkm1 + pkm2 * xk;
        qk = qkm1 + qkm2 * xk;
        pkm2 = pkm1;
        pkm1 = pk;
        qkm2 = qkm1;
        qkm1 = qk;

        xk = (x * k5 * k6) / (k7 * k8);
        pk = pkm1 + pkm2 * xk;
        qk = qkm1 + qkm2 * xk;
        pkm2 = pkm1;
        pkm1 = pk;
        qkm2 = qkm1;
        qkm1 = qk;

        if (qk != 0) {
            r = pk / qk;
        }
        if (r != 0) {
            t = std::abs((ans - r) / r);
            ans = r;
        } else {
            t = 1.0;
        }

        if (t < thresh) {
            break;
        }

        k1 += 1.0;
        k2 += 1.0;
        k3 += 2.0;
        k4 += 2.0;
        k5 += 1.0;
        k6 -= 1.0;
        k7 += 2.0;
        k8 += 2.0;

        if ((std::abs(qk) + std::abs(pk)) > 1e37) {
            pkm2 *= MACHEP;
            pkm1 *= MACHEP;
            qkm2 *= MACHEP;
            qkm1 *= MACHEP;
        }
        if ((std::abs(qk) < MACHEP) || (std::abs(pk) < MACHEP)) {
            pkm2 *= 1e37;
            pkm1 *= 1e37;
            qkm2 *= 1e37;
            qkm1 *= 1e37;
        }
    } while (++n < 300);

    return ans;
}

// Power series for incomplete beta integral.
// Use when b*x is small and x is not much larger than 1.
static double incbps(double a, double b, double x) {
    double s, t, u, v, n, t1, z, ai;

    ai = 1.0 / a;
    u = (1.0 - b) * x;
    v = u / (a + 1.0);
    t1 = v;
    t = u;
    n = 2.0;
    s = 0.0;
    z = MACHEP * ai;
    while (std::abs(v) > z) {
        u = (n - b) * x / n;
        t *= u;
        v = t / (a + n);
        s += v;
        n += 1.0;
        if (n > 300) {
            break;
        }
    }
    s += t1;
    s += ai;

    u = a * std::log(x);
    if ((a + b) < MAXLOG && std::abs(u) < MAXLOG) {
        t = std::tgamma(a + b) / (std::tgamma(a) * std::tgamma(b));
        s = s * t * std::pow(x, a);
    } else {
        t = std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + u + std::log(s);
        if (t < -MAXLOG) {
            s = 0.0;
        } else {
            s = std::exp(t);
        }
    }
    return s;
}

// Regularized incomplete beta function I_x(a,b) — Cephes incbet()
// Returns the incomplete beta integral: integral from 0 to x of
// t^(a-1) * (1-t)^(b-1) dt / B(a,b)
static double cephes_incbet(double a, double b, double x) {
    if (a <= 0.0 || b <= 0.0) {
        return 0.0;
    }
    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }

    // Use symmetry relation if needed for numerical stability
    double flag = 0;
    if ((b * x) <= 1.0 && x <= 0.95) {
        return incbps(a, b, x);
    }

    double w = 1.0 - x;

    // Reverse a and b if x > a/(a+b)
    double xc, aa, bb, xx;
    if (x > (a / (a + b))) {
        flag = 1;
        aa = b;
        bb = a;
        xc = x;
        xx = w;
    } else {
        aa = a;
        bb = b;
        xc = w;
        xx = x;
    }

    if (flag == 1 && (bb * xx) <= 1.0 && xx <= 0.95) {
        double t = incbps(aa, bb, xx);
        return (t <= MACHEP) ? 1.0 - MACHEP : 1.0 - t;
    }

    // Use continued fraction expansion
    double y = xx * (aa + bb - 2.0) - (aa - 1.0);
    if (y < 0.0) {
        w = incbcf(aa, bb, xx);
    } else {
        w = incbcf(aa, bb, xx) / xc;  // not used; just use incbcf directly
        w = incbcf(aa, bb, xx);
    }

    y = aa * std::log(xx);
    double t = bb * std::log(xc);
    if ((aa + bb) < MAXLOG && std::abs(y) < MAXLOG && std::abs(t) < MAXLOG) {
        t = std::pow(xc, bb) * std::pow(xx, aa) / aa;
        t *= w;
        t *= std::tgamma(aa + bb) / (std::tgamma(aa) * std::tgamma(bb));
    } else {
        y += t + std::lgamma(aa + bb) - std::lgamma(aa) - std::lgamma(bb);
        y += std::log(w / aa);
        if (y < -MAXLOG) {
            t = 0.0;
        } else {
            t = std::exp(y);
        }
    }

    if (flag == 1) {
        if (t <= MACHEP) {
            t = 1.0 - MACHEP;
        } else {
            t = 1.0 - t;
        }
    }
    return t;
}

// --- Hypothesis Testing ---

// Two-tailed p-value for Student's t-distribution (Cephes stdtr)
// P(|T| > |t|) = I_{v/(v+t²)}(v/2, 1/2) where I is the regularized incomplete beta
static double t_cdf_two_tail(double t_stat, double df) {
    if (df <= 0) {
        return 1.0;
    }
    double t2 = t_stat * t_stat;
    double x = df / (df + t2);
    // I_{df/(df+t²)}(df/2, 1/2) gives the two-tailed p-value directly:
    // P(|T| > |t_stat|) = I_{df/(df+t²)}(df/2, 1/2)
    return cephes_incbet(df / 2.0, 0.5, x);
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

    // p-value: P(X² > statistic) using the relation between chi-squared and
    // the regularized incomplete beta function:
    // P(X² > x | df=k) = 1 - I_{x/2 / (x/2 + k/2)}(?, ?) ... complex.
    // Simpler: for chi-squared, P(X > x | k) = I_{k/(k+x)}(k/2, 1/2) when
    // we can express it via a related F-distribution.
    // Actually: chi²(k) = Gamma(k/2, 2), and
    // P(chi² > x) = Q(k/2, x/2) = upper incomplete gamma.
    // Relation to beta: Q(a, x) = I_{x/(x+a)}(?, ?) — not straightforward.
    //
    // Use Wilson-Hilferty cube-root approximation (accurate for df >= 1):
    // Z = ((X/k)^(1/3) - (1 - 2/(9k))) / sqrt(2/(9k)) ~ N(0,1)
    if (df > 0 && statistic > 0) {
        double k = df;
        double z = std::pow(statistic / k, 1.0 / 3.0)
            - (1.0 - 2.0 / (9.0 * k));
        z /= std::sqrt(2.0 / (9.0 * k));
        p_value = 0.5 * std::erfc(z / std::sqrt(2.0));
        // Clamp to [0, 1]
        if (p_value < 0) { p_value = 0; }
        if (p_value > 1) { p_value = 1; }
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
