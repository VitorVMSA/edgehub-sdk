#include "edgehub_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int checkin_calls;
    int executed_calls;
    int on_command_calls;
    char last_telemetry_body[8192];
    int force_429;
    int force_offline;
    uint32_t retry_after_sec;
} simulator_ctx_t;

static int write_response(
    uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
    int status, const char *json)
{
    size_t len = strlen(json);
    if (!resp || resp_cap <= len) {
        return 0;
    }
    memcpy(resp, json, len);
    resp[len] = '\0';
    if (resp_len) {
        *resp_len = len;
    }
    if (http_status) {
        *http_status = status;
    }
    return 1;
}

static int stub_http_post(
    void *user_ctx, const char *url, const char *headers,
    const uint8_t *body, size_t body_len,
    uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status)
{
    simulator_ctx_t *ctx = (simulator_ctx_t *)user_ctx;
    (void)headers;

    if (strstr(url, "/device/telemetry")) {
        if (ctx->force_offline) {
            return 0;
        }
        if (body && body_len < sizeof(ctx->last_telemetry_body)) {
            memcpy(ctx->last_telemetry_body, body, body_len);
            ctx->last_telemetry_body[body_len] = '\0';
        } else {
            ctx->last_telemetry_body[0] = '\0';
        }
        if (ctx->force_429) {
            return write_response(resp, resp_cap, resp_len, http_status, 429, "{}");
        }
        return write_response(
            resp, resp_cap, resp_len, http_status, 201,
            "{\"pendingCommands\":[{\"executionId\":\"exec-force-checkin\","
            "\"commandKey\":\"device.force_checkin\",\"payload\":{}}]}");
    }
    if (strstr(url, "/device/checkin")) {
        ctx->checkin_calls++;
        return write_response(resp, resp_cap, resp_len, http_status, 200, "{}");
    }
    if (strstr(url, "/command-executions/")) {
        ctx->executed_calls++;
        return write_response(resp, resp_cap, resp_len, http_status, 204, "");
    }
    return write_response(resp, resp_cap, resp_len, http_status, 200, "{}");
}

static int stub_http_post_ex(
    void *user_ctx, const char *url, const char *headers,
    const uint8_t *body, size_t body_len,
    uint8_t *resp, size_t resp_cap, size_t *resp_len, int *http_status,
    uint32_t *retry_after_sec)
{
    simulator_ctx_t *ctx = (simulator_ctx_t *)user_ctx;
    int ok = stub_http_post(
        user_ctx, url, headers, body, body_len, resp, resp_cap, resp_len, http_status);
    if (retry_after_sec) {
        *retry_after_sec = (ctx->force_429 && http_status && *http_status == 429)
            ? ctx->retry_after_sec
            : 0;
    }
    return ok;
}

static int stub_telemetry_collect(void *user_ctx, eh_values_writer_t *writer)
{
    (void)user_ctx;
    if (eh_values_add_str(writer, "type", "telemetry") != EH_OK) {
        return 0;
    }
    return 1;
}

static int stub_on_command(
    void *user_ctx, const char *execution_id,
    const char *command_key, const char *payload_json)
{
    simulator_ctx_t *ctx = (simulator_ctx_t *)user_ctx;
    (void)execution_id;
    (void)command_key;
    (void)payload_json;
    ctx->on_command_calls++;
    return 1;
}

static void fill_cfg(eh_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->base_url, "https://example.edgehub.test", sizeof(cfg->base_url) - 1);
    strncpy(cfg->device_uid, "SIM-001", sizeof(cfg->device_uid) - 1);
    cfg->enable_checkin = 1;
    cfg->enable_telemetry = 1;
    cfg->enable_commands = 1;
    cfg->enable_auto_confirm_commands = 1;
    cfg->telemetry_manual_only = 1;
}

int main(void)
{
    eh_config_t cfg;
    eh_callbacks_t cb = {0};
    eh_client_t cli;
    simulator_ctx_t ctx = {0};
    eh_client_status_t st;

    if (eh_lib_abi_check() != EH_OK) {
        fprintf(stderr, "ABI check failed\n");
        return 1;
    }

    printf("edgehub-device-sdk %s ABI %u\n", eh_lib_version_string(), eh_lib_abi_version());

    fill_cfg(&cfg);
    cb.http_post = stub_http_post;
    cb.telemetry_collect = stub_telemetry_collect;
    cb.on_command = stub_on_command;
    cb.user_ctx = &ctx;

    if (eh_client_init(&cli, &cfg, &cb, 0) != EH_OK) {
        fprintf(stderr, "eh_client_init failed\n");
        return 1;
    }

    if (eh_client_set_secret(&cli, "host-simulator-device-secret", NULL) != EH_OK) {
        fprintf(stderr, "eh_client_set_secret failed\n");
        return 1;
    }

    if (eh_client_force_telemetry(&cli, 100) != EH_OK) {
        fprintf(stderr, "eh_client_force_telemetry failed\n");
        return 1;
    }

    if (ctx.checkin_calls != 1 || ctx.executed_calls != 1 || ctx.on_command_calls != 0) {
        fprintf(
            stderr,
            "force check-in command failed: checkins=%d executed=%d on_command=%d\n",
            ctx.checkin_calls, ctx.executed_calls, ctx.on_command_calls);
        return 1;
    }

    if (!strstr(ctx.last_telemetry_body, "\"ts\":\"1970-01-01T00:00:00.100Z\"")) {
        fprintf(stderr, "auto telemetry ts missing: %s\n", ctx.last_telemetry_body);
        return 1;
    }

    if (eh_client_set_telemetry_ts(&cli, "2020-01-02T03:04:05.006Z") != EH_OK) {
        fprintf(stderr, "eh_client_set_telemetry_ts failed\n");
        return 1;
    }
    ctx.checkin_calls = 0;
    ctx.executed_calls = 0;
    if (eh_client_force_telemetry(&cli, 200) != EH_OK) {
        fprintf(stderr, "eh_client_force_telemetry (setter) failed\n");
        return 1;
    }
    if (!strstr(ctx.last_telemetry_body, "\"ts\":\"2020-01-02T03:04:05.006Z\"")) {
        fprintf(stderr, "setter telemetry ts not honoured: %s\n", ctx.last_telemetry_body);
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    memset(&cb, 0, sizeof(cb));
    fill_cfg(&cfg);
    ctx.force_429 = 1;
    ctx.retry_after_sec = 7;
    cb.http_post_ex = stub_http_post_ex;
    cb.telemetry_collect = stub_telemetry_collect;
    cb.user_ctx = &ctx;
    if (eh_client_init(&cli, &cfg, &cb, 0) != EH_OK) {
        fprintf(stderr, "eh_client_init (429) failed\n");
        return 1;
    }
    if (eh_client_set_secret(&cli, "host-simulator-device-secret", NULL) != EH_OK) {
        fprintf(stderr, "eh_client_set_secret (429) failed\n");
        return 1;
    }
    if (eh_client_force_telemetry(&cli, 100) == EH_OK) {
        fprintf(stderr, "expected HTTP 429 to fail telemetry\n");
        return 1;
    }
    eh_client_get_status(&cli, &st);
    if (st.backoff_until_ms != 7100) {
        fprintf(
            stderr,
            "retry_after backoff mismatch: backoff_until_ms=%llu\n",
            (unsigned long long)st.backoff_until_ms);
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    memset(&cb, 0, sizeof(cb));
    fill_cfg(&cfg);
    cfg.telemetry_batch_max = 2;
    cb.http_post = stub_http_post;
    cb.telemetry_collect = stub_telemetry_collect;
    cb.user_ctx = &ctx;
    if (eh_client_init(&cli, &cfg, &cb, 0) != EH_OK) {
        fprintf(stderr, "eh_client_init (batch) failed\n");
        return 1;
    }
    if (eh_client_set_secret(&cli, "host-simulator-device-secret", NULL) != EH_OK) {
        fprintf(stderr, "eh_client_set_secret (batch) failed\n");
        return 1;
    }
    if (eh_client_enqueue_reading(&cli, "2024-06-01T12:00:00.000Z", 0) != EH_OK ||
        eh_client_enqueue_reading(&cli, "2024-06-01T12:00:01.000Z", 0) != EH_OK) {
        fprintf(stderr, "eh_client_enqueue_reading failed\n");
        return 1;
    }
    if (eh_client_telemetry_queue_count(&cli) != 2) {
        fprintf(stderr, "expected queue count 2, got %u\n", (unsigned)eh_client_telemetry_queue_count(&cli));
        return 1;
    }
    ctx.force_offline = 1;
    if (eh_client_flush_telemetry(&cli, 300) == EH_OK) {
        fprintf(stderr, "expected offline flush to fail\n");
        return 1;
    }
    if (eh_client_telemetry_queue_count(&cli) != 2) {
        fprintf(stderr, "offline flush dropped queue: %u\n", (unsigned)eh_client_telemetry_queue_count(&cli));
        return 1;
    }
    ctx.force_offline = 0;
    if (eh_client_flush_telemetry(&cli, 400) != EH_OK) {
        fprintf(stderr, "eh_client_flush_telemetry (batch) failed\n");
        return 1;
    }
    if (eh_client_telemetry_queue_count(&cli) != 0) {
        fprintf(stderr, "batch flush did not drain queue: %u\n", (unsigned)eh_client_telemetry_queue_count(&cli));
        return 1;
    }
    if (!strstr(ctx.last_telemetry_body,
            "{\"deviceUid\":\"SIM-001\",\"readings\":["
            "{\"ts\":\"2024-06-01T12:00:00.000Z\",\"values\":{\"type\":\"telemetry\"}},"
            "{\"ts\":\"2024-06-01T12:00:01.000Z\",\"values\":{\"type\":\"telemetry\"}}]}")) {
        fprintf(stderr, "batch body mismatch: %s\n", ctx.last_telemetry_body);
        return 1;
    }

    printf("host_simulator OK\n");
    return 0;
}
