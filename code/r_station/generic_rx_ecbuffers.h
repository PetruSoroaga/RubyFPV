#pragma once


typedef struct
{
   u8 uPacketData[MAX_PACKET_PAYLOAD];
   u32 uReceivedTime;
   int iFilledBytes;
   bool bEmpty;
   bool bReconstructed;
   bool bOutputed;
}
type_generic_rx_ec_packet;

typedef struct
{
   u32 uBlockIndex;
   bool bEmpty;
   int iMaxReceivedDataPacketIndex;
   int iReceivedDataPackets;
   int iReceivedECPackets;
   type_generic_rx_ec_packet* pPackets[MAX_TOTAL_PACKETS_IN_BLOCK];
}
type_generic_rx_ec_block;

class GenericRxECBuffers
{
   public:
      GenericRxECBuffers();
      virtual ~GenericRxECBuffers();

      void init(int iMaxBlocks, bool bEnableCRC, u32 uDataPackets, u32 uECPackets, int iPacketLength);
      void checkAddPacket(u32 uBlockIndex, u32 uPacketIndex, u8* pData, int iDataLength, u32 uTimeNow);
      u8* getMarkFirstPacketToOutput(int* piLength, u32* puBlockIndex, u32* puBlockPacketIndex, bool bPushIncompleteBlocks);

   protected:
      void _deleteBuffers();
      void _reinitBuffers();
      void _clearBufferBlock(int iBlockIndex);
      void _clearBuffers(const char* szReason);
      void _addPacketToBuffer(u32 uBlockIndex, u32 uPacketIndex, int iBufferIndex, u8* pData, int iDataLength, u32 uTimeNow);
      void _computeECDataOnBlock(int iBufferIndex);
      bool m_bEnableCRC;
      int  m_iMaxBlocks;
      type_generic_rx_ec_block* m_pBlocks;
      u32  m_uBlockDataPackets;
      u32  m_uBlockECPackets;
      int  m_iBlockPacketLength;

      int m_iTopBufferIndex;
      int m_iBottomBufferIndexToOutput;
      int m_iBottomPacketIndexToOutput;

      unsigned int m_ec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
      unsigned int m_ec_decode_ec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
      u8* m_p_ec_decode_data_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
      u8* m_p_ec_decode_ec_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
      unsigned int m_missing_packets_count_for_ec;
};
