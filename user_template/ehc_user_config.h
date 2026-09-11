#ifndef EHC_USER_CONFIG_H
#define EHC_USER_CONFIG_H

#define EHC_MAX_NESTING_DEPTH     2
#define EHC_OBJECT_SCRATCH_BYTES  256
#define EHC_MAX_METRICS           64
#define EHC_TEL_STR_SLOTS         8
#define EHC_TEL_STR_MAX           32
#define EHC_MAX_COMMANDS_TABLE    32
#define EHC_CMD_MESSAGE_MAX       128
#define EHC_ARGS_MAX_DEPTH        2
#define EHC_MAX_CHECKIN_KEYS      24
#define EHC_CHECKIN_INTERVAL_MS   43200000UL
#define EHC_TELEMETRY_INTERVAL_MS 10000UL
#define EHC_TELEMETRY_BATCH_MAX   1 /* live sensors; replay histórico: 8 + ehc_tel_enqueue/flush */
#define EHC_NOT_READY_LOG_MS      60000UL
/* ESP32 com pool grande: #define EHC_CTX_ATTR __attribute__((section(".ext_ram.bss"))) */

#endif
