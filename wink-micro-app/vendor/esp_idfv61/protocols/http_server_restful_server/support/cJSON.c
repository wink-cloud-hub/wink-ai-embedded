/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2009-2017 Dave Gamble and cJSON contributors */

#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static cJSON *cJSON_New_Item(void) {
    cJSON *node = (cJSON *)calloc(1, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *item) {
    cJSON *next;
    while (item != NULL) {
        next = item->next;
        if (item->child != NULL) {
            cJSON_Delete(item->child);
        }
        if (item->valuestring != NULL) {
            free(item->valuestring);
        }
        if (item->string != NULL) {
            free(item->string);
        }
        free(item);
        item = next;
    }
}

cJSON *cJSON_CreateObject(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Object;
    return item;
}

cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint = (int)num;
    }
    return item;
}

cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_String;
        item->valuestring = string ? strdup(string) : NULL;
    }
    return item;
}

static void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    cJSON *child;
    if (!item) return;
    child = array->child;
    if (!child) {
        array->child = item;
        item->prev = item;
        item->next = NULL;
    } else {
        while (child->next) child = child->next;
        child->next = item;
        item->prev = child;
    }
}

static void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!item) return;
    if (item->string) free(item->string);
    item->string = string ? strdup(string) : NULL;
    cJSON_AddItemToArray(object, item);
}

cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number) {
    cJSON *number_item = cJSON_CreateNumber(number);
    if (!number_item) return NULL;
    cJSON_AddItemToObject(object, name, number_item);
    return number_item;
}

cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string) {
    cJSON *string_item = cJSON_CreateString(string);
    if (!string_item) return NULL;
    cJSON_AddItemToObject(object, name, string_item);
    return string_item;
}

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    cJSON *current = NULL;
    if (!object || !string) return NULL;
    current = object->child;
    while (current != NULL) {
        if (current->string && strcmp(current->string, string) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void append_to_buf(char **buf, size_t *len, size_t *cap, const char *s) {
    if (!s) return;
    size_t slen = strlen(s);
    while (*len + slen + 1 >= *cap) {
        *cap = (*cap == 0) ? 256 : (*cap * 2);
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static void print_json_recursive(const cJSON *item, char **buf, size_t *len, size_t *cap) {
    char temp[128];
    if (!item) return;

    if (item->string) {
        append_to_buf(buf, len, cap, "\"");
        append_to_buf(buf, len, cap, item->string);
        append_to_buf(buf, len, cap, "\":");
    }

    switch (item->type) {
        case cJSON_Number:
            if ((double)item->valueint == item->valuedouble) {
                snprintf(temp, sizeof(temp), "%d", item->valueint);
            } else {
                snprintf(temp, sizeof(temp), "%f", item->valuedouble);
            }
            append_to_buf(buf, len, cap, temp);
            break;
        case cJSON_String:
            append_to_buf(buf, len, cap, "\"");
            if (item->valuestring) append_to_buf(buf, len, cap, item->valuestring);
            append_to_buf(buf, len, cap, "\"");
            break;
        case cJSON_Object: {
            append_to_buf(buf, len, cap, "{");
            cJSON *child = item->child;
            bool first = true;
            while (child) {
                if (!first) append_to_buf(buf, len, cap, ",");
                print_json_recursive(child, buf, len, cap);
                first = false;
                child = child->next;
            }
            append_to_buf(buf, len, cap, "}");
            break;
        }
        default:
            append_to_buf(buf, len, cap, "null");
            break;
    }
}


char *cJSON_Print(const cJSON *item) {
    size_t len = 0;
    size_t cap = 256;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    print_json_recursive(item, &buf, &len, &cap);
    return buf;
}

char *cJSON_PrintUnformatted(const cJSON *item) {
    return cJSON_Print(item);
}

static const char *skip_whitespace(const char *str) {
    while (*str && isspace((unsigned char)*str)) str++;
    return str;
}

static const char *parse_string(const char *str, char **out) {
    str = skip_whitespace(str);
    if (*str != '\"') return NULL;
    str++;
    const char *start = str;
    while (*str && *str != '\"') {
        if (*str == '\\' && *(str + 1)) str++;
        str++;
    }
    if (*str != '\"') return NULL;
    size_t len = (size_t)(str - start);
    char *res = (char *)malloc(len + 1);
    if (!res) return NULL;
    memcpy(res, start, len);
    res[len] = '\0';
    *out = res;
    return str + 1;
}

static const char *parse_value(const char *str, cJSON *item);

static const char *parse_object(const char *str, cJSON *item) {
    str = skip_whitespace(str);
    if (*str != '{') return NULL;
    str++;
    item->type = cJSON_Object;
    str = skip_whitespace(str);
    if (*str == '}') return str + 1;

    while (*str) {
        char *key = NULL;
        str = parse_string(str, &key);
        if (!str || !key) return NULL;

        str = skip_whitespace(str);
        if (*str != ':') {
            free(key);
            return NULL;
        }
        str++;

        cJSON *child = cJSON_New_Item();
        child->string = key;
        str = parse_value(str, child);
        if (!str) {
            cJSON_Delete(child);
            return NULL;
        }

        cJSON_AddItemToArray(item, child);

        str = skip_whitespace(str);
        if (*str == ',') {
            str++;
            str = skip_whitespace(str);
        } else if (*str == '}') {
            return str + 1;
        } else {
            return NULL;
        }
    }
    return NULL;
}

static const char *parse_number(const char *str, cJSON *item) {
    str = skip_whitespace(str);
    char *end = NULL;
    double d = strtod(str, &end);
    if (end == str) return NULL;
    item->type = cJSON_Number;
    item->valuedouble = d;
    item->valueint = (int)d;
    return end;
}

static const char *parse_value(const char *str, cJSON *item) {
    str = skip_whitespace(str);
    if (!*str) return NULL;
    if (*str == '{') return parse_object(str, item);
    if (*str == '\"') {
        char *s = NULL;
        str = parse_string(str, &s);
        if (!str) return NULL;
        item->type = cJSON_String;
        item->valuestring = s;
        return str;
    }
    if (*str == '-' || isdigit((unsigned char)*str)) return parse_number(str, item);
    return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    if (!value) return NULL;
    cJSON *item = cJSON_New_Item();
    if (!item) return NULL;
    const char *end = parse_value(value, item);
    if (!end) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}
