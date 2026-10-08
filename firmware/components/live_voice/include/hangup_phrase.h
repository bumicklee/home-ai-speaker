/**
 * @file hangup_phrase.h
 * @brief 종료 문구 매처 ("Goodbye Computer"·"잘가 컴퓨터", "Goodbye Alexa"·"잘가 알렉사", "Goodbye Jarvis"·"잘가 자비스", "대화 종료")
 *
 * 입력 전사(transcript) 조각을 이어 붙여, 마지막 단어가 0.6초 동안 더 오지 않을 때
 * 종료 문구가 단어 경계에 맞게 들어 있는지 확인한다. 한글은 UTF-8 바이트를 그대로 둔다.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char text[128];
    unsigned used;
    uint32_t last_ms;
} hangup_phrase_t;

void hangup_phrase_reset(hangup_phrase_t *p);
void hangup_phrase_feed(hangup_phrase_t *p, const char *delta, uint32_t now_ms);
bool hangup_phrase_ready(const hangup_phrase_t *p, uint32_t now_ms);
bool hangup_phrase_selftest(void);
