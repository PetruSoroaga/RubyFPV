#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"


#define TEST_LINK_STATE_NONE 0
#define TEST_LINK_STATE_START 1
#define TEST_LINK_STATE_APPLY_RADIO_PARAMS 2
#define TEST_LINK_STATE_PING_UPLINK 3
#define TEST_LINK_STATE_PING_DOWNLINK 4
#define TEST_LINK_STATE_REVERT_RADIO_PARAMS 8
#define TEST_LINK_STATE_END 10
#define TEST_LINK_STATE_ENDED 11

void test_link_state_get_state_string(int iState, char* szBuffer);
void update_atheros_card_datarate(Model* pModel, int iInterfaceIndex, int iDataRateBPS, shared_mem_process_stats* pProcessStats);
