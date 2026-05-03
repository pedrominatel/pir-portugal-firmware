#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPMA_MONITOR_DATE_STR_LEN 16
#define IPMA_MONITOR_FILE_DATE_STR_LEN 24
#define IPMA_MONITOR_DICO_STR_LEN 8

typedef enum {
    IPMA_MONITOR_OK = 0,
    IPMA_MONITOR_ERR_INVALID_ARG,
    IPMA_MONITOR_ERR_NOT_INITIALIZED,
    IPMA_MONITOR_ERR_NETWORK,
    IPMA_MONITOR_ERR_HTTP_STATUS,
    IPMA_MONITOR_ERR_JSON_PARSE,
    IPMA_MONITOR_ERR_DICO_NOT_FOUND,
    IPMA_MONITOR_ERR_INVALID_RCM,
    IPMA_MONITOR_ERR_NO_MEMORY,
} ipma_monitor_error_t;

typedef struct {
    const char *dico;
    int http_timeout_ms;
} ipma_monitor_config_t;

typedef struct {
    char data_prev[IPMA_MONITOR_DATE_STR_LEN];
    char data_run[IPMA_MONITOR_DATE_STR_LEN];
    char file_date[IPMA_MONITOR_FILE_DATE_STR_LEN];
    char dico[IPMA_MONITOR_DICO_STR_LEN];
    float latitude;
    float longitude;
    uint8_t rcm_code;
    const char *rcm_label;
} ipma_monitor_result_t;

esp_err_t ipma_monitor_init(const ipma_monitor_config_t *config);
ipma_monitor_error_t ipma_monitor_fetch_today(ipma_monitor_result_t *out_result);
const char *ipma_monitor_err_to_name(ipma_monitor_error_t error);

#ifdef __cplusplus
}
#endif
