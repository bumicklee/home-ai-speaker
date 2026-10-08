#include "saver_text.h"

#include <ctype.h>
#include <string.h>

static bool is_ascii_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* 의미 없다고 치는 ASCII 구두점(공백 포함). 한글 자모·글자는 모두 "의미 있음"으로 센다 */
static bool is_ascii_filler_char(unsigned char c)
{
    return is_ascii_space(c) || strchr(".,!?~'\"-", (char)c) != NULL;
}

static const char *trim(const char *s, size_t *len)
{
    size_t n = strlen(s);
    size_t start = 0;
    while (start < n && is_ascii_space((unsigned char)s[start])) {
        start++;
    }
    size_t end = n;
    while (end > start && is_ascii_space((unsigned char)s[end - 1])) {
        end--;
    }
    *len = end - start;
    return s + start;
}

/* 잡음성 문구 (docs/design-voice-saver-mode.md 11.6). 끝의 ASCII 구두점·앞뒤 공백을 떼고, ASCII만 소문자로
 * 바꿔 비교한다(한글은 대소문자가 없다). 256바이트보다 길면 애초에 이런 짧은 문구가 아니라고 본다 */
static const char *const FILLERS[] = {
    "감사합니다",
    "시청해 주셔서 감사합니다",
    "thank you",
};
#define FILLER_COUNT (sizeof(FILLERS) / sizeof(FILLERS[0]))

static bool matches_filler(const char *text, size_t len)
{
    if (len == 0 || len >= 256) {
        return false;
    }
    char buf[256];
    memcpy(buf, text, len);
    buf[len] = '\0';
    /* 끝의 ASCII 구두점(마침표 등) 떼기 */
    while (len > 0 && strchr(".,!?~ \t", buf[len - 1])) {
        buf[--len] = '\0';
    }
    /* ASCII만 소문자로 */
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)buf[i] < 0x80) {
            buf[i] = (char)tolower((unsigned char)buf[i]);
        }
    }
    for (size_t i = 0; i < FILLER_COUNT; i++) {
        if (!strcmp(buf, FILLERS[i])) {
            return true;
        }
    }
    return false;
}

bool saver_text_is_noise(const char *text)
{
    if (!text) {
        return true;
    }
    size_t len;
    const char *trimmed = trim(text, &len);
    if (len == 0) {
        return true;
    }
    if (matches_filler(trimmed, len)) {
        return true;
    }
    /* 의미 있는 글자(공백·ASCII 구두점이 아닌 UTF-8 코드포인트) 수를 센다. UTF-8 이어지는 바이트
     * (10xxxxxx)는 건너뛰어 코드포인트 하나로 센다 */
    unsigned meaningful = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)trimmed[i];
        if ((c & 0xC0) == 0x80) {
            continue;   /* UTF-8 이어지는 바이트: 앞 코드포인트의 일부 */
        }
        if (c < 0x80 && is_ascii_filler_char(c)) {
            continue;
        }
        meaningful++;
    }
    return meaningful <= 2;
}

void saver_text_split_sentences(const char *text, saver_sentence_cb_t cb, void *ctx)
{
    if (!text || !cb) {
        return;
    }
    size_t start = 0, n = strlen(text);
    for (size_t i = 0; i <= n; i++) {
        bool boundary = i == n || text[i] == '.' || text[i] == '!' || text[i] == '?' || text[i] == '\n';
        if (!boundary) {
            continue;
        }
        size_t end = i < n ? i + 1 : i;   /* 마침표류는 문장에 포함 */
        /* 앞뒤 공백을 건너뛴다 */
        size_t s = start;
        while (s < end && is_ascii_space((unsigned char)text[s])) {
            s++;
        }
        size_t e = end;
        while (e > s && is_ascii_space((unsigned char)text[e - 1])) {
            e--;
        }
        if (e > s) {
            cb(text + s, e - s, ctx);
        }
        start = end;
    }
}

/* ── 셀프테스트 ───────────────────────────────────────────── */

typedef struct {
    unsigned count;
    size_t lens[8];
    char first[64];
} split_capture_t;

static void capture_cb(const char *s, size_t len, void *ctx)
{
    split_capture_t *c = ctx;
    if (c->count < 8) {
        c->lens[c->count] = len;
    }
    if (c->count == 0) {
        size_t n = len < sizeof(c->first) - 1 ? len : sizeof(c->first) - 1;
        memcpy(c->first, s, n);
        c->first[n] = '\0';
    }
    c->count++;
}

bool saver_text_selftest(void)
{
    bool ok = true;

    ok &= saver_text_is_noise("");
    ok &= saver_text_is_noise("   ");
    ok &= saver_text_is_noise("네");          /* 1글자 */
    ok &= saver_text_is_noise("응응");        /* 2글자 */
    ok &= saver_text_is_noise("감사합니다");
    ok &= saver_text_is_noise("감사합니다.");
    ok &= saver_text_is_noise("  Thank you.  ");
    ok &= saver_text_is_noise("시청해 주셔서 감사합니다");
    ok &= !saver_text_is_noise("내일 날씨 어때?");      /* 의미 있는 질문 */
    ok &= !saver_text_is_noise("고마워요");             /* 3글자, 목록에 없음 */
    ok &= saver_text_is_noise(NULL);                     /* NULL은 잡음 취급 */

    split_capture_t c = {0};
    saver_text_split_sentences("오늘은 맑아요. 내일은 비가 와요! 우산 챙기세요?", capture_cb, &c);
    ok &= c.count == 3;
    ok &= !strcmp(c.first, "오늘은 맑아요.");

    split_capture_t c2 = {0};
    saver_text_split_sentences("  ", capture_cb, &c2);
    ok &= c2.count == 0;      /* 빈 조각은 건너뛴다 */

    split_capture_t c3 = {0};
    saver_text_split_sentences("한 줄에 끝 문장 (마침표 없음)", capture_cb, &c3);
    ok &= c3.count == 1;      /* 끝에 마침표가 없어도 마지막 조각은 나온다 */

    return ok;
}
