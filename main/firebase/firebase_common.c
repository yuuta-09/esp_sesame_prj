#include "esp_http_client.h"
#include "firebase_internal.h"

/* firebase_response_ctx_tの操作を行う関数群 */
firebase_response_ctx_t *
firebase_create_response_ctx(firebase_auth_info_t *auth,
                             firebase_request_type_t type) {
  firebase_response_ctx_t *ctx = calloc(1, sizeof(firebase_response_ctx_t));
  if (!ctx)
    return NULL;

  ctx->auth = auth;
  ctx->type = type;
  ctx->buf = malloc(DEFAULT_HTTP_BUF_SIZE);
  ctx->buf_size = DEFAULT_HTTP_BUF_SIZE;
  return ctx;
}

void firebase_free_response_ctx(firebase_response_ctx_t *ctx) {
  if (!ctx)
    return;
  if (ctx->buf)
    free(ctx->buf);
  free(ctx);
}

char *build_api_url(const char *base, const char *api_key) {
  size_t len = strlen(base) + strlen(api_key) + 1;
  char *url = malloc(len);
  if (url)
    snprintf(url, len, "%s%s", base, api_key);

  return url;
}
