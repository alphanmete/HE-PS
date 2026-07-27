#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${UNDER_INHIBIT:-}" ]] && command -v systemd-inhibit >/dev/null 2>&1; then
  export UNDER_INHIBIT=1
  exec systemd-inhibit \
    --what=sleep:idle:handle-lid-switch \
    --why="Running H/B/C benchmark campaign" \
    --mode=block \
    "$0" "$@"
fi

OUTDIR="logs"

COUNTS=(1000 500 250 100 50 10)
REPS=1000
CACHE_MODE="cold"
VERBOSITY="quiet"

mkdir -p "${OUTDIR}"

echo "Starting final H/B/C full-convergence benchmark campaign"
echo "Output directory: ${OUTDIR}"
echo "Household counts: ${COUNTS[*]}"
echo "Simulations per condition: ${REPS}"
echo "Cache mode: ${CACHE_MODE}"
echo "Verbosity: ${VERBOSITY}"
echo

sudo -v

for H in "${COUNTS[@]}"; do
  echo "============================================================"
  echo "Plain H/B/C benchmark | households=${H} | simulations=${REPS}"
  echo "============================================================"
  sudo perf stat ./cmake-build-plain/he_component_bench "${H}" "${REPS}" "${CACHE_MODE}" "${VERBOSITY}" \
    2>&1 | tee "${OUTDIR}/plain_h${H}_s${REPS}_${CACHE_MODE}.log"

  echo "============================================================"
  echo "BFV H/B/C benchmark | households=${H} | simulations=${REPS}"
  echo "============================================================"
  sudo perf stat ./cmake-build-bfv/he_component_bench "${H}" "${REPS}" "${CACHE_MODE}" "${VERBOSITY}" \
    2>&1 | tee "${OUTDIR}/bfv_h${H}_s${REPS}_${CACHE_MODE}.log"

  echo "============================================================"
  echo "CKKS H/B/C benchmark | households=${H} | simulations=${REPS}"
  echo "============================================================"
  sudo perf stat ./cmake-build-ckks/he_component_bench "${H}" "${REPS}" "${CACHE_MODE}" "${VERBOSITY}" \
    2>&1 | tee "${OUTDIR}/ckks_h${H}_s${REPS}_${CACHE_MODE}.log"

  echo
done

echo "Final H/B/C benchmark campaign finished."
echo "Logs saved in: ${OUTDIR}/"