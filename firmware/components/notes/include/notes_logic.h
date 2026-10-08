/**
 * @file notes_logic.h
 * @brief notes 모듈의 순수 로직 (날짜·UTF-8·검색·id·문구). ESP-IDF에 기대지 않는다 (Mac에서 시험, tool_guard.c와 같은 방식)
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 달력 (Howard Hinnant day-number 공식) ────────────────────────────── */
long notes_days_from_civil(int y, int m, int d);
void notes_civil_from_days(long z, int *y, int *m, int *d);
bool notes_is_leap_year(int y);
/** y-m-d가 그레고리력에 실제 있는 날인지(윤년 2월 포함). y는 1970-2200만 받는다 */
bool notes_valid_calendar_date(int y, int m, int d);
/** 요일 0=일 1=월 … 6=토 */
int notes_weekday(int y, int m, int d);

/* ── start/end 문자열 ─────────────────────────────────────────────────── */
/**
 * "YYYY-MM-DD" 또는 "YYYY-MM-DDTHH:MM"과 실제 날짜·시각인지 검사한다(자릿수까지 정확히).
 * 통과하면 true, y/m/d/hh/mm을 채운다(all_day면 hh=mm=0). 실패하면 값을 바꾸지 않는다.
 */
bool notes_parse_start(const char *s, int *y, int *m, int *d, int *hh, int *mm, bool *all_day);

/** 일정의 기준(절대) 분. 하루 종일이면 all_day_hh:all_day_mm을 쓴다. 뺄셈·비교에만 쓴다 */
int64_t notes_base_minutes(int y, int m, int d, int hh, int mm, bool all_day, int all_day_hh, int all_day_mm);

/* ── UTF-8 ────────────────────────────────────────────────────────────── */
bool notes_utf8_valid(const char *s, size_t len);
/** s를 max바이트 이하로 자르되 글자 중간을 자르지 않는다(out은 max+1바이트 이상 버퍼) */
void notes_utf8_truncate(const char *s, size_t max, char *out);

/* ── 검색 ─────────────────────────────────────────────────────────────── */
/** 대소문자(ASCII)·공백 무시하고 haystack에 needle이 들어 있는지. needle이 비었으면 true */
bool notes_search_match(const char *haystack, const char *needle);

/* ── id·문구 ──────────────────────────────────────────────────────────── */
/** 무작위 32비트 값에서 "n_"+16진 8자 id를 만든다(out은 11바이트 이상) */
void notes_id_from_random(uint32_t r, char out[11]);
bool notes_id_looks_valid(const char *id);
/** 0,10,30,60,120,1440분은 짧은 문구, 그 밖엔 분/시간/일 단위로(모두 영어: 비서 모델이 읽는 데이터) */
void notes_remind_phrase(int minutes, char *out, size_t cap);

/** 날짜·UTF-8·검색·id·기준시각 계산 셀프테스트 (ESP-IDF 없이도 cc로 컴파일·실행됨) */
bool notes_selftest(void);

#ifdef __cplusplus
}
#endif
