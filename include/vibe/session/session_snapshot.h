#ifndef VIBE_SESSION_SESSION_SNAPSHOT_H
#define VIBE_SESSION_SESSION_SNAPSHOT_H

#include <cstdint>
#include <string>
#include <vector>

#include "vibe/session/session_types.h"

namespace vibe::session {

struct GitSummary {
  std::string branch;
  std::vector<std::string> modified_files;
  std::vector<std::string> staged_files;
  std::vector<std::string> untracked_files;
};

struct SessionSnapshot {
  SessionMetadata metadata;
  std::uint64_t current_sequence{0};
  std::string recent_terminal_tail;
  std::vector<std::string> recent_file_changes;
  GitSummary git_summary;
};

}  // namespace vibe::session

#endif
