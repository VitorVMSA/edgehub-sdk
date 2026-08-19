#ifndef EHC_TYPES_H
#define EHC_TYPES_H

#include "ehc_config_defaults.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EHC_OK = 0,
  EHC_EINVAL = -1,
  EHC_ENOTREADY = -2,
  EHC_ECONFIG = -3,
  EHC_EIO = -4,
  EHC_EABI = -5,
  EHC_EAGAIN = -6
} ehc_status_t;

typedef enum {
  EHC_LOG_ERROR = 0,
  EHC_LOG_WARN = 1,
  EHC_LOG_INFO = 2,
  EHC_LOG_DEBUG = 3
} ehc_log_level_t;

typedef enum {
  EHC_UNCONFIGURED = 0,
  EHC_MINIMAL = 1,
  EHC_FULL = 2,
  EHC_ACTIVE = 3
} ehc_readiness_t;

typedef enum {
  EHC_IDENT_LITERAL = 0,
  EHC_IDENT_STORAGE = 1,
  EHC_IDENT_FN = 2
} ehc_ident_kind_t;

typedef struct {
  ehc_ident_kind_t kind;
  const char *value;
  int (*fn)(char *out, size_t cap);
} ehc_identity_source_t;

#define EHC_IDENT_LIT(s) { EHC_IDENT_LITERAL, (s), NULL }
#define EHC_IDENT_STORE(k) { EHC_IDENT_STORAGE, (k), NULL }
#define EHC_IDENT_FN(f) { EHC_IDENT_FN, NULL, (f) }
#define EHC_IDENT_NONE { EHC_IDENT_LITERAL, "", NULL }

#ifdef __cplusplus
}
#endif

#endif
