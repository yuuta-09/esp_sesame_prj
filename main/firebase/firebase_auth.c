#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "firebase_auth.h"
#include "firebase_common.h"
#include "firebase_config.h"
#include "firebase_internal.h"
#include "utils.h"

#define TAG "firebase_auth"

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  firebase_response_ctx_t *ctx = (firebase_response_ctx_t *)evt->user_data;

  switch (evt->event_id) {
  case HTTP_EVENT_ON_DATA:
    size_t req_size = ctx->len + evt->data_len + 1; // NULL終端分を加算
    if (req_size > MAX_HTTP_BUF_SIZE) {
      ESP_LOGE(TAG, "Buffer overflow: %d + %d > %d", ctx->len, evt->data_len,
               MAX_HTTP_BUF_SIZE);
      ctx->handler_err = ESP_ERR_NO_MEM;
      return ESP_FAIL;
    }

    // 必要ならバッファサイズを拡張
    if (req_size > ctx->buf_size) {
      ctx->buf = realloc(ctx->buf, req_size);

      if (!ctx->buf) {
        ESP_LOGE(TAG, "Buffer realloc failed: %zu", req_size);
        ctx->handler_err = ESP_ERR_NO_MEM;
        return ESP_FAIL;
      }
    }

    memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
    ctx->len += evt->data_len;

    break;
  case HTTP_EVENT_ON_FINISH:
    ctx->buf[ctx->len] = '\0'; // NULL終端

    cJSON *json = cJSON_Parse(ctx->buf);
    if (!json) {
      ESP_LOGE(TAG, "Handler: JSON parse error: %s", ctx->buf);
      ctx->handler_err = ESP_FAIL;
      break;
    }

    const cJSON *id_token_json;
    const cJSON *refresh_token_json;
    if (ctx->type == FIREBASE_USE_IDENTITY_TOOLKIT) {
      id_token_json = cJSON_GetObjectItem(json, "idToken");
      refresh_token_json = cJSON_GetObjectItem(json, "refreshToken");
    } else if (ctx->type == FIREBASE_USE_SECURE_TOKEN) {
      id_token_json = cJSON_GetObjectItem(json, "id_token");
      refresh_token_json = cJSON_GetObjectItem(json, "refresh_token");
    } else {
      ESP_LOGE(TAG, "Handler: Unknown request type: %d", ctx->type);
      ctx->handler_err = ESP_FAIL;
      break;
    }

    if (cJSON_IsString(id_token_json)) {
      firebase_auth_info_set_id_token(ctx->auth, id_token_json->valuestring);
    } else {
      ctx->handler_err = ESP_FAIL;
    }

    if (cJSON_IsString(refresh_token_json)) {
      firebase_auth_info_set_refresh_token(ctx->auth,
                                           refresh_token_json->valuestring);
    } else {
      ESP_LOGE(TAG, "Handler: No refreshToken in json");
      ctx->handler_err = ESP_FAIL;
    }
    cJSON_Delete(json);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static char *build_post_data(const char *fmt, va_list args) {
  // vsnprintfは破壊的操作なので、引数をコピーする
  // 2回呼び出す必要があるので、2つコピーを作成する
  va_list args_copy1, args_copy2;
  va_copy(args_copy1, args);
  va_copy(args_copy2, args);
  int len = vsnprintf(NULL, 0, fmt, args_copy1);
  va_end(args_copy1);

  if (len < 0)
    return NULL;

  char *buffer = malloc(len + 1); // +1はNULL終端のため
  if (!buffer) {
    va_end(args_copy2);
    return NULL;
  }

  vsnprintf(buffer, len + 1, fmt, args_copy2);
  va_end(args_copy2);

  return buffer;
}

// firebaseの認証データを取得するための構造体を初期化する関数
void firebase_auth_info_init(firebase_auth_info_t *auth, const char *email,
                             const char *password, const char *api_key,
                             const char *database_url) {
  memset(auth, 0, sizeof(firebase_auth_info_t));

  if (email)
    snprintf(auth->email, EMAIL_MAX_LEN - 1, "%s", email);
  if (password)
    snprintf(auth->password, PASSWORD_MAX_LEN - 1, "%s", password);
  if (api_key)
    snprintf(auth->api_key, API_MAX_LEN - 1, "%s", api_key);
  if (database_url)
    snprintf(auth->database_url, URL_MAX_LEN - 1, "%s", database_url);
  // id_token, refresh_token はゼロクリアのまま
}

// firebaseの認証情報のid_tokenをセットする関数
void firebase_auth_info_set_id_token(firebase_auth_info_t *auth,
                                     const char *id_token) {
  if (auth && id_token) {
    memset(auth->id_token, 0, ID_TOKEN_MAX_LEN);
    snprintf(auth->id_token, ID_TOKEN_MAX_LEN, "%s", id_token);
  }
}

// firebaseの認証情報のrefresh_tokenをセットする関数
void firebase_auth_info_set_refresh_token(firebase_auth_info_t *auth,
                                          const char *refresh_token) {
  if (auth && refresh_token) {
    memset(auth->refresh_token, 0, REFRESH_TOKEN_MAX_LEN);
    snprintf(auth->refresh_token, REFRESH_TOKEN_MAX_LEN, "%s", refresh_token);
  }
}

/*
 * Firebaseの認証関連のrequestを行う関数
 * 可変長引数でpostデータを受け取る
 */
static esp_err_t firebase_auth_post(const firebase_auth_info_t *auth,
                                    const firebase_request_param_t *param,
                                    ...) {
  esp_err_t err = ESP_FAIL;
  char *url = NULL;
  char *post_data = NULL;
  firebase_response_ctx_t *ctx = NULL;
  esp_http_client_handle_t client = NULL;

  if (!param || param->method != HTTP_METHOD_POST ||
      !param->data.write.post_data_format) {
    ESP_LOGE(TAG, "param is NULL or method is not POST");
    err = ESP_ERR_INVALID_ARG;
    goto cleanup;
  }

  // post_data assemble
  va_list args;
  va_start(args, param);
  post_data = build_post_data(param->data.write.post_data_format, args);
  va_end(args);
  if (!post_data)
    goto cleanup;

  // url assemble
  const char *base_url = param->url_base;
  url = build_api_url(base_url, auth->api_key);
  ESP_LOGI(TAG, "url: %s", url);
  if (is_null_or_empty(url)) {
    ESP_LOGI(TAG, "url is NULL or empty");
    goto cleanup;
  }

  ctx = firebase_create_response_ctx((firebase_auth_info_t *)auth, param->type);
  if (!ctx)
    goto cleanup;

  esp_http_client_config_t config = {
      .url = url,
      .cert_pem = root_cert_pem_start,
      .event_handler = _http_event_handler,
      .user_data = ctx,
      .timeout_ms = 20000, // 20秒
  };

  client = esp_http_client_init(&config);
  if (!client)
    goto cleanup;

  esp_http_client_set_header(client, "Content-Type",
                             param->data.write.content_type);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_post_field(client, post_data, strlen(post_data));

  err = esp_http_client_perform(client);

  // handlerでのエラー
  if (ctx->handler_err != ESP_OK)
    err = ctx->handler_err;

cleanup:
  if (client)
    esp_http_client_cleanup(client);
  if (url)
    free(url);
  if (post_data)
    free(post_data);
  firebase_free_response_ctx(ctx);
  return err;
}

// email, passwordを使用してfirebaseの認証データを取得するための関数
static esp_err_t get_auth_info(firebase_auth_info_t *auth) {
  firebase_request_param_t sign_in_email_password_req = {
      .url_base = FIREBASE_AUTH_URL_BASE,
      .type = FIREBASE_USE_IDENTITY_TOOLKIT,
      .method = HTTP_METHOD_POST,
      .data.write = {
          .content_type = "application/json",
          .post_data_format = "{\"email\":\"%s\",\"password\":\"%s\","
                              "\"returnSecureToken\":true}",
      }};
  return firebase_auth_post(auth, &sign_in_email_password_req, auth->email,
                            auth->password);
}

// refresh_tokenを使用して再度id_tokenを取得する
static esp_err_t refresh_id_token(firebase_auth_info_t *auth) {
  firebase_request_param_t refresh_token_req = {
      .url_base = FIREBASE_REFRESH_URL_BASE,
      .type = FIREBASE_USE_SECURE_TOKEN,
      .method = HTTP_METHOD_POST,
      .data.write = {
          .content_type = "application/x-www-form-urlencoded",
          .post_data_format = "grant_type=refresh_token&refresh_token=%s",
      }};
  return firebase_auth_post(auth, &refresh_token_req, auth->refresh_token);
}

// Firebaseの認証データを取得する関数
esp_err_t firebase_perform_auth(firebase_auth_info_t *auth) {
  if (!auth) {
    ESP_LOGE(TAG, "auth is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (is_null_or_empty(auth->api_key)) {
    ESP_LOGE(TAG, "api_key is NULL or empty");
    return ESP_ERR_INVALID_ARG;
  }

  if (is_null_or_empty(auth->database_url)) {
    ESP_LOGE(TAG, "database_url is NULL or empty");
    return ESP_ERR_INVALID_ARG;
  }

  if (is_null_or_empty(auth->email) || is_null_or_empty(auth->password)) {
    ESP_LOGE(TAG, "email or password is NULL or empty");
    return ESP_ERR_INVALID_ARG;
  }

  if (!is_null_or_empty(auth->id_token) &&
      !is_null_or_empty(auth->refresh_token)) {
    return refresh_id_token(auth);
  }

  if (is_null_or_empty(auth->id_token) &&
      is_null_or_empty(auth->refresh_token)) {
    return get_auth_info(auth);
  }

  ESP_LOGE(TAG,
           "given firebase_auth_info_t is invalid (mismatched token states)");
  return ESP_ERR_INVALID_ARG;
}

firebase_auth_info_t *firebase_setup_auth(const char *email,
                                          const char *password,
                                          const char *api_key,
                                          const char *db_url, int max_retry) {
  for (int retry = 0; retry < max_retry; retry++) {
    firebase_auth_info_t *auth = malloc(sizeof(firebase_auth_info_t));
    if (!auth) {
      ESP_LOGE("firebase_setup_auth_with_retry", "malloc failed: %s",
               esp_err_to_name(ESP_ERR_NO_MEM));
      continue;
    }
    firebase_auth_info_init(auth, email, password, api_key, db_url);

    esp_err_t status = firebase_perform_auth(auth);
    if (status == ESP_OK) {
      return auth;
    } else {
      ESP_LOGW("firebase_setup_auth_with_retry", "auth failed (try %d/%d): %s",
               retry + 1, max_retry, esp_err_to_name(status));
      free(auth);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  ESP_LOGE("firebase_setup_auth_with_retry", "auth failed after %d retries",
           max_retry);
  return NULL;
}
