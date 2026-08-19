#ifndef EHC_TELEMETRY_H
#define EHC_TELEMETRY_H

#include "ehc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EHC_VAL_NUM = 0,
  EHC_VAL_STR = 1,
  EHC_VAL_BOOL = 2,
  EHC_VAL_OBJECT = 3
} ehc_val_kind_t;

typedef struct {
  const char *name;
  const char *parent;
  ehc_val_kind_t kind;
  int (*get_num)(double *out);
  int (*get_str)(char *out, size_t cap);
  int (*get_bool)(int *out);
  uint8_t send_when_stale;
} ehc_metric_t;

#define EHC_METRIC_NUM(name) { (name), NULL, EHC_VAL_NUM, NULL, NULL, NULL, 0 }
#define EHC_METRIC_STR(name) { (name), NULL, EHC_VAL_STR, NULL, NULL, NULL, 0 }
#define EHC_METRIC_BOOL(name) { (name), NULL, EHC_VAL_BOOL, NULL, NULL, NULL, 0 }
#define EHC_METRIC_OBJECT(name) { (name), NULL, EHC_VAL_OBJECT, NULL, NULL, NULL, 0 }
#define EHC_METRIC_NUM_IN(parent, name) { (name), (parent), EHC_VAL_NUM, NULL, NULL, NULL, 0 }
#define EHC_METRIC_STR_IN(parent, name) { (name), (parent), EHC_VAL_STR, NULL, NULL, NULL, 0 }
#define EHC_METRIC_BOOL_IN(parent, name) { (name), (parent), EHC_VAL_BOOL, NULL, NULL, NULL, 0 }
#define EHC_METRIC_OBJECT_IN(parent, name) { (name), (parent), EHC_VAL_OBJECT, NULL, NULL, NULL, 0 }
#define EHC_METRIC_NUM_GET(name, fn) { (name), NULL, EHC_VAL_NUM, (fn), NULL, NULL, 0 }
#define EHC_METRIC_STR_GET(name, fn) { (name), NULL, EHC_VAL_STR, NULL, (fn), NULL, 0 }
#define EHC_METRIC_BOOL_GET(name, fn) { (name), NULL, EHC_VAL_BOOL, NULL, NULL, (fn), 0 }
#define EHC_METRIC_NUM_GET_IN(parent, name, fn) { (name), (parent), EHC_VAL_NUM, (fn), NULL, NULL, 0 }
#define EHC_METRIC_NUM_STALE(name) { (name), NULL, EHC_VAL_NUM, NULL, NULL, NULL, 1 }
#define EHC_METRIC_STR_STALE(name) { (name), NULL, EHC_VAL_STR, NULL, NULL, NULL, 1 }
#define EHC_METRIC_BOOL_STALE(name) { (name), NULL, EHC_VAL_BOOL, NULL, NULL, NULL, 1 }

ehc_status_t ehc_tel_set_num(const char *name, double value);
ehc_status_t ehc_tel_set_str(const char *name, const char *value);
ehc_status_t ehc_tel_set_bool(const char *name, int value);
ehc_status_t ehc_tel_set_num_in(const char *parent, const char *name, double value);
ehc_status_t ehc_tel_set_str_in(const char *parent, const char *name, const char *value);
ehc_status_t ehc_tel_set_bool_in(const char *parent, const char *name, int value);
ehc_status_t ehc_tel_set_num_at(const char *path, double value);
ehc_status_t ehc_tel_set_str_at(const char *path, const char *value);
ehc_status_t ehc_tel_set_bool_at(const char *path, int value);
/** Enfileira a leitura actual. `ts_iso` NULL usa `now_ms` do port. Não faz flush. */
ehc_status_t ehc_tel_enqueue(const char *ts_iso);
/** Envia a fila se ACTIVE; caso contrário OK e a fila permanece. */
ehc_status_t ehc_tel_flush(void);
/** Leituras na fila do device SDK (0 se o client ainda não arrancou). */
uint8_t ehc_tel_queued(void);
/** Atalho: `ehc_tel_enqueue(NULL)` + `ehc_tel_flush()`. */
ehc_status_t ehc_tel_publish(void);

#ifdef __cplusplus
}
#endif

#endif
