/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/*
 * 툴 워커 (설계서 2.8절 "툴 비동기 실행").
 * 기존 펌웨어는 세션 태스크가 툴을 직접 실행해서, 오래 걸리는 툴 동안 마이크 업로드가
 * 멈췄다. 여기서는 응답 해석 태스크가 큐에 넣기만 하고 이 태스크가 실행한 뒤
 * live_voice_submit_tool_outputs()로 결과를 돌려준다. radio_play의 방송 검색이 여기서 HTTPS를 쓴다
 * (TLS 핸드셰이크 + JSON 해석) → 스택 12KB, 실측 후 조정
 */
#include "assistant.h"

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "live_voice.h"

static const char *TAG = "TOOLS";

typedef struct {
    cJSON *calls;
    unsigned generation;
    void (*fn)(void *arg);      /* 일반 작업 (calls 대신) */
    void *arg;
} tool_job_t;

static QueueHandle_t s_jobs;

void assistant_queue_tool_calls(cJSON *calls, unsigned generation)
{
    tool_job_t job = {.calls = calls, .generation = generation};
    if (!s_jobs || xQueueSend(s_jobs, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "tool queue full, dropping %d call(s)", cJSON_GetArraySize(calls));
        cJSON_Delete(calls);
    }
}

bool assistant_queue_work(void (*fn)(void *arg), void *arg)
{
    tool_job_t job = {.fn = fn, .arg = arg};
    return s_jobs && xQueueSend(s_jobs, &job, 0) == pdTRUE;
}

static void tool_worker(void *arg)
{
    for (;;) {
        tool_job_t job;
        if (xQueueReceive(s_jobs, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (job.fn) {
            job.fn(job.arg);
            continue;
        }
        cJSON *outputs = cJSON_CreateArray();
        const cJSON *call;
        cJSON_ArrayForEach(call, job.calls) {
            /* 대화가 끝났거나(종료 문구·버튼·시간 초과) 새 세션이 시작됐으면 남은 툴을 실행하지 않는다.
             * 기존 펌웨어 run_tools()의 stopping 검사와 같다 */
            if (job.generation != live_voice_generation() || !live_voice_ready()) {
                break;
            }
            const cJSON *name = cJSON_GetObjectItem(call, "name");
            const cJSON *args = cJSON_GetObjectItem(call, "arguments");
            const cJSON *id = cJSON_GetObjectItem(call, "call_id");
            if (!cJSON_IsString(name) || !cJSON_IsString(args) || !cJSON_IsString(id)) {
                continue;
            }
            /* 툴 이름만 남긴다 (인자는 사용자 정보일 수 있음) */
            ESP_LOGI(TAG, "run %s", name->valuestring);
            cJSON *result = assistant_execute(name->valuestring, args->valuestring);
            /* 오류 문구는 기기가 만든 고정 문장이라 남겨도 된다 */
            const cJSON *error = cJSON_GetObjectItem(result, "error");
            if (cJSON_IsString(error)) {
                ESP_LOGW(TAG, "%s failed: %s", name->valuestring, error->valuestring);
            } else {
                ESP_LOGI(TAG, "%s ok", name->valuestring);
            }
            char *text = cJSON_PrintUnformatted(result);
            cJSON_Delete(result);
            if (!text) {
                continue;
            }
            cJSON *out = cJSON_CreateObject();
            cJSON_AddStringToObject(out, "call_id", id->valuestring);
            cJSON_AddStringToObject(out, "output", text);
            free(text);
            cJSON_AddItemToArray(outputs, out);
        }
        cJSON_Delete(job.calls);
        ESP_LOGI(TAG, "stack free min %u B", (unsigned)uxTaskGetStackHighWaterMark(NULL));
        if (cJSON_GetArraySize(outputs) > 0 && job.generation == live_voice_generation()) {
            assistant_note_device_turn();
            live_voice_submit_tool_outputs(outputs, job.generation);
        } else {
            cJSON_Delete(outputs);
        }
    }
}

esp_err_t assistant_tool_worker_start(void)
{
    s_jobs = xQueueCreate(6, sizeof(tool_job_t));
    if (!s_jobs) {
        return ESP_ERR_NO_MEM;
    }
    /* 스택은 PSRAM (XIP 덕분에 NVS 사용 가능, 책 7장 7.3절) */
    return xTaskCreatePinnedToCoreWithCaps(tool_worker, "tool_worker", 12288, NULL, 4, NULL, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS ? ESP_OK
                                                                                                 : ESP_ERR_NO_MEM;
}
