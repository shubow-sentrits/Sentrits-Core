#ifndef VIBE_SERVICE_WORKSPACE_FILE_WATCHER_H
#define VIBE_SERVICE_WORKSPACE_FILE_WATCHER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __linux__
#include <sys/inotify.h>
#endif

namespace vibe::service {

class WorkspaceFileWatcher {
 public:
  struct ObservedFile {
    std::string workspace_path;
    std::uint64_t write_time_ticks{0};
  };

  explicit WorkspaceFileWatcher(std::string workspace_root,
                                std::size_t recent_limit = 32U);
  ~WorkspaceFileWatcher();

  [[nodiscard]] auto PollChangedFiles() -> std::vector<std::string>;

 private:
  [[nodiscard]] auto PollChangedFilesByScan() -> std::vector<std::string>;
  [[nodiscard]] auto ScanFiles() const -> std::vector<ObservedFile>;
  [[nodiscard]] auto ShouldSkipDirectory(const std::filesystem::path& path) const -> bool;
  [[nodiscard]] auto NormalizeWorkspacePath(const std::filesystem::path& path) const
      -> std::optional<std::string>;

#ifdef __linux__
  void InitializeNativeWatcher();
  void AddWatchRecursive(const std::filesystem::path& directory);
  void AddSingleWatch(const std::filesystem::path& directory);
  [[nodiscard]] auto PollChangedFilesNative() -> std::vector<std::string>;
#endif

  std::string workspace_root_;
  std::filesystem::path root_path_;
  std::size_t recent_limit_{32U};
  bool initialized_{false};
  std::vector<ObservedFile> known_files_;

#ifdef __linux__
  int inotify_fd_{-1};
  std::unordered_map<int, std::filesystem::path> watch_roots_;
#endif
};

}  // namespace vibe::service

#endif
