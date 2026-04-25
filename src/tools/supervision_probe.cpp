// supervision_probe: challenges the supervision/attention signaling model with real CLI behaviors.
//
// Two test layers:
//
//   [MUX]   Direct TerminalMultiplexer tests — feeds crafted PTY sequences into libvterm and
//           inspects the rendered-screen classification at the source. No process involved.
//
//   [STACK] Full SessionManager integration — spawns real processes through the PTY stack and
//           asserts that specific supervision/attention/semantic states are observed.
//
// Usage:
//   supervision_probe                         run both layers
//   supervision_probe --mux                   mux layer only
//   supervision_probe --stack                 full-stack layer only
//   supervision_probe --stack <scenario>      one named stack scenario
//   supervision_probe --stack -- cmd [args]   arbitrary command (observe mode, no assertions)
//
// Timing note:
//   last_output_at_unix_ms  = time of last non-cosmetic PTY output (raw-bytes cosmetic filter)
//   last_activity_at_unix_ms = time of any activity (output, status, controller change, git, files)
//   A separate lastRawOutputAtUnixMs / lastMeaningfulOutputAtUnixMs split is not yet in the model.

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "vibe/service/session_manager.h"
#include "vibe/session/pty_process_factory.h"
#include "vibe/session/session_snapshot.h"
#include "vibe/session/session_types.h"
#include "vibe/session/terminal_multiplexer.h"

// ─── helpers ─────────────────────────────────────────────────────────────────

namespace {

auto PadRight(const std::string& s, const std::size_t width) -> std::string {
  if (s.size() >= width) {
    return s;
  }
  return s + std::string(width - s.size(), ' ');
}

auto FormatMs(const long long ms) -> std::string {
  std::ostringstream out;
  out << std::setw(6) << ms << "ms";
  return out.str();
}

auto EscapeForDisplay(const std::string_view data) -> std::string {
  std::string out;
  out.reserve(data.size() * 2);
  for (const char raw_ch : data) {
    const unsigned char ch = static_cast<unsigned char>(raw_ch);
    switch (ch) {
      case '\r': out += "\\r"; break;
      case '\n': out += "\\n"; break;
      case '\x1b': out += "\\e"; break;
      case '\x07': out += "\\a"; break;
      default:
        if (ch < 0x20 || ch == 0x7f) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\x%02x", ch);
          out += buf;
        } else {
          out += static_cast<char>(ch);
        }
        break;
    }
  }
  return out;
}

// ─── MUX LAYER ───────────────────────────────────────────────────────────────

struct MuxCase {
  std::string name;
  std::string description;
  std::string setup_data;
  std::string input_data;
  vibe::session::TerminalSemanticChangeKind expected_kind;
};

// Each case targets a specific path in ClassifySemanticChange.
// CosmeticChurn requires: appended_visible_character_count > 0 AND <= 4 AND only_trivial_tail_growth
// \r-overwrites produce chars=0 (after doesn't have before as prefix) → fall to MeaningfulOutput.
// This is the known gap these cases expose.
const std::vector<MuxCase> kMuxCases = {
    // ── spinners: \r-overwrite on existing content ──
    {
        .name = "spinner_pipe",
        .description = "\\r| on existing content: chars=0, line changed in-place — gap",
        .setup_data = "waiting...",
        .input_data = "\r|",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    {
        .name = "spinner_cycle",
        .description = "\\r/ same pattern, second cycle",
        .setup_data = "waiting...",
        .input_data = "\r/",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    {
        .name = "spinner_on_empty",
        .description = "\\r| on blank line: empty→'|' is tail-append of trivial mark → passes",
        .setup_data = "",
        .input_data = "\r|",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    // ── progress bars: \r-overwrite with alnum content ──
    {
        .name = "progress_bar_plain",
        .description = "\\r[====>    ] 50%: overwrites '[         ]  0%' in-place — gap",
        .setup_data = "[         ]  0%",
        .input_data = "\r[====>    ] 50%",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    {
        .name = "progress_bar_with_erase",
        .description = "\\x1b[2K\\r[====>] 50%: erase-then-overwrite — gap",
        .setup_data = "[         ]  0%",
        .input_data = "\x1b[2K\r[====>    ] 50%",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    {
        .name = "progress_bar_no_prior",
        .description = "progress bar on blank screen: chars=15, exceeds <=4 CosmeticChurn threshold — gap",
        .setup_data = "",
        .input_data = "\r[====>    ] 50%",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    // ── cursor-only sequences ──
    {
        .name = "cursor_up_only",
        .description = "\\x1b[A: cursor moves, no content change",
        .setup_data = "line1\r\nline2",
        .input_data = "\x1b[A",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CursorOnly,
    },
    {
        .name = "erase_line_carriage",
        .description = "\\x1b[2K\\r: erases line content → lines=1, blocks CursorOnly — gap",
        .setup_data = "some content",
        .input_data = "\x1b[2K\r",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CursorOnly,
    },
    {
        .name = "cursor_home",
        .description = "\\x1b[H: absolute position, no content change",
        .setup_data = "hello",
        .input_data = "\x1b[H",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CursorOnly,
    },
    // ── blocked prompt: stable content with cosmetic churn ──
    {
        .name = "blocked_prompt_stable",
        .description = "re-appending identical content: render_revision unchanged → None (visually stable)",
        .setup_data = "Allow this? [y/n]: ",
        .input_data = "\r\x1b[2KAllow this? [y/n]: ",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::None,
    },
    {
        .name = "blocked_prompt_spinner",
        .description = "spinner char appended after stable prompt — small trivial tail growth",
        .setup_data = "Allow this? [y/n]:  ",
        .input_data = "\x1b[19G|",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    // ── true meaningful output ──
    {
        .name = "newline_text",
        .description = "text + \\r\\n: scrollback push → MeaningfulOutput",
        .setup_data = "",
        .input_data = "Compiling main.cpp\r\n",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::MeaningfulOutput,
    },
    {
        .name = "multi_line_burst",
        .description = "two \\r\\n lines: multiple scrollback pushes",
        .setup_data = "",
        .input_data = "error: foo\r\nnote: bar\r\n",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::MeaningfulOutput,
    },
    // ── alt screen ──
    {
        .name = "alt_screen_enter",
        .description = "\\x1b[?1049h: vim/htop TUI open",
        .setup_data = "$ ",
        .input_data = "\x1b[?1049h",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::AltScreenTransition,
    },
    {
        .name = "alt_screen_exit",
        .description = "\\x1b[?1049l: vim/htop restore shell",
        .setup_data = "$ ",
        .input_data = "\x1b[?1049l",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::AltScreenTransition,
    },
    // ── mixed: npm/pip install style ──
    {
        .name = "download_progress_line",
        .description = "progress rewrite after meaningful header — gap",
        .setup_data = "Fetching packages...\r\n[         ]  0%",
        .input_data = "\r[#####    ] 50%",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::CosmeticChurn,
    },
    {
        .name = "meaningful_after_progress",
        .description = "\\r\\n completion line after progress state",
        .setup_data = "[#########] 100%",
        .input_data = "\r\nDone in 1.23s\r\n",
        .expected_kind = vibe::session::TerminalSemanticChangeKind::MeaningfulOutput,
    },
};

struct MuxResult {
  vibe::session::TerminalSemanticChange change;
  std::string before_cursor_line;
  std::string after_cursor_line;
  std::size_t cursor_row_before{0};
  std::size_t cursor_col_before{0};
  std::string all_visible_before;
  bool passed{false};
};

auto RunMuxCase(const MuxCase& tc) -> MuxResult {
  using vibe::session::TerminalMultiplexer;
  using vibe::session::TerminalSize;

  TerminalMultiplexer mux(TerminalSize{.columns = 80, .rows = 6});

  if (!tc.setup_data.empty()) {
    mux.Append(tc.setup_data);
  }

  const auto snap_before = mux.snapshot();
  const std::size_t crow = snap_before.cursor_row;
  const std::size_t ccol = snap_before.cursor_column;
  const std::string before_line =
      crow < snap_before.visible_lines.size() ? snap_before.visible_lines[crow] : "";

  // Compact multi-line summary for small terminal
  std::string all_before;
  for (const auto& l : snap_before.visible_lines) {
    if (!l.empty()) {
      if (!all_before.empty()) {
        all_before += " | ";
      }
      all_before += l;
    }
  }

  mux.Append(tc.input_data);

  const auto snap_after = mux.snapshot();
  const std::string after_line =
      crow < snap_after.visible_lines.size() ? snap_after.visible_lines[crow] : "";

  const auto change = mux.last_semantic_change();
  return MuxResult{
      .change = change,
      .before_cursor_line = before_line,
      .after_cursor_line = after_line,
      .cursor_row_before = crow,
      .cursor_col_before = ccol,
      .all_visible_before = all_before,
      .passed = change.kind == tc.expected_kind,
  };
}

auto RunMuxLayer() -> int {
  std::cout << "\n\033[1;34m╔══ MUX LAYER: TerminalMultiplexer classification ══╗\033[0m\n";
  std::cout << "Feeds crafted PTY sequences directly into libvterm. No process involved.\n\n";

  std::cout << "  " << PadRight("case", 34) << PadRight("input", 26)
            << PadRight("got", 22) << PadRight("expected", 22) << "\n";
  std::cout << "  " << std::string(104, '-') << "\n";

  int passed = 0;
  int failed = 0;

  for (const auto& tc : kMuxCases) {
    const MuxResult r = RunMuxCase(tc);
    const std::string got_str(vibe::session::ToString(r.change.kind));
    const std::string exp_str(vibe::session::ToString(tc.expected_kind));
    const std::string input_disp = "\"" + EscapeForDisplay(tc.input_data) + "\"";

    const std::string status_str = r.passed ? "\033[32mPASS\033[0m" : "\033[31mFAIL\033[0m";
    std::cout << "  " << status_str << " " << PadRight(tc.name, 32)
              << PadRight(input_disp, 26);
    if (r.passed) {
      std::cout << PadRight(got_str, 22);
    } else {
      std::cout << "\033[31m" << PadRight(got_str, 22) << "\033[0m";
    }
    std::cout << PadRight(exp_str, 22);
    std::cout << " [lines=" << r.change.changed_visible_line_count
              << " scroll=" << r.change.scrollback_lines_added
              << " chars=" << r.change.appended_visible_character_count
              << " cursor=" << (r.change.cursor_moved ? "y" : "n")
              << " cur=(" << r.cursor_row_before << "," << r.cursor_col_before << ")]\n";

    if (!r.passed) {
      if (!r.all_visible_before.empty()) {
        std::cout << "       screen: \"" << EscapeForDisplay(r.all_visible_before) << "\"\n";
      }
      std::cout << "       cursor line before: \"" << EscapeForDisplay(r.before_cursor_line)
                << "\"  →  after: \"" << EscapeForDisplay(r.after_cursor_line) << "\"\n";
      std::cout << "       WHY: " << tc.description << "\n\n";
      failed += 1;
    } else {
      passed += 1;
    }
  }

  std::cout << "\n  MUX: " << passed << " passed, " << failed << " failed";
  if (failed > 0) {
    std::cout << "  ← gaps in ClassifySemanticChange";
  }
  std::cout << "\n";
  return failed;
}

// ─── STACK LAYER ─────────────────────────────────────────────────────────────

// A state that must be observed at least once during a scenario run.
// Fields left nullopt are wildcards — not checked.
struct StackExpectation {
  std::string label;
  std::optional<vibe::session::SessionStatus> status;
  std::optional<vibe::session::SupervisionState> supervision;
  std::optional<vibe::session::AttentionState> attention;
  std::optional<vibe::session::AttentionReason> reason;
  std::optional<vibe::session::TerminalSemanticChangeKind> semantic;
};

struct StackScenario {
  std::string name;
  std::string description;
  std::string shell_command;
  std::chrono::milliseconds max_duration{std::chrono::seconds(12)};
  std::vector<StackExpectation> must_observe;
};

const std::vector<StackScenario> kStackScenarios = {
    {
        .name = "spinner",
        .description = "\\r-only spinner — single trivial-mark char overwritten each frame",
        .shell_command =
            "i=0; while [ $i -lt 20 ]; do "
            "printf '\\r|'; sleep 0.04; printf '\\r/'; sleep 0.04; "
            "printf '\\r-'; sleep 0.04; printf '\\r\\\\'; sleep 0.04; "
            "i=$((i+1)); done; printf '\\r \\n'",
        .max_duration = std::chrono::seconds(8),
        .must_observe = {
            {.label = "running", .status = vibe::session::SessionStatus::Running},
            {.label = "exited with clean attention",
             .status = vibe::session::SessionStatus::Exited,
             .attention = vibe::session::AttentionState::Info,
             .reason = vibe::session::AttentionReason::SessionExitedCleanly},
        },
    },
    {
        .name = "progress_bar",
        .description = "\\r-overwriting progress bar — semantic=cosmetic_churn, supervision stays quiet",
        .shell_command =
            "i=0; while [ $i -le 100 ]; do "
            "bar=$(printf '%0.s=' $(seq 1 $((i/5+1)))); "
            "pad=$(printf '%0.s ' $(seq $((i/5+1)) 22)); "
            "printf '\\r[%s%s] %3d%%' \"$bar\" \"$pad\" \"$i\"; "
            "sleep 0.03; i=$((i+5)); done; printf '\\n'",
        .max_duration = std::chrono::seconds(8),
        .must_observe = {
            // Progress bar \r-overwrites are cosmetic — supervision must stay quiet.
            // If this FAILS it means cosmetic output is incorrectly triggering active supervision.
            {.label = "supervision quiet during cosmetic progress bar",
             .status = vibe::session::SessionStatus::Running,
             .supervision = vibe::session::SupervisionState::Quiet},
            {.label = "exited cleanly",
             .status = vibe::session::SessionStatus::Exited,
             .attention = vibe::session::AttentionState::Info,
             .reason = vibe::session::AttentionReason::SessionExitedCleanly},
        },
    },
    {
        .name = "build_output",
        .description = "newline-terminated compiler-style burst — MeaningfulOutput throughout",
        .shell_command =
            "for f in main.cpp utils.cpp server.cpp client.cpp proto.cpp; do "
            "printf 'Compiling %s...\\n' \"$f\"; sleep 0.05; done; "
            "printf 'Linking binary...\\n'; sleep 0.05; printf 'Build complete.\\n'",
        .max_duration = std::chrono::seconds(5),
        .must_observe = {
            {.label = "meaningful output observed",
             .semantic = vibe::session::TerminalSemanticChangeKind::MeaningfulOutput},
            {.label = "supervision became active",
             .supervision = vibe::session::SupervisionState::Active},
            {.label = "clean exit",
             .status = vibe::session::SessionStatus::Exited,
             .attention = vibe::session::AttentionState::Info,
             .reason = vibe::session::AttentionReason::SessionExitedCleanly},
        },
    },
    {
        .name = "blocked_prompt",
        .description = "stable permission prompt with cosmetic spinner — the key real-world case",
        .shell_command =
            // Print the prompt text (meaningful), then spin a cosmetic indicator for 3s.
            // Expected: initial meaningful output, then repeated cosmetic-only updates.
            "printf 'Allow this operation? [y/n]: '; "
            "i=0; while [ $i -lt 15 ]; do "
            "printf '\\r\\033[2KAllow this operation? [y/n]: |'; sleep 0.1; "
            "printf '\\r\\033[2KAllow this operation? [y/n]: /'; sleep 0.1; "
            "printf '\\r\\033[2KAllow this operation? [y/n]: -'; sleep 0.1; "
            "i=$((i+1)); done; "
            "printf '\\r\\033[2KAllow this operation? [y/n]: '; "
            "printf '\\n'",
        .max_duration = std::chrono::seconds(6),
        .must_observe = {
            {.label = "session runs while prompt is active",
             .status = vibe::session::SessionStatus::Running},
            // Supervision should not stay permanently active if output is cosmetic-only.
            // This assertion will FAIL if cosmetic churn keeps supervision=active.
            // That failure is informative — it means the raw-bytes filter disagrees
            // with the rendered semantic classifier.
            {.label = "supervision goes quiet while prompt spins",
             .supervision = vibe::session::SupervisionState::Quiet},
        },
    },
    {
        .name = "cursor_spam",
        .description = "cursor movement via \\x1b[1A\\x1b[2K\\x1b[1B — no visible content",
        .shell_command =
            "printf 'Header line\\n'; "
            "i=0; while [ $i -lt 30 ]; do "
            "printf '\\033[1A\\033[2K\\033[1B'; sleep 0.05; i=$((i+1)); done; "
            "printf 'done\\n'",
        .max_duration = std::chrono::seconds(5),
        .must_observe = {
            {.label = "session ran", .status = vibe::session::SessionStatus::Running},
            {.label = "clean exit", .status = vibe::session::SessionStatus::Exited},
        },
    },
    {
        .name = "quiet_then_idle",
        .description = "burst then 6s silence — supervision active→quiet decay at 5s",
        .shell_command =
            "printf 'Starting task...\\n'; sleep 0.1; "
            "printf 'Processing data...\\n'; sleep 0.1; "
            "printf 'Done.\\n'; sleep 7",
        .max_duration = std::chrono::seconds(10),
        .must_observe = {
            {.label = "supervision active after burst",
             .supervision = vibe::session::SupervisionState::Active},
            {.label = "supervision quiet after 5s window",
             .status = vibe::session::SessionStatus::Running,
             .supervision = vibe::session::SupervisionState::Quiet},
        },
    },
    {
        .name = "exit_clean",
        .description = "immediate clean exit → info/session_exited_cleanly",
        .shell_command = "printf 'Hello from probe\\n'; exit 0",
        .max_duration = std::chrono::seconds(3),
        .must_observe = {
            {.label = "exited with info/session_exited_cleanly",
             .status = vibe::session::SessionStatus::Exited,
             .attention = vibe::session::AttentionState::Info,
             .reason = vibe::session::AttentionReason::SessionExitedCleanly},
        },
    },
    {
        .name = "exit_error",
        .description = "non-zero exit → intervention/session_error",
        .shell_command = "printf 'Something went wrong\\n'; exit 1",
        .max_duration = std::chrono::seconds(3),
        .must_observe = {
            {.label = "error status with intervention attention",
             .status = vibe::session::SessionStatus::Error,
             .attention = vibe::session::AttentionState::Intervention,
             .reason = vibe::session::AttentionReason::SessionError},
        },
    },
};

struct ObservedState {
  vibe::session::SessionStatus status{vibe::session::SessionStatus::Created};
  vibe::session::SupervisionState supervision{vibe::session::SupervisionState::Quiet};
  vibe::session::AttentionState attention{vibe::session::AttentionState::None};
  vibe::session::AttentionReason reason{vibe::session::AttentionReason::None};
  vibe::session::SessionInteractionKind interaction{vibe::session::SessionInteractionKind::Unknown};
  vibe::session::TerminalSemanticChangeKind semantic{vibe::session::TerminalSemanticChangeKind::None};
  // Timing signals relative to session start_ms
  long long output_age_ms{-1};    // ms since last non-cosmetic output (-1 = never)
  long long activity_age_ms{-1};  // ms since last any activity (-1 = never)

  [[nodiscard]] auto CoreEquals(const ObservedState& o) const -> bool {
    return status == o.status && supervision == o.supervision && attention == o.attention &&
           reason == o.reason && interaction == o.interaction && semantic == o.semantic;
  }
};

auto ExtractState(const vibe::service::SessionSummary& s,
                  const vibe::session::SessionSnapshot& snap,
                  const std::int64_t now_unix_ms) -> ObservedState {
  ObservedState state;
  state.status = s.status;
  state.supervision = s.supervision_state;
  state.attention = s.attention_state;
  state.reason = s.attention_reason;
  state.interaction = s.interaction_kind;
  state.semantic = snap.signals.terminal_semantic_change.kind;
  if (s.last_output_at_unix_ms.has_value()) {
    state.output_age_ms = now_unix_ms - *s.last_output_at_unix_ms;
  }
  if (s.last_activity_at_unix_ms.has_value()) {
    state.activity_age_ms = now_unix_ms - *s.last_activity_at_unix_ms;
  }
  return state;
}

auto MatchesExpectation(const ObservedState& s, const StackExpectation& e) -> bool {
  if (e.status.has_value() && s.status != *e.status) return false;
  if (e.supervision.has_value() && s.supervision != *e.supervision) return false;
  if (e.attention.has_value() && s.attention != *e.attention) return false;
  if (e.reason.has_value() && s.reason != *e.reason) return false;
  if (e.semantic.has_value() && s.semantic != *e.semantic) return false;
  return true;
}

void PrintStackHeader() {
  std::cout << "  " << PadRight("t=", 9) << "  " << PadRight("status", 10) << " "
            << "supervision=" << PadRight("...", 8) << " "
            << "semantic=" << PadRight("...", 22) << " "
            << "attn=" << PadRight(".../...", 32) << " "
            << "out_age  act_age\n";
  std::cout << "  " << std::string(118, '-') << "\n";
}

void PrintStackRow(const long long elapsed_ms, const ObservedState& prev,
                   const ObservedState& curr,
                   const vibe::session::TerminalSemanticChange& detail) {
  const bool first = prev.status == vibe::session::SessionStatus::Created;

  std::cout << "  t=" << FormatMs(elapsed_ms) << "  ";

  const std::string status_str(vibe::session::ToString(curr.status));
  if (curr.status != prev.status || first) {
    std::cout << "\033[1m" << PadRight(status_str, 10) << "\033[0m";
  } else {
    std::cout << PadRight(status_str, 10);
  }
  std::cout << " ";

  const std::string sup_str(vibe::session::ToString(curr.supervision));
  const bool sup_chg = curr.supervision != prev.supervision;
  if (sup_chg) std::cout << "\033[33m";
  std::cout << "supervision=" << PadRight(sup_str, 8);
  if (sup_chg) std::cout << "\033[0m";
  std::cout << " ";

  const std::string sem_str(vibe::session::ToString(curr.semantic));
  const bool sem_chg = curr.semantic != prev.semantic;
  if (sem_chg) std::cout << "\033[36m";
  std::cout << "semantic=" << PadRight(sem_str, 22);
  if (sem_chg) std::cout << "\033[0m";
  std::cout << " ";

  const std::string attn_str =
      std::string(vibe::session::ToString(curr.attention)) + "/" +
      std::string(vibe::session::ToString(curr.reason));
  const bool attn_chg = curr.attention != prev.attention || curr.reason != prev.reason;
  if (attn_chg) std::cout << "\033[35m";
  std::cout << "attn=" << PadRight(attn_str, 32);
  if (attn_chg) std::cout << "\033[0m";

  // Timing columns: last_output age and last_activity age
  if (curr.output_age_ms >= 0) {
    std::cout << " " << PadRight(std::to_string(curr.output_age_ms) + "ms", 9);
  } else {
    std::cout << " " << PadRight("never", 9);
  }
  if (curr.activity_age_ms >= 0) {
    std::cout << " " << std::to_string(curr.activity_age_ms) << "ms";
  } else {
    std::cout << " never";
  }

  if (curr.semantic != vibe::session::TerminalSemanticChangeKind::None) {
    std::cout << "  [lines=" << detail.changed_visible_line_count
              << " scroll=" << detail.scrollback_lines_added
              << " chars=" << detail.appended_visible_character_count << "]";
  }
  std::cout << "\n";

  // Flag supervision/semantic disagreement inline
  const bool sup_active = curr.supervision == vibe::session::SupervisionState::Active;
  const bool sem_cosmetic =
      curr.semantic == vibe::session::TerminalSemanticChangeKind::CosmeticChurn ||
      curr.semantic == vibe::session::TerminalSemanticChangeKind::CursorOnly;
  if (sup_active && sem_cosmetic) {
    std::cout << "         \033[33m↑ raw-bytes filter says active, vterm classifier says "
              << vibe::session::ToString(curr.semantic) << " — model layer disagree\033[0m\n";
  }
}

auto RunStackScenario(const std::string& name, const std::string& description,
                      const std::string& shell_cmd,
                      const std::chrono::milliseconds max_dur,
                      const std::vector<StackExpectation>& must_observe,
                      const std::string& workspace) -> bool {
  std::cout << "\n\033[1;34m--- STACK: " << name << " ---\033[0m\n";
  std::cout << "  " << description << "\n\n";
  PrintStackHeader();

  vibe::service::SessionManager manager(nullptr, vibe::session::CreatePlatformPtyProcess,
                                        std::chrono::milliseconds(500),
                                        std::chrono::milliseconds(500));

  const auto session = manager.CreateSession(vibe::service::CreateSessionRequest{
      .provider = vibe::session::ProviderType::Claude,
      .workspace_root = workspace,
      .title = name,
      .conversation_id = std::nullopt,
      .command_argv = std::nullopt,
      .command_shell = shell_cmd,
      .group_tags = {},
      .env_mode = vibe::session::EnvMode::Clean,
  });

  if (!session.has_value()) {
    std::cout << "  \033[31mFAIL: session creation failed: "
              << manager.last_create_error_message() << "\033[0m\n";
    return false;
  }

  const std::string session_id = session->id.value();
  const auto start_time = std::chrono::steady_clock::now();
  const auto start_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

  ObservedState prev{};
  std::size_t change_count = 0;
  bool process_exited = false;

  // Track which expectations have been satisfied
  std::vector<bool> satisfied(must_observe.size(), false);

  auto CheckExpectations = [&](const ObservedState& curr) {
    for (std::size_t i = 0; i < must_observe.size(); ++i) {
      if (!satisfied[i] && MatchesExpectation(curr, must_observe[i])) {
        satisfied[i] = true;
      }
    }
  };

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    if (elapsed >= max_dur) {
      break;
    }

    manager.PollAll(10);
    const auto now_unix_ms =
        start_unix_ms +
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time)
            .count();

    const auto summary = manager.GetSession(session_id);
    const auto snapshot = manager.GetSnapshot(session_id);
    if (!summary.has_value() || !snapshot.has_value()) {
      break;
    }

    const ObservedState curr = ExtractState(*summary, *snapshot, now_unix_ms);
    CheckExpectations(curr);

    if (!curr.CoreEquals(prev)) {
      PrintStackRow(elapsed.count(), prev, curr, snapshot->signals.terminal_semantic_change);
      prev = curr;
      change_count += 1;
    }

    const bool done = summary->status == vibe::session::SessionStatus::Exited ||
                      summary->status == vibe::session::SessionStatus::Error;
    if (done && !process_exited) {
      process_exited = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      manager.PollAll(10);
      const auto final_unix_ms =
          start_unix_ms +
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start_time)
              .count();
      const auto fs = manager.GetSession(session_id);
      const auto fn = manager.GetSnapshot(session_id);
      if (fs.has_value() && fn.has_value()) {
        const ObservedState fc = ExtractState(*fs, *fn, final_unix_ms);
        CheckExpectations(fc);
        if (!fc.CoreEquals(prev)) {
          const auto fe = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start_time);
          PrintStackRow(fe.count(), prev, fc, fn->signals.terminal_semantic_change);
          prev = fc;
          change_count += 1;
        }
      }
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  static_cast<void>(manager.StopSession(session_id));

  const auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time);
  std::cout << "\n  " << change_count << " transition(s) in " << total.count() << "ms\n";

  // Print assertion results
  int assertion_failures = 0;
  if (!must_observe.empty()) {
    std::cout << "\n  Assertions:\n";
    for (std::size_t i = 0; i < must_observe.size(); ++i) {
      const bool ok = satisfied[i];
      std::cout << "    " << (ok ? "\033[32mPASS\033[0m" : "\033[31mFAIL\033[0m")
                << "  must observe: " << must_observe[i].label << "\n";
      if (!ok) {
        assertion_failures += 1;
      }
    }
  }

  return assertion_failures == 0;
}

auto RunStackLayer(const std::optional<std::string>& only_scenario,
                   const std::optional<std::string>& custom_command,
                   const std::string& workspace) -> int {
  std::cout << "\n\033[1;34m╔══ STACK LAYER: SessionManager end-to-end ══╗\033[0m\n";
  std::cout << "Real PTY processes. Timing columns: out_age=ms since last non-cosmetic output,\n";
  std::cout << "act_age=ms since last activity. (rawOutput/meaningfulOutput split not yet in model.)\n";

  if (custom_command.has_value()) {
    RunStackScenario("custom", *custom_command, *custom_command,
                     std::chrono::seconds(30), {}, workspace);
    return 0;
  }

  if (only_scenario.has_value()) {
    for (const auto& s : kStackScenarios) {
      if (s.name == *only_scenario) {
        const bool ok = RunStackScenario(s.name, s.description, s.shell_command,
                                         s.max_duration, s.must_observe, workspace);
        return ok ? 0 : 1;
      }
    }
    std::cerr << "Unknown stack scenario: " << *only_scenario << "\n";
    return 1;
  }

  int failures = 0;
  for (const auto& s : kStackScenarios) {
    if (!RunStackScenario(s.name, s.description, s.shell_command,
                          s.max_duration, s.must_observe, workspace)) {
      failures += 1;
    }
  }
  return failures;
}

void PrintUsage() {
  std::cout << "Usage:\n"
            << "  supervision_probe                  run both layers\n"
            << "  supervision_probe --mux            mux layer only\n"
            << "  supervision_probe --stack          stack layer only\n"
            << "  supervision_probe --stack <name>   one named scenario\n"
            << "  supervision_probe --stack -- cmd   arbitrary command (observe only)\n"
            << "\nStack scenarios: ";
  for (const auto& s : kStackScenarios) {
    std::cout << s.name << " ";
  }
  std::cout << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::string workspace = std::filesystem::temp_directory_path().string();

  if (argc >= 2 && std::string(argv[1]) == "--help") {
    PrintUsage();
    return 0;
  }

  bool run_mux = true;
  bool run_stack = true;
  std::optional<std::string> only_scenario;
  std::optional<std::string> custom_command;

  if (argc >= 2) {
    const std::string flag = argv[1];
    if (flag == "--mux") {
      run_stack = false;
    } else if (flag == "--stack") {
      run_mux = false;
      if (argc >= 4 && std::string(argv[2]) == "--") {
        std::string cmd = "exec";
        for (int i = 3; i < argc; ++i) {
          cmd += " '";
          for (const char ch : std::string(argv[i])) {
            if (ch == '\'') cmd += "'\"'\"'";
            else cmd += ch;
          }
          cmd += "'";
        }
        custom_command = cmd;
      } else if (argc >= 3 && std::string(argv[2]) != "--") {
        only_scenario = argv[2];
      }
    } else {
      PrintUsage();
      return 1;
    }
  }

  std::cout << "\033[1mSupervision Probe — challenging the signaling model\033[0m\n";

  int total_failures = 0;
  if (run_mux) {
    total_failures += RunMuxLayer();
  }
  if (run_stack) {
    total_failures += RunStackLayer(only_scenario, custom_command, workspace);
  }

  std::cout << "\n\033[1mDone.\033[0m";
  if (total_failures > 0) {
    std::cout << "  \033[31m" << total_failures << " failure(s)\033[0m";
  } else {
    std::cout << "  \033[32mall passed\033[0m";
  }
  std::cout << "\n";
  return total_failures > 0 ? 1 : 0;
}
