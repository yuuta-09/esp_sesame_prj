#include <stdbool.h>
#include <string.h>

bool is_null_or_empty(const char *str) {
  return (str == NULL || str[0] == '\0');
}
