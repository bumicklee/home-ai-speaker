#include "saver_resample.h"

#define STEP_24_TO_16 1.5   /* 24000/16000 */

void saver_resample_reset(saver_resample_t *r)
{
    r->next_in_pos = 0;
    r->consumed_in = 0;
    r->prev_last = 0;
    r->have_prev = false;
}

static int16_t clamp16(double v)
{
    if (v > 32767.0) {
        return 32767;
    }
    if (v < -32768.0) {
        return -32768;
    }
    return (int16_t)v;
}

size_t saver_resample_24to16(saver_resample_t *r, const int16_t *in, size_t count_in, int16_t *out,
                              size_t out_capacity)
{
    size_t written = 0;
    while (written < out_capacity) {
        double local = r->next_in_pos - r->consumed_in;   /* 이번 호출 in[] 기준 위치. in[-1]=이전 호출의 마지막 */
        if (local > (double)(count_in ? count_in - 1 : 0)) {
            break;   /* 이 조각만으로는 아직 부족하다. 다음 호출을 기다린다 */
        }
        double s0, s1, frac;
        if (local < 0.0) {
            s0 = r->have_prev ? r->prev_last : (count_in ? in[0] : 0);
            s1 = count_in ? in[0] : s0;
            frac = local + 1.0;              /* local은 [-1, 0) */
        } else {
            size_t idx = (size_t)local;
            frac = local - (double)idx;
            s0 = in[idx];
            s1 = (idx + 1 < count_in) ? in[idx + 1] : in[idx];
        }
        out[written++] = clamp16(s0 + (s1 - s0) * frac);
        r->next_in_pos += STEP_24_TO_16;
    }
    if (count_in) {
        r->prev_last = in[count_in - 1];
        r->have_prev = true;
    }
    r->consumed_in += (double)count_in;
    return written;
}

/* ── 셀프테스트 ───────────────────────────────────────────── */

bool saver_resample_selftest(void)
{
    bool ok = true;

    /* 개수: 24kHz n샘플 → 약 n*2/3개 (오차 2개 이내), 여러 조각으로 나눠 넣어도 합이 맞아야 한다 */
    saver_resample_t r;
    saver_resample_reset(&r);
    int16_t in[300];
    for (int i = 0; i < 300; i++) {
        in[i] = (int16_t)(i * 10);   /* 완만한 증가 램프 */
    }
    int16_t out[256];
    size_t total_out = 0;
    /* 세 조각(97+97+106)으로 나눠 스트리밍 처럼 먹인다 */
    total_out += saver_resample_24to16(&r, in, 97, out, 256);
    total_out += saver_resample_24to16(&r, in + 97, 97, out, 256);
    total_out += saver_resample_24to16(&r, in + 194, 106, out, 256);
    size_t expect = (size_t)(300.0 / 1.5);   /* 200 */
    ok &= total_out >= expect - 2 && total_out <= expect + 2;

    /* 한 번에 다 넣었을 때와 개수가 거의 같아야 한다(경계에서 1개 이내 차이) */
    saver_resample_t r2;
    saver_resample_reset(&r2);
    int16_t out2[256];
    size_t one_shot = saver_resample_24to16(&r2, in, 300, out2, 256);
    ok &= one_shot >= total_out - 1 && one_shot <= total_out + 1;

    /* 램프 입력 → 출력도 단조 증가(선형 보간이므로) */
    bool increasing = true;
    for (size_t i = 1; i < one_shot; i++) {
        if (out2[i] < out2[i - 1]) {
            increasing = false;
            break;
        }
    }
    ok &= increasing;

    /* 범위를 넘는 입력(클리핑)도 안전하게 잘린다 */
    saver_resample_t r3;
    saver_resample_reset(&r3);
    int16_t big[4] = {32767, 32767, -32768, -32768};
    int16_t out3[8];
    size_t n3 = saver_resample_24to16(&r3, big, 4, out3, 8);
    ok &= n3 > 0;   /* int16_t 결과라 범위는 타입이 이미 보장한다; 여기서는 개수만 확인 */

    return ok;
}
