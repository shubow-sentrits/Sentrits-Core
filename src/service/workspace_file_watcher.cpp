#include "vibe/service/workspace_file_watcher.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <utility>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#endif

namespace vibe::service {

namespace {

auto FileTimeToTicks(const std::filesystem::file_time_type value) -> std::uint64_t {
  return static_cast<std::uint64_t>(value.time_since_epoch().count());
}

auto CompareObservedFilePath(const WorkspaceFileWatcher::ObservedFile& left,
                             const WorkspaceFileWatcher::ObservedFile& right) -> bool {
  return left.workspace_path < right.workspace_path;
}

auto TrimRecent(std::vector<std::string> changed_files, const std::size_t recent_limit)
    -> std::vector<std::string> {
  std::sort(changed_files.begin(), changed_files.end());
  changed_files.erase(std::unique(changed_files.begin(), changed_files.end()), changed_files.end());
  if (changed_files.size() > recent_limit) {
    changed_files.erase(
        changed_files.begin(),
        changed_files.begin() +
            static_cast<std::ptrdiff_t>(changed_files.size() - recent_limit));
  }
  return changed_files;
}

}  // namespace

WorkspaceFileWatcher::WorkspaceFileWatcher(std::string workspace_root, const std::size_t recent_limit)
    : workspace_root_(std::move(workspace_root)),
      recent_limit_(recent_limit) {
  std::error_code error_code;
  root_path_ =
      std::filesystem::weakly_canonical(std::filesystem::absolute(workspace_root_), error_code);
  if (error_code) {
    root_path_.clear();
  }

#ifdef __linux__
  InitializeNativeWatcher();
#endif
}

WorkspaceFileWatcher::~WorkspaceFileWatcher() {
#ifdef __linux__
  if (inotify_fd_ >= 0) {
    close(inotify_fd_);
    inotify_fd_ = -1;
  }
#endif
}

auto WorkspaceFileWatcher::PollChangedFiles() -> std::vector<std::string> {
#ifdef __linux__
  if (inotify_fd_ >= 0) {
    return PollChangedFilesNative();
  }
#endif
  return PollChangedFilesByScan();
}

auto WorkspaceFileWatcher::PollChangedFilesByScan() -> std::vector<std::string> {
  const std::vector<ObservedFile> scanned_files = ScanFiles();
  if (!initialized_) {
    known_files_ = scanned_files;
    initialized_ = true;
    return {};
  }

  std::vector<std::string> changed_files;
  for (const ObservedFile& current : scanned_files) {
    const auto previous = std::lower_bound(
        known_files_.begin(), known_files_.end(), current, CompareObservedFilePath);
    if (previous == known_files_.end() || previous->workspace_path != current.workspace_path ||
        previous->write_time_ticks != current.write_time_ticks) {
      changed_files.push_back(current.workspace_path);
    }
  }

  known_files_ = scanned_files;
  return TrimRecent(std::move(changed_files), recent_limit_);
}

auto WorkspaceFileWatcher::ScanFiles() const -> std::vector<ObservedFile> {
  std::vector<ObservedFile> files;

  std::error_code error_code;
  if (root_path_.empty() || !std::filesystem::exists(root_path_) ||
      !std::filesystem::is_directory(root_path_)) {
    return files;
  }

  std::filesystem::recursive_directory_iterator iterator(
      root_path_, std::filesystem::directory_options::skip_permission_denied, error_code);
  const std::filesystem::recursive_directory_iterator end;
  while (!error_code && iterator != end) {
    const auto& entry = *iterator;
    const std::filesystem::path path = entry.path();
    if (entry.is_directory(error_code)) {
      if (ShouldSkipDirectory(path)) {
        iterator.disable_recursion_pending();
      }
      iterator.increment(error_code);
      continue;
    }

    if (!entry.is_regular_file(error_code)) {
      iterator.increment(error_code);
      continue;
    }

    const auto workspace_path = NormalizeWorkspacePath(path);
    if (!workspace_path.has_value()) {
      error_code.clear();
      iterator.increment(error_code);
      continue;
    }

    const auto write_time = entry.last_write_time(error_code);
    if (error_code) {
      error_code.clear();
      iterator.increment(error_code);
      continue;
    }

    files.push_back(ObservedFile{
        .workspace_path = *workspace_path,
        .write_time_ticks = FileTimeToTicks(write_time),
    });

    iterator.increment(error_code);
  }

  std::sort(files.begin(), files.end(), CompareObservedFilePath);
  return files;
}

auto WorkspaceFileWatcher::ShouldSkipDirectory(const std::filesystem::path& path) const -> bool {
  if (!root_path_.empty()) {
    std::error_code error_code;
    if (std::filesystem::equivalent(path, root_path_, error_code) && !error_code) {
      return false;
    }
  }

  const std::string name = path.filename().string();
  return name == ".git" || name == "node_modules" || name == ".venv" || name == "venv" ||
         name == "__pycache__" || name == "build" || name == "dist" || name == "out" ||
         name == "target" || name == ".next" || name == ".turbo" || name == ".cache";
}

auto WorkspaceFileWatcher::NormalizeWorkspacePath(const std::filesystem::path& path) const
    -> std::optional<std::string> {
  if (root_path_.empty()) {
    return std::nullopt;
  }

  std::error_code error_code;
  const std::filesystem::path relative = std::filesystem::relative(path, root_path_, error_code);
  if (error_code) {
    return std::nullopt;
  }
  return relative.generic_string();
}

#ifdef __linux__
void WorkspaceFileWatcher::InitializeNativeWatcher() {
  if (root_path_.empty() || !std::filesystem::exists(root_path_) ||
      !std::filesystem::is_directory(root_path_)) {
    return;
  }

  inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (inotify_fd_ < 0) {
    inotify_fd_ = -1;
    return;
  }

  AddWatchRecursive(root_path_);
}

void WorkspaceFileWatcher::AddWatchRecursive(const std::filesystem::path& directory) {
  std::error_code error_code;
  if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
    return;
  }

  AddSingleWatch(directory);

  std::filesystem::recursive_directory_iterator iterator(
      directory, std::filesystem::directory_options::skip_permission_denied, error_code);
  const std::filesystem::recursive_directory_iterator end;
  while (!error_code && iterator != end) {
    const auto& entry = *iterator;
    if (entry.is_directory(error_code)) {
      const auto& path = entry.path();
      if (ShouldSkipDirectory(path)) {
        iterator.disable_recursion_pending();
      } else {
        AddSingleWatch(path);
      }
    }
    iterator.increment(error_code);
  }
}

void WorkspaceFileWatcher::AddSingleWatch(const std::filesystem::path& directory) {
  if (inotify_fd_ < 0 || ShouldSkipDirectory(directory)) {
    return;
  }

  for (const auto& [existing_wd, existing_path] : watch_roots_) {
    if (existing_path == directory) {
      return;
    }
  }

  constexpr std::uint32_t mask = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM |
                                 IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF |
                                 IN_IGNORED;
  const int watch_descriptor = inotify_add_watch(inotify_fd_, directory.c_str(), mask);
  if (watch_descriptor >= 0) {
    watch_roots_[watch_descriptor] = directory;
  }
}

auto WorkspaceFileWatcher::PollChangedFilesNative() -> std::vector<std::string> {
  std::vector<std::string> changed_files;
  if (inotify_fd_ < 0) {
    return changed_files;
  }

  std::array<char, 64 * 1024> buffer{};
  while (true) {
    const ssize_t bytes_read = read(inotify_fd_, buffer.data(), buffer.size());
    if (bytes_read <= 0) {
      if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }
      break;
    }

    std::size_t offset = 0;
    while (offset + sizeof(inotify_event) <= static_cast<std::size_t>(bytes_read)) {
      const auto* event =
          reinterpret_cast<const inotify_event*>(buffer.data() + offset);
      offset += sizeof(inotify_event) + static_cast<std::size_t>(event->len);

      const auto watch_it = watch_roots_.find(event->wd);
      if (watch_it == watch_roots_.end()) {
        continue;
      }

      const std::filesystem::path base_path = watch_it->second;
      const std::string event_name =
          event->len > 0 ? std::string(event->name) : std::string();
      const std::filesystem::path event_path =
          event_name.empty() ? base_path : (base_path / event_name);

      if ((event->mask & IN_IGNORED) != 0U) {
        watch_roots_.erase(watch_it);
        continue;
      }

      if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0U && (event->mask & IN_ISDIR) != 0U) {
        AddWatchRecursive(event_path);
        continue;
      }

      if ((event->mask & IN_ISDIR) != 0U) {
        continue;
      }

      const auto workspace_path = NormalizeWorkspacePath(event_path);
      if (!workspace_path.has_value()) {
        continue;
      }
      changed_files.push_back(*workspace_path);
    }
  }

  return TrimRecent(std::move(changed_files), recent_limit_);
}
#endif

}  // namespace vibe::service
