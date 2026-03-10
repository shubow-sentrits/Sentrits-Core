#include "vibe/session/session_output_buffer.h"

#include <algorithm>
#include <utility>

namespace vibe::session {

SessionOutputBuffer::SessionOutputBuffer(const std::size_t capacity_bytes)
    : capacity_bytes_(capacity_bytes) {}

void SessionOutputBuffer::Append(std::string data) {
  if (data.empty()) {
    return;
  }

  if (data.size() > capacity_bytes_) {
    data = data.substr(data.size() - capacity_bytes_);
  }

  size_bytes_ += data.size();
  chunks_.push_back(OutputChunk{
      .seq = next_sequence_,
      .data = std::move(data),
  });
  next_sequence_ += 1;

  EvictIfNeeded();
}

auto SessionOutputBuffer::capacity_bytes() const -> std::size_t { return capacity_bytes_; }

auto SessionOutputBuffer::size_bytes() const -> std::size_t { return size_bytes_; }

auto SessionOutputBuffer::next_sequence() const -> std::uint64_t { return next_sequence_; }

auto SessionOutputBuffer::latest_sequence() const -> std::optional<std::uint64_t> {
  if (chunks_.empty()) {
    return std::nullopt;
  }

  return chunks_.back().seq;
}

auto SessionOutputBuffer::Tail(const std::size_t max_bytes) const -> OutputSlice {
  if (chunks_.empty() || max_bytes == 0) {
    return OutputSlice{};
  }

  std::size_t bytes_collected = 0;
  std::size_t first_index = chunks_.size();

  for (std::size_t index = chunks_.size(); index > 0; --index) {
    const OutputChunk& chunk = chunks_[index - 1];
    if (bytes_collected >= max_bytes) {
      break;
    }

    bytes_collected += std::min(chunk.data.size(), max_bytes);
    first_index = index - 1;

    if (bytes_collected >= max_bytes) {
      break;
    }
  }

  OutputSlice slice{
      .seq_start = chunks_[first_index].seq,
      .seq_end = chunks_.back().seq,
      .data = "",
  };

  for (std::size_t index = first_index; index < chunks_.size(); ++index) {
    slice.data += chunks_[index].data;
  }

  if (slice.data.size() > max_bytes) {
    slice.data = slice.data.substr(slice.data.size() - max_bytes);
  }

  return slice;
}

auto SessionOutputBuffer::SliceFromSequence(const std::uint64_t first_sequence) const -> OutputSlice {
  if (chunks_.empty()) {
    return OutputSlice{};
  }

  std::size_t first_index = chunks_.size();
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    if (chunks_[index].seq >= first_sequence) {
      first_index = index;
      break;
    }
  }

  if (first_index == chunks_.size()) {
    return OutputSlice{};
  }

  OutputSlice slice{
      .seq_start = chunks_[first_index].seq,
      .seq_end = chunks_.back().seq,
      .data = "",
  };

  for (std::size_t index = first_index; index < chunks_.size(); ++index) {
    slice.data += chunks_[index].data;
  }

  return slice;
}

void SessionOutputBuffer::EvictIfNeeded() {
  while (size_bytes_ > capacity_bytes_ && !chunks_.empty()) {
    size_bytes_ -= chunks_.front().data.size();
    chunks_.pop_front();
  }
}

}  // namespace vibe::session
