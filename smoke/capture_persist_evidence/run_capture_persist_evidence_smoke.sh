#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SENTRITS_BIN="${SENTRITS_BIN:-${REPO_ROOT}/build/sentrits}"
SENTRITS_ADMIN_HOST="${SENTRITS_ADMIN_HOST:-127.0.0.1}"
SENTRITS_ADMIN_PORT="${SENTRITS_ADMIN_PORT:-19285}"
SENTRITS_REMOTE_HOST="${SENTRITS_REMOTE_HOST:-127.0.0.1}"
SENTRITS_REMOTE_PORT="${SENTRITS_REMOTE_PORT:-19286}"
SMOKE_TITLE="${SMOKE_TITLE:-smoke-persist-evidence-log}"
SMOKE_EVIDENCE_TITLE="${SMOKE_EVIDENCE_TITLE:-Smoke persist snapshot}"
SMOKE_PRODUCER_HOLD_SECONDS="${SMOKE_PRODUCER_HOLD_SECONDS:-10}"
RUNTIME_DIR="${SCRIPT_DIR}/.runtime"
DATA_DIR="${RUNTIME_DIR}/data"
PID_FILE="${RUNTIME_DIR}/smoke-host.pid"
LOG_FILE="${RUNTIME_DIR}/smoke-host.log"

require_executable() {
  if [[ ! -x "$1" ]]; then
    echo "missing executable: $1" >&2
    exit 1
  fi
}

json_field() {
  local field="$1"
  python3 - "$field" <<'PY'
import json
import sys

field = sys.argv[1]
value = json.load(sys.stdin).get(field)
if not isinstance(value, str) or value == "":
    raise SystemExit(f"missing string field: {field}")
print(value)
PY
}

persist_tail_evidence() {
  local session_id="$1"
  local title="$2"
  python3 - "${SENTRITS_ADMIN_HOST}" "${SENTRITS_ADMIN_PORT}" "${session_id}" "${title}" <<'PY'
import sys
import urllib.parse
import urllib.request

host, port, session_id, title = sys.argv[1:]
query = urllib.parse.urlencode({
    "lines": "20",
    "persist": "true",
    "title": title,
})
url = f"http://{host}:{port}/sessions/{session_id}/evidence/tail?{query}"
with urllib.request.urlopen(url, timeout=5) as response:
    sys.stdout.write(response.read().decode("utf-8"))
PY
}

get_stored_evidence() {
  local session_id="$1"
  local evidence_id="$2"
  python3 - "${SENTRITS_ADMIN_HOST}" "${SENTRITS_ADMIN_PORT}" "${session_id}" "${evidence_id}" <<'PY'
import sys
import urllib.request

host, port, session_id, evidence_id = sys.argv[1:]
url = f"http://{host}:{port}/sessions/{session_id}/stored-evidence/{evidence_id}"
with urllib.request.urlopen(url, timeout=5) as response:
    sys.stdout.write(response.read().decode("utf-8"))
PY
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local label="$3"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    echo "assertion failed: ${label} did not contain ${needle}" >&2
    echo "--- ${label} ---" >&2
    echo "${haystack}" >&2
    exit 1
  fi
}

wait_for_file_exists() {
  local path="$1"
  for _ in {1..50}; do
    if [[ -f "${path}" ]]; then
      return 0
    fi
    sleep 0.1
  done
  return 1
}

cleanup() {
  if [[ -f "${PID_FILE}" ]]; then
    local pid
    pid="$(cat "${PID_FILE}")"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" >/dev/null 2>&1 || true
      for _ in {1..20}; do
        if ! kill -0 "${pid}" >/dev/null 2>&1; then
          break
        fi
        sleep 0.1
      done
      if kill -0 "${pid}" >/dev/null 2>&1; then
        kill -9 "${pid}" >/dev/null 2>&1 || true
      fi
      wait "${pid}" 2>/dev/null || true
    fi
    rm -f "${PID_FILE}"
  fi
}
trap cleanup EXIT

require_executable "${SENTRITS_BIN}"
require_executable "${SCRIPT_DIR}/producer.sh"

mkdir -p "${RUNTIME_DIR}"
rm -rf "${DATA_DIR}"
rm -f "${PID_FILE}" "${LOG_FILE}"

echo "Starting Sentrits smoke host on ${SENTRITS_ADMIN_HOST}:${SENTRITS_ADMIN_PORT}"
"${SENTRITS_BIN}" serve \
  --admin-host "${SENTRITS_ADMIN_HOST}" \
  --admin-port "${SENTRITS_ADMIN_PORT}" \
  --remote-host "${SENTRITS_REMOTE_HOST}" \
  --remote-port "${SENTRITS_REMOTE_PORT}" \
  --datadir "${DATA_DIR}" \
  --no-discovery \
  </dev/null >"${LOG_FILE}" 2>&1 &
host_pid="$!"
echo "${host_pid}" > "${PID_FILE}"

ready=false
for _ in {1..50}; do
  if "${SENTRITS_BIN}" host status \
    --host "${SENTRITS_ADMIN_HOST}" \
    --port "${SENTRITS_ADMIN_PORT}" \
    --json >/dev/null 2>&1; then
    ready=true
    break
  fi
  sleep 0.1
done

if [[ "${ready}" != "true" ]]; then
  echo "smoke host did not become reachable; log follows:" >&2
  sed -n '1,160p' "${LOG_FILE}" >&2
  exit 1
fi

echo "Starting capture session..."
create_json="$("${SENTRITS_BIN}" capture start \
  --host "${SENTRITS_ADMIN_HOST}" \
  --port "${SENTRITS_ADMIN_PORT}" \
  --title "${SMOKE_TITLE}" \
  --workspace "${REPO_ROOT}" \
  --json \
  -e "SMOKE_PRODUCER_HOLD_SECONDS=${SMOKE_PRODUCER_HOLD_SECONDS}" \
  -- "${SCRIPT_DIR}/producer.sh")"

capture_session_id="$(printf '%s' "${create_json}" | json_field sessionId)"
if [[ -z "${capture_session_id}" ]]; then
  echo "failed to parse capture sessionId from capture start response:" >&2
  echo "${create_json}" >&2
  exit 1
fi

echo "Capture session: ${capture_session_id}"
echo "Waiting for deterministic evidence..."
tail_output=""
for _ in {1..50}; do
  tail_output="$("${SENTRITS_BIN}" evidence tail \
    --host "${SENTRITS_ADMIN_HOST}" \
    --port "${SENTRITS_ADMIN_PORT}" \
    --lines 20 \
    "${capture_session_id}")"
  if [[ "${tail_output}" == *"DONE marker=delta"* ]]; then
    break
  fi
  sleep 0.2
done

assert_contains "${tail_output}" "BOOT marker=alpha" "tail evidence"
assert_contains "${tail_output}" "READY marker=beta" "tail evidence"
assert_contains "${tail_output}" "STDERR marker=gamma" "tail evidence"
assert_contains "${tail_output}" "DONE marker=delta" "tail evidence"

echo "Persisting live tail evidence..."
persist_response="$(persist_tail_evidence "${capture_session_id}" "${SMOKE_EVIDENCE_TITLE}")"
stored_evidence_id="$(printf '%s' "${persist_response}" | json_field storedEvidenceId)"
if [[ -z "${stored_evidence_id}" ]]; then
  echo "failed to parse storedEvidenceId from persist response:" >&2
  echo "${persist_response}" >&2
  exit 1
fi

evidence_dir="${DATA_DIR}/evidence/sessions/${capture_session_id}/${stored_evidence_id}"
metadata_file="${evidence_dir}/metadata.json"
evidence_file="${evidence_dir}/evidence.json"
if ! wait_for_file_exists "${metadata_file}" || ! wait_for_file_exists "${evidence_file}"; then
  echo "persisted evidence files were not created under ${evidence_dir}" >&2
  exit 1
fi

metadata_json="$(cat "${metadata_file}")"
evidence_json="$(cat "${evidence_file}")"
assert_contains "${metadata_json}" "\"title\":\"${SMOKE_EVIDENCE_TITLE}\"" "metadata.json"
assert_contains "${metadata_json}" "\"kind\":\"log_snapshot\"" "metadata.json"
assert_contains "${evidence_json}" "DONE marker=delta" "evidence.json"

stored_response="$(get_stored_evidence "${capture_session_id}" "${stored_evidence_id}")"
assert_contains "${stored_response}" "\"title\":\"${SMOKE_EVIDENCE_TITLE}\"" "stored evidence response"
assert_contains "${stored_response}" "\"kind\":\"log_snapshot\"" "stored evidence response"
assert_contains "${stored_response}" "DONE marker=delta" "stored evidence response"

echo
echo "--- persist response ---"
echo "${persist_response}"
echo
echo "--- stored evidence response ---"
echo "${stored_response}"
echo
echo "Smoke passed. Stored evidence files: ${evidence_dir}"
