/**
 * @file gemini_schema.h
 * @brief OpenAI Responses 형식 함수 도구 → Gemini functionDeclarations 변환 (docs/design-voice-saver-mode.md stage 2)
 *
 * assistant.c의 build_saver_tools()는 실시간·OpenAI 절약 모드와 같은 OpenAI Responses 형식
 * ({"type":"function","name":...,"strict":true,"parameters":{...,"type":["string","null"], ...}})을 돌려준다.
 * Gemini generateContent의 tools[].functionDeclarations는 OpenAPI 부분집합 Schema를 쓰고 몇 가지가 다르다
 * (docs/design-voice-saver-mode.md 이전 "Gemini 도구 리뷰"에서 이미 정리됨, 2026-10 ai.google.dev 확인):
 *   - type은 배열이 아니라 문자열 하나. ["string","null"] 같은 nullable union은 Gemini가 거부한다
 *     (FunctionDeclaration.parameters는 OpenAPI 기반이라 type이 스칼라 enum) → type은 대문자 하나로,
 *     "null"이 섞여 있으면 nullable:true를 더하고 그 이름을 required에서 뺀다.
 *   - "strict"·"additionalProperties"는 Gemini Schema에 없는 필드라 버린다.
 *   - enum은 문자열에만 쓸 수 있다(정수 enum을 그대로 보내면 거부당한다). 정수 enum은 type INTEGER +
 *     minimum/maximum으로 바꾸고, 정확한 허용값은 description 끝에 적어 모델이 범위 안에서도 그 값만
 *     고르도록 안내한다(강제는 아니다).
 *   - maxItems/minItems 등은 Gemini의 Schema가 protobuf int64 필드(json:",string")라 문자열로 보내야
 *     모든 구현에서 안전하다(둘 다 받아 주는 경우가 많지만 문자열이 공식 표현).
 *   - 인자가 없는 함수(properties 비어 있음)는 "parameters" 자체를 뺀다.
 * 이 파일은 cJSON만 쓴다(tool_guard.c처럼 Mac에서 바로 컴파일·시험 가능, ESP-IDF 의존 없음).
 */
#pragma once

#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * OpenAI Responses 도구 배열(assistant_build_saver_tools()가 돌려주는 것, web_search 같은 비함수 항목은
 * 건너뛴다)을 Gemini functionDeclarations 배열로 바꾼다. 새로 만든 cJSON 배열(호출한 쪽이 cJSON_Delete),
 * 입력은 바꾸지 않는다. openai_tools가 NULL이거나 배열이 아니면 빈 배열.
 */
cJSON *gemini_schema_convert_tools(const cJSON *openai_tools);

/**
 * 스키마 노드 하나(함수의 parameters, 또는 그 안의 속성·items)를 바꾼다. convert_tools가 재귀적으로 쓰며,
 * 셀프테스트도 바로 쓴다. 새로 만든 cJSON(호출한 쪽이 cJSON_Delete).
 */
cJSON *gemini_schema_convert_node(const cJSON *openai_node);

bool gemini_schema_selftest(void);

#ifdef __cplusplus
}
#endif
