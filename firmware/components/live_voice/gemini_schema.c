/* 설명은 gemini_schema.h. cJSON 말고는 아무것도 쓰지 않는다 (Mac에서 바로 시험) */
#include "gemini_schema.h"

#include <stdio.h>
#include <string.h>

/* OpenAI 소문자 타입 → Gemini(OpenAPI 부분집합) 대문자 타입 */
static const char *upper_type(const char *t)
{
    if (!t) {
        return NULL;
    }
    if (!strcmp(t, "string")) return "STRING";
    if (!strcmp(t, "integer")) return "INTEGER";
    if (!strcmp(t, "number")) return "NUMBER";
    if (!strcmp(t, "boolean")) return "BOOLEAN";
    if (!strcmp(t, "object")) return "OBJECT";
    if (!strcmp(t, "array")) return "ARRAY";
    return NULL;    /* 모르는 타입: 가져오지 않는다(Gemini가 거부하는 것보다 안전) */
}

/* {"type":"x"} 또는 {"type":["x","null"]}에서 실제 타입 문자열과 nullable 여부를 뽑는다 */
static void read_type(const cJSON *node, const char **type_out, bool *nullable_out)
{
    *type_out = NULL;
    *nullable_out = false;
    const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(node, "type");
    if (cJSON_IsString(type_item)) {
        *type_out = type_item->valuestring;
    } else if (cJSON_IsArray(type_item)) {
        const cJSON *t;
        cJSON_ArrayForEach(t, type_item) {
            if (!cJSON_IsString(t)) {
                continue;
            }
            if (!strcmp(t->valuestring, "null")) {
                *nullable_out = true;
            } else {
                *type_out = t->valuestring;
            }
        }
    }
}

/* 정수 enum(예: gain_db 0,3,6,...,36)을 min/max + description 안내로 바꾼다 */
static void add_integer_enum_note(cJSON *dst, const cJSON *enum_arr)
{
    int lo = 0, hi = 0;
    bool first = true;
    char list[160] = "";
    size_t list_len = 0;
    const cJSON *v;
    cJSON_ArrayForEach(v, enum_arr) {
        if (!cJSON_IsNumber(v)) {
            continue;
        }
        int iv = v->valueint;
        if (first || iv < lo) {
            lo = iv;
        }
        if (first || iv > hi) {
            hi = iv;
        }
        first = false;
        if (list_len < sizeof(list) - 8) {
            list_len += (size_t)snprintf(list + list_len, sizeof(list) - list_len, "%s%d", list_len ? "," : "", iv);
        }
    }
    cJSON_AddNumberToObject(dst, "minimum", lo);
    cJSON_AddNumberToObject(dst, "maximum", hi);
    cJSON *desc = cJSON_GetObjectItemCaseSensitive(dst, "description");
    char combined[320];
    if (cJSON_IsString(desc) && desc->valuestring[0]) {
        snprintf(combined, sizeof(combined), "%s (allowed values: %s)", desc->valuestring, list);
    } else {
        snprintf(combined, sizeof(combined), "allowed values: %s", list);
    }
    if (desc) {
        cJSON_ReplaceItemInObjectCaseSensitive(dst, "description", cJSON_CreateString(combined));
    } else {
        cJSON_AddStringToObject(dst, "description", combined);
    }
}

/* maxItems/minItems 등 Gemini의 protobuf int64 필드는 문자열로 보내는 쪽이 모든 구현에서 안전하다 */
static void copy_count_field_as_string(const cJSON *src, cJSON *dst, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(src, key);
    if (cJSON_IsNumber(v)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v->valueint);
        cJSON_AddStringToObject(dst, key, buf);
    } else if (cJSON_IsString(v)) {
        cJSON_AddStringToObject(dst, key, v->valuestring);
    }
}

cJSON *gemini_schema_convert_node(const cJSON *src)
{
    if (!cJSON_IsObject(src)) {
        return cJSON_CreateObject();
    }
    cJSON *dst = cJSON_CreateObject();
    if (!dst) {
        return NULL;
    }
    const cJSON *desc = cJSON_GetObjectItemCaseSensitive(src, "description");
    if (cJSON_IsString(desc)) {
        cJSON_AddStringToObject(dst, "description", desc->valuestring);
    }

    const char *type_str = NULL;
    bool nullable = false;
    read_type(src, &type_str, &nullable);
    const char *gemini_type = upper_type(type_str);

    const cJSON *enum_arr = cJSON_GetObjectItemCaseSensitive(src, "enum");
    bool enum_is_integer = cJSON_IsArray(enum_arr) && gemini_type && !strcmp(gemini_type, "INTEGER");
    if (gemini_type) {
        cJSON_AddStringToObject(dst, "type", gemini_type);
    }
    if (nullable) {
        cJSON_AddBoolToObject(dst, "nullable", true);
    }

    if (cJSON_IsArray(enum_arr) && enum_is_integer) {
        add_integer_enum_note(dst, enum_arr);  /* Gemini: enum은 문자열에만. 정수는 범위+안내문으로 */
    } else if (cJSON_IsArray(enum_arr)) {
        cJSON_AddItemToObject(dst, "enum", cJSON_Duplicate(enum_arr, true));  /* 문자열 enum은 그대로 */
    }

    if (gemini_type && !strcmp(gemini_type, "OBJECT")) {
        const cJSON *props = cJSON_GetObjectItemCaseSensitive(src, "properties");
        const cJSON *required = cJSON_GetObjectItemCaseSensitive(src, "required");
        cJSON *out_props = cJSON_AddObjectToObject(dst, "properties");
        cJSON *out_required = cJSON_CreateArray();
        const cJSON *prop;
        cJSON_ArrayForEach(prop, props) {
            cJSON *converted = gemini_schema_convert_node(prop);
            bool prop_nullable = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(converted, "nullable"));
            cJSON_AddItemToObject(out_props, prop->string, converted);
            bool was_required = false;
            const cJSON *req_item;
            cJSON_ArrayForEach(req_item, required) {
                if (cJSON_IsString(req_item) && !strcmp(req_item->valuestring, prop->string)) {
                    was_required = true;
                }
            }
            if (was_required && !prop_nullable) {
                cJSON_AddItemToArray(out_required, cJSON_CreateString(prop->string));
            }
        }
        if (cJSON_GetArraySize(out_required)) {
            cJSON_AddItemToObject(dst, "required", out_required);
        } else {
            cJSON_Delete(out_required);
        }
        /* properties가 비어 있으면(인자 없는 함수) 호출한 쪽(convert_tools)이 "parameters" 자체를 뺀다 */
    } else if (gemini_type && !strcmp(gemini_type, "ARRAY")) {
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(src, "items");
        cJSON *out_items = gemini_schema_convert_node(items);
        cJSON_AddItemToObject(dst, "items", out_items);
        copy_count_field_as_string(src, dst, "maxItems");
        copy_count_field_as_string(src, dst, "minItems");
    }
    return dst;
}

cJSON *gemini_schema_convert_tools(const cJSON *openai_tools)
{
    cJSON *out = cJSON_CreateArray();
    if (!out || !cJSON_IsArray(openai_tools)) {
        return out;
    }
    const cJSON *tool;
    cJSON_ArrayForEach(tool, openai_tools) {
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(tool, "type");
        if (!cJSON_IsString(type) || strcmp(type->valuestring, "function")) {
            continue;   /* web_search 등 비함수 도구는 Gemini로 가져가지 않는다 (2.5절: 검색은 뺀다) */
        }
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(tool, "name");
        if (!cJSON_IsString(name)) {
            continue;
        }
        cJSON *decl = cJSON_CreateObject();
        cJSON_AddStringToObject(decl, "name", name->valuestring);
        const cJSON *desc = cJSON_GetObjectItemCaseSensitive(tool, "description");
        if (cJSON_IsString(desc)) {
            cJSON_AddStringToObject(decl, "description", desc->valuestring);
        }
        const cJSON *params = cJSON_GetObjectItemCaseSensitive(tool, "parameters");
        const cJSON *props = params ? cJSON_GetObjectItemCaseSensitive(params, "properties") : NULL;
        if (props && cJSON_GetArraySize(props) > 0) {
            cJSON *converted = gemini_schema_convert_node(params);
            cJSON_AddItemToObject(decl, "parameters", converted);
        }
        /* properties가 없거나 비어 있으면 "parameters"를 아예 뺀다 (인자 없는 함수) */
        cJSON_AddItemToArray(out, decl);
    }
    return out;
}

/* ── 셀프테스트 ───────────────────────────────────────────── */

static cJSON *find_prop(const cJSON *decl, const char *name)
{
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(decl, "parameters");
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(params, "properties");
    return cJSON_GetObjectItemCaseSensitive(props, name);
}

static bool required_has(const cJSON *decl, const char *name)
{
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(decl, "parameters");
    const cJSON *required = cJSON_GetObjectItemCaseSensitive(params, "required");
    const cJSON *r;
    cJSON_ArrayForEach(r, required) {
        if (cJSON_IsString(r) && !strcmp(r->valuestring, name)) {
            return true;
        }
    }
    return false;
}

bool gemini_schema_selftest(void)
{
    bool ok = true;
    cJSON *tools = cJSON_CreateArray();
    cJSON *ws = cJSON_Parse("{\"type\":\"web_search\"}");
    cJSON_AddItemToArray(tools, ws);
    cJSON *radio = cJSON_Parse(
        "{\"type\":\"function\",\"name\":\"radio_play\",\"description\":\"d\",\"strict\":true,\"parameters\":"
        "{\"type\":\"object\",\"properties\":{\"station\":{\"type\":[\"string\",\"null\"]},\"genre\":{\"type\":"
        "[\"string\",\"null\"]}},\"required\":[\"station\",\"genre\"],\"additionalProperties\":false}}");
    cJSON_AddItemToArray(tools, radio);
    cJSON *gain = cJSON_Parse(
        "{\"type\":\"function\",\"name\":\"set_microphone_gain\",\"description\":\"d\",\"strict\":true,"
        "\"parameters\":{\"type\":\"object\",\"properties\":{\"gain_db\":{\"type\":\"integer\",\"enum\":[0,3,6,9,"
        "12,15,18,21,24,27,30,36]}},\"required\":[\"gain_db\"],\"additionalProperties\":false}}");
    cJSON_AddItemToArray(tools, gain);
    cJSON *notes_add = cJSON_Parse(
        "{\"type\":\"function\",\"name\":\"notes_add\",\"description\":\"d\",\"strict\":true,\"parameters\":"
        "{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":\"string\",\"enum\":[\"memo\",\"event\"]},"
        "\"title\":{\"type\":\"string\"},\"remind\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":"
        "\"integer\"},\"maxItems\":3}},\"required\":[\"kind\",\"title\",\"remind\"],\"additionalProperties\":false}}");
    cJSON_AddItemToArray(tools, notes_add);
    cJSON *noargs = cJSON_Parse(
        "{\"type\":\"function\",\"name\":\"get_now_playing\",\"description\":\"d\",\"strict\":true,\"parameters\":"
        "{\"type\":\"object\",\"properties\":{},\"required\":[],\"additionalProperties\":false}}");
    cJSON_AddItemToArray(tools, noargs);

    cJSON *out = gemini_schema_convert_tools(tools);
    ok &= cJSON_GetArraySize(out) == 4;     /* web_search는 빠진다 */

    cJSON *decl_radio = NULL, *decl_gain = NULL, *decl_notes = NULL, *decl_noargs = NULL;
    const cJSON *d;
    cJSON_ArrayForEach(d, out) {
        const char *n = cJSON_GetObjectItemCaseSensitive(d, "name")->valuestring;
        if (!strcmp(n, "radio_play")) decl_radio = (cJSON *)d;
        if (!strcmp(n, "set_microphone_gain")) decl_gain = (cJSON *)d;
        if (!strcmp(n, "notes_add")) decl_notes = (cJSON *)d;
        if (!strcmp(n, "get_now_playing")) decl_noargs = (cJSON *)d;
    }
    ok &= decl_radio && decl_gain && decl_notes && decl_noargs;

    /* ["string","null"] → type STRING, nullable true, required에서 빠짐 */
    cJSON *station = find_prop(decl_radio, "station");
    ok &= station && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(station, "nullable"));
    ok &= station && !strcmp(cJSON_GetObjectItemCaseSensitive(station, "type")->valuestring, "STRING");
    ok &= !required_has(decl_radio, "station");
    ok &= !cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(decl_radio, "parameters"), "strict");

    /* 정수 enum → INTEGER + minimum/maximum, enum 없음, description에 허용값 */
    cJSON *gain_prop = find_prop(decl_gain, "gain_db");
    ok &= gain_prop && !strcmp(cJSON_GetObjectItemCaseSensitive(gain_prop, "type")->valuestring, "INTEGER");
    ok &= gain_prop && !cJSON_GetObjectItemCaseSensitive(gain_prop, "enum");
    ok &= gain_prop && cJSON_GetObjectItemCaseSensitive(gain_prop, "minimum")->valueint == 0;
    ok &= gain_prop && cJSON_GetObjectItemCaseSensitive(gain_prop, "maximum")->valueint == 36;
    ok &= gain_prop && strstr(cJSON_GetObjectItemCaseSensitive(gain_prop, "description")->valuestring, "36") != NULL;

    /* 문자열 enum은 그대로 */
    cJSON *kind_prop = find_prop(decl_notes, "kind");
    ok &= kind_prop && cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(kind_prop, "enum"));

    /* nullable 배열 + items 변환 + maxItems 문자열 */
    cJSON *remind_prop = find_prop(decl_notes, "remind");
    ok &= remind_prop && !strcmp(cJSON_GetObjectItemCaseSensitive(remind_prop, "type")->valuestring, "ARRAY");
    ok &= remind_prop && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(remind_prop, "nullable"));
    cJSON *remind_items = cJSON_GetObjectItemCaseSensitive(remind_prop, "items");
    ok &= remind_items && !strcmp(cJSON_GetObjectItemCaseSensitive(remind_items, "type")->valuestring, "INTEGER");
    cJSON *max_items = cJSON_GetObjectItemCaseSensitive(remind_prop, "maxItems");
    ok &= max_items && cJSON_IsString(max_items) && !strcmp(max_items->valuestring, "3");
    ok &= !required_has(decl_notes, "remind");     /* nullable은 required에서 빠짐 */
    ok &= required_has(decl_notes, "kind") && required_has(decl_notes, "title");

    /* 인자 없는 함수는 parameters 자체가 없다 */
    ok &= cJSON_GetObjectItemCaseSensitive(decl_noargs, "parameters") == NULL;

    cJSON_Delete(tools);
    cJSON_Delete(out);
    return ok;
}
