# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreHistogramProcessor class definition

/** QoreHistogramProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
*/

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Pipeline processor that computes histogram distributions over windows
/** @since %DataProvider 3.3
*/
public class QoreHistogramProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreHistogramProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "n_bins": <DataProviderOptionInfo>{
                "display_name": "Number of Bins",
                "short_desc": "Number of histogram bins",
                "desc": "The number of bins to divide the value range into; "
                    "default: `10`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "bin_method": <DataProviderOptionInfo>{
                "display_name": "Binning Method",
                "short_desc": "Method used to determine bin boundaries",
                "desc": "How to determine bin boundaries: `uniform` (equal-width) or "
                    "`quantile` (equal-count)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "uniform",
                        "display_name": "Uniform (equal-width bins)",
                    },
                    <AllowedValueInfo>{
                        "value": "quantile",
                        "display_name": "Quantile (equal-count bins)",
                    },
                ),
            },
        };
    }

    private {
        #! Number of bins
        int n_bins = 10;

        #! Binning method
        string bin_method = "uniform";
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.n_bins) {
            int b = options.n_bins.toInt();
            if (b < 1) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("n_bins must be >= 1; got %d", b);
            }
            n_bins = b;
        }
        if (exists options.bin_method) {
            string m = options.bin_method;
            if (m != "uniform" && m != "quantile") {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("bin_method must be 'uniform' or 'quantile'; got %y", m);
            }
            bin_method = m;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "histogram";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes frequency distribution histograms over a window of records "
            "for each configured numeric field; supports uniform (equal-width) and "
            "quantile (equal-count) binning methods";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyzes a complete window of values
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        int n = values.lsize();
        list<float> sorted = sort(values);

        list<float> edges;
        list<int> counts;
        float bw = 0.0;

        if (bin_method == "quantile") {
            # quantile binning: equal number of values per bin
            int actual_bins = min(n_bins, n);
            edges = ();
            for (int i = 0; i <= actual_bins; ++i) {
                float p = float(i) / actual_bins;
                int idx = min(int(p * n), n - 1);
                push edges, sorted[idx];
            }
            # ensure first edge is min, last is max
            edges[0] = sorted[0];
            edges[edges.lsize() - 1] = sorted[n - 1];
            bw = (sorted[n - 1] - sorted[0]) / actual_bins;

            # count values in each bin
            counts = ();
            for (int i = 0; i < actual_bins; ++i) {
                push counts, 0;
            }
            foreach float v in (sorted) {
                # find bin
                bool placed = False;
                for (int i = 0; i < actual_bins; ++i) {
                    if (i == actual_bins - 1) {
                        # last bin includes upper edge
                        if (v >= edges[i] && v <= edges[i + 1]) {
                            counts[i]++;
                            placed = True;
                            break;
                        }
                    } else {
                        if (v >= edges[i] && v < edges[i + 1]) {
                            counts[i]++;
                            placed = True;
                            break;
                        }
                    }
                }
                if (!placed) {
                    # edge case: put in last bin
                    counts[actual_bins - 1]++;
                }
            }
        } else {
            # uniform binning: equal-width bins
            float min_val = sorted[0];
            float max_val = sorted[n - 1];
            if (min_val == max_val) {
                # all values identical — single bin
                edges = (min_val, max_val);
                counts = (n,);
                bw = 0.0;
            } else {
                bw = (max_val - min_val) / n_bins;
                edges = ();
                for (int i = 0; i <= n_bins; ++i) {
                    push edges, min_val + i * bw;
                }
                counts = ();
                for (int i = 0; i < n_bins; ++i) {
                    push counts, 0;
                }
                foreach float v in (sorted) {
                    int bin = int(floor((v - min_val) / bw));
                    if (bin >= n_bins) {
                        bin = n_bins - 1;
                    }
                    if (bin < 0) {
                        bin = 0;
                    }
                    counts[bin]++;
                }
            }
        }

        # compute frequencies
        list<float> frequencies;
        foreach int c in (counts) {
            push frequencies, n > 0 ? float(c) / n : 0.0;
        }

        return <HistogramEventInfo>{
            "event_type": "histogram",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "bin_edges": edges,
            "bin_counts": counts,
            "bin_frequencies": frequencies,
            "bin_width": bw,
            "count": n,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        throw "INTERNAL-ERROR", "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::HistogramEventInfo"));
    }
}
}
