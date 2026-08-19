#ifndef EHC_CHECKIN_H
#define EHC_CHECKIN_H

#include "ehc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EHC_KEY_KIND_CONST = 0,
  EHC_KEY_KIND_FN = 1,
  EHC_KEY_KIND_ASSET = 2
} ehc_key_kind_t;

#define EHC_KEY_REQUIRED 1u

typedef struct {
  const char *name;
  ehc_key_kind_t kind;
  const char *const_value;
  int (*fn)(char *out, size_t cap);
  uint32_t flags;
} ehc_checkin_key_t;

#define EHC_KEY_CONST(name, value) { (name), EHC_KEY_KIND_CONST, (value), NULL, 0 }
#define EHC_KEY_FN(name, fn, flags) { (name), EHC_KEY_KIND_FN, NULL, (fn), (flags) }
#define EHC_KEY_ASSET(name) { (name), EHC_KEY_KIND_ASSET, NULL, NULL, 0 }
#define EHC_KEY_ASSET_FLAGS(name, flags) { (name), EHC_KEY_KIND_ASSET, NULL, NULL, (flags) }

#ifdef __cplusplus
}
#endif

#endif
