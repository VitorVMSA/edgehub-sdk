#include "ehc_user.h"
#include <string.h>

static int app_fw_version(char *out, size_t cap)
{
  if (!out || cap == 0) {
    return 0;
  }
  strncpy(out, "0.1.0", cap - 1);
  out[cap - 1] = '\0';
  return 1;
}

const ehc_checkin_key_t ehc_user_checkin_keys[] = {
  EHC_KEY_CONST("boardModel", "example_board"),
  EHC_KEY_FN("firmware", app_fw_version, EHC_KEY_REQUIRED),
  EHC_KEY_ASSET("classifications"),
  EHC_KEY_ASSET("internalConfigurations"),
};

const size_t ehc_user_checkin_keys_count =
  sizeof(ehc_user_checkin_keys) / sizeof(ehc_user_checkin_keys[0]);

int ehc_user_on_config_apply(
  const char *asset_key, const char *version, const char *source, const char *payload, void *ctx)
{
  (void)asset_key;
  (void)version;
  (void)source;
  (void)payload;
  (void)ctx;
  return 1;
}
