/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2009-2017 Dave Gamble and cJSON contributors */
#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7)

typedef struct cJSON
{
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

cJSON *cJSON_Parse(const char *value);
char *cJSON_Print(const cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);
void cJSON_Delete(cJSON *item);

cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string);
cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);

#ifdef __cplusplus
}
#endif

#endif
