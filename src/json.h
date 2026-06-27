#ifndef CPKG_JSON_H
#define CPKG_JSON_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUM,
    JSON_STR,
    JSON_ARR,
    JSON_OBJ
} json_type_t;

typedef struct json_val json_val_t;
typedef struct { char *key; json_val_t *val; } json_pair_t;

struct json_val {
    json_type_t type;
    union {
        int bool_val;
        double num_val;
        char *str_val;
        struct { json_val_t **items; size_t len; size_t cap; } arr;
        struct { json_pair_t *pairs; size_t len; size_t cap; } obj;
    };
};

json_val_t *json_parse(const char *input);
void        json_free(json_val_t *val);
json_val_t *json_get(const json_val_t *obj, const char *key);
const char *json_get_str(const json_val_t *obj, const char *key);
double      json_get_num(const json_val_t *obj, const char *key);
int         json_get_bool(const json_val_t *obj, const char *key);
json_val_t *json_get_arr(const json_val_t *obj, const char *key, size_t idx);
size_t      json_arr_len(const json_val_t *arr);
const char *json_str(const json_val_t *val);
double      json_num(const json_val_t *val);
int         json_bool(const json_val_t *val);
int         json_is_str(const json_val_t *val, const char *s);
char       *json_stringify(const json_val_t *val);

#endif
