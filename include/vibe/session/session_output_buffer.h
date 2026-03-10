#ifndef VIBE_SESSION_SESSION_OUTPUT_BUFFER_H
#define VIBE_SESSION_SESSION_OUTPUT_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace vibe::session {

struct OutputChunk {
  std::uint64_t seq{0};
  std::string data;
};

struct OutputSlice {
  std::uint64_t seq_start{0};
  std::uint64_t seq_end{0};
  std::string data;
};

class SessionOutputBuffer {
 public:
  explicit SessionOutputBuffer(std::size_t capacity_bytes);

  void Append(std::string data);

  [[nodiscard]] auto capacity_bytes() const -> std::size_t;
  [[nodiscard]] auto size_bytes() const -> std::size_t;
  [[nodiscard]] auto next_sequence() const -> std::uint64_t;
  [[nodiscard]] auto latest_sequence() const -> std::optional<std::uint64_t>;
  [[nodiscard]] auto Tail(std::size_t max_bytes) const -> OutputSlice;
  [[nodiscard]] auto SliceFromSequence(std::uint64_t first_sequence) const -> OutputSlice;

 private:
  void EvictIfNeeded();

  std::size_t capacity_bytes_;
  std::size_t size_bytes_{0};
  std::uint64_t next_sequence_{1};
  std::deque<OutputChunk> chunks_;
};

}  // namespace vibe::session

#endif
