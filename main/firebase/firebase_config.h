#pragma once

#include "sdkconfig.h"

#define FIREBASE_EMAIL CONFIG_FIREBASE_EMAIL
#define FIREBASE_PASSWORD CONFIG_FIREBASE_PASSWORD
#define FIREBASE_API_KEY CONFIG_FIREBASE_API_KEY
#define FIREBASE_DB_URL_BASE                                                   \
  "https://smarthome-2cc07-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH_URL_BASE                                                 \
  "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key="
#define FIREBASE_REFRESH_URL_BASE                                              \
  "https://securetoken.googleapis.com/v1/token?key="

extern const char root_cert_pem_start[] asm("_binary_roots_pem_start");
extern const char root_cert_pem_end[] asm("_binary_roots_pem_end");
