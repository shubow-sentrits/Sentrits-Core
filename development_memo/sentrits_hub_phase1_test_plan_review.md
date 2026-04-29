# Sentrits-Hub Phase 1 Test Plan Review

## Overall

The plan is well-structured. Priority order is correct (unit → integration,
Core persistence and URL parsing first). The minimum gate before Phase 2 is
the right set of requirements.

## Gaps and Issues

### 1. Hub "unit" tests require a real database

Tests 1–4 target `db/queries.go` directly. They need a live Postgres instance
to run. The plan labels them unit tests but they are integration tests by
dependency. Label them correctly so CI infrastructure expectations are clear
from the start.

### 2. Test 8 — `ParseUrl` is not reachable from tests

`ParseUrl` lives in an anonymous namespace in `src/net/hub_client.cpp` and
cannot be called from outside the translation unit. Two options:

- Expose it under a separate header gated on a build flag, or
- Redirect the test through the public heartbeat payload shape (spin up a
  mock server, fire a heartbeat, assert the JSON body). This tests parsing
  indirectly but covers the same correctness surface.

The second option fits the existing integration test pattern and requires no
API surface changes.

### 3. Test 9 — HTTPS verification fixture setup is unspecified

The plan says "HTTPS fails on hostname mismatch" but does not identify how
the test TLS server and certs are provisioned. A self-signed cert generated
at test time via OpenSSL is the natural approach. The existing
`tests/net/http_server_integration_test.cpp` already has TLS fixture
infrastructure that can be reused or extended for this case.

### 4. Test 10 — bind-order invariant is not a unit test

"Hub client starts only after server bind succeeds" is an invariant enforced
inside `HttpServer::Run()`. It cannot be meaningfully verified at the unit
level without constructing a full server. It fits as a named assertion in
`http_server_integration_test.cpp` rather than a standalone unit test.

### 5. Test 12 — shape needs updating after the io_context fix

The cross-thread access issue was resolved by posting snapshot collection to
the server's `io_context`. The concurrency test should now verify the specific
fix:

- snapshot collection does not happen before `Start()` is called (no
  `io_context_` pointer set yet)
- a pending `future.wait_for()` does not block `Stop()` past `kRequestTimeout`
- the background thread exits cleanly when the io_context is stopped before
  the future resolves

The stress-test variant (concurrent create/list/stop) described in the plan
is still worth keeping as a regression guard.

### 6. go.sum is still missing

Issue 3 from the commit audit is unresolved. Run `go mod tidy` in
`~/dev/Sentrits-Hub`, then commit `go.sum`. Until then the Hub build requires
a post-clone step and is not reproducible from the committed state.

## Minimum Gate Before Phase 2

The plan's gate is correct. No changes needed:

- Hub ownership conflict tests passing
- Hub heartbeat replacement tests passing
- Core HTTPS verification tests passing
- Core outbound heartbeat integration test passing
- one explicit test covering the `io_context` snapshot model (updated from
  the original "chosen synchronization model" language now that the fix is
  known)
