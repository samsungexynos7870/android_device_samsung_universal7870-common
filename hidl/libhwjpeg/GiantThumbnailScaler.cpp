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
#include <linux/videodev2.h>

#include <system/graphics.h>
#include <log/log.h>

#include <exynos_format.h>

#include "GiantThumbnailScaler.h"

const static unsigned int v4l2_to_hal_format_table[][2] = {
    {V4L2_PIX_FMT_YUYV, HAL_PIXEL_FORMAT_YCBCR_422_I},
    // we treat both of nv12 and nv21 as nv21 because we have no HAL format for nv12 single plain
    {V4L2_PIX_FMT_NV12, HAL_PIXEL_FORMAT_YCRCB_420_SP},
    {V4L2_PIX_FMT_NV21, HAL_PIXEL_FORMAT_YCRCB_420_SP},
    {V4L2_PIX_FMT_NV12M, HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M},
    {V4L2_PIX_FMT_NV21M, HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M},
};

static unsigned int getHalFormat(unsigned int v4l2fmt)
{
    for (auto &ent: v4l2_to_hal_format_table)
        if (v4l2fmt == ent[0])
            return ent[1];

    char fourcc[5];
    unsigned int fmt = v4l2fmt;
    unsigned int i = 4;
    while (i-- > 0) {
        fourcc[i] = fmt & 0xFF;
        fmt >>= 8;
    }
    fourcc[4] = '\0';

    ALOGE("V4L2 format %#x (%s) is not found", v4l2fmt, fourcc);
    return ~0;
}

bool GiantThumbnailScaler::SetSrcImage(unsigned int width, unsigned int height, unsigned int v4l2_format)
{
    return mScaler.setSrc(width, height, getHalFormat(v4l2_format));
}

bool GiantThumbnailScaler::SetDstImage(unsigned int width, unsigned int height, unsigned int v4l2_format)
{
    return mScaler.setDst(width, height, getHalFormat(v4l2_format));
}

bool GiantThumbnailScaler::RunStream(int srcBuf[SCALER_MAX_PLANES], int __unused srcLen[SCALER_MAX_PLANES],
                                     int dstBuf, size_t __unused dstLen)
{
    int dst[3]{dstBuf, 0, 0};
    return mScaler.run(srcBuf, dst);
}

bool GiantThumbnailScaler::RunStream(char __unused *srcBuf[SCALER_MAX_PLANES], int __unused srcLen[SCALER_MAX_PLANES],
                                     int __unused dstBuf, size_t __unused dstLen)
{
    ALOGE("GiantMSCL does not support userptr buffers");
    return false;
}
