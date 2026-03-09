/*
 * MCU Malloc Tracker - MTV1 Streamer Configuration
 *
 * Control MTV1 snapshot stream behavior.
 * No malloc, deterministic.
 */

#ifndef MTV1_CONFIG_H
#define MTV1_CONFIG_H

/* Enable MTV1 streamer (requires MT_ENABLE_SNAPSHOT) */
#define MTV1_ENABLE 1

/* Maximum payload per frame (bytes) — adjusted for MTU */
#define MTV1_TX_MAX_PAYLOAD 512

/* Frame types */
#define MTV1_TYPE_SNAPSHOT_MTS1   1
#define MTV1_TYPE_TELEMETRY_TEXT  2
#define MTV1_TYPE_MARK_TEXT       3
#define MTV1_TYPE_END             4

#endif /* MTV1_CONFIG_H */
