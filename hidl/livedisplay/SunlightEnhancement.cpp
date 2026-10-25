/*
 * Copyright (C) 2019-2025 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <unistd.h>

#include "SunlightEnhancement.h"

using android::base::ParseInt;
using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_1 {
namespace implementation {

static constexpr const char* kPanelLuxPath = "/sys/class/lcd/panel/lux";
static constexpr const char* kMdnieLuxPath = "/sys/class/mdnie/mdnie/lux";

static const char* getLuxPath() {
    if (access(kPanelLuxPath, R_OK | W_OK) == 0) {
        return kPanelLuxPath;
    }
    return kMdnieLuxPath;
}

Return<bool> SunlightEnhancement::isEnabled() {
    std::string tmp;
    int32_t contents = 0;

    if (ReadFileToString(getLuxPath(), &tmp)) {
        ParseInt(Trim(tmp), &contents);
    }

    return contents > 0;
}

Return<bool> SunlightEnhancement::setEnabled(bool enabled) {
    /*
     * Writing 40000 to /sys/class/lcd/panel/lux updates lcd->lux (enabling
     * panel HBM where supported) and forwards to mdnie_class ("lux") to
     * engage the outdoor MDNIE HBM table (see get_hbm_index).
     */
    return WriteStringToFile(enabled ? "40000" : "0", getLuxPath(), true);
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
