#include "saver_endpoint.h"

#include <stddef.h>

void saver_endpoint_reset(saver_endpoint_t *ep)
{
    ep->in_speech = false;
    ep->speech_started_ms = 0;
    ep->last_speech_ms = 0;
}

saver_endpoint_result_t saver_endpoint_feed(saver_endpoint_t *ep, bool speech, uint32_t now_ms, bool *started_out)
{
    if (started_out) {
        *started_out = false;
    }
    if (!ep->in_speech) {
        if (!speech) {
            return SAVER_EP_CONTINUE;
        }
        ep->in_speech = true;
        ep->speech_started_ms = now_ms;
        ep->last_speech_ms = now_ms;
        if (started_out) {
            *started_out = true;
        }
        return SAVER_EP_CONTINUE;
    }
    if (speech) {
        ep->last_speech_ms = now_ms;
    }
    if (now_ms - ep->speech_started_ms >= SAVER_EP_MAX_UTTERANCE_MS) {
        return SAVER_EP_END_MAXLEN;
    }
    if (now_ms - ep->last_speech_ms >= SAVER_EP_SILENCE_END_MS) {
        return SAVER_EP_END_SILENCE;
    }
    return SAVER_EP_CONTINUE;
}

/* ── 셀프테스트 ───────────────────────────────────────────── */

bool saver_endpoint_selftest(void)
{
    bool ok = true;
    saver_endpoint_t ep;
    bool started;

    /* 조용함만 계속되면 시작도 끝도 없다 */
    saver_endpoint_reset(&ep);
    for (uint32_t t = 0; t < 5000; t += 20) {
        ok &= saver_endpoint_feed(&ep, false, t, &started) == SAVER_EP_CONTINUE;
        ok &= !started;
    }
    ok &= !ep.in_speech;

    /* 말이 시작되면 started=true, 그 뒤 계속 말하면 CONTINUE */
    saver_endpoint_reset(&ep);
    ok &= saver_endpoint_feed(&ep, true, 1000, &started) == SAVER_EP_CONTINUE;
    ok &= started;
    ok &= saver_endpoint_feed(&ep, true, 1020, &started) == SAVER_EP_CONTINUE;
    ok &= !started;        /* 두 번째 호출은 "시작"이 아니다 */

    /* 0.69초 조용은 아직 안 끝남, 0.70초째는 끝 */
    saver_endpoint_reset(&ep);
    saver_endpoint_feed(&ep, true, 0, &started);
    ok &= saver_endpoint_feed(&ep, false, 690, NULL) == SAVER_EP_CONTINUE;
    ok &= saver_endpoint_feed(&ep, false, 700, NULL) == SAVER_EP_END_SILENCE;

    /* 중간에 다시 말하면 조용 시간이 리셋된다 */
    saver_endpoint_reset(&ep);
    saver_endpoint_feed(&ep, true, 0, NULL);
    ok &= saver_endpoint_feed(&ep, false, 500, NULL) == SAVER_EP_CONTINUE;
    ok &= saver_endpoint_feed(&ep, true, 690, NULL) == SAVER_EP_CONTINUE;    /* 다시 말함 */
    ok &= saver_endpoint_feed(&ep, false, 1000, NULL) == SAVER_EP_CONTINUE; /* 690+700=1390이라 아직 */
    ok &= saver_endpoint_feed(&ep, false, 1390, NULL) == SAVER_EP_END_SILENCE;

    /* 쉬지 않고 30초를 채우면 MAXLEN (조용 끝보다 먼저 와야 한다) */
    saver_endpoint_reset(&ep);
    saver_endpoint_feed(&ep, true, 0, NULL);
    saver_endpoint_result_t last = SAVER_EP_CONTINUE;
    for (uint32_t t = 20; t <= 30000; t += 20) {
        last = saver_endpoint_feed(&ep, true, t, NULL);
        if (last != SAVER_EP_CONTINUE) {
            break;
        }
    }
    ok &= last == SAVER_EP_END_MAXLEN;

    return ok;
}
