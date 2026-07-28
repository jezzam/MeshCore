#pragma once

// Minimal fs::FS / fs::File compatibility shim for LINUX_PLATFORM, matching
// the ESP32 SPIFFS calling convention (open(path, mode="r", create=false),
// exists/mkdir/remove) that the rest of the codebase already falls through to
// via its "#else" (ESP32-pattern) branches - verified against src/helpers/
// IdentityStore.cpp and examples/simple_repeater/MyMesh.cpp's openAppend(),
// both of which need zero edits because they already match this convention.
// Backed by real POSIX file I/O, rooted at a configurable base directory
// (LINUX_MESHCORE_DATA_DIR env var, default /data) for genuine persistence
// across container restarts via a bind mount.

#include "Stream.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace fs {

class File : public Stream {
public:
  File() : _fd(-1) { }
  File(int fd) : _fd(fd) { }

  operator bool() const { return _fd >= 0; }

  // Multi-byte read - Arduino's File adds this beyond Stream's single-byte read().
  size_t read(uint8_t* buf, size_t len) {
    if (_fd < 0) return 0;
    ssize_t n = ::read(_fd, buf, len);
    return n > 0 ? (size_t)n : 0;
  }

  size_t write(uint8_t c) override { return _fd >= 0 && ::write(_fd, &c, 1) == 1 ? 1 : 0; }
  size_t write(const uint8_t* buf, size_t len) override {
    if (_fd < 0) return 0;
    size_t total = 0;
    while (total < len) {
      ssize_t n = ::write(_fd, buf + total, len - total);
      if (n <= 0) break;
      total += (size_t)n;
    }
    return total;
  }

  int available() override {
    if (_fd < 0) return 0;
    off_t cur = lseek(_fd, 0, SEEK_CUR);
    struct stat st;
    if (cur < 0 || fstat(_fd, &st) != 0) return 0;
    return (int)(st.st_size - cur);
  }

  int read() override {
    if (_fd < 0) return -1;
    uint8_t c;
    return ::read(_fd, &c, 1) == 1 ? c : -1;
  }

  void close() {
    if (_fd >= 0) { ::close(_fd); _fd = -1; }
  }

private:
  int _fd;
};

class FS {
public:
  explicit FS(const char* base) : _base(base) { ::mkdir(_base.c_str(), 0755); }

  bool exists(const char* path) {
    struct stat st;
    return stat(fullPath(path).c_str(), &st) == 0;
  }

  File open(const char* path, const char* mode = "r", bool create = false) {
    std::string fp = fullPath(path);
    int flags;
    if (strcmp(mode, "w") == 0) flags = O_WRONLY | O_TRUNC | (create ? O_CREAT : 0);
    else if (strcmp(mode, "a") == 0) flags = O_WRONLY | O_APPEND | (create ? O_CREAT : 0);
    else flags = O_RDONLY;
    int fd = ::open(fp.c_str(), flags, 0644);
    return File(fd);
  }

  bool mkdir(const char* path) {
    int rc = ::mkdir(fullPath(path).c_str(), 0755);
    return rc == 0 || errno == EEXIST;
  }

  bool remove(const char* path) {
    return ::remove(fullPath(path).c_str()) == 0;
  }

  // Used for LINUX_PLATFORM's formatFileSystem() - wipes all regular files
  // directly under the data directory (non-recursive; this codebase doesn't
  // create subdirectories beyond what IdentityStore::begin() mkdir's).
  bool wipeAll();

  const char* base() const { return _base.c_str(); }

private:
  std::string fullPath(const char* path) const {
    if (path[0] == '/') return _base + path;
    return _base + "/" + path;
  }
  std::string _base;
};

}  // namespace fs

// Real Arduino ESP32 core's FS.h also pulls these into the global namespace
// (`using fs::File;` etc.) - several helper .cpp files in this codebase
// (ClientACL.cpp, CommonCLI.cpp, IdentityStore.cpp, RegionMap.cpp) rely on
// unqualified `File`, matching that convention.
using fs::File;
using fs::FS;

extern fs::FS LinuxFS;
