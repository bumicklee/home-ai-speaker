/**
 * @file gemini_base64.h
 * @brief base64 인코딩·디코딩 (순수 로직, Gemini 요청(소리)·응답(합성 PCM)에 쓴다)
 *
 * cJSON 문자열 값은 자기 자신이 복사를 하므로, 인코딩한 글자는 넣고 나면 바로 지워도 된다. 아무 ESP-IDF
 * 의존이 없다(saver_endpoint.c처럼 Mac에서 바로 컴파일·시험 가능).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** in_len바이트를 인코딩했을 때 필요한 글자 수 (패딩 포함, NUL 제외) */
size_t gemini_base64_encoded_len(size_t in_len);

/**
 * in(in_len바이트)을 표준 base64(RFC 4648, '+'/'/' 알파벳, '=' 패딩)로 out에 쓴다. out_cap은
 * gemini_base64_encoded_len(in_len) 이상이어야 한다(NUL은 호출한 쪽이 따로 붙인다). 반환: 쓴 글자 수
 * (out_cap이 모자라면 0, out에는 아무것도 쓰지 않는다).
 */
size_t gemini_base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap);

/**
 * base64 글자(in_len글자, 줄바꿈·공백은 건너뛴다)를 디코딩해 out에 쓴다. 반환: 쓴 바이트 수, out_cap이
 * 모자라거나 글자가 잘못되면 SIZE_MAX.
 */
size_t gemini_base64_decode(const char *in, size_t in_len, uint8_t *out, size_t out_cap);

bool gemini_base64_selftest(void);

#ifdef __cplusplus
}
#endif
