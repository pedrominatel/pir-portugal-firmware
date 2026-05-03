#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "ipma_monitor.h"
#include "ipma_monitor_parser.h"

static const char *TAG = "ipma_monitor";
static const char *IPMA_MONITOR_URL =
    "https://api.ipma.pt/open-data/forecast/meteorology/rcm/rcm-d0.json";

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} http_buffer_t;

static ipma_monitor_config_t s_config;
static bool s_initialized;

static const char *skip_json_whitespace(const char *cursor)
{
    while (cursor != NULL && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        cursor++;
    }

    return cursor;
}

static const char *find_json_key(const char *json, const char *key)
{
    char pattern[64];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(pattern)) {
        return NULL;
    }

    return strstr(json, pattern);
}

static ipma_monitor_error_t copy_json_string_value(const char *json,
                                                   const char *key,
                                                   char *buffer,
                                                   size_t buffer_len)
{
    const char *key_pos = find_json_key(json, key);
    if (key_pos == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *colon = strchr(key_pos, ':');
    if (colon == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *value = skip_json_whitespace(colon + 1);
    if (value == NULL || *value != '"') {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    value++;
    const char *end = strchr(value, '"');
    if (end == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    size_t value_len = (size_t)(end - value);
    if (value_len >= buffer_len) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    memcpy(buffer, value, value_len);
    buffer[value_len] = '\0';
    return IPMA_MONITOR_OK;
}

static ipma_monitor_error_t copy_json_string_value_any(const char *json,
                                                       const char *const *keys,
                                                       size_t key_count,
                                                       char *buffer,
                                                       size_t buffer_len)
{
    for (size_t i = 0; i < key_count; i++) {
        ipma_monitor_error_t err =
            copy_json_string_value(json, keys[i], buffer, buffer_len);
        if (err == IPMA_MONITOR_OK) {
            return IPMA_MONITOR_OK;
        }
    }

    return IPMA_MONITOR_ERR_JSON_PARSE;
}

static const char *find_matching_brace(const char *object_start)
{
    int depth = 0;

    for (const char *cursor = object_start; cursor != NULL && *cursor != '\0'; cursor++) {
        if (*cursor == '{') {
            depth++;
        } else if (*cursor == '}') {
            depth--;
            if (depth == 0) {
                return cursor;
            }
        }
    }

    return NULL;
}

static ipma_monitor_error_t copy_json_object_for_key(const char *json,
                                                     const char *key,
                                                     const char **object_start,
                                                     const char **object_end)
{
    const char *key_pos = find_json_key(json, key);
    if (key_pos == NULL) {
        return IPMA_MONITOR_ERR_DICO_NOT_FOUND;
    }

    const char *colon = strchr(key_pos, ':');
    if (colon == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *value = skip_json_whitespace(colon + 1);
    if (value == NULL || *value != '{') {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *end = find_matching_brace(value);
    if (end == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    *object_start = value;
    *object_end = end;
    return IPMA_MONITOR_OK;
}

static ipma_monitor_error_t copy_json_number_value(const char *json,
                                                   const char *key,
                                                   double *out_value)
{
    const char *key_pos = find_json_key(json, key);
    if (key_pos == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *colon = strchr(key_pos, ':');
    if (colon == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    const char *value = skip_json_whitespace(colon + 1);
    if (value == NULL) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    char *parse_end = NULL;
    double parsed = strtod(value, &parse_end);
    if (parse_end == value) {
        return IPMA_MONITOR_ERR_JSON_PARSE;
    }

    *out_value = parsed;
    return IPMA_MONITOR_OK;
}

static const char *rcm_label_from_code(int rcm_code)
{
    switch (rcm_code) {
    case 1:
        return "reduced";
    case 2:
        return "moderate";
    case 3:
        return "high";
    case 4:
        return "very high";
    case 5:
        return "maximum";
    default:
        return NULL;
    }
}

static ipma_monitor_error_t parse_location_entry(const char *location_json,
                                                 const char *target_dico,
                                                 ipma_monitor_result_t *out_result)
{
    static const char *const dico_keys[] = {"DICO", "dico"};
    char dico[IPMA_MONITOR_DICO_STR_LEN] = {0};
    double rcm_value = 0;
    double latitude = 0;
    double longitude = 0;

    ipma_monitor_error_t error =
        copy_json_string_value_any(location_json, dico_keys, 2, dico, sizeof(dico));
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    if (strcmp(dico, target_dico) != 0) {
        return IPMA_MONITOR_ERR_DICO_NOT_FOUND;
    }

    error = copy_json_number_value(location_json, "rcm", &rcm_value);
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    error = copy_json_number_value(location_json, "latitude", &latitude);
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    error = copy_json_number_value(location_json, "longitude", &longitude);
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    const char *rcm_label = rcm_label_from_code((int)rcm_value);
    if (rcm_label == NULL) {
        return IPMA_MONITOR_ERR_INVALID_RCM;
    }

    strlcpy(out_result->dico, dico, sizeof(out_result->dico));
    out_result->latitude = (float)latitude;
    out_result->longitude = (float)longitude;
    out_result->rcm_code = (uint8_t)rcm_value;
    out_result->rcm_label = rcm_label;
    return IPMA_MONITOR_OK;
}

ipma_monitor_error_t ipma_monitor_parse_payload(const char *payload,
                                                const char *target_dico,
                                                ipma_monitor_result_t *out_result)
{
    if (payload == NULL || target_dico == NULL || out_result == NULL) {
        return IPMA_MONITOR_ERR_INVALID_ARG;
    }

    memset(out_result, 0, sizeof(*out_result));

    ipma_monitor_error_t error = copy_json_string_value(payload, "dataPrev",
                                                        out_result->data_prev,
                                                        sizeof(out_result->data_prev));
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    error = copy_json_string_value(payload, "dataRun",
                                   out_result->data_run,
                                   sizeof(out_result->data_run));
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    error = copy_json_string_value(payload, "fileDate",
                                   out_result->file_date,
                                   sizeof(out_result->file_date));
    if (error != IPMA_MONITOR_OK) {
        return error;
    }

    const char *local_start = NULL;
    const char *local_end = NULL;
    error = copy_json_object_for_key(payload, "local", &local_start, &local_end);
    if (error != IPMA_MONITOR_OK) {
        return error == IPMA_MONITOR_ERR_DICO_NOT_FOUND ? IPMA_MONITOR_ERR_JSON_PARSE : error;
    }

    const char *location_start = NULL;
    const char *location_end = NULL;
    error = copy_json_object_for_key(local_start, target_dico, &location_start, &location_end);
    if (error == IPMA_MONITOR_OK) {
        return parse_location_entry(location_start, target_dico, out_result);
    }

    char dico_pattern[64];
    int written = snprintf(dico_pattern, sizeof(dico_pattern), "\"DICO\":\"%s\"", target_dico);
    if (written <= 0 || (size_t)written >= sizeof(dico_pattern)) {
        return IPMA_MONITOR_ERR_INVALID_ARG;
    }

    const char *dico_pos = strstr(local_start, dico_pattern);
    if (dico_pos != NULL && dico_pos < local_end) {
        const char *cursor = dico_pos;
        while (cursor > local_start && *cursor != '{') {
            cursor--;
        }

        if (*cursor == '{') {
            return parse_location_entry(cursor, target_dico, out_result);
        }
    }

    return IPMA_MONITOR_ERR_DICO_NOT_FOUND;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    http_buffer_t *buffer = event->user_data;
    if (buffer == NULL) {
        return ESP_FAIL;
    }

    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }

    size_t required = buffer->length + (size_t)event->data_len + 1;
    if (required > buffer->capacity) {
        size_t new_capacity = buffer->capacity == 0 ? 512 : buffer->capacity;
        while (new_capacity < required) {
            new_capacity *= 2;
        }

        char *new_data = realloc(buffer->data, new_capacity);
        if (new_data == NULL) {
            ESP_LOGE(TAG, "Failed to grow HTTP buffer");
            return ESP_ERR_NO_MEM;
        }

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->length, event->data, (size_t)event->data_len);
    buffer->length += (size_t)event->data_len;
    buffer->data[buffer->length] = '\0';
    return ESP_OK;
}

esp_err_t ipma_monitor_init(const ipma_monitor_config_t *config)
{
    if (config == NULL || config->dico == NULL || config->dico[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_config, 0, sizeof(s_config));
    s_config = *config;
    s_initialized = true;
    return ESP_OK;
}

ipma_monitor_error_t ipma_monitor_fetch_today(ipma_monitor_result_t *out_result)
{
    if (!s_initialized) {
        return IPMA_MONITOR_ERR_NOT_INITIALIZED;
    }
    if (out_result == NULL) {
        return IPMA_MONITOR_ERR_INVALID_ARG;
    }

    http_buffer_t response = {0};
    esp_http_client_config_t config = {
        .url = IPMA_MONITOR_URL,
        .method = HTTP_METHOD_GET,
        .timeout_ms = s_config.http_timeout_ms,
        .event_handler = http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return IPMA_MONITOR_ERR_NO_MEMORY;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(response.data);
        return err == ESP_ERR_NO_MEM ? IPMA_MONITOR_ERR_NO_MEMORY : IPMA_MONITOR_ERR_NETWORK;
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code != 200) {
        free(response.data);
        ESP_LOGE(TAG, "Unexpected HTTP status: %d", status_code);
        return IPMA_MONITOR_ERR_HTTP_STATUS;
    }

    ipma_monitor_error_t parse_result =
        ipma_monitor_parse_payload(response.data, s_config.dico, out_result);
    free(response.data);
    return parse_result;
}

const char *ipma_monitor_err_to_name(ipma_monitor_error_t error)
{
    switch (error) {
    case IPMA_MONITOR_OK:
        return "IPMA_MONITOR_OK";
    case IPMA_MONITOR_ERR_INVALID_ARG:
        return "IPMA_MONITOR_ERR_INVALID_ARG";
    case IPMA_MONITOR_ERR_NOT_INITIALIZED:
        return "IPMA_MONITOR_ERR_NOT_INITIALIZED";
    case IPMA_MONITOR_ERR_NETWORK:
        return "IPMA_MONITOR_ERR_NETWORK";
    case IPMA_MONITOR_ERR_HTTP_STATUS:
        return "IPMA_MONITOR_ERR_HTTP_STATUS";
    case IPMA_MONITOR_ERR_JSON_PARSE:
        return "IPMA_MONITOR_ERR_JSON_PARSE";
    case IPMA_MONITOR_ERR_DICO_NOT_FOUND:
        return "IPMA_MONITOR_ERR_DICO_NOT_FOUND";
    case IPMA_MONITOR_ERR_INVALID_RCM:
        return "IPMA_MONITOR_ERR_INVALID_RCM";
    case IPMA_MONITOR_ERR_NO_MEMORY:
        return "IPMA_MONITOR_ERR_NO_MEMORY";
    default:
        return "IPMA_MONITOR_ERR_UNKNOWN";
    }
}
