#pragma once

void onModelAdded(u32 uModelId);
void onModelDeleted(u32 uModelId);

void onNewVehicleActive();

void onEventReboot();
void onEventBeforePairing();
void onEventPaired();
void onEventPairingStopped();
void onEventPairingStartReceivingData();

void onEventArmed();
void onEventDisarmed();

bool onEventReceivedModelSettings(u8* pBuffer, int length, bool bUnsolicited);
