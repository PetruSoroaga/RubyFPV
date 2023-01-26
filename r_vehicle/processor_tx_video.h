#pragma once


bool process_data_tx_video_init();
bool process_data_tx_video_uninit();

bool process_data_tx_video_command(int iRadioInterface, u8* pPacketBuffer);
bool process_data_tx_video_loop();

u8* process_data_tx_video_get_current_buffer_to_read_pointer();
int process_data_tx_video_get_current_buffer_to_read_size();

bool process_data_tx_video_on_data_read_complete(int countRead);

int process_data_tx_video_has_packets_ready_to_send();
int process_data_tx_video_send_packets_ready_to_send(int howMany);

int process_data_tx_video_has_block_ready_to_send();
int process_data_tx_video_get_pending_blocks_to_send_count();
int process_data_tx_video_send_first_complete_block(bool isLastBlockToSend);

void process_data_tx_video_signal_encoding_changed();
void process_data_tx_video_signal_model_changed();

void process_data_tx_video_pause_tx();
void process_data_tx_video_resume_tx();

u32 process_data_tx_video_get_current_video_bitrate();
u32 process_data_tx_video_get_current_total_bitrate();

u32 process_data_tx_video_get_current_video_bitrate_average();
u32 process_data_tx_video_get_current_total_bitrate_average();

void process_data_tx_video_set_current_keyframe_interval(int iKeyframe);
int process_data_tx_video_get_current_keyframe_interval();

void process_data_tx_video_reset_retransmissions_history_info();

