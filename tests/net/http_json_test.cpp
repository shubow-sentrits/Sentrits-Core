#include <gtest/gtest.h>

#include "vibe/net/json.h"

namespace vibe::net {
namespace {

TEST(HttpJsonTest, EscapesQuotedAndControlCharacters) {
  EXPECT_EQ(JsonEscape("a\"b\\c\n"), "a\\\"b\\\\c\\n");
}

TEST(HttpJsonTest, SerializesHostInfo) {
  const std::string json = ToJsonHostInfo();
  EXPECT_NE(json.find("\"hostId\":\"local-dev-host\""), std::string::npos);
  EXPECT_NE(json.find("\"capabilities\":[\"sessions\",\"rest\"]"), std::string::npos);
}

TEST(HttpJsonTest, SerializesOutputSlice) {
  const std::string json = ToJson(vibe::session::OutputSlice{
      .seq_start = 3,
      .seq_end = 4,
      .data = "hello\n",
  });

  EXPECT_NE(json.find("\"seqStart\":3"), std::string::npos);
  EXPECT_NE(json.find("\"seqEnd\":4"), std::string::npos);
  EXPECT_NE(json.find("\"data\":\"hello\\n\""), std::string::npos);
}

}  // namespace
}  // namespace vibe::net
