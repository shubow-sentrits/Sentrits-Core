#include "vibe/session/session_record.h"

#include <utility>

namespace vibe::session {

SessionRecord::SessionRecord(SessionMetadata metadata) : metadata_(std::move(metadata)) {}

auto SessionRecord::metadata() const -> const SessionMetadata& { return metadata_; }

auto SessionRecord::lifecycle() const -> const SessionLifecycle& { return lifecycle_; }

auto SessionRecord::TryTransition(SessionStatus next_status) -> bool {
  if (!lifecycle_.TryTransition(next_status)) {
    return false;
  }

  metadata_.status = lifecycle_.state();
  return true;
}

void SessionRecord::SetCurrentSequence(const std::uint64_t current_sequence) {
  current_sequence_ = current_sequence;
}

void SessionRecord::SetRecentTerminalTail(std::string recent_terminal_tail) {
  recent_terminal_tail_ = std::move(recent_terminal_tail);
}

void SessionRecord::SetRecentFileChanges(std::vector<std::string> recent_file_changes) {
  recent_file_changes_ = std::move(recent_file_changes);
}

void SessionRecord::SetGitSummary(GitSummary git_summary) { git_summary_ = std::move(git_summary); }

auto SessionRecord::snapshot() const -> SessionSnapshot {
  return SessionSnapshot{
      .metadata = metadata_,
      .current_sequence = current_sequence_,
      .recent_terminal_tail = recent_terminal_tail_,
      .recent_file_changes = recent_file_changes_,
      .git_summary = git_summary_,
  };
}

}  // namespace vibe::session
