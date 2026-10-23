#!/usr/bin/env bash
# Run the KAS build and on failure collect every error log into one report
# that can be pasted to Claude for diagnosis.
#
# Usage (from ~/VirtualDrive):
#   bash scripts/build-and-report.sh
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KAS_ARGS="kas.yml:kas-utm.yml"
BUILD_DIR="${REPO_ROOT}/build"
REPORT="/tmp/virtualdrive-build-report.txt"

cd "${REPO_ROOT}"

echo "==> Starting build: kas build ${KAS_ARGS}"
echo "    Report will be saved to ${REPORT}"
echo ""

# Run build, capture output
BUILD_LOG="/tmp/virtualdrive-build.log"
kas build ${KAS_ARGS} 2>&1 | tee "${BUILD_LOG}"
BUILD_EXIT=${PIPESTATUS[0]}

if [[ "${BUILD_EXIT}" -eq 0 ]]; then
    echo ""
    echo "Build SUCCEEDED."
    exit 0
fi

echo ""
echo "==> Build FAILED — collecting error report ..."

{
    echo "======================================================"
    echo " VirtualDrive Build Error Report"
    echo " $(date)"
    echo " KAS: $(kas --version 2>&1 | head -1)"
    echo " Host: $(uname -a)"
    echo "======================================================"
    echo ""

    echo "--- kas build output (last 60 lines) ---"
    tail -60 "${BUILD_LOG}"
    echo ""

    echo "--- Failed tasks ---"
    grep -E "^(ERROR|WARNING|NOTE:.*failed)" "${BUILD_LOG}" | head -40
    echo ""

    echo "--- BitBake error logs ---"
    # Find log files modified in the last build run
    if [[ -d "${BUILD_DIR}/tmp" ]]; then
        # Collect the most recently modified error/run logs
        mapfile -t LOGS < <(
            find "${BUILD_DIR}/tmp/work" \
                -name "log.do_*" -o \
                -name "log.task_order" \
                2>/dev/null \
            | xargs ls -t 2>/dev/null \
            | head -20
        )

        for log in "${LOGS[@]}"; do
            # Only include logs that contain errors
            if grep -qiE "(error|failed|exception|traceback)" "${log}" 2>/dev/null; then
                echo ""
                echo ">>> ${log}"
                echo "---"
                tail -80 "${log}"
            fi
        done
    fi

    echo ""
    echo "--- bblayers.conf ---"
    cat "${BUILD_DIR}/conf/bblayers.conf" 2>/dev/null || echo "not found"

    echo ""
    echo "--- local.conf ---"
    cat "${BUILD_DIR}/conf/local.conf" 2>/dev/null || echo "not found"

    echo ""
    echo "--- kas.yml ---"
    cat "${REPO_ROOT}/kas.yml"

    echo ""
    echo "--- kas-utm.yml ---"
    cat "${REPO_ROOT}/kas-utm.yml"

} > "${REPORT}" 2>&1

echo ""
echo "==> Report saved to ${REPORT}"
echo ""
echo "Paste this to Claude:"
echo ""
echo "---------- CUT HERE ----------"
cat "${REPORT}"
echo "---------- CUT HERE ----------"
