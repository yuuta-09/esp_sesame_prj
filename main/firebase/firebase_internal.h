#pragma once

#include "esp_err.h"
#include "esp_http_client.h"

#define ID_TOKEN_MAX_LEN 2048
#define REFRESH_TOKEN_MAX_LEN 256
#define URL_MAX_LEN 256
#define API_MAX_LEN 128
#define EMAIL_MAX_LEN 32
#define PASSWORD_MAX_LEN 32
#define MAX_HTTP_BUF_SIZE 4096
// #define DEFAULT_HTTP_BUF_SIZE 512 すでにesp_http_clientで定義されている

typedef enum {
  FIREBASE_AUTH_SUCCESS = 0, // 認証成功
  FIREBASE_AUTH_FAILURE,     // 認証失敗
  FIREBASE_AUTH_TIMEOUT,     // タイムアウト

} firebase_auth_status_t;

typedef enum {
  FIREBASE_USE_IDENTITY_TOOLKIT,  // Firebase Authenticationを使用する
  FIREBASE_USE_SECURE_TOKEN,      // Firebase Secure Tokenを使用する
  FIREBASE_USE_REALTIME_DATABASE, // Firebase Realtime Databaseを使用する
} firebase_request_type_t;

typedef struct {
  const char *url_base;            // FIREBASE_AUTH_UR_BASE等のURL
  firebase_request_type_t type;    // リクエストの種類
  esp_http_client_method_t method; // HTTPメソッド
  const char *path;                // Firebaseのパス(/usrs/uid123.json等)
  union {
    struct {
      const char *content_type;     // Content-Type(application/json等)
      const char *post_data_format; // POSTデータのformat文字列
    } write;                        // PUT or PATH or POST
    struct {
      const char *accept_tuype;        // Accept-Type(application/json等)
      const char *query_string_format; // クエリ文字列のformat文字列
    } read;

  } data;
} firebase_request_param_t;

typedef struct {
  char database_url[URL_MAX_LEN]; // データベースのURL
  char api_key[API_MAX_LEN];      // APIキー
  // Firebase Authenticationのためのトークン
  char id_token[ID_TOKEN_MAX_LEN];
  char refresh_token[REFRESH_TOKEN_MAX_LEN]; // id_tokenを更新するためのトークン
  char email[EMAIL_MAX_LEN];                 // メールアドレス
  char password[PASSWORD_MAX_LEN];           // パスワード
} firebase_auth_info_t;

typedef struct {
  char *buf;                    // レスポンスバッファを格納するポインタ
  int buf_size;                 // バッファサイズ
  int len;                      // 現在の長さ
  esp_err_t handler_err;        // イベントハンドラのエラー
  firebase_auth_info_t *auth;   // 認証情報
  firebase_request_type_t type; // リクエストの種類
  char *body;                   // レスポンスボディ
  size_t body_len;              // レスポンスボディの長さ
} firebase_response_ctx_t;
