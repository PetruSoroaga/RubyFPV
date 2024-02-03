#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem.h"

void vehicle_launch_tx_telemetry(Model* pModel);
void vehicle_stop_tx_telemetry();

void vehicle_launch_rx_rc(Model* pModel);
void vehicle_stop_rx_rc();

void vehicle_launch_rx_commands(Model* pModel);
void vehicle_stop_rx_commands();

void vehicle_launch_tx_router(Model* pModel);
void vehicle_stop_tx_router();

bool vehicle_launch_video_capture(Model* pModel, shared_mem_video_link_overwrites* pVideoOverwrites);
void vehicle_stop_video_capture(Model* pModel);
void vehicle_update_camera_params(Model* pModel, int iCameraIndex);


void vehicle_launch_audio_capture(Model* pModel);
void vehicle_stop_audio_capture(Model* pModel);

void vehicle_check_update_processes_affinities(bool bUseThread, bool bVeYe);