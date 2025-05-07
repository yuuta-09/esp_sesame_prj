#pragma once

#include "esp_err.h"
#include "firebase_internal.h"

void firebase_auth_info_init(firebase_auth_info_t *auth, const char *email,
                             const char *password, const char *api_key,
                             const char *database_url);
void firebase_auth_info_set_id_token(firebase_auth_info_t *auth,
                                     const char *id_token);
void firebase_auth_info_set_refresh_token(firebase_auth_info_t *auth,
                                          const char *refresh_token);

esp_err_t firebase_perform_auth(firebase_auth_info_t *auth);

firebase_auth_info_t *firebase_setup_auth(const char *email,
                                          const char *password,
                                          const char *api_key,
                                          const char *db_url, int max_retry);
