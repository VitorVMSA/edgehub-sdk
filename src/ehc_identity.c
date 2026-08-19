#include "ehc_internal.h"

/* Identity resolution lives next to readiness in ehc_core.c. This unit exists
 * so the layout matches the plan (ehc_identity.c) and can grow PKCS#8 helpers. */

int ehc_identity_placeholder(void)
{
  return 0;
}
