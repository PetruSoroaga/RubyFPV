#pragma once

// Returns 1 if tx cand start was flagged, 0 for duplicate packet, -1 for error

int relay_rx_process_single_received_packet(int iRadioInterfaceIndex, u8* pPacketData, int iPacketLength);
