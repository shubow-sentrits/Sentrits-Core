#include <iostream>

#include "vibe/session/session_lifecycle.h"
#include "vibe/session/session_types.h"

auto main() -> int {
  using vibe::session::SessionLifecycle;
  using vibe::session::SessionStatus;
  using vibe::session::ToString;

  const SessionLifecycle lifecycle;
  std::cout << "vibe-hostd bootstrap state: " << ToString(lifecycle.state()) << '\n';
  if (lifecycle.state() != SessionStatus::Created) {
    return 1;
  }
  return 0;
}
