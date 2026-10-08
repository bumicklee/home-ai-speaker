/**
 * @file saver_endpoint.h
 * @brief 절약 모드 VAD 말 끝 판단 (순수 로직, docs/design-voice-saver-mode.md 11.6/5.1)
 *
 * esp-sr의 VAD(esp_vad.h)는 프레임(20ms)마다 "말인가 조용인가"만 돌려준다. 이 파일은 그 결과를 이어 받아
 * "말이 막 시작됐다 / 계속 듣는다 / 0.7초 조용해서 끝났다 / 30초가 차서 끝났다"를 정한다. esp-sr·FreeRTOS에
 * 기대지 않아 호스트에서 바로 컴파일·시험할 수 있다(E절 셀프테스트).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAVER_EP_SILENCE_END_MS   700    /* 0.7초 조용하면 말 끝 (11.6절) */
#define SAVER_EP_MAX_UTTERANCE_MS 30000  /* 말 최대 30초 */

typedef struct {
    bool     in_speech;
    uint32_t speech_started_ms;
    uint32_t last_speech_ms;
} saver_endpoint_t;

typedef enum {
    SAVER_EP_CONTINUE,      /* 계속 (듣는 중이거나, 아직 말이 시작 안 됨) */
    SAVER_EP_END_SILENCE,   /* 0.7초 조용 → 말 끝 */
    SAVER_EP_END_MAXLEN,    /* 30초 차서 끝 (여기까지만 올려보낸다) */
} saver_endpoint_result_t;

void saver_endpoint_reset(saver_endpoint_t *ep);

/**
 * 프레임(보통 20ms) 하나의 VAD 결과를 먹인다.
 * @param started_out  널이 아니면, 이 호출로 말이 "막 시작"됐을 때 true (프리롤을 커밋하는 신호로 쓴다)
 */
saver_endpoint_result_t saver_endpoint_feed(saver_endpoint_t *ep, bool frame_is_speech, uint32_t now_ms,
                                            bool *started_out);

/** 통과하면 true. 실패한 조건을 로그로 남긴다(dev console selftest) */
bool saver_endpoint_selftest(void);

#ifdef __cplusplus
}
#endif
