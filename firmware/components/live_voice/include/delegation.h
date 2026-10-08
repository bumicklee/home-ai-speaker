/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file delegation.h
 * @brief 위임(Responses) 이벤트에서 완성된 function_call 모으기 (기존 펌웨어 delegation.c)
 *
 * 응답 수신 태스크만 쓴다. 리셋은 그 태스크가 멈춘 뒤에만 한다.
 */
#pragma once

#include "cJSON.h"

void     delegation_reset(void);
/** 완성된 function_call 항목 배열(호출한 쪽 소유) 또는 NULL */
cJSON   *delegation_event(const cJSON *envelope);
unsigned delegation_selftest(void);     /* 통과하면 0x1F */
