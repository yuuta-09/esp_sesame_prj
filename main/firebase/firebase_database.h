#pragma once

#include "firebase_internal.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t firebase_https_mutex;

esp_err_t firebase_database_get(const firebase_auth_info_t *auth,
                                const firebase_request_param_t *param,
                                char **response_out);
esp_err_t firebase_database_put(const firebase_auth_info_t *auth,
                                const firebase_request_param_t *param,
                                const char *json_body);
esp_err_t firebase_database_patch(const firebase_auth_info_t *auth,
                                  const firebase_request_param_t *param,
                                  const char *patch_data);
