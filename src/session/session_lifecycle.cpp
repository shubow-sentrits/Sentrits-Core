#include "vibe/session/session_lifecycle.h"

namespace vibe::session {

auto IsValidTransition(SessionStatus from, SessionStatus to) -> bool {
  switch (from) {
    case SessionStatus::Created:
      return to == SessionStatus::Starting;
    case SessionStatus::Starting:
      return to == SessionStatus::Running || to == SessionStatus::Error;
    case SessionStatus::Running:
      return to == SessionStatus::AwaitingInput || to == SessionStatus::Exited ||
             to == SessionStatus::Error;
    case SessionStatus::AwaitingInput:
      return to == SessionStatus::Running || to == SessionStatus::Exited ||
             to == SessionStatus::Error;
    case SessionStatus::Exited:
    case SessionStatus::Error:
      return false;
  }

  return false;
}

auto SessionLifecycle::state() const -> SessionStatus { return state_; }

auto SessionLifecycle::TryTransition(SessionStatus next_state) -> bool {
  if (!IsValidTransition(state_, next_state)) {
    return false;
  }

  state_ = next_state;
  return true;
}

}  // namespace vibe::session
