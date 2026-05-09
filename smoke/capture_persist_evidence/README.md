# Capture Persist Evidence Smoke

This smoke test verifies the explicit live-evidence persistence path:

- starts a temporary local Sentrits host with its datadir under `smoke/capture_persist_evidence/.runtime/data`
- starts a managed capture session that writes deterministic stdout/stderr lines
- waits until `/evidence/tail` can see the deterministic output
- calls `GET /sessions/{sessionId}/evidence/tail?persist=true&title=...`
- verifies the generated `storedEvidenceId`, the stored-evidence read endpoint, and the files under `evidence/sessions/{sessionId}/{evidenceId}`

Run it after building `sentrits`:

```bash
cmake --build build
smoke/capture_persist_evidence/run_capture_persist_evidence_smoke.sh
```

Useful overrides:

```bash
SENTRITS_BIN=/path/to/sentrits \
SENTRITS_ADMIN_PORT=19295 \
smoke/capture_persist_evidence/run_capture_persist_evidence_smoke.sh
```
