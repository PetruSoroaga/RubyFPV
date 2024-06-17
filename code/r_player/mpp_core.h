#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../renderer/drm_core.h"
#include <ctype.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <rockchip/rk_mpi.h>


int mpp_init();
int mpp_uninit();
int mpp_start_decoding_thread();
int mpp_feed_data_to_decoder(void* pData, int iLength);
int mpp_mark_end_of_stream();

