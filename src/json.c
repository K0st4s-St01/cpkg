#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

static json_val_t *json_val_alloc(json_type_t type) {
    json_val_t *v = calloc(1, sizeof(json_val_t));
    if (!v) return NULL;
    v->type = type;
    return v;
}

void json_free(json_val_t *v) {
    if (!v) return;
    if (v->type == JSON_STR) {
        free(v->str_val);
    } else if (v->type == JSON_ARR) {
        for (size_t i = 0; i < v->arr.len; i++)
            json_free(v->arr.items[i]);
        free(v->arr.items);
    } else if (v->type == JSON_OBJ) {
        for (size_t i = 0; i < v->obj.len; i++) {
            free(v->obj.pairs[i].key);
            json_free(v->obj.pairs[i].val);
        }
        free(v->obj.pairs);
    }
    free(v);
}

static void skip_ws(const char **p) {
    while (**p && (unsigned char)**p <= ' ') (*p)++;
}

static json_val_t *parse_value(const char **p);

static char *parse_string(const char **p) {
    if (**p != '"') return NULL;
    (*p)++;
    size_t cap = 64, len = 0;
    char *s = malloc(cap);
    if (!s) return NULL;
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            char c = 0;
            switch (**p) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    char hex[5] = {0};
                    for (int i = 0; i < 4; i++) { (*p)++; hex[i] = **p; }
                    c = (char)strtol(hex, NULL, 16);
                    break;
                }
                default: c = **p; break;
            }
            if (c) { s[len++] = c; if (len >= cap) { cap *= 2; s = realloc(s, cap); } }
            (*p)++;
        } else {
            s[len++] = **p;
            if (len >= cap) { cap *= 2; s = realloc(s, cap); }
            (*p)++;
        }
    }
    if (**p == '"') (*p)++;
    s[len] = '\0';
    return s;
}

static json_val_t *parse_object(const char **p) {
    json_val_t *obj = json_val_alloc(JSON_OBJ);
    if (!obj) return NULL;
    if (**p == '{') (*p)++;
    skip_ws(p);
    if (**p == '}') { (*p)++; return obj; }
    while (1) {
        skip_ws(p);
        char *key = parse_string(p);
        if (!key) break;
        skip_ws(p);
        if (**p == ':') (*p)++;
        skip_ws(p);
        json_val_t *val = parse_value(p);
        if (!val) { free(key); break; }
        if (obj->obj.len >= obj->obj.cap) {
            obj->obj.cap = obj->obj.cap ? obj->obj.cap * 2 : 8;
            obj->obj.pairs = realloc(obj->obj.pairs, obj->obj.cap * sizeof(json_pair_t));
        }
        obj->obj.pairs[obj->obj.len].key = key;
        obj->obj.pairs[obj->obj.len].val = val;
        obj->obj.len++;
        skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == '}') { (*p)++; break; }
    }
    return obj;
}

static json_val_t *parse_array(const char **p) {
    json_val_t *arr = json_val_alloc(JSON_ARR);
    if (!arr) return NULL;
    if (**p == '[') (*p)++;
    skip_ws(p);
    if (**p == ']') { (*p)++; return arr; }
    while (1) {
        skip_ws(p);
        json_val_t *val = parse_value(p);
        if (!val) break;
        if (arr->arr.len >= arr->arr.cap) {
            arr->arr.cap = arr->arr.cap ? arr->arr.cap * 2 : 8;
            arr->arr.items = realloc(arr->arr.items, arr->arr.cap * sizeof(json_val_t *));
        }
        arr->arr.items[arr->arr.len++] = val;
        skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == ']') { (*p)++; break; }
    }
    return arr;
}

static json_val_t *parse_number(const char **p) {
    char *end;
    double n = strtod(*p, &end);
    if (end == *p) return NULL;
    json_val_t *v = json_val_alloc(JSON_NUM);
    if (!v) return NULL;
    v->num_val = n;
    *p = end;
    return v;
}

static json_val_t *parse_value(const char **p) {
    skip_ws(p);
    if (!**p) return NULL;
    if (**p == '{') return parse_object(p);
    if (**p == '[') return parse_array(p);
    if (**p == '"') {
        json_val_t *v = json_val_alloc(JSON_STR);
        if (!v) return NULL;
        v->str_val = parse_string(p);
        return v;
    }
    if (**p == 't' && strncmp(*p, "true", 4) == 0) {
        json_val_t *v = json_val_alloc(JSON_BOOL);
        if (!v) return NULL;
        v->bool_val = 1; *p += 4; return v;
    }
    if (**p == 'f' && strncmp(*p, "false", 5) == 0) {
        json_val_t *v = json_val_alloc(JSON_BOOL);
        if (!v) return NULL;
        v->bool_val = 0; *p += 5; return v;
    }
    if (**p == 'n' && strncmp(*p, "null", 4) == 0) {
        json_val_t *v = json_val_alloc(JSON_NULL);
        if (!v) return NULL;
        *p += 4; return v;
    }
    if (**p == '-' || (**p >= '0' && **p <= '9')) return parse_number(p);
    return NULL;
}

json_val_t *json_parse(const char *input) {
    if (!input) return NULL;
    const char *p = input;
    json_val_t *v = parse_value(&p);
    return v;
}

json_val_t *json_get(const json_val_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJ) return NULL;
    for (size_t i = 0; i < obj->obj.len; i++) {
        if (strcmp(obj->obj.pairs[i].key, key) == 0)
            return obj->obj.pairs[i].val;
    }
    return NULL;
}

const char *json_get_str(const json_val_t *obj, const char *key) {
    json_val_t *v = json_get(obj, key);
    if (!v || v->type != JSON_STR) return NULL;
    return v->str_val;
}

double json_get_num(const json_val_t *obj, const char *key) {
    json_val_t *v = json_get(obj, key);
    if (!v || v->type != JSON_NUM) return 0;
    return v->num_val;
}

int json_get_bool(const json_val_t *obj, const char *key) {
    json_val_t *v = json_get(obj, key);
    if (!v || v->type != JSON_BOOL) return 0;
    return v->bool_val;
}

json_val_t *json_get_arr(const json_val_t *obj, const char *key, size_t idx) {
    json_val_t *v = json_get(obj, key);
    if (!v || v->type != JSON_ARR || idx >= v->arr.len) return NULL;
    return v->arr.items[idx];
}

size_t json_arr_len(const json_val_t *arr) {
    if (!arr || arr->type != JSON_ARR) return 0;
    return arr->arr.len;
}

const char *json_str(const json_val_t *val) {
    if (!val || val->type != JSON_STR) return NULL;
    return val->str_val;
}

double json_num(const json_val_t *val) {
    if (!val || val->type != JSON_NUM) return 0;
    return val->num_val;
}

int json_bool(const json_val_t *val) {
    if (!val || val->type != JSON_BOOL) return 0;
    return val->bool_val;
}

int json_is_str(const json_val_t *val, const char *s) {
    const char *str = json_str(val);
    return str && strcmp(str, s) == 0;
}

static void json_stringify_internal(const json_val_t *val, char **buf, size_t *len, size_t *cap, int indent) {
    if (!val) { return; }
    switch (val->type) {
        case JSON_NULL: {
            const char *s = "null"; size_t slen = 4;
            while (*len + slen + 1 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            memcpy(*buf + *len, s, slen); *len += slen;
            break;
        }
        case JSON_BOOL: {
            const char *s = val->bool_val ? "true" : "false"; size_t slen = val->bool_val ? 4 : 5;
            while (*len + slen + 1 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            memcpy(*buf + *len, s, slen); *len += slen;
            break;
        }
        case JSON_NUM: {
            char tmp[64];
            if (fmod(val->num_val, 1.0) == 0)
                snprintf(tmp, sizeof(tmp), "%.0f", val->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", val->num_val);
            size_t slen = strlen(tmp);
            while (*len + slen + 1 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            memcpy(*buf + *len, tmp, slen); *len += slen;
            break;
        }
        case JSON_STR: {
            while (*len + strlen(val->str_val) + 3 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '"';
            for (const char *p = val->str_val; *p; p++) {
                switch (*p) {
                    case '"': (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = '"'; break;
                    case '\\': (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = '\\'; break;
                    case '\n': (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = 'n'; break;
                    case '\t': (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = 't'; break;
                    case '\r': (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = 'r'; break;
                    default: (*buf)[(*len)++] = *p; break;
                }
            }
            (*buf)[(*len)++] = '"';
            break;
        }
        case JSON_ARR: {
            while (*len + 2 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '[';
            for (size_t i = 0; i < val->arr.len; i++) {
                if (i > 0) { while (*len + 1 > *cap) { *cap *= 2; *buf = realloc(*buf, *cap); } (*buf)[(*len)++] = ','; }
                json_stringify_internal(val->arr.items[i], buf, len, cap, indent);
            }
            while (*len + 1 > *cap) { *cap *= 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = ']';
            break;
        }
        case JSON_OBJ: {
            while (*len + 2 > *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '{';
            for (size_t i = 0; i < val->obj.len; i++) {
                if (i > 0) { while (*len + 1 > *cap) { *cap *= 2; *buf = realloc(*buf, *cap); } (*buf)[(*len)++] = ','; }
                json_val_t *key_v = json_val_alloc(JSON_STR);
                key_v->str_val = strdup(val->obj.pairs[i].key);
                json_stringify_internal(key_v, buf, len, cap, indent);
                json_free(key_v);
                while (*len + 1 > *cap) { *cap *= 2; *buf = realloc(*buf, *cap); } (*buf)[(*len)++] = ':';
                json_stringify_internal(val->obj.pairs[i].val, buf, len, cap, indent);
            }
            while (*len + 1 > *cap) { *cap *= 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '}';
            break;
        }
    }
    (*buf)[*len] = '\0';
}

char *json_stringify(const json_val_t *val) {
    char *buf = NULL; size_t len = 0, cap = 0;
    json_stringify_internal(val, &buf, &len, &cap, 0);
    if (!buf) { buf = strdup("null"); }
    return buf;
}
