#include "firebase_ssm_cmd.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "firebase/firebase_database.h"
#include "firebase/firebase_internal.h"

#define SSM_COMMAND_PATH "sesami5pro/commands/command.json"
#define SSM_CURRENT_STATUS_PATH "sesami5pro/status.json"

#define TAG "sesame_command"

static firebase_ssm_cmd_type_t _parse_cmd_type(const char *name) {
  if (!strcmp(name, "lock"))
    return SSM_CMD_LOCK;
  if (!strcmp(name, "unlock"))
    return SSM_CMD_UNLOCK;
  // else
  return SSM_CMD_NONE;
}

esp_err_t firebase_ssm_get_current_status(const firebase_auth_info_t *auth,
                                          firebase_ssm_status_t *out_status) {
  if (!auth || !out_status)
    return ESP_ERR_INVALID_ARG;

  char *response = NULL;

  firebase_request_param_t req = {
      .url_base = auth->database_url,
      .type = FIREBASE_USE_REALTIME_DATABASE,
      .method = HTTP_METHOD_GET,
      .path = SSM_CURRENT_STATUS_PATH,
  };

  esp_err_t err = firebase_database_get(auth, &req, &response);
  if (err != ESP_OK || response == NULL) {
    return err != ESP_OK ? err : ESP_FAIL;
  }

  /*
   * レスポンス例:
   * "locked"
   * "unlocked"
   */
  cJSON *root = cJSON_Parse(response);
  if (root == NULL) {
    free(response);
    return ESP_ERR_INVALID_RESPONSE;
  }

  esp_err_t ret = ESP_OK;
  if (cJSON_IsString(root)) {
    const char *status_value = root->valuestring;
    if (strcmp(status_value, "locked") == 0) {
      *out_status = SSM_STATUS_LOCKED;
    } else if (strcmp(status_value, "unlocked") == 0) {
      *out_status = SSM_STATUS_UNLOCKED;
    } else {
      *out_status = SSM_STATUS_UNKNOWN;
      ret = ESP_ERR_INVALID_RESPONSE;
    }
  } else {
    *out_status = SSM_STATUS_UNKNOWN;
    ret = ESP_ERR_INVALID_RESPONSE;
  }

  cJSON_Delete(root);
  free(response);
  return ret;
}

esp_err_t firebase_ssm_update_current_status(const firebase_auth_info_t *auth,
                                             firebase_ssm_status_t status) {
  if (!auth)
    return ESP_ERR_INVALID_ARG;

  // enum → 文字列変換
  const char *status_str = NULL;
  switch (status) {
  case SSM_STATUS_LOCKED:
    status_str = "\"locked\"";
    break;
  case SSM_STATUS_UNLOCKED:
    status_str = "\"unlocked\"";
    break;
  default:
    return ESP_ERR_INVALID_ARG;
  }

  firebase_request_param_t req = {
      .url_base = auth->database_url,
      .type = FIREBASE_USE_REALTIME_DATABASE,
      .method = HTTP_METHOD_PUT,
      .path = SSM_CURRENT_STATUS_PATH,
  };

  // status.jsonには "locked" のような文字列（ダブルクォート必須）を書き込む
  return firebase_database_put(auth, &req, status_str);
}

esp_err_t firebase_ssm_get_commands(const firebase_auth_info_t *auth,
                                    firebase_ssm_cmd_t *out_cmd) {
  char *response = NULL;
  firebase_request_param_t req = {
      .url_base = auth->database_url,
      .type = FIREBASE_USE_REALTIME_DATABASE,
      .method = HTTP_METHOD_GET,
      .path = SSM_COMMAND_PATH,
  };

  esp_err_t err = firebase_database_get(auth, &req, &response);
  if (err != ESP_OK || response == NULL)
    return err;

  cJSON *root = cJSON_Parse(response);
  if (!root) {
    free(response);
    return ESP_FAIL;
  }

  cJSON *jname = cJSON_GetObjectItem(root, "name");
  cJSON *juser = cJSON_GetObjectItem(root, "user_name");
  cJSON *jfin = cJSON_GetObjectItem(root, "is_finished");
  cJSON *jsucc = cJSON_GetObjectItem(root, "is_success");

  if (jname && cJSON_IsString(jname)) {
    out_cmd->cmd_type = _parse_cmd_type(jname->valuestring);
  } else {
    out_cmd->cmd_type = SSM_CMD_NONE;
  }
  strncpy(out_cmd->user_name,
          juser && cJSON_IsString(juser) ? juser->valuestring : "",
          sizeof(out_cmd->user_name));
  out_cmd->is_finished =
      jfin && cJSON_IsBool(jfin) ? cJSON_IsTrue(jfin) : false;
  out_cmd->is_success =
      jsucc && cJSON_IsBool(jsucc) ? cJSON_IsTrue(jsucc) : false;

  cJSON_Delete(root);
  free(response);
  return ESP_OK;
}

esp_err_t firebase_ssm_update_status(const firebase_auth_info_t *auth,
                                     const firebase_ssm_cmd_t *cmd) {
  firebase_request_param_t req = {
      .url_base = auth->database_url,
      .type = FIREBASE_USE_REALTIME_DATABASE,
      .method = HTTP_METHOD_PATCH,
      .path = SSM_COMMAND_PATH,
  };

  char status_json[128];
  snprintf(status_json, sizeof(status_json),
           "{ \"is_finished\": %s, \"is_success\": %s }",
           cmd->is_finished ? "true" : "false",
           cmd->is_success ? "true" : "false");

  return firebase_database_patch(auth, &req, status_json);
}
