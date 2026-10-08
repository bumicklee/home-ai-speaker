/**
 * @file saver_resample.h
 * @brief 24kHz → 16kHz 간단 선형 보간 리샘플러 (docs/design-voice-saver-mode.md 11.6/5.5)
 *
 * gpt-4o-mini-tts의 pcm 출력(24kHz)을 지금의 음성 재생 큐(16kHz)에 맞춘다. esp_audio_effects 리샘플러
 * 대신 의도적으로 간단한 선형 보간을 쓴다(이식 범위를 줄이려고, 1단계 TODO로 품질이 부족하면 교체).
 * 스트리밍 호출 사이의 위상·이전 샘플을 들고 있어 HTTP 조각이 임의 크기로 와도 이어진다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double  next_in_pos;    /* 다음에 만들 출력 샘플이 필요로 하는, 전체 스트림 기준 입력 위치(샘플 단위) */
    double  consumed_in;    /* 지금까지 이 스트림에서 소비한 입력 샘플 수 (= 이번 호출 in[0]의 절대 위치) */
    int16_t prev_last;      /* 이전 호출의 마지막 입력 샘플 (경계 보간용) */
    bool    have_prev;
} saver_resample_t;

void saver_resample_reset(saver_resample_t *r);

/**
 * in(count_in개, 24kHz)을 out에 16kHz로 채운다(최대 out_capacity개). 돌려준 개수만큼 out이 채워졌다.
 * 여러 번 나눠 불러도(같은 r로) 이어진다.
 */
size_t saver_resample_24to16(saver_resample_t *r, const int16_t *in, size_t count_in, int16_t *out,
                              size_t out_capacity);

bool saver_resample_selftest(void);

#ifdef __cplusplus
}
#endif
