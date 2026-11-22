#pragma once

#include <android/native_activity.h>

#include <memory>
#include <string>

namespace ab {

class Storage;

namespace platform {

void setAndroidActivity(ANativeActivity* activity);
std::unique_ptr<Storage> makeAndroidStorage();

void androidRequestFolderPick(int kind);
bool androidPollFolderPick(std::string& uri, int& kind);

}  // namespace platform
}  // namespace ab
