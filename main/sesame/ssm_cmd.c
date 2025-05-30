#include "ssm_cmd.h"
#include "aes-cbc-cmac.h"
#include "esp_log.h"
#include "esp_random.h"
#include "uECC.h"
#include <string.h>

static const char *TAG = "ssm_cmd.c";
static uint8_t tag_esp32[] = {'S', 'E', 'S', 'A', 'M', 'E',
                              ' ', 'E', 'S', 'P', '3', '2'};
static uint8_t ecc_private_esp32[32];

static int crypto_backend_micro_ecc_rng_callback(uint8_t *dest, unsigned size) {
  esp_fill_random(dest, (size_t)size);
  return 1;
}

void send_reg_cmd_to_ssm(sesame *ssm) {
  ESP_LOGW(TAG, "[esp32->ssm][register]");
  uECC_set_rng(crypto_backend_micro_ecc_rng_callback);
  uint8_t ecc_public_esp32[64];
  uECC_make_key_lit(ecc_public_esp32, ecc_private_esp32, uECC_secp256r1());
  ssm->c_offset = sizeof(ecc_public_esp32) + 1;
  ssm->b_buf[0] = SSM_ITEM_CODE_REGISTRATION;
  memcpy(ssm->b_buf + 1, ecc_public_esp32, sizeof(ecc_public_esp32));
  talk_to_ssm(ssm, SSM_SEG_PARSING_TYPE_PLAINTEXT);
}

void handle_reg_data_from_ssm(sesame *ssm) {
  ESP_LOGW(TAG, "[esp32<-ssm][register]");
  memcpy(ssm->public_key, &ssm->b_buf[13], 64);
  uint8_t ecdh_secret_ssm[32];
  uECC_shared_secret_lit(ssm->public_key, ecc_private_esp32, ecdh_secret_ssm,
                         uECC_secp256r1());
  memcpy(ssm->device_secret, ecdh_secret_ssm, 16);
  // ESP_LOG_BUFFER_HEX("deviceSecret", ssm->device_secret, 16);
  AES_CMAC(ssm->device_secret,
           (const unsigned char *)ssm->cipher.decrypt.random_code, 4,
           ssm->cipher.token);
  ssm->device_status = SSM_LOGGIN;
  p_ssms_env->ssm_cb__(ssm); // callback: ssm_action_handle() in main.c
}

void send_login_cmd_to_ssm(sesame *ssm) {
  ESP_LOGW(TAG, "[esp32->ssm][login]");
  ssm->b_buf[0] = SSM_ITEM_CODE_LOGIN;
  AES_CMAC(ssm->device_secret,
           (const unsigned char *)ssm->cipher.decrypt.random_code, 4,
           ssm->cipher.token);
  memcpy(&ssm->b_buf[1], ssm->cipher.token, 4);
  ssm->c_offset = 5;
  talk_to_ssm(ssm, SSM_SEG_PARSING_TYPE_PLAINTEXT);
}

void send_read_history_cmd_to_ssm(sesame *ssm) {
  ESP_LOGI(TAG, "[send_read_history_cmd_to_ssm]");
  ssm->c_offset = 2;
  ssm->b_buf[0] = SSM_ITEM_CODE_HISTORY;
  ssm->b_buf[1] = 1;
  talk_to_ssm(ssm, SSM_SEG_PARSING_TYPE_CIPHERTEXT);
}

void ssm_lock(uint8_t *tag, uint8_t tag_length) {
  // ESP_LOGI(TAG, "[ssm][ssm_lock][%s]",
  // SSM_STATUS_STR(p_ssms_env->ssm.device_status));
  sesame *ssm = &p_ssms_env->ssm;
  if (ssm->device_status >= SSM_LOGGIN) {
    if (tag_length == 0) {
      tag = tag_esp32;
      tag_length = sizeof(tag_esp32);
    }
    ssm->b_buf[0] = SSM_ITEM_CODE_LOCK;
    ssm->b_buf[1] = tag_length;
    ssm->c_offset = tag_length + 2;
    memcpy(ssm->b_buf + 2, tag, tag_length);
    talk_to_ssm(ssm, SSM_SEG_PARSING_TYPE_CIPHERTEXT);
  }
}

void ssm_unlock(uint8_t *tag, uint8_t tag_length) {
  // ESP_LOGI(TAG, "[ssm][ssm_lock][%s]",
  // SSM_STATUS_STR(p_ssms_env->ssm.device_status));
  sesame *ssm = &p_ssms_env->ssm;
  if (ssm->device_status >= SSM_LOGGIN) {
    if (tag_length == 0) {
      tag = tag_esp32;
      tag_length = sizeof(tag_esp32);
    }
    ssm->b_buf[0] = SSM_ITEM_CODE_UNLOCK;
    ssm->b_buf[1] = tag_length;
    ssm->c_offset = tag_length + 2;
    memcpy(ssm->b_buf + 2, tag, tag_length);
    talk_to_ssm(ssm, SSM_SEG_PARSING_TYPE_CIPHERTEXT);
  }
}
