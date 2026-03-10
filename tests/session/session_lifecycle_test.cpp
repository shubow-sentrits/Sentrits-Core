#include <gtest/gtest.h>

#include "vibe/session/session_lifecycle.h"

namespace vibe::session {
namespace {

TEST(SessionLifecycleTest, StartsInCreatedState) {
  const SessionLifecycle lifecycle;
  EXPECT_EQ(lifecycle.state(), SessionStatus::Created);
}

TEST(SessionLifecycleTest, AcceptsValidTransitions) {
  SessionLifecycle lifecycle;

  EXPECT_TRUE(lifecycle.TryTransition(SessionStatus::Starting));
  EXPECT_TRUE(lifecycle.TryTransition(SessionStatus::Running));
  EXPECT_TRUE(lifecycle.TryTransition(SessionStatus::AwaitingInput));
  EXPECT_TRUE(lifecycle.TryTransition(SessionStatus::Running));
  EXPECT_TRUE(lifecycle.TryTransition(SessionStatus::Exited));
  EXPECT_EQ(lifecycle.state(), SessionStatus::Exited);
}

TEST(SessionLifecycleTest, RejectsInvalidTransitionsWithoutChangingState) {
  SessionLifecycle lifecycle;

  EXPECT_FALSE(lifecycle.TryTransition(SessionStatus::Running));
  EXPECT_EQ(lifecycle.state(), SessionStatus::Created);

  ASSERT_TRUE(lifecycle.TryTransition(SessionStatus::Starting));
  ASSERT_TRUE(lifecycle.TryTransition(SessionStatus::Running));
  ASSERT_TRUE(lifecycle.TryTransition(SessionStatus::Exited));

  EXPECT_FALSE(lifecycle.TryTransition(SessionStatus::Running));
  EXPECT_EQ(lifecycle.state(), SessionStatus::Exited);
}

TEST(SessionLifecycleTest, AllowsFailureFromStartingOrAwaitingInput) {
  SessionLifecycle launch_failure;
  EXPECT_TRUE(launch_failure.TryTransition(SessionStatus::Starting));
  EXPECT_TRUE(launch_failure.TryTransition(SessionStatus::Error));
  EXPECT_EQ(launch_failure.state(), SessionStatus::Error);

  SessionLifecycle runtime_failure;
  ASSERT_TRUE(runtime_failure.TryTransition(SessionStatus::Starting));
  ASSERT_TRUE(runtime_failure.TryTransition(SessionStatus::Running));
  ASSERT_TRUE(runtime_failure.TryTransition(SessionStatus::AwaitingInput));
  EXPECT_TRUE(runtime_failure.TryTransition(SessionStatus::Error));
  EXPECT_EQ(runtime_failure.state(), SessionStatus::Error);
}

TEST(SessionLifecycleTest, ExposesStableStateNames) {
  EXPECT_EQ(ToString(SessionStatus::Created), "Created");
  EXPECT_EQ(ToString(SessionStatus::Starting), "Starting");
  EXPECT_EQ(ToString(SessionStatus::Running), "Running");
  EXPECT_EQ(ToString(SessionStatus::AwaitingInput), "AwaitingInput");
  EXPECT_EQ(ToString(SessionStatus::Exited), "Exited");
  EXPECT_EQ(ToString(SessionStatus::Error), "Error");
}

}  // namespace
}  // namespace vibe::session
