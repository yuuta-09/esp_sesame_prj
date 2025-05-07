#pragma once

#include "firebase_internal.h"

firebase_response_ctx_t *
firebase_create_response_ctx(firebase_auth_info_t *auth,
                             firebase_request_type_t type);
void firebase_free_response_ctx(firebase_response_ctx_t *ctx);
char *build_api_url(const char *base, const char *api_key);
