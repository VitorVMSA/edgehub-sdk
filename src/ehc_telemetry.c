#include "ehc_internal.h"
#include <stdlib.h>

static uint32_t fnv1a(const char *s)
{
  uint32_t h = 2166136261u;
  if (!s) {
    return 0;
  }
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619u;
  }
  return h;
}

static int names_equal(const char *a, const char *b)
{
  if (!a || !b) {
    return 0;
  }
  return strcmp(a, b) == 0;
}

int ehc_metric_index(const char *parent, const char *name)
{
  size_t i;
  uint32_t h;
  if (!g_ehc.used || !name) {
    return -1;
  }
  h = fnv1a(name);
  for (i = 0; i < g_ehc.user.metric_count; i++) {
    const ehc_metric_t *m = &g_ehc.user.metrics[i];
    if (g_ehc.metrics[i].name_hash && g_ehc.metrics[i].name_hash != h) {
      continue;
    }
    if (!names_equal(m->name, name)) {
      continue;
    }
    if (parent == NULL || parent[0] == '\0') {
      if (!m->parent || !m->parent[0]) {
        return (int)i;
      }
    } else if (m->parent && names_equal(m->parent, parent)) {
      return (int)i;
    }
  }
  return -1;
}

int ehc_metric_index_at(const char *path)
{
  char buf[96];
  char *p;
  char *seg;
  char *save = NULL;
  int idx = -1;
  const char *parent = NULL;
  if (!path) {
    return -1;
  }
  strncpy(buf, path, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  p = buf;
  while (p && *p) {
    seg = p;
    save = strchr(p, '.');
    if (save) {
      *save = '\0';
      p = save + 1;
    } else {
      p = NULL;
    }
    idx = ehc_metric_index(parent, seg);
    if (idx < 0) {
      return -1;
    }
    parent = g_ehc.user.metrics[idx].name;
  }
  return idx;
}

static int depth_of(size_t idx)
{
  int depth = 1;
  const char *parent = g_ehc.user.metrics[idx].parent;
  int guard = 0;
  while (parent && parent[0]) {
    int pidx = -1;
    size_t i;
    depth++;
    if (depth > EHC_MAX_NESTING_DEPTH) {
      return depth;
    }
    for (i = 0; i < g_ehc.user.metric_count; i++) {
      if (g_ehc.user.metrics[i].kind == EHC_VAL_OBJECT && names_equal(g_ehc.user.metrics[i].name, parent)) {
        pidx = (int)i;
        break;
      }
    }
    if (pidx < 0) {
      return -1;
    }
    parent = g_ehc.user.metrics[pidx].parent;
    if (++guard > EHC_MAX_METRICS) {
      return -1;
    }
  }
  return depth;
}

int ehc_telemetry_validate_table(void)
{
  size_t i;
  size_t j;
  int str_needed = 0;
  memset(g_ehc.metrics, 0, sizeof(g_ehc.metrics));
  memset(g_ehc.str_used, 0, sizeof(g_ehc.str_used));
  for (i = 0; i < g_ehc.user.metric_count; i++) {
    const ehc_metric_t *m = &g_ehc.user.metrics[i];
    int d;
    if (!m->name || !m->name[0]) {
      ehc_logf(EHC_LOG_ERROR, "metric %u has empty name", (unsigned)i);
      return -1;
    }
    g_ehc.metrics[i].name_hash = fnv1a(m->name);
    g_ehc.metrics[i].str_slot = 0xFF;
    if (m->kind == EHC_VAL_OBJECT) {
      for (j = 0; j < i; j++) {
        if (g_ehc.user.metrics[j].kind == EHC_VAL_OBJECT && names_equal(g_ehc.user.metrics[j].name, m->name)) {
          ehc_logf(EHC_LOG_ERROR, "duplicate object name '%s'", m->name);
          return -1;
        }
      }
    }
    d = depth_of(i);
    if (d < 0) {
      ehc_logf(EHC_LOG_ERROR, "metric '%s' parent not found", m->name);
      return -1;
    }
    if (d > EHC_MAX_NESTING_DEPTH) {
      ehc_logf(EHC_LOG_ERROR, "metric '%s' exceeds EHC_MAX_NESTING_DEPTH", m->name);
      return -1;
    }
    if (m->kind == EHC_VAL_STR) {
      if (str_needed >= EHC_TEL_STR_SLOTS) {
        ehc_logf(EHC_LOG_ERROR, "string pool exhausted at metric '%s'", m->name);
        return -1;
      }
      g_ehc.metrics[i].str_slot = (uint8_t)str_needed;
      str_needed++;
    }
  }
  return 0;
}

static ehc_status_t set_num_idx(int idx, double value)
{
  if (idx < 0 || g_ehc.user.metrics[idx].kind != EHC_VAL_NUM) {
    return EHC_EINVAL;
  }
  g_ehc.metrics[idx].num = value;
  g_ehc.metrics[idx].filled = 1;
  return EHC_OK;
}

static ehc_status_t set_bool_idx(int idx, int value)
{
  if (idx < 0 || g_ehc.user.metrics[idx].kind != EHC_VAL_BOOL) {
    return EHC_EINVAL;
  }
  g_ehc.metrics[idx].boolean = value ? 1 : 0;
  g_ehc.metrics[idx].filled = 1;
  return EHC_OK;
}

static ehc_status_t set_str_idx(int idx, const char *value)
{
  uint8_t slot;
  if (idx < 0 || g_ehc.user.metrics[idx].kind != EHC_VAL_STR) {
    return EHC_EINVAL;
  }
  slot = g_ehc.metrics[idx].str_slot;
  if (slot == 0xFF) {
    return EHC_ECONFIG;
  }
  strncpy(g_ehc.str_pool[slot], value ? value : "", EHC_TEL_STR_MAX - 1);
  g_ehc.str_pool[slot][EHC_TEL_STR_MAX - 1] = '\0';
  g_ehc.metrics[idx].filled = 1;
  return EHC_OK;
}

ehc_status_t ehc_tel_set_num(const char *name, double value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_num_idx(ehc_metric_index(NULL, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_str(const char *name, const char *value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_str_idx(ehc_metric_index(NULL, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_bool(const char *name, int value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_bool_idx(ehc_metric_index(NULL, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_num_in(const char *parent, const char *name, double value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_num_idx(ehc_metric_index(parent, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_str_in(const char *parent, const char *name, const char *value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_str_idx(ehc_metric_index(parent, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_bool_in(const char *parent, const char *name, int value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_bool_idx(ehc_metric_index(parent, name), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_num_at(const char *path, double value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_num_idx(ehc_metric_index_at(path), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_str_at(const char *path, const char *value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_str_idx(ehc_metric_index_at(path), value);
  ehc_unlock();
  return rc;
}

ehc_status_t ehc_tel_set_bool_at(const char *path, int value)
{
  ehc_status_t rc;
  ehc_lock();
  rc = set_bool_idx(ehc_metric_index_at(path), value);
  ehc_unlock();
  return rc;
}

static int leaf_ready(size_t idx, double *num, int *boolean, const char **str)
{
  const ehc_metric_t *m = &g_ehc.user.metrics[idx];
  ehc_metric_state_t *st = &g_ehc.metrics[idx];
  if (m->kind == EHC_VAL_NUM) {
    if (m->get_num && m->get_num(num)) {
      return 1;
    }
    if (st->filled) {
      *num = st->num;
      return 1;
    }
    if (m->send_when_stale) {
      *num = 0;
      return 1;
    }
    return 0;
  }
  if (m->kind == EHC_VAL_BOOL) {
    if (m->get_bool && m->get_bool(boolean)) {
      return 1;
    }
    if (st->filled) {
      *boolean = st->boolean;
      return 1;
    }
    if (m->send_when_stale) {
      *boolean = 0;
      return 1;
    }
    return 0;
  }
  if (m->kind == EHC_VAL_STR) {
    static char tmp[EHC_TEL_STR_MAX];
    if (m->get_str && m->get_str(tmp, sizeof(tmp))) {
      *str = tmp;
      return 1;
    }
    if (st->filled && st->str_slot != 0xFF) {
      *str = g_ehc.str_pool[st->str_slot];
      return 1;
    }
    if (m->send_when_stale) {
      *str = "";
      return 1;
    }
    return 0;
  }
  return 0;
}

static int object_has_content(const char *obj_name);

static int child_of(size_t idx, const char *parent)
{
  const char *p = g_ehc.user.metrics[idx].parent;
  if (!parent || !parent[0]) {
    return !p || !p[0];
  }
  return p && names_equal(p, parent);
}

static int object_has_content(const char *obj_name)
{
  size_t i;
  for (i = 0; i < g_ehc.user.metric_count; i++) {
    if (!child_of(i, obj_name)) {
      continue;
    }
    if (g_ehc.user.metrics[i].kind == EHC_VAL_OBJECT) {
      if (object_has_content(g_ehc.user.metrics[i].name)) {
        return 1;
      }
    } else {
      double n = 0;
      int b = 0;
      const char *s = NULL;
      if (leaf_ready(i, &n, &b, &s)) {
        return 1;
      }
    }
  }
  return 0;
}

static int emit_children(eh_values_writer_t *w, const char *parent, int depth);

static int emit_object_into(eh_values_writer_t *parent_w, const char *name, int depth)
{
  eh_values_writer_t child;
  eh_status_t rc;
  if (depth >= EHC_MAX_NESTING_DEPTH) {
    ehc_logf(EHC_LOG_ERROR, "nesting overflow at '%s'", name);
    return 0;
  }
  if (!object_has_content(name)) {
    return 1;
  }
  rc = eh_values_begin(&child, g_ehc.scratch[depth], EHC_OBJECT_SCRATCH_BYTES);
  if (rc != EH_OK) {
    return 0;
  }
  if (!emit_children(&child, name, depth + 1)) {
    return 0;
  }
  if (eh_values_end(&child) != EH_OK) {
    ehc_logf(EHC_LOG_ERROR, "scratch overflow at '%s'", name);
    return 0;
  }
  if (eh_values_add_raw(parent_w, name, g_ehc.scratch[depth]) != EH_OK) {
    ehc_logf(EHC_LOG_ERROR, "add_raw failed for '%s'", name);
    return 0;
  }
  return 1;
}

static int emit_children(eh_values_writer_t *w, const char *parent, int depth)
{
  size_t i;
  for (i = 0; i < g_ehc.user.metric_count; i++) {
    const ehc_metric_t *m = &g_ehc.user.metrics[i];
    double n = 0;
    int b = 0;
    const char *s = NULL;
    if (!child_of(i, parent)) {
      continue;
    }
    if (m->kind == EHC_VAL_OBJECT) {
      if (!emit_object_into(w, m->name, depth)) {
        return 0;
      }
      continue;
    }
    if (!leaf_ready(i, &n, &b, &s)) {
      continue;
    }
    if (m->kind == EHC_VAL_NUM) {
      if (eh_values_add_num(w, m->name, n) != EH_OK) {
        return 0;
      }
    } else if (m->kind == EHC_VAL_BOOL) {
      if (eh_values_add_bool(w, m->name, b) != EH_OK) {
        return 0;
      }
    } else if (eh_values_add_str(w, m->name, s) != EH_OK) {
      return 0;
    }
  }
  return 1;
}

int ehc_telemetry_collect(eh_values_writer_t *writer)
{
  return emit_children(writer, NULL, 0) ? 1 : 0;
}

void ehc_tel_reset(void)
{
  size_t i;
  if (!g_ehc.used) {
    return;
  }
  ehc_lock();
  for (i = 0; i < g_ehc.user.metric_count; i++) {
    g_ehc.metrics[i].filled = 0;
  }
  for (i = 0; i < EHC_TEL_STR_SLOTS; i++) {
    g_ehc.str_pool[i][0] = '\0';
  }
  ehc_unlock();
}
