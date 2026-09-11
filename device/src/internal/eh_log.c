#include "eh_log.h"

void eh_logf(eh_client_t *cli, eh_log_level_t level, const char *msg) {
  if (!cli || !msg) return;
  if (!cli->cfg.enable_logs) return;
  if ((int)level > (int)cli->cfg.log_level) return;
  if (cli->cb.log_fn) cli->cb.log_fn(cli->cb.user_ctx, level, msg);
}
