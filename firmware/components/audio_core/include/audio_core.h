/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/**
 * @file audio_core.h
 * @brief I2S 전이중, 에코 제거(AEC), 음성 재생 큐, 알림음 (설계서 2.7절)
 *
 * Phase 1은 실기기에서 검증된 16kHz 구성(기존 펌웨어)으로 동작한다.
 * 48kHz 믹서·리샘플러·음악 덕킹은 Phase 0 실측(Q7) 뒤 Phase 3에서 붙인다.
 *
 * 마이크 경로: ES7210 4채널 → (ref=ch0, mic=ch1) → AEC → 마이크 처리 훅 → 싱크들
 * 재생 경로:   음성 큐 + 알림(부팅음·톤) + 음악(덕킹) → 볼륨 → ES8311
 *
 * 음악 채널은 출력 레이트(AUDIO_SAMPLE_RATE) 모노로 받는다. 레이트 변환은 넣는 쪽(media_radio)이 한다.
 * 출력 레이트를 48kHz로 바꿀 때(Q7) 음악 쪽은 이 상수만 따라간다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_FRAME_SAMPLES 320     /* 20ms */

/** 에코가 제거된 16kHz 모노 마이크 샘플을 받는 함수. 마이크 태스크에서 불리므로 막지 않는다 */
typedef void (*audio_mic_sink_t)(const int16_t *samples, size_t count);

/** AEC 결과(clean)를 제자리에서 고치는 훅 (자동 게인, 보정 측정). raw는 AEC 전 마이크 값 */
typedef void (*audio_mic_processor_t)(int16_t *clean, const int16_t *raw, size_t count);

/**
 * I2S 송수신 → 코덱 → 저장된 볼륨·마이크 게인 → 무음 출력 시작 → 50ms 뒤 앰프 켜기
 * → 마이크 태스크 시작 순서로 초기화한다 (2.19절).
 */
esp_err_t audio_core_init(void);

/** 펌웨어에 들어 있는 startup.mp3를 해독해 재생한다 (백그라운드) */
esp_err_t audio_core_play_startup(void);
/** "AI 스피커가 준비되었습니다" (ready.mp3). 다른 소리가 나오는 중이면 끝날 때까지 기다린다 */
esp_err_t audio_core_play_ready(void);

esp_err_t audio_core_add_mic_sink(audio_mic_sink_t sink);

/** 호출어 입력. AEC(VOIP 모드)는 통화용 처리라 호출어 특징을 바꿀 수 있어, 스피커가 조용하면 원본 마이크를 준다 */
typedef enum {
    AUDIO_WAKE_SRC_AUTO = 0,    /* 기본: 스피커 레퍼런스가 조용하면 원본, 소리가 나면 AEC 출력 */
    AUDIO_WAKE_SRC_AEC,         /* 항상 AEC 출력 (예전 동작) */
    AUDIO_WAKE_SRC_RAW,         /* 항상 원본 마이크 (시험용) */
} audio_wake_src_t;

/** 호출어 전용 싱크 (하나). 위 규칙으로 고른 소리를 받는다 */
void             audio_core_set_wake_sink(audio_mic_sink_t sink);
void             audio_core_set_wake_source(audio_wake_src_t src);
audio_wake_src_t audio_core_get_wake_source(void);
void      audio_core_set_mic_processor(audio_mic_processor_t processor);

/* 음성(대화 응답) 재생 큐. 16kHz 모노 */
bool audio_core_voice_enqueue(const int16_t *samples, size_t count);
void audio_core_voice_clear(void);

/* 음악 채널 (MUSIC). 링버퍼 약 1초 */

/** 음악 샘플을 넣는다. 버퍼가 차면 timeout_ms까지 기다린다(다운로드 속도 조절). 넣은 개수 */
size_t audio_core_music_write(const int16_t *samples, size_t count, uint32_t timeout_ms);
/** 쌓인 음악을 버린다 (정지·방송 변경) */
void   audio_core_music_flush(void);
/** 대화 중 음악 소리 줄이기: 200ms에 걸쳐 duck_db만큼 낮추고, 해제는 500ms (2.5절) */
void   audio_core_music_duck(bool duck);
/** 음악 버퍼에 쌓인 양 (ms) */
uint32_t audio_core_music_buffered_ms(void);
/**
 * 에코 제거(AEC)를 내리거나 다시 올린다 (내부 RAM 약 91KB). BLE 창을 여는 동안 쓴다.
 * 내려 둔 동안은 호출어·대화에 마이크 소리가 가지 않는다. 마이크 태스크가 처리할 때까지 최대 timeout_ms 기다린다.
 */
esp_err_t audio_core_aec_suspend(bool suspend, uint32_t timeout_ms);
bool      audio_core_aec_suspended(void);
/** 재생 중 음악 버퍼가 비어 끊긴 누적 횟수 (진단) */
unsigned audio_core_music_underruns(void);

/** 짧은 준비음 (440Hz, 0.3초) */
void audio_core_tone(void);
/** 메시지 알림음 (두 음 차임, 약 0.35초) */
void audio_core_chime(void);

/** 스피커로 소리가 나가는 중이거나 0.5초 안에 나갔으면 true */
bool     audio_core_is_playing(void);
/** 비서 목소리가 나오는 중 (라디오 음악은 빼고) */
bool     audio_core_voice_playing(void);
/** 소리가 나갈 때마다 증가하는 카운터 (보정 측정이 "안내 음성이 나왔는지" 확인하는 용도) */
unsigned audio_core_output_generation(void);

/** 볼륨 0–80. 코덱에 적용하고 NVS에 저장한다 */
esp_err_t audio_core_set_volume(int volume);
int       audio_core_get_volume(void);
/** 하드웨어 마이크 게인(dB). 적용만 한다 (저장은 호출한 쪽) */
esp_err_t audio_core_set_mic_gain(int db);

#ifdef __cplusplus
}
#endif
