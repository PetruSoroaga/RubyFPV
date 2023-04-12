#pragma once

#define MAX_VIDEO_BITRATE_HISTORY_VALUES 40

class ProcessorTxVideo
{
   public:
      ProcessorTxVideo(int iVideoStreamIndex, int iCameraIndex);
      virtual ~ProcessorTxVideo();

      bool init();
      bool uninit();
      
      void periodicLoop();

      u32 getCurrentVideoBitrate();
      u32 getCurrentVideoBitrateAverage();
      u32 getCurrentVideoBitrateAverage500Ms();
      u32 getCurrentVideoBitrateAverage1Sec();
      u32 getCurrentTotalVideoBitrate();
      u32 getCurrentTotalVideoBitrateAverage();
      u32 getCurrentTotalVideoBitrateAverage1Sec();
      u32 getCurrentTotalVideoBitrateAverage500Ms();

      static int m_siInstancesCount;

   protected:
      bool m_bInitialized;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;

      u32 m_uTimeIntervalVideoBitrateCompute;
      u32 m_uHistoryVideoBitrateValues[MAX_VIDEO_BITRATE_HISTORY_VALUES];
      u32 m_uHistoryTotalVideoBitrateValues[MAX_VIDEO_BITRATE_HISTORY_VALUES];
      u32 m_uHistoryVideoBitrateIndex;
      u32 m_uTimeLastHistoryVideoBitrateUpdate;
      u32 m_uVideoBitrateAverageSum;
      u32 m_uTotalVideoBitrateAverageSum;
      u32 m_uVideoBitrateAverage;
      u32 m_uTotalVideoBitrateAverage;
};

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

void process_data_tx_video_set_current_keyframe_interval(int iKeyframe);

void process_data_tx_video_reset_retransmissions_history_info();

