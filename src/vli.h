#ifndef SP_VLI_H
#define SP_VLI_H
#include "spectrum.h"
#define SP_VLI_SECTOR 0x1000u
#define SP_VLI_PD_BASE 0x20000u
#define SP_VLI_PD_BYTES 0x8000u
typedef enum { SP_VLI_HUB=1, SP_VLI_PD=2 } sp_vli_kind;
typedef struct {
    sp_vli_kind kind;
    char sha256[65];
    const char *version;
    uint32_t payload_offset, payload_bytes;
} sp_vli_image;
typedef struct {
    sp_transport io;
    char error[256];
    uint8_t jedec[3], registers[4];
    bool register_saved[4], ready;
    uint32_t capacity;
    unsigned cleanup_errors;
} sp_vli_session;
int sp_vli_validate(const uint8_t *, size_t, sp_vli_image *, char *, size_t);
int sp_vli_begin(sp_vli_session *);
int sp_vli_finish(sp_vli_session *);
int sp_vli_read(sp_vli_session *, uint32_t, uint8_t *, size_t, sp_progress, void *);
/* Pure planner. Copies the complete shared flash, preserving every byte outside
 * the selected component. Refuses unknown headers or overlapping partitions. */
int sp_vli_plan(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t *, sp_vli_image *, char *, size_t);
/* A verified, durable complete shared-SPI backup is required before calling.
 * Each changed sector is rechecked before erase; every byte is checked after. */
int sp_vli_program(sp_vli_session *, const uint8_t *, const uint8_t *, size_t, sp_progress, void *);
#endif
