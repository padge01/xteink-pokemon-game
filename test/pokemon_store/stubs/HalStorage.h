#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using oflag_t = uint8_t;
constexpr oflag_t O_RDONLY = 0;
constexpr oflag_t O_WRONLY = 1U << 0U;
constexpr oflag_t O_RDWR = 1U << 1U;
constexpr oflag_t O_CREAT = 1U << 2U;
constexpr oflag_t O_TRUNC = 1U << 3U;

class HalStorage;

class HalFile {
 public:
  HalFile() = default;
  ~HalFile() { close(); }
  HalFile(HalFile&& other) noexcept { moveFrom(other); }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this != &other) {
      close();
      moveFrom(other);
    }
    return *this;
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  uint64_t fileSize64() const;
  bool seek(size_t position);
  int read(void* output, size_t count);
  size_t write(const void* input, size_t count);
  bool sync();
  bool close();
  bool isOpen() const { return open_; }
  explicit operator bool() const { return open_; }

 private:
  friend class HalStorage;
  HalFile(HalStorage* owner, std::string path, std::vector<uint8_t>* bytes, bool writable)
      : owner_(owner), path_(std::move(path)), bytes_(bytes), writable_(writable), open_(true) {}
  void moveFrom(HalFile& other) noexcept {
    owner_ = other.owner_;
    path_ = std::move(other.path_);
    bytes_ = other.bytes_;
    position_ = other.position_;
    writable_ = other.writable_;
    open_ = other.open_;
    other.owner_ = nullptr;
    other.bytes_ = nullptr;
    other.position_ = 0;
    other.writable_ = false;
    other.open_ = false;
  }

  HalStorage* owner_ = nullptr;
  std::string path_{};
  std::vector<uint8_t>* bytes_ = nullptr;
  size_t position_ = 0;
  bool writable_ = false;
  bool open_ = false;
};

using FsFile = HalFile;

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  void clear() {
    files_.clear();
    openCounts_.clear();
    writeLimit_ = SIZE_MAX;
    writtenBytes_ = 0;
    failSync_ = false;
    failWritableOpen_ = false;
    failRemove_ = false;
    failRename_ = false;
    failRead_ = false;
  }

  void setWriteLimit(const size_t limit) {
    writeLimit_ = limit;
    writtenBytes_ = 0;
  }
  void clearWriteLimit() {
    writeLimit_ = SIZE_MAX;
    writtenBytes_ = 0;
  }
  void setFailSync(const bool fail) { failSync_ = fail; }
  void setFailWritableOpen(const bool fail) { failWritableOpen_ = fail; }
  void setFailRemove(const bool fail) { failRemove_ = fail; }
  void setFailRename(const bool fail) { failRename_ = fail; }
  void setFailRead(const bool fail) { failRead_ = fail; }
  void setByte(const char* path, const size_t offset, const uint8_t value) {
    auto found = files_.find(path);
    if (found != files_.end() && offset < found->second.size()) found->second[offset] = value;
  }

  bool ensureDirectoryExists(const char*) { return true; }
  bool exists(const char* path) const { return files_.find(path) != files_.end(); }
  bool remove(const char* path) { return !failRemove_ && files_.erase(path) != 0; }
  bool rename(const char* oldPath, const char* newPath) {
    if (failRename_ || exists(newPath)) return false;
    auto found = files_.find(oldPath);
    if (found == files_.end() || openCounts_[oldPath] != 0) return false;
    files_.emplace(newPath, std::move(found->second));
    files_.erase(found);
    return true;
  }

  HalFile open(const char* path, const oflag_t flags = O_RDONLY) {
    const std::string key(path);
    if (openCounts_[key] != 0) return {};
    const bool writable = (flags & (O_WRONLY | O_RDWR)) != 0;
    if (writable && failWritableOpen_) return {};
    auto found = files_.find(key);
    if (found == files_.end()) {
      if ((flags & O_CREAT) == 0) return {};
      found = files_.try_emplace(key).first;
    }
    if ((flags & O_TRUNC) != 0) found->second.clear();
    ++openCounts_[key];
    return HalFile(this, key, &found->second, writable);
  }

  size_t writableBytes(const size_t requested) {
    if (writtenBytes_ >= writeLimit_) return 0;
    const size_t allowed = std::min(requested, writeLimit_ - writtenBytes_);
    writtenBytes_ += allowed;
    return allowed;
  }
  bool syncAllowed() const { return !failSync_; }
  bool readAllowed() const { return !failRead_; }

  void fileClosed(const std::string& path) {
    auto found = openCounts_.find(path);
    if (found != openCounts_.end() && found->second != 0) --found->second;
  }

 private:
  std::map<std::string, std::vector<uint8_t>> files_{};
  std::map<std::string, size_t> openCounts_{};
  size_t writeLimit_ = SIZE_MAX;
  size_t writtenBytes_ = 0;
  bool failSync_ = false;
  bool failWritableOpen_ = false;
  bool failRemove_ = false;
  bool failRename_ = false;
  bool failRead_ = false;
};

#define Storage HalStorage::getInstance()

inline uint64_t HalFile::fileSize64() const { return open_ ? bytes_->size() : 0; }

inline bool HalFile::seek(const size_t position) {
  if (!open_ || position > bytes_->size()) return false;
  position_ = position;
  return true;
}

inline int HalFile::read(void* output, const size_t count) {
  if (!open_ || !owner_->readAllowed() || position_ > bytes_->size()) return -1;
  const size_t available = bytes_->size() - position_;
  const size_t copied = std::min(count, available);
  if (copied != 0) std::memcpy(output, bytes_->data() + position_, copied);
  position_ += copied;
  return static_cast<int>(copied);
}

inline size_t HalFile::write(const void* input, const size_t count) {
  if (!open_ || !writable_) return 0;
  const size_t allowed = owner_->writableBytes(count);
  if (position_ + allowed > bytes_->size()) bytes_->resize(position_ + allowed);
  if (allowed != 0) std::memcpy(bytes_->data() + position_, input, allowed);
  position_ += allowed;
  return allowed;
}

inline bool HalFile::sync() { return open_ && owner_->syncAllowed(); }

inline bool HalFile::close() {
  if (!open_) return true;
  owner_->fileClosed(path_);
  owner_ = nullptr;
  bytes_ = nullptr;
  position_ = 0;
  writable_ = false;
  open_ = false;
  return true;
}
