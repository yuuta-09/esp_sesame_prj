#pragma once

#include "firebase/firebase_internal.h" // firebase_auth_info_t等

typedef enum {
  SSM_CMD_NONE = 0,
  SSM_CMD_LOCK,
  SSM_CMD_UNLOCK,
} firebase_ssm_cmd_type_t;

typedef struct {
  firebase_ssm_cmd_type_t cmd_type;
  char user_name[32];
  bool is_finished;
  bool is_success;
} firebase_ssm_cmd_t;

typedef enum {
  SSM_STATUS_UNKNOWN = 0,
  SSM_STATUS_LOCKED,
  SSM_STATUS_UNLOCKED
} firebase_ssm_status_t;

/**
 * @brief Firebaseから現在のsesame 5 proの状態を取得
 * @param auth Firebase認証情報
 * @param out_status 状態(enum)
 * @return esp_err_t
 */
esp_err_t firebase_ssm_get_current_status(const firebase_auth_info_t *auth,
                                          firebase_ssm_status_t *out_status);

/**
 * @brief Firebaseに現在状態（locked/unlocked）を反映
 * @param auth Firebase認証情報
 * @param status 状態(enum) SSM_STATUS_LOCKEDまたはSSM_STATUS_UNLOCKED
 * @return esp_err_t
 */
esp_err_t firebase_ssm_update_current_status(const firebase_auth_info_t *auth,
                                             firebase_ssm_status_t status);

/**
 * @brief Firebaseからコマンドを取得
 * @param auth Firebaes認証情報
 * @param out_cmd 取得したコマンドの情報を保存する
 * @return esp_err_t
 */
esp_err_t firebase_ssm_get_commands(const firebase_auth_info_t *auth,
                                    firebase_ssm_cmd_t *out_cmd);

// コマンドの実行結果を更新（PATCH、is_finished等書き換え）
/**
 * @brief コマンドの実行結果を更新(PATCH, is_finished等を置き換え)
 * @param auth Firebase認証情報
 * @param cmd この値をもとに更新する。更新後のデータを渡す。
 * @return esp_err_t
 */
esp_err_t firebase_ssm_update_status(const firebase_auth_info_t *auth,
                                     const firebase_ssm_cmd_t *cmd);
