from pathlib import Path
import csv
import sys


LOG_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("logs")
OUT_FILE = Path("hbc_metrics_wide.csv")

SCHEME_ORDER = {
    "PLAIN": 0,
    "BFV": 1,
    "CKKS": 2,
}

METRICS_TO_KEEP = [
    "H_initial_device_planning",
    "H_encrypt_X_i",
    "H_plan_compute",
    "H_accept_compute",
    "H_encrypt_delta_i",
    "H_total_household_side",

    "B_init_X_from_first_ciphertext",
    "B_add_X_remaining",
    "B_init_delta_from_first_ciphertext",
    "B_add_delta_remaining",
    "B_total_aggregation",

    "C_decrypt_X",
    "C_decrypt_delta",
    "C_control_logic_until_convergence",
    "C_total_decrypt_plus_control",

    "SIM_total_measured_H_B_C",
]

BASE_COLUMNS = [
    "source_file",
    "run_label",
    "scheme",
    "households",
    "simulations",
    "cache_mode",
]

METRIC_COLUMNS = [metric + "_ms" for metric in METRICS_TO_KEEP]

EXTRA_COLUMNS = [
    "avg_iterations_per_sim",
    "avg_accepted_deltas_per_sim",
    "H_share",
    "B_share",
    "C_share",
]

FIELDNAMES = BASE_COLUMNS + METRIC_COLUMNS + EXTRA_COLUMNS


def safe_float(value):
    try:
        return float(value)
    except (ValueError, TypeError):
        return 0.0


def safe_int(value):
    try:
        return int(float(value))
    except (ValueError, TypeError):
        return 0


def fmt_ms(value):
    # Fixed decimal, no scientific notation.
    return f"{value:.6f}"


def fmt_ratio(value):
    # Fixed decimal, no scientific notation.
    return f"{value:.6f}"


def fmt_count(value):
    # Iterations / accepted deltas may be fractional averages.
    return f"{value:.6f}"


def new_row(source_file):
    row = {field: "" for field in FIELDNAMES}
    row["source_file"] = source_file

    for col in METRIC_COLUMNS:
        row[col] = "0.000000"

    row["avg_iterations_per_sim"] = "0.000000"
    row["avg_accepted_deltas_per_sim"] = "0.000000"
    row["H_share"] = "0.000000"
    row["B_share"] = "0.000000"
    row["C_share"] = "0.000000"

    return row


if not LOG_DIR.exists():
    print(f"ERROR: log directory does not exist: {LOG_DIR}")
    sys.exit(1)

log_files = sorted(LOG_DIR.glob("*.log"))

print(f"Log directory: {LOG_DIR}")
print(f"Log files found: {len(log_files)}")

if not log_files:
    print("ERROR: no .log files found.")
    sys.exit(1)

rows = []
total_csv_rows_found = 0

for log_path in log_files:
    row = new_row(log_path.name)
    csv_rows_found_in_file = 0

    with log_path.open("r", errors="replace") as f:
        for line in f:
            line = line.strip()

            if not line:
                continue

            if line.startswith("scheme,metric,"):
                continue

            if not line.startswith(("PLAIN,", "BFV,", "CKKS,", "SUMMARY,")):
                continue

            parts = line.split(",")

            if len(parts) != 9:
                continue

            csv_rows_found_in_file += 1
            total_csv_rows_found += 1

            scheme, metric, households, simulations, cache_mode, total_us, avg_us, count, cipher_bytes = parts

            if scheme in {"PLAIN", "BFV", "CKKS"}:
                h = safe_int(households)

                row["scheme"] = scheme
                row["households"] = str(h)
                row["simulations"] = str(safe_int(simulations))
                row["cache_mode"] = cache_mode
                row["run_label"] = f"{scheme.lower()}{h}"

                if metric in METRICS_TO_KEEP:
                    avg_ms = safe_float(avg_us) / 1000.0
                    row[metric + "_ms"] = fmt_ms(avg_ms)

            elif scheme == "SUMMARY":
                h = safe_int(households)

                row["households"] = str(h)
                row["simulations"] = str(safe_int(simulations))
                row["cache_mode"] = cache_mode

                if metric == "total_iterations":
                    row["avg_iterations_per_sim"] = fmt_count(safe_float(avg_us))

                elif metric == "accepted_delta_count":
                    row["avg_accepted_deltas_per_sim"] = fmt_count(safe_float(avg_us))

    if csv_rows_found_in_file == 0:
        print(f"WARNING: no CSV rows found in {log_path.name}")
        continue

    sim_total = safe_float(row["SIM_total_measured_H_B_C_ms"])
    h_total = safe_float(row["H_total_household_side_ms"])
    b_total = safe_float(row["B_total_aggregation_ms"])
    c_total = safe_float(row["C_total_decrypt_plus_control_ms"])

    if sim_total > 0:
        row["H_share"] = fmt_ratio(h_total / sim_total)
        row["B_share"] = fmt_ratio(b_total / sim_total)
        row["C_share"] = fmt_ratio(c_total / sim_total)

    rows.append(row)

print(f"CSV rows found total: {total_csv_rows_found}")
print(f"Benchmark runs parsed: {len(rows)}")

if not rows:
    print("ERROR: no benchmark runs parsed. The output would contain only headers.")
    sys.exit(1)


def sort_key(row):
    households = safe_int(row["households"])
    scheme = row["scheme"]
    return households, SCHEME_ORDER.get(scheme, 99)


rows.sort(key=sort_key)

with OUT_FILE.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=FIELDNAMES)
    writer.writeheader()
    writer.writerows(rows)

print(f"Wrote {OUT_FILE}")
