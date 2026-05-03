#include <string.h>

#include "ipma_monitor.h"
#include "ipma_monitor_parser.h"
#include "unity.h"

static const char *VALID_PAYLOAD =
    "{"
    "\"dataPrev\":\"2026-05-02\","
    "\"dataRun\":\"2026-05-02\","
    "\"fileDate\":\"2026-05-02 09:05:01\","
    "\"local\":{"
    "\"1106\":{"
    "\"data\":{\"rcm\":3},"
    "\"DICO\":\"1106\","
    "\"latitude\":39.0,"
    "\"longitude\":-8.9"
    "}"
    "}"
    "}";

void test_ipma_monitor_parse_payload_returns_expected_result(void)
{
    ipma_monitor_result_t result = {0};

    TEST_ASSERT_EQUAL(IPMA_MONITOR_OK,
                      ipma_monitor_parse_payload(VALID_PAYLOAD, "1106", &result));
    TEST_ASSERT_EQUAL_STRING("2026-05-02", result.data_prev);
    TEST_ASSERT_EQUAL_STRING("2026-05-02", result.data_run);
    TEST_ASSERT_EQUAL_STRING("2026-05-02 09:05:01", result.file_date);
    TEST_ASSERT_EQUAL_STRING("1106", result.dico);
    TEST_ASSERT_EQUAL_UINT8(3, result.rcm_code);
    TEST_ASSERT_EQUAL_STRING("high", result.rcm_label);
}

void test_ipma_monitor_parse_payload_returns_not_found_for_unknown_dico(void)
{
    ipma_monitor_result_t result = {0};

    TEST_ASSERT_EQUAL(IPMA_MONITOR_ERR_DICO_NOT_FOUND,
                      ipma_monitor_parse_payload(VALID_PAYLOAD, "9999", &result));
}

void test_ipma_monitor_parse_payload_rejects_missing_local_object(void)
{
    ipma_monitor_result_t result = {0};
    const char *payload = "{\"dataPrev\":\"2026-05-02\",\"dataRun\":\"2026-05-02\",\"fileDate\":\"x\"}";

    TEST_ASSERT_EQUAL(IPMA_MONITOR_ERR_JSON_PARSE,
                      ipma_monitor_parse_payload(payload, "1106", &result));
}

void test_ipma_monitor_parse_payload_rejects_unsupported_rcm(void)
{
    ipma_monitor_result_t result = {0};
    const char *payload =
        "{"
        "\"dataPrev\":\"2026-05-02\","
        "\"dataRun\":\"2026-05-02\","
        "\"fileDate\":\"2026-05-02 09:05:01\","
    "\"local\":{\"1106\":{\"data\":{\"rcm\":8},\"DICO\":\"1106\",\"latitude\":39.0,\"longitude\":-8.9}}"
        "}";

    TEST_ASSERT_EQUAL(IPMA_MONITOR_ERR_INVALID_RCM,
                      ipma_monitor_parse_payload(payload, "1106", &result));
}

void test_ipma_monitor_parse_payload_accepts_live_lowercase_dico_format(void)
{
    ipma_monitor_result_t result = {0};
    const char *payload =
        "{"
        "\"dataPrev\":\"2026-05-03\","
        "\"dataRun\":\"2026-05-02\","
        "\"fileDate\":\"2026-05-03 00:05:02\","
        "\"local\":{\"1106\":{\"data\":{\"rcm\":3},\"dico\":\"1106\",\"latitude\":39.0,\"longitude\":-8.9}}"
        "}";

    TEST_ASSERT_EQUAL(IPMA_MONITOR_OK,
                      ipma_monitor_parse_payload(payload, "1106", &result));
    TEST_ASSERT_EQUAL_STRING("1106", result.dico);
    TEST_ASSERT_EQUAL_UINT8(3, result.rcm_code);
    TEST_ASSERT_EQUAL_STRING("high", result.rcm_label);
}
