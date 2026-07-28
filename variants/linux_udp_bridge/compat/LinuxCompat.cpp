#include "Arduino.h"
#include "FS.h"
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>

SerialStream Serial;

static const char* dataDir() {
  const char* env = getenv("LINUX_MESHCORE_DATA_DIR");
  return env && env[0] ? env : "/data";
}

fs::FS LinuxFS(dataDir());

namespace fs {

bool FS::wipeAll() {
  DIR* d = opendir(_base.c_str());
  if (!d) return false;
  bool ok = true;
  struct dirent* entry;
  while ((entry = readdir(d)) != nullptr) {
    if (entry->d_type != DT_REG) continue;  // only regular files, non-recursive
    std::string path = _base + "/" + entry->d_name;
    if (::remove(path.c_str()) != 0) ok = false;
  }
  closedir(d);
  return ok;
}

}  // namespace fs
