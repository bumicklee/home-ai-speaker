/**
 * @file saver_text.h
 * @brief 절약 모드 받아쓰기 잡음 거르기 · 문장 나누기 (순수 로직, docs/design-voice-saver-mode.md 11.6/5.2)
 *
 * 둘 다 메모리를 새로 만들지 않는다(문자열만 읽거나, 조각 경계를 콜백으로 알려 준다) → 호스트에서
 * cJSON 없이도 바로 컴파일·시험할 수 있다.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 받아쓰기 결과가 "의미 없음"인가: 비어 있거나, 의미 있는 글자(공백·구두점 제외) 2자 이하이거나,
 * (다듬고 나면) 잡음성 문구 그대로인 경우. true면 버리고 계속 듣는다(요금·엉뚱한 대답 방지).
 */
bool saver_text_is_noise(const char *text);

typedef void (*saver_sentence_cb_t)(const char *sentence, size_t len, void *ctx);

/**
 * text를 문장 단위(". ", "!", "?", 줄바꿈 뒤)로 나눠 cb를 부른다. 문장 안의 선행 공백은 건너뛰고,
 * 비어 있는 조각은 건너뛴다. 각 sentence는 text 안을 가리키며(복사 없음) NUL로 끝나지 않으니 len을 쓴다.
 */
void saver_text_split_sentences(const char *text, saver_sentence_cb_t cb, void *ctx);

bool saver_text_selftest(void);

#ifdef __cplusplus
}
#endif
