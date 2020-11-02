/*
  * This file is part of Moonlight Embedded.
  *
  * Copyright (C) 2015-2017 Iwan Timmer
  * Copyright (C) 2016 OtherCrashOverride, Daniel Mehrwald
  *
  * Moonlight is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 3 of the License, or
  * (at your option) any later version.
  *
  * Moonlight is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
*/

#include <Limelight.h>

#include <sys/utsname.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <codec.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "../logging.h"

#define SYNC_OUTSIDE 0x02
#define UCODE_IP_ONLY_PARAM 0x08
#define DECODER_BUFFER_SIZE 120*1024

static codec_para_t codecParam = { 0 };
static char* frame_buffer;

time_t lastMeasuredTime;
int lastFrameNumber = -1, droppedFrames = 0, totalFrames = 0, nfis = 0, hFPS = -1, lFPS = 1000, avgFPS = -1, decodeTime = -1, avgDec = -1;

int aml_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  codecParam.stream_type = STREAM_TYPE_ES_VIDEO;
  codecParam.has_video = 1;
  codecParam.noblock = 0;
  codecParam.am_sysinfo.param = 0;
  
  switch (videoFormat) {
    case VIDEO_FORMAT_H264:
      if (width > 1920 || height > 1080) {
        codecParam.video_type = VFORMAT_H264_4K2K;
        codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K;
      } else {
        codecParam.video_type = VFORMAT_H264;
        codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
        
        // Workaround for decoding special case of C1, 1080p, H264
        int major, minor;
        struct utsname name;
        uname(&name);
        int ret = sscanf(name.release, "%d.%d", &major, &minor);
        if (!(major > 3 || (major == 3 && minor >= 14)) && width == 1920 && height == 1080)
          codecParam.am_sysinfo.param = (void*) UCODE_IP_ONLY_PARAM;
      }
      break;
    case VIDEO_FORMAT_H265:
      codecParam.video_type = VFORMAT_HEVC;
      codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;
      break;
    default:
      _moonlight_log(ERR, "Video format not supported\n");
      return -1;
  }
  
  frame_buffer = malloc(DECODER_BUFFER_SIZE);
  if (frame_buffer == NULL) {
    _moonlight_log(ERR, "Not enough memory to initialize frame buffer\n");
    return -2;
  }
  
  codecParam.am_sysinfo.width = width;
  codecParam.am_sysinfo.height = height;
  codecParam.am_sysinfo.rate = 96000 / redrawRate;
  codecParam.am_sysinfo.param = (void*) ((size_t) codecParam.am_sysinfo.param | SYNC_OUTSIDE);
  
  int ret;
  if ((ret = codec_init(&codecParam)) != 0) {
    _moonlight_log(ERR, "codec_init error: %x\n", ret);
    return -2;
  }
  
  if ((ret = codec_set_freerun_mode(&codecParam, 1)) != 0) {
    _moonlight_log(ERR, "Can't set Freerun mode: %x\n", ret);
    return -2;
  }
  
  return 0;
}

void aml_cleanup() {
  codec_close(&codecParam);
  free(frame_buffer);

  // HACK: Write amlogic decoder stats here.

  FILE* stats = fopen("aml_decoder.stats", "w");
  int stream_status = 1;
  if (lFPS == 1000 || avgFPS == -1) {
    lFPS = -1;
    stream_status = -1;
  }
  fprintf(stats, "StreamStatus = %i\n", stream_status);
  fprintf(stats, "AverageFPS = %i\n", avgFPS);
  fprintf(stats, "LowestFPS = %i\n", lFPS);
  fprintf(stats, "HighestFPS = %i\n", hFPS);
  fprintf(stats, "NetworkDroppedFrames = %i\n", droppedFrames);
  fprintf(stats, "AvgDecodingTime = %d us", avgDec);
  fclose(stats);
}

struct timespec start, lastMeasure, end;
int aml_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  int result = DR_OK, api, length = 0;

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  if (lastMeasure.tv_nsec == 0 || (start.tv_sec - lastMeasure.tv_sec) >= 1) {
    if (nfis > 0) {
      avgFPS = nfis / (start.tv_sec - lastMeasure.tv_sec);
      avgDec = decodeTime / nfis;
      if (nfis < lFPS)
        lFPS = nfis;
      if (nfis > hFPS)
        hFPS = nfis;
    }
    nfis = 0;
    decodeTime = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &lastMeasure);
  }
  
  if (decodeUnit->frameNumber != lastFrameNumber && decodeUnit->frameNumber != lastFrameNumber+1) {
    int framesDropped = decodeUnit->frameNumber - lastFrameNumber - 1;
    droppedFrames += framesDropped;
    _moonlight_log(WARN,"Dropped %d frames!\n", framesDropped);
  }
  lastFrameNumber = decodeUnit->frameNumber;
  totalFrames++;
  nfis++;

  if (decodeUnit->fullLength > 0 && decodeUnit->fullLength < DECODER_BUFFER_SIZE) {
    PLENTRY entry = decodeUnit->bufferList;
    while (entry != NULL) {
      memcpy(frame_buffer+length, entry->data, entry->length);
      length += entry->length;
      entry = entry->next;
    }
    while (1) {
      api = codec_write(&codecParam, frame_buffer, length);
      if (api < 0) {
        if (errno != EAGAIN) {
          _moonlight_log(ERR, "codec_write error: %x %d\n", api, errno);
          droppedFrames += 1;
          codec_reset(&codecParam);
          result = DR_NEED_IDR;
          break;
        } else {
          _moonlight_log(ERR, "EAGAIN triggered, trying again...\n");
          usleep(5000);
          continue;
        }
      }
      break;
    }
  } else {
    _moonlight_log(ERR, "Video decode buffer too small, %i < %i\n", decodeUnit->fullLength, DECODER_BUFFER_SIZE);
    exit(1);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  decodeTime += (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  return result;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_aml = {
  .setup = aml_setup,
  .cleanup = aml_cleanup,
  .submitDecodeUnit = aml_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(8),
};