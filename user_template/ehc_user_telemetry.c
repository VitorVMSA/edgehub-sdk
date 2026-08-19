#include "ehc_user.h"

static int get_uptime(double *out)
{
  if (out) {
    *out = 0;
  }
  return 1;
}

const ehc_metric_t ehc_user_metrics[] = {
  EHC_METRIC_NUM("temperature"),
  EHC_METRIC_OBJECT("motor"),
  EHC_METRIC_NUM_IN("motor", "rpm"),
  EHC_METRIC_BOOL_IN("motor", "running"),
  EHC_METRIC_NUM_GET("uptimeSec", get_uptime),
};

const size_t ehc_user_metrics_count = sizeof(ehc_user_metrics) / sizeof(ehc_user_metrics[0]);
