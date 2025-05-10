#include "esp_err.h"
#include "esp_log.h"

#include "esp_http_client.h"
#include "firebase/firebase_common.h"
#include "firebase/firebase_config.h"
#include "firebase/firebase_internal.h"
#include "firebase_database.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

const char *TAG = "firebase_database";
SemaphoreHandle_t firebase_https_mutex = NULL;

static char *build_database_url(const char *url_base, const char *path,
                                const char *id_token_optional) {
  int len = strlen(url_base) + strlen(path) + 1; // +1はNULL終端用
  if (id_token_optional) {
    len += strlen("&auth=") + strlen(id_token_optional);
  }

  char *url = malloc(len);
  if (!url)
    return NULL;

  strcpy(url, url_base);
  strcat(url, path);

  if (id_token_optional) {
    strcat(url, "?auth=");
    strcat(url, id_token_optional);
  }

  return url;
}

static esp_err_t
_firebase_database_http_event_handler(esp_http_client_event_t *evt) {
  firebase_response_ctx_t *ctx = (firebase_response_ctx_t *)evt->user_data;

  switch (evt->event_id) {
  case HTTP_EVENT_ON_DATA:
    if (evt->data_len > 0) {
      // 本体データを動的に連結
      char *new_body = realloc(ctx->body, ctx->body_len + evt->data_len + 1);
      if (!new_body) {
        ctx->handler_err = ESP_ERR_NO_MEM;
        return ESP_FAIL;
      }
      ctx->body = new_body;
      memcpy(ctx->body + ctx->body_len, evt->data, evt->data_len);
      ctx->body_len += evt->data_len;
      ctx->body[ctx->body_len] = '\0';
    }
    break;

  case HTTP_EVENT_ON_FINISH:
    if (ctx->body && strstr(ctx->body, "Permission denied")) {
      ctx->handler_err =
          ESP_ERR_INVALID_STATE; // 適切なカスタムエラーにしてもよい
    }
    break;

  case HTTP_EVENT_ERROR:
    ctx->handler_err = ESP_FAIL;
    break;

  default:
    break;
  }

  return ESP_OK;
}

esp_err_t firebase_database_get(const firebase_auth_info_t *auth,
                                const firebase_request_param_t *param,
                                char **response_out) {
  esp_err_t err = ESP_FAIL;
  char *url = NULL;
  firebase_response_ctx_t *ctx = NULL;
  esp_http_client_handle_t client = NULL;

  if (xSemaphoreTake(firebase_https_mutex, pdMS_TO_TICKS(20000)) == pdTRUE) {
    const char *id_token = auth ? auth->id_token : NULL;
    url = build_database_url(param->url_base, param->path, id_token);
    if (!url) {
      xSemaphoreGive(firebase_https_mutex);
      return ESP_ERR_NO_MEM;
    }

    ctx =
        firebase_create_response_ctx((firebase_auth_info_t *)auth, param->type);
    if (!ctx) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = root_cert_pem_start,
        .event_handler = _firebase_database_http_event_handler,
        .user_data = ctx,
        .timeout_ms = 15000, // 15秒
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    client = esp_http_client_init(&config);
    if (!client) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);

    if (ctx->handler_err != ESP_OK)
      err = ctx->handler_err;

    ESP_LOGI(TAG, "HTTP Status = %d, response_code = %d", err,
             esp_http_client_get_status_code(client));

    if (response_out && ctx->body)
      *response_out = strdup(ctx->body);

    xSemaphoreGive(firebase_https_mutex);
  }

cleanup:
  if (client)
    esp_http_client_cleanup(client);
  if (url)
    free(url);
  firebase_free_response_ctx(ctx);
  return err;
}

esp_err_t firebase_database_put(const firebase_auth_info_t *auth,
                                const firebase_request_param_t *param,
                                const char *json_body) {
  // ToDo
  esp_err_t err = ESP_FAIL;
  char *url = NULL;
  firebase_response_ctx_t *ctx = NULL;
  esp_http_client_handle_t client = NULL;

  if (xSemaphoreTake(firebase_https_mutex, pdMS_TO_TICKS(20000)) == pdTRUE) {
    const char *id_token = auth ? auth->id_token : NULL;
    url = build_database_url(param->url_base, param->path, id_token);
    if (!url) {
      xSemaphoreGive(firebase_https_mutex);
      return ESP_ERR_NO_MEM;
    }

    ctx =
        firebase_create_response_ctx((firebase_auth_info_t *)auth, param->type);
    if (!ctx) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = root_cert_pem_start,
        .event_handler = _firebase_database_http_event_handler,
        .user_data = ctx,
        .timeout_ms = 15000, // 15秒
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };

    client = esp_http_client_init(&config);
    if (!client) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_post_field(client, json_body, strlen(json_body));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    err = esp_http_client_perform(client);

    if (ctx->handler_err != ESP_OK)
      err = ctx->handler_err;

    ESP_LOGI(TAG, "HTTP Status = %d, response_code = %d", err,
             esp_http_client_get_status_code(client));

    xSemaphoreGive(firebase_https_mutex);
  }

cleanup:
  if (client)
    esp_http_client_cleanup(client);
  if (url)
    free(url);
  firebase_free_response_ctx(ctx);
  return err;
}

esp_err_t firebase_database_patch(const firebase_auth_info_t *auth,
                                  const firebase_request_param_t *param,
                                  const char *patch_data) {
  esp_err_t err = ESP_FAIL;
  char *url = NULL;
  firebase_response_ctx_t *ctx = NULL;
  esp_http_client_handle_t client = NULL;

  if (xSemaphoreTake(firebase_https_mutex, pdMS_TO_TICKS(20000)) == pdTRUE) {
    const char *id_token = auth ? auth->id_token : NULL;
    url = build_database_url(param->url_base, param->path, id_token);
    if (!url) {
      xSemaphoreGive(firebase_https_mutex);
      return ESP_ERR_NO_MEM;
    }

    ctx =
        firebase_create_response_ctx((firebase_auth_info_t *)auth, param->type);
    if (!ctx) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_config_t config = {
        .url = url,
        .cert_pem = root_cert_pem_start,
        .event_handler = _firebase_database_http_event_handler,
        .user_data = ctx,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    client = esp_http_client_init(&config);
    if (!client) {
      xSemaphoreGive(firebase_https_mutex);
      goto cleanup;
    }

    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, patch_data, strlen(patch_data));

    err = esp_http_client_perform(client);

    if (ctx->handler_err != ESP_OK)
      err = ctx->handler_err;

    ESP_LOGI(TAG, "PATCH HTTP Status = %d, response_code = %d", err,
             esp_http_client_get_status_code(client));

    xSemaphoreGive(firebase_https_mutex);
  }

cleanup:
  if (client)
    esp_http_client_cleanup(client);
  if (url)
    free(url);
  firebase_free_response_ctx(ctx);
  return err;
}
