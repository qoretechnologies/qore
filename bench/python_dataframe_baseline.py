#!/usr/bin/env python3
"""Pandas baseline for Qore DataFrame benchmarks.

This mirrors the deterministic data and major operations in
bench/cases/bench_dataframe_*.qr.  It is intentionally standalone so it can run
from a throwaway Python environment without changing the Qore benchmark runner.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import statistics
import tempfile
import time
from pathlib import Path
from urllib.parse import quote_plus

import pandas as pd


def default_rows() -> int:
    return int(os.environ.get("QORE_BENCH_DF_ROWS", "100000"))


def sql_rows() -> int:
    return int(os.environ.get("QORE_BENCH_DF_SQL_ROWS", "20000"))


def solution_rows() -> int:
    return int(os.environ.get("QORE_BENCH_DF_SOLUTION_ROWS", str(default_rows())))


def ai_rows() -> int:
    return int(os.environ.get("QORE_BENCH_DF_AI_ROWS", "20000"))


def approx_bytes(rows: int) -> int:
    return rows * 64


def approx_analytic_bytes(rows: int) -> int:
    return rows * 56


def status_for(i: int) -> str:
    return "inactive" if (i % 5) == 0 else "active"


def region_for(i: int) -> str:
    return ("north", "south", "east", "west", "central", "remote")[i % 6]


def amount_for(i: int) -> float:
    return float((i * 37) % 100000) / 10.0


def score_for(i: int) -> float:
    return float(50 + ((i * 17) % 5000)) / 50.0


def make_columns(rows: int) -> dict[str, list[object]]:
    ids: list[int] = []
    customer_ids: list[int] = []
    statuses: list[str] = []
    amounts: list[float] = []
    scores: list[float] = []
    regions: list[str] = []
    flags: list[bool] = []

    for i in range(rows):
        ids.append(i)
        customer_ids.append(i % 10000)
        statuses.append(status_for(i))
        amounts.append(amount_for(i))
        scores.append(score_for(i))
        regions.append(region_for(i))
        flags.append((i % 7) == 0)

    return {
        "id": ids,
        "customer_id": customer_ids,
        "status": statuses,
        "amount": amounts,
        "score": scores,
        "region": regions,
        "flag": flags,
    }


def make_records(rows: int) -> list[dict[str, object]]:
    return [
        {
            "id": i,
            "customer_id": i % 10000,
            "status": status_for(i),
            "amount": amount_for(i),
            "score": score_for(i),
            "region": region_for(i),
            "flag": (i % 7) == 0,
        }
        for i in range(rows)
    ]


def make_customer_columns(rows: int) -> dict[str, list[object]]:
    segments = ("enterprise", "midmarket", "smb")
    return {
        "customer_id": list(range(rows)),
        "segment": [segments[i % 3] for i in range(rows)],
        "customer_region": [region_for(i) for i in range(rows)],
        "weight": [float(100 + (i % 25)) / 100.0 for i in range(rows)],
    }


def feature1_for(i: int) -> float:
    return float((i % 1000) - 500) / 25.0


def feature2_for(i: int) -> float:
    return float(((i * 17) % 1200) - 600) / 30.0


def feature3_for(i: int) -> float:
    return float(((i * 31) % 900) - 450) / 20.0


def target_for(i: int) -> float:
    x1 = feature1_for(i)
    x2 = feature2_for(i)
    x3 = feature3_for(i)
    return (x1 * 1.7) - (x2 * 0.8) + (x3 * 0.35) + float(i % 11) / 10.0


def make_analytic_columns(rows: int) -> dict[str, list[object]]:
    target = [target_for(i) for i in range(rows)]
    return {
        "feature1": [feature1_for(i) for i in range(rows)],
        "feature2": [feature2_for(i) for i in range(rows)],
        "feature3": [feature3_for(i) for i in range(rows)],
        "target": target,
        "label": [1 if value >= 0.0 else 0 for value in target],
        "cohort": [region_for(i) for i in range(rows)],
    }


def percentile_median(values: list[float]) -> float:
    return sorted(values)[len(values) // 2]


def run_case(
    name: str,
    iterations: int,
    warmup: int,
    bytes_processed: int,
    measure,
) -> dict[str, object]:
    for _ in range(warmup):
        measure()

    timings_ms: list[float] = []
    for _ in range(iterations):
        t0 = time.perf_counter()
        measure()
        timings_ms.append((time.perf_counter() - t0) * 1000.0)

    median_ms = percentile_median(timings_ms)
    mean_ms = statistics.fmean(timings_ms)
    stdev_ms = statistics.pstdev(timings_ms)
    result: dict[str, object] = {
        "name": name,
        "iterations": iterations,
        "warmup": warmup,
        "timings_ms": timings_ms,
        "min_ms": min(timings_ms),
        "max_ms": max(timings_ms),
        "median_ms": median_ms,
        "mean_ms": mean_ms,
        "stdev_ms": stdev_ms,
        "bytes_processed": bytes_processed,
        "python_version": "{}.{}.{}".format(*os.sys.version_info[:3]),
        "pandas_version": pd.__version__,
    }
    if bytes_processed > 0 and median_ms > 0:
        result["throughput_mb_s"] = (bytes_processed / 1048576.0) / (median_ms / 1000.0)
    print_report(result)
    return result


def print_report(result: dict[str, object]) -> None:
    throughput = result.get("throughput_mb_s")
    suffix = f"   {throughput:8.1f} MB/s" if isinstance(throughput, float) else ""
    print(
        f"{result['name']:<32} mean {result['mean_ms']:8.3f} ms"
        f"   min {result['min_ms']:8.3f}   median {result['median_ms']:8.3f}"
        f"   stdev {result['stdev_ms']:7.3f}{suffix}"
    )


def qore_pgsql_to_sqlalchemy_url(connstr: str) -> str:
    match = re.fullmatch(r"pgsql:([^/]+)/([^@]+)@([^%]+)(?:%(.+))?", connstr)
    if not match:
        raise ValueError(f"unsupported Qore PostgreSQL connection string: {connstr!r}")
    user, password, dbname, host = match.groups()
    host = host or "localhost"
    return (
        "postgresql+psycopg://"
        f"{quote_plus(user)}:{quote_plus(password)}@{quote_plus(host)}/{quote_plus(dbname)}"
    )


def add_sql_cases(results: dict[str, dict[str, object]]) -> None:
    connstr = os.environ.get("QORE_DB_CONNSTR_PGSQL")
    if not connstr:
        print("pandas_sql_from_query: skipped (QORE_DB_CONNSTR_PGSQL not set)")
        print("pandas_sql_to_table: skipped (QORE_DB_CONNSTR_PGSQL not set)")
        return

    from sqlalchemy import create_engine, text

    rows = sql_rows()
    url = qore_pgsql_to_sqlalchemy_url(connstr)
    engine = create_engine(url)
    conn = engine.connect()

    try:
        seed = pd.DataFrame(make_columns(rows))
        conn.execute(text(
            "CREATE TEMP TABLE qore_df_pybench_query ("
            "id int, customer_id int, status text, amount float8, score float8, region text, flag bool)"
        ))
        seed.to_sql("qore_df_pybench_query", conn, if_exists="append", index=False)
        conn.commit()

        def read_query() -> None:
            df = pd.read_sql_query(
                "SELECT id, customer_id, status, amount, score, region, flag FROM qore_df_pybench_query",
                conn,
            )
            if len(df) < rows:
                raise RuntimeError("pandas_sql_from_query returned too few rows")

        results["pandas_sql_from_query"] = run_case(
            "pandas_sql_from_query", 5, 1, approx_bytes(rows), read_query
        )

        insert_rows = rows // 2
        insert_df = pd.DataFrame(make_columns(insert_rows))
        conn.execute(text("DROP TABLE IF EXISTS qore_df_pybench_insert"))
        conn.execute(text(
            "CREATE TABLE qore_df_pybench_insert ("
            "id int, customer_id int, status text, amount float8, score float8, region text, flag bool)"
        ))
        conn.commit()

        def to_table() -> None:
            conn.execute(text("TRUNCATE qore_df_pybench_insert"))
            insert_df.to_sql("qore_df_pybench_insert", conn, if_exists="append", index=False)
            conn.commit()

        results["pandas_sql_to_table"] = run_case(
            "pandas_sql_to_table", 5, 1, approx_bytes(insert_rows), to_table
        )
    finally:
        try:
            conn.execute(text("DROP TABLE IF EXISTS qore_df_pybench_insert"))
            conn.commit()
        except Exception:
            conn.rollback()
        conn.close()
        engine.dispose()


def add_arrow_cases(df: pd.DataFrame, rows: int, results: dict[str, dict[str, object]]) -> None:
    try:
        import pyarrow  # noqa: F401
    except Exception as ex:
        print(f"pandas_arrow_ipc_roundtrip: skipped (pyarrow not available: {ex})")
        print(f"pandas_parquet_roundtrip: skipped (pyarrow not available: {ex})")
        return

    with tempfile.TemporaryDirectory(prefix="qore_df_pybench_") as tmpdir:
        feather_path = Path(tmpdir) / "dataframe.feather"
        parquet_path = Path(tmpdir) / "dataframe.parquet"

        def arrow_ipc_roundtrip() -> None:
            df.to_feather(feather_path)
            copy = pd.read_feather(feather_path)
            if len(copy) < rows:
                raise RuntimeError("pandas_arrow_ipc_roundtrip returned too few rows")

        results["pandas_arrow_ipc_roundtrip"] = run_case(
            "pandas_arrow_ipc_roundtrip", 6, 2, approx_bytes(rows) * 2, arrow_ipc_roundtrip
        )

        def parquet_roundtrip() -> None:
            df.to_parquet(parquet_path, index=False)
            copy = pd.read_parquet(parquet_path)
            if len(copy) < rows:
                raise RuntimeError("pandas_parquet_roundtrip returned too few rows")

        results["pandas_parquet_roundtrip"] = run_case(
            "pandas_parquet_roundtrip", 5, 1, approx_bytes(rows) * 2, parquet_roundtrip
        )


def add_solution_cases(results: dict[str, dict[str, object]]) -> None:
    rows = solution_rows()
    df = pd.DataFrame(make_columns(rows))
    customers = pd.DataFrame(make_customer_columns(10000))

    def etl_dataframe() -> None:
        filtered = df[(df["status"] == "active") & (df["score"] >= 90.0)]
        enriched = filtered.merge(customers, on="customer_id", how="left")
        ordered = enriched.sort_values(["region", "segment", "amount"], kind="mergesort").copy()
        ordered["segment_rank"] = ordered.groupby(["region", "segment"], sort=False).cumcount() + 1
        ordered["rolling_amount"] = (
            ordered.groupby(["region", "segment"], sort=False)["amount"]
            .rolling(5, min_periods=1)
            .mean()
            .reset_index(level=[0, 1], drop=True)
        )
        grouped = ordered.groupby(["region", "segment"], sort=False).agg(
            amount_sum=("amount", "sum"),
            amount_mean=("amount", "mean"),
            score_max=("score", "max"),
            id_count=("id", "count"),
        )
        if len(grouped) < 1:
            raise RuntimeError("pandas_solution_etl_dataframe produced no rows")

    results["pandas_solution_etl_dataframe"] = run_case(
        "pandas_solution_etl_dataframe", 5, 1, approx_bytes(rows) * 4, etl_dataframe
    )


def add_ai_cases(results: dict[str, dict[str, object]]) -> None:
    try:
        from sklearn.cluster import KMeans
        from sklearn.decomposition import PCA
        from sklearn.preprocessing import StandardScaler
    except Exception as ex:
        print(f"sklearn_solution_ai_analytics: skipped (scikit-learn not available: {ex})")
        return

    rows = max(ai_rows(), 8)
    df = pd.DataFrame(make_analytic_columns(rows))
    features = df[["feature1", "feature2", "feature3"]]

    def ai_analytics() -> None:
        matrix = features.to_numpy()
        scaled = StandardScaler().fit_transform(matrix)
        if len(scaled) != rows:
            raise RuntimeError("sklearn scaler produced too few rows")

        model = KMeans(n_clusters=4, n_init=10, random_state=42)
        clusters = model.fit_predict(matrix)
        distances = model.transform(matrix).min(axis=1)
        summary = pd.DataFrame({"cluster": clusters, "distance": distances}).groupby("cluster").agg(
            distance_mean=("distance", "mean"),
            distance_max=("distance", "max"),
            id_count=("distance", "count"),
        )
        if len(summary) < 1:
            raise RuntimeError("sklearn kmeans produced no clusters")

        components = PCA(n_components=2).fit_transform(matrix)
        if len(components) != rows:
            raise RuntimeError("sklearn PCA produced too few rows")

    results["sklearn_solution_ai_analytics"] = run_case(
        "sklearn_solution_ai_analytics", 3, 1, approx_analytic_bytes(rows) * 3, ai_analytics
    )


def compare_to_qore(qore_path: Path, results: dict[str, dict[str, object]]) -> None:
    qore = json.loads(qore_path.read_text())
    qbench = qore["benchmarks"]
    pairs = (
        ("construct columns", "dataframe_construct_columns", "pandas_construct_columns"),
        ("construct records", "dataframe_construct_records", "pandas_construct_records"),
        ("filter mask", "dataframe_filter_mask", "pandas_filter_mask"),
        ("groupby agg", "dataframe_groupby_agg", "pandas_groupby_agg"),
        ("filter+group native", "dataframe_filter_groupby", "pandas_filter_group"),
        ("left join", "dataframe_join_left", "pandas_join_left"),
        ("filter+group direct", "dataframe_dp_shape_filter_group", "pandas_filter_group"),
        ("filter+group pipeline", "dataframe_pipeline_filter_group", "pandas_filter_group"),
        ("SQL read", "dataframe_sql_from_query", "pandas_sql_from_query"),
        ("SQL write", "dataframe_sql_to_table", "pandas_sql_to_table"),
        ("Arrow IPC roundtrip", "dataframe_arrow_ipc_roundtrip", "pandas_arrow_ipc_roundtrip"),
        ("Parquet roundtrip", "dataframe_parquet_roundtrip", "pandas_parquet_roundtrip"),
        ("solution ETL", "solution_etl_dataframe", "pandas_solution_etl_dataframe"),
        ("solution AI", "solution_ai_analytics", "sklearn_solution_ai_analytics"),
    )

    print("\ncomparison vs Qore benchmark JSON")
    print(f"{'case':<22} {'qore_ms':>12} {'pandas_ms':>12} {'pandas/qore':>13}")
    print("-" * 63)
    for label, qname, pname in pairs:
        if qname not in qbench or pname not in results:
            continue
        qms = float(qbench[qname]["median_ms"])
        pms = float(results[pname]["median_ms"])
        ratio = pms / qms
        print(f"{label:<22} {qms:12.3f} {pms:12.3f} {ratio:12.2f}x")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--save", help="write pandas benchmark JSON")
    parser.add_argument("--qore-json", help="compare against a Qore benchmark JSON")
    parser.add_argument("--no-sql", action="store_true", help="skip SQL cases")
    parser.add_argument("--no-arrow", action="store_true", help="skip Arrow IPC and Parquet cases")
    parser.add_argument("--no-ai", action="store_true", help="skip scikit-learn AI/analytics solution case")
    args = parser.parse_args()

    rows = default_rows()
    columns = make_columns(rows)
    records = make_records(rows)
    df = pd.DataFrame(columns)
    customers = pd.DataFrame(make_customer_columns(10000))

    results: dict[str, dict[str, object]] = {}
    results["pandas_construct_columns"] = run_case(
        "pandas_construct_columns",
        8,
        2,
        approx_bytes(rows),
        lambda: pd.DataFrame(columns),
    )
    results["pandas_construct_records"] = run_case(
        "pandas_construct_records",
        8,
        2,
        approx_bytes(rows),
        lambda: pd.DataFrame(records),
    )

    def filter_mask() -> None:
        filtered = df[
            (df["status"] == "active")
            & (df["score"] >= 90.0)
            & (df["amount"] > 1000.0)
        ]
        if len(filtered) < 1:
            raise RuntimeError("pandas_filter_mask produced no rows")

    results["pandas_filter_mask"] = run_case(
        "pandas_filter_mask", 12, 3, approx_bytes(rows), filter_mask
    )

    def groupby_agg() -> None:
        grouped = df.groupby(["region", "status"], sort=False).agg(
            amount_sum=("amount", "sum"),
            amount_mean=("amount", "mean"),
            score_max=("score", "max"),
            id_count=("id", "count"),
        )
        if len(grouped) < 1:
            raise RuntimeError("pandas_groupby_agg produced no rows")

    results["pandas_groupby_agg"] = run_case(
        "pandas_groupby_agg", 10, 2, approx_bytes(rows), groupby_agg
    )

    def join_left() -> None:
        joined = df.merge(customers, on="customer_id", how="left")
        if len(joined) < rows:
            raise RuntimeError("pandas_join_left produced too few rows")

    results["pandas_join_left"] = run_case(
        "pandas_join_left", 8, 2, approx_bytes(rows), join_left
    )

    def filter_group() -> None:
        filtered = df[(df["status"] == "active") & (df["score"] >= 90.0)]
        grouped = filtered.groupby("region", sort=False).agg(
            orders=("id", "count"),
            total=("amount", "sum"),
        )
        if len(grouped) < 1:
            raise RuntimeError("pandas_filter_group produced no rows")

    results["pandas_filter_group"] = run_case(
        "pandas_filter_group", 8, 2, approx_bytes(rows), filter_group
    )

    if not args.no_sql:
        add_sql_cases(results)
    if not args.no_arrow:
        add_arrow_cases(df, rows, results)
    add_solution_cases(results)
    if not args.no_ai:
        add_ai_cases(results)

    suite = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "python_version": "{}.{}.{}".format(*os.sys.version_info[:3]),
        "pandas_version": pd.__version__,
        "benchmarks": results,
    }
    if args.save:
        Path(args.save).write_text(json.dumps(suite, indent=2) + "\n")
        print(f"\nsaved: {args.save}")
    if args.qore_json:
        compare_to_qore(Path(args.qore_json), results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
