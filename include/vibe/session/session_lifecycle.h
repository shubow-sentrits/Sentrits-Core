#ifndef VIBE_SESSION_SESSION_LIFECYCLE_H
#define VIBE_SESSION_SESSION_LIFECYCLE_H

#include "vibe/session/session_types.h"

namespace vibe::session {

[[nodiscard]] auto IsValidTransition(SessionStatus from, SessionStatus to) -> bool;

class SessionLifecycle {
 public:
  SessionLifecycle() = default;

  [[nodiscard]] auto state() const -> SessionStatus;
  [[nodiscard]] auto TryTransition(SessionStatus next_state) -> bool;

 private:
  SessionStatus state_{SessionStatus::Created};
};

}  // namespace vibe::session

#endif
