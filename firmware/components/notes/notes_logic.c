/* 설명은 notes_logic.h. cJSON조차 쓰지 않는다 (Mac에서 cc로 시험) */
#include "notes_logic.h"

#include <stdio.h>
#include <string.h>

/* ── 달력 ─────────────────────────────────────────────────────────────── */

long notes_days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);                                  /* 0..399 */
    unsigned mp = (unsigned)(m + (m > 2 ? -3 : 9));                            /* Mar=0 .. Feb=11 */
    unsigned doy = (153 * mp + 2) / 5 + (unsigned)d - 1;                       /* 0..365 */
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                     /* 0..146096 */
    return era * 146097L + (long)doe - 719468L;
}

void notes_civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097L);                             /* 0..146096 */
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;      /* 0..399 */
    long yy = (long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);                   /* 0..365 */
    unsigned mp = (5 * doy + 2) / 153;                                         /* 0..11 */
    unsigned dd = doy - (153 * mp + 2) / 5 + 1;                               /* 1..31 */
    unsigned mm = mp + (mp < 10 ? 3 : -9);                                    /* 1..12 */
    *y = (int)(yy + (mm <= 2 ? 1 : 0));
    *m = (int)mm;
    *d = (int)dd;
}

bool notes_is_leap_year(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

bool notes_valid_calendar_date(int y, int m, int d)
{
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1970 || y > 2200 || m < 1 || m > 12 || d < 1) {
        return false;
    }
    int max_d = DAYS[m - 1] + (m == 2 && notes_is_leap_year(y) ? 1 : 0);
    return d <= max_d;
}

int notes_weekday(int y, int m, int d)
{
    long days = notes_days_from_civil(y, m, d);
    long wd = (days % 7 + 11) % 7;   /* 1970-01-01(0일)은 목요일(4) */
    return (int)wd;
}

/* ── start/end 문자열 ─────────────────────────────────────────────────── */

static bool digits_n(const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }
    return true;
}

static int two(const char *s)
{
    return (s[0] - '0') * 10 + (s[1] - '0');
}

bool notes_parse_start(const char *s, int *y, int *m, int *d, int *hh, int *mm, bool *all_day)
{
    if (!s) {
        return false;
    }
    size_t len = strlen(s);
    if (len != 10 && len != 16) {
        return false;
    }
    if (!digits_n(s, 4) || s[4] != '-' || !digits_n(s + 5, 2) || s[7] != '-' || !digits_n(s + 8, 2)) {
        return false;
    }
    int yy = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    int mo = two(s + 5);
    int dd = two(s + 8);
    int h = 0, mi = 0;
    bool day_only = len == 10;
    if (!day_only) {
        if (s[10] != 'T' || !digits_n(s + 11, 2) || s[13] != ':' || !digits_n(s + 14, 2)) {
            return false;
        }
        h = two(s + 11);
        mi = two(s + 14);
        if (h > 23 || mi > 59) {
            return false;
        }
    }
    if (!notes_valid_calendar_date(yy, mo, dd)) {
        return false;
    }
    *y = yy;
    *m = mo;
    *d = dd;
    *hh = h;
    *mm = mi;
    *all_day = day_only;
    return true;
}

int64_t notes_base_minutes(int y, int m, int d, int hh, int mm, bool all_day, int all_day_hh, int all_day_mm)
{
    long days = notes_days_from_civil(y, m, d);
    int h = all_day ? all_day_hh : hh;
    int mi = all_day ? all_day_mm : mm;
    return (int64_t)days * 1440 + h * 60 + mi;
}

/* ── UTF-8 ────────────────────────────────────────────────────────────── */

bool notes_utf8_valid(const char *s, size_t len)
{
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        int n;
        uint32_t cp, min_cp;
        if (c < 0x80) {
            i++;
            continue;
        }
        if ((c & 0xE0) == 0xC0) {
            n = 1;
            cp = c & 0x1F;
            min_cp = 0x80;
        } else if ((c & 0xF0) == 0xE0) {
            n = 2;
            cp = c & 0x0F;
            min_cp = 0x800;
        } else if ((c & 0xF8) == 0xF0) {
            n = 3;
            cp = c & 0x07;
            min_cp = 0x10000;
        } else {
            return false;
        }
        if (i + (size_t)n >= len) {
            return false;
        }
        for (int k = 1; k <= n; k++) {
            unsigned char cc = (unsigned char)s[i + (size_t)k];
            if ((cc & 0xC0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return false;
        }
        i += (size_t)n + 1;
    }
    return true;
}

void notes_utf8_truncate(const char *s, size_t max, char *out)
{
    size_t len = strlen(s);
    size_t cut = len < max ? len : max;
    while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) {
        cut--;
    }
    memcpy(out, s, cut);
    out[cut] = '\0';
}

/* ── 검색 ─────────────────────────────────────────────────────────────── */

static void normalize(const char *in, char *out, size_t cap)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < cap; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }
        out[j++] = (char)c;
    }
    out[j] = '\0';
}

bool notes_search_match(const char *haystack, const char *needle)
{
    if (!needle || !needle[0]) {
        return true;
    }
    char h[896], n[160];
    normalize(haystack ? haystack : "", h, sizeof(h));
    normalize(needle, n, sizeof(n));
    return n[0] != '\0' && strstr(h, n) != NULL;
}

/* ── id·문구 ──────────────────────────────────────────────────────────── */

void notes_id_from_random(uint32_t r, char out[11])
{
    static const char HEX[] = "0123456789abcdef";
    out[0] = 'n';
    out[1] = '_';
    for (int i = 0; i < 8; i++) {
        out[2 + i] = HEX[(r >> ((7 - i) * 4)) & 0xF];
    }
    out[10] = '\0';
}

bool notes_id_looks_valid(const char *id)
{
    if (!id || strlen(id) != 10 || id[0] != 'n' || id[1] != '_') {
        return false;
    }
    for (int i = 2; i < 10; i++) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

void notes_remind_phrase(int minutes, char *out, size_t cap)
{
    if (minutes == 0) {
        snprintf(out, cap, "at the time");
    } else if (minutes == 10) {
        snprintf(out, cap, "10 min before");
    } else if (minutes == 30) {
        snprintf(out, cap, "30 min before");
    } else if (minutes == 60) {
        snprintf(out, cap, "1 hour before");
    } else if (minutes == 120) {
        snprintf(out, cap, "2 hours before");
    } else if (minutes == 1440) {
        snprintf(out, cap, "1 day before");
    } else if (minutes % 1440 == 0) {
        snprintf(out, cap, "%d days before", minutes / 1440);
    } else if (minutes % 60 == 0) {
        snprintf(out, cap, "%d hours before", minutes / 60);
    } else {
        snprintf(out, cap, "%d min before", minutes);
    }
}

/* ── 셀프테스트 ───────────────────────────────────────────────────────── */

bool notes_selftest(void)
{
    bool ok = true;

    /* 달력: 윤년, 월 경계 */
    ok &= notes_valid_calendar_date(2024, 2, 29);          /* 2024는 윤년 */
    ok &= !notes_valid_calendar_date(2025, 2, 29);         /* 2025는 평년 */
    ok &= !notes_valid_calendar_date(2026, 4, 31);         /* 4월은 30일 */
    ok &= notes_valid_calendar_date(2000, 2, 29);          /* 400의 배수: 윤년 */
    ok &= !notes_valid_calendar_date(1900, 2, 29);         /* 100의 배수(400 아님): 평년 */
    ok &= notes_days_from_civil(1970, 1, 1) == 0;
    ok &= notes_days_from_civil(2026, 10, 5) - notes_days_from_civil(2026, 10, 4) == 1;
    {
        int y, m, d;
        notes_civil_from_days(notes_days_from_civil(2026, 10, 5), &y, &m, &d);
        ok &= y == 2026 && m == 10 && d == 5;
    }
    ok &= notes_weekday(2026, 10, 4) == 0;   /* 2026-10-04는 일요일 */
    ok &= notes_weekday(1970, 1, 1) == 4;    /* 목요일 */

    /* start 문자열 파싱 */
    {
        int y, m, d, hh, mm;
        bool all_day;
        ok &= notes_parse_start("2026-10-05", &y, &m, &d, &hh, &mm, &all_day) && all_day && y == 2026 && m == 10 &&
              d == 5 && hh == 0 && mm == 0;
        ok &= notes_parse_start("2026-10-05T15:00", &y, &m, &d, &hh, &mm, &all_day) && !all_day && hh == 15 && mm == 0;
        ok &= !notes_parse_start("2026-10-05T15:00:00", &y, &m, &d, &hh, &mm, &all_day);   /* 초는 지원 안 함 */
        ok &= !notes_parse_start("2026-2-5", &y, &m, &d, &hh, &mm, &all_day);              /* 자릿수 고정 */
        ok &= !notes_parse_start("2026-13-01", &y, &m, &d, &hh, &mm, &all_day);             /* 13월 없음 */
        ok &= !notes_parse_start("2026-04-31", &y, &m, &d, &hh, &mm, &all_day);             /* 4월 31일 없음 */
        ok &= !notes_parse_start("2026-10-05T24:00", &y, &m, &d, &hh, &mm, &all_day);        /* 24시 없음 */
        ok &= !notes_parse_start(NULL, &y, &m, &d, &hh, &mm, &all_day);
    }

    /* 기준 시각: 시각 있는 일정 vs 하루 종일, 1440분(하루 전) 오프셋이 월 경계를 넘는 경우 */
    {
        /* 2026-11-01 09:00 일정, 하루 전(1440분) 알림 == 2026-10-31 09:00 */
        int64_t base = notes_base_minutes(2026, 11, 1, 9, 0, false, 0, 0);
        int64_t due = base - 1440;   /* 분 단위 오프셋 */
        int64_t expect = notes_base_minutes(2026, 10, 31, 9, 0, false, 0, 0);
        ok &= due == expect;

        /* 하루 종일 일정(all_day_time 09:00 기준), 정각(0) == 당일 09:00, 하루 전 == 전날 09:00 */
        int64_t ad_base = notes_base_minutes(2026, 3, 1, 0, 0, true, 9, 0);
        ok &= ad_base == notes_base_minutes(2026, 3, 1, 9, 0, false, 0, 0);
        int64_t ad_due_1d = ad_base - 1440;
        ok &= ad_due_1d == notes_base_minutes(2026, 2, 28, 9, 0, false, 0, 0);   /* 2026은 평년 */
    }

    /* UTF-8 */
    ok &= notes_utf8_valid("hello", 5);
    ok &= notes_utf8_valid("우유 사기", strlen("우유 사기"));
    ok &= !notes_utf8_valid("\xC0\x80", 2);          /* overlong */
    ok &= !notes_utf8_valid("\xE0\x80", 2);          /* 잘림 */
    ok &= !notes_utf8_valid("\xED\xA0\x80", 3);      /* UTF-16 서로게이트 범위 */
    {
        char out[16];
        notes_utf8_truncate("우유 사기 우유 사기", 5, out);   /* "우유" 2글자=6바이트라 5에서 "우유"만 자르면 3바이트(한 글자) */
        /* "우"=3바이트. 5바이트 한도면 "우유"(6바이트)는 안 들어가고 "우"(3바이트)만 */
        ok &= strlen(out) == 3;
        notes_utf8_truncate("hello", 3, out);
        ok &= !strcmp(out, "hel");
        notes_utf8_truncate("hi", 10, out);
        ok &= !strcmp(out, "hi");
    }

    /* 검색 */
    ok &= notes_search_match("병원 (정형외과) 예약", "정형외과");
    ok &= notes_search_match("Milk and Eggs", "milkandeggs");      /* 대소문자·공백 무시 */
    ok &= notes_search_match("우유 사기", "우 유");                  /* 공백 무시(양쪽 다) */
    ok &= !notes_search_match("우유 사기", "계란");
    ok &= notes_search_match("아무거나", "");                       /* 빈 질의는 통과 */

    /* id */
    {
        char id[11];
        notes_id_from_random(0x7f3a9c21, id);
        ok &= !strcmp(id, "n_7f3a9c21");
        ok &= notes_id_looks_valid(id);
        ok &= !notes_id_looks_valid("n_7F3A9C21");   /* 대문자 불가 */
        ok &= !notes_id_looks_valid("x_7f3a9c21");
        ok &= !notes_id_looks_valid("n_7f3a9c2");    /* 길이 */
    }

    return ok;
}
