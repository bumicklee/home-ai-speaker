/* Based on waveshare-ai-speaker-assistant (MIT, Copyright (c) 2026 KMX415). See THIRD_PARTY_NOTICES.md. */
/*
 * 기존 펌웨어 hangup_phrase.c 이식. 바뀐 점:
 * - 0x80 이상 바이트(UTF-8 한글)를 공백으로 바꾸지 않고 그대로 둔다
 * - 한국어 종료 문구를 추가했다 (설계서 2.8절)
 * - 호출어 기본이 Computer로 바뀌어(2026-10-03) "goodbye computer", "잘가 컴퓨터"도 받는다.
 *   호출어 Jarvis 자리는 Alexa로 바뀌었다(2026-10-04). 비서 이름은 여전히 Jarvis라 자비스 문구도 남긴다
 */
#include "hangup_phrase.h"

#include <string.h>

static const char *const PHRASES[] = {
    "goodbye jarvis",
    "good bye jarvis",
    "잘가 자비스",
    "잘 가 자비스",
    "잘가요 자비스",
    "goodbye alexa",
    "good bye alexa",
    "잘가 알렉사",
    "잘 가 알렉사",
    "잘가요 알렉사",
    "goodbye computer",
    "good bye computer",
    "잘가 컴퓨터",
    "잘 가 컴퓨터",
    "잘가요 컴퓨터",
    "대화 종료",
    "대화종료",
    "대화 정지",     /* 2026-10-04 사용자가 실제로 쓴 말 */
    "대화정지",
    "대화 끝",
    "대화끝",
};

#define SETTLE_MS   600     /* 전사 완료 이벤트가 없으므로 마지막 조각 뒤 기다린다 */
#define GAP_MS      3000    /* 이보다 오래 끊기면 앞 내용을 버린다 */

void hangup_phrase_reset(hangup_phrase_t *p)
{
    memset(p, 0, sizeof(*p));
}

static void drop_first_word(hangup_phrase_t *p)
{
    /* 버퍼가 차면 앞 단어를 통째로 버린다 (잘린 꼬리가 명령이 되지 않게) */
    char *space = strchr(p->text, ' ');
    if (space) {
        unsigned n = (unsigned)(space - p->text) + 1;
        memmove(p->text, p->text + n, p->used - n + 1);
        p->used -= n;
    } else {
        p->used = 1;
        p->text[0] = 'x';
        p->text[1] = '\0';
    }
}

void hangup_phrase_feed(hangup_phrase_t *p, const char *delta, uint32_t now)
{
    if (!delta || !*delta) {
        return;
    }
    if ((uint32_t)(now - p->last_ms) > GAP_MS) {
        hangup_phrase_reset(p);
    }
    p->last_ms = now;
    while (*delta) {
        unsigned char c = (unsigned char)*delta++;
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
        if (c < 0x80 && !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            c = ' ';
        }
        if (c == ' ' && (!p->used || p->text[p->used - 1] == ' ')) {
            continue;
        }
        if (p->used == sizeof(p->text) - 1) {
            drop_first_word(p);
        }
        p->text[p->used++] = (char)c;
        p->text[p->used] = '\0';
    }
}

bool hangup_phrase_ready(const hangup_phrase_t *p, uint32_t now)
{
    if ((uint32_t)(now - p->last_ms) < SETTLE_MS) {
        return false;
    }
    for (unsigned i = 0; i < sizeof(PHRASES) / sizeof(PHRASES[0]); i++) {
        const char *s = p->text;
        size_t n = strlen(PHRASES[i]);
        while ((s = strstr(s, PHRASES[i]))) {
            /* 한국어 문구는 뒤에 "해(줘·요)", "할게", "하자"가 붙어도 인정한다 ("대화 종료해 줘").
             * "하지 마"처럼 반대 뜻은 인정하지 않는다 */
            bool korean = (unsigned char)PHRASES[i][n - 1] >= 0x80;
            const char *tail = s + n;
            bool polite = korean && (!strncmp(tail, "해", strlen("해")) || !strncmp(tail, "할게", strlen("할게")) ||
                                     !strncmp(tail, "하자", strlen("하자")));
            if ((s == p->text || s[-1] == ' ') && (s[n] == '\0' || s[n] == ' ' || polite)) {
                return true;
            }
            s++;
        }
    }
    return false;
}

bool hangup_phrase_selftest(void)
{
    hangup_phrase_t p = {0};
    /* 조각으로 나뉘어 와도 이어 붙인다. 마지막 조각 뒤 0.6초가 지나야 확정 */
    hangup_phrase_feed(&p, "GOOD", 100);
    hangup_phrase_feed(&p, "bye, Jar", 200);
    hangup_phrase_feed(&p, "vis!", 300);
    if (hangup_phrase_ready(&p, 500) || !hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 단어 경계가 맞아야 한다 */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "goodbye Jarvison", 100);
    if (hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 3초 넘게 끊기면 이어 붙이지 않는다 */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "goodbye", 100);
    hangup_phrase_feed(&p, " Jarvis", 4000);
    if (hangup_phrase_ready(&p, 5000)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "Good-bye, JARVIS.", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 문구가 끝에 있지 않아도 단어 경계면 인정 (기존 동작 유지) */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "hello Jarvis goodbye", 100);
    if (hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 대화 정지, 한국어 뒤에 붙는 말 (2026-10-04) */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "대화 정지", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "대화 종료해 줘", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "대화끝.", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 호출어 Alexa (2026-10-04) */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "Goodbye, Alexa.", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "잘가 알렉사", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* 한국어 */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "잘가, ", 100);
    hangup_phrase_feed(&p, "자비스.", 200);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    /* Computer (기본 호출어) */
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "Goodbye, Computer.", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "잘 가 컴퓨터", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "goodbye computers", 100);
    if (hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "이제 대화 종료", 100);
    if (!hangup_phrase_ready(&p, 900)) {
        return false;
    }
    hangup_phrase_reset(&p);
    hangup_phrase_feed(&p, "대화 종료하지 마", 100);
    return !hangup_phrase_ready(&p, 900);
}
