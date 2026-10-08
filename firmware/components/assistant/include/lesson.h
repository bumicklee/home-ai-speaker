/**
 * @file lesson.h
 * @brief 영어 선생님: 코스를 고르면 바로 스피커와 영어로 프리토킹 (2026-10-03 사용자 요청, 앱 디자인 "영어 선생님")
 *
 * - 수업 세션은 일반 대화와 같은 live_voice 세션이지만, 선생님 지침(코스·설명 말·빠르기·시간)으로 시작한다.
 * - 선생님은 대화하면서 말로 고쳐 준다. 스피커는 대화 전사를 모아 약 45초마다 따로 OpenAI(Responses API)에
 *   보내 "고쳐 준 표현·발음·주제"를 뽑아 앱에 보낸다(WS `lesson`). 수업이 끝나면 전체 정리를 한 번 더 뽑아 저장한다.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LESSON_PRON,    /* 발음 교정 */
    LESSON_BASIC,   /* 기초 영어 회화 */
    LESSON_DAILY,   /* 생활 영어 회화 */
    LESSON_BIZ,     /* 비즈니스 영어 회화 */
    LESSON_ADV,     /* 고급 영어 회화 */
    LESSON_COURSE_COUNT,
} lesson_course_t;

typedef struct {
    lesson_course_t course;
    bool explain_korean;    /* true: 막히면 한국어로 설명, false: 영어로만 */
    bool slow;              /* 천천히 또박또박 */
    uint8_t minutes;        /* 3, 5 (요금 때문에 최대 5분, 2026-10-04) */
} lesson_cfg_t;

esp_err_t lesson_init(void);

/** API 본문·툴 인자 → 설정. 없는 키는 마지막으로 쓴 값. 형식이 틀리면 false */
bool lesson_parse_cfg(const cJSON *j, lesson_cfg_t *out);

/** 앱: 지금 수업 시작. 대화 중이거나 BLE 창이 열려 있으면 ESP_ERR_INVALID_STATE */
esp_err_t lesson_start(const lesson_cfg_t *cfg);
/** 음성("영어 공부하자"): 지금 대화가 끝나면 수업을 시작한다 */
esp_err_t lesson_start_after_session(const lesson_cfg_t *cfg);
/** 수업 끝내기 (정리는 세션이 끝난 뒤 만든다) */
void      lesson_stop(void);
/** 다른 주제로. 수업 중이 아니면 false */
bool      lesson_change_topic(void);
/** 지금(또는 마지막) 수업 상태·정리. 호출한 쪽이 cJSON_Delete */
cJSON    *lesson_json(void);

/** 수업을 기다리거나 진행·정리 중이면 true (자동 읽기가 끼어들지 않게) */
bool      lesson_busy(void);

/* ── assistant가 live_voice 핸들러에서 부른다 ── */
bool  lesson_wants_session(void);
char *lesson_start_event(const char *voice);
void  lesson_on_transcript(bool from_user, const char *delta);
void  lesson_tick(void);
void  lesson_on_session_end(void);

#ifdef __cplusplus
}
#endif
