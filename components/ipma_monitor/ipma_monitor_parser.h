#pragma once

#include "ipma_monitor.h"

ipma_monitor_error_t ipma_monitor_parse_payload(const char *payload,
                                                const char *target_dico,
                                                ipma_monitor_result_t *out_result);
