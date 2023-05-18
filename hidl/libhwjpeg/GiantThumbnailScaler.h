/*
 * Copyright (C) 2019 Samsung Electronics Co.,LTD.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __HARDWARE_EXYNOS_GIANT_THUMBNAIL_SCALER_H__
#define __HARDWARE_EXYNOS_GIANT_THUMBNAIL_SCALER_H__

#include <hardware/exynos/giant_mscl.h>

#include "ThumbnailScaler.h"

class GiantThumbnailScaler: public ThumbnailScaler {
public:
    GiantThumbnailScaler() { }
    virtual ~GiantThumbnailScaler() { }

    virtual bool SetSrcImage(unsigned int width, unsigned int height, unsigned int v4l2_format);
    virtual bool SetDstImage(unsigned int width, unsigned int height, unsigned int v4l2_format);

    virtual bool RunStream(int srcBuf[SCALER_MAX_PLANES], int srcLen[SCALER_MAX_PLANES], int dstBuf, size_t dstLen);
    virtual bool RunStream(char *srcBuf[SCALER_MAX_PLANES], int srcLen[SCALER_MAX_PLANES], int dstBuf, size_t dstLen);

    bool available() { return mScaler; }
private:
    GiantMscl mScaler{true};
};

#endif //__HARDWARE_EXYNOS_GIANT_THUMBNAIL_SCALER_H__
