#include "file-system.h"
#include "logging/debug-log.h"
#include <stdio.h>
#include <string.h>

FileWriter FileSystem::open(const char* path) const {
    // A game stays inside its directory, so a path from the root and a step up are refused.
    if ((!root && running == nullptr) || path == nullptr || path[0] == '\0' || path[0] == '/' ||
        strstr(path, "..") != nullptr) {
        DEBUG_LOG("FileSystem::open: %s refused, no game named or a path outside its directory\n",
                  path != nullptr ? path : "(none)");
        return FileWriter();
    }
    char full[FILE_PATH_BYTES];
    const int n = root ? snprintf(full, sizeof full, "/%s", path)
                       : snprintf(full, sizeof full, "/games/%s/%s", running, path);
    if (n < 0 || size_t(n) >= sizeof full) {
        DEBUG_LOG("FileSystem::open: %s is too long inside %s%s/\n", path, root ? "" : "/games/",
                  root ? "" : running);
        return FileWriter();
    }
    return storage.openWriter(full);
}

FileWriter FileSystem::openFull(const char* path) const {
    return storage.openWriter(path);
}
