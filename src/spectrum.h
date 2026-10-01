#ifndef SPECTRUM_H
#define SPECTRUM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#define SP_BASE 0x400000u
#define SP_LIMIT 0x800000u
#define SP_BLOCK 0x10000u
#define SP_PAGE 256u
typedef struct {
    void *context;
    ssize_t (*control)(void *, uint8_t, uint8_t, uint16_t, uint16_t, void *, uint16_t, uint32_t);
    uint64_t (*clock_ms)(void *);
    void (*sleep_ms)(void *, uint32_t);
} sp_transport;
typedef struct {
    sp_transport io;
    char error[256];
    bool pins_saved[2], isp_attempted, debug_attempted, step_attempted;
    bool reg426_saved, reg4_saved;
    bool flash_ready;
    uint8_t jedec[3];
    bool geometry_known;
    uint32_t flash_capacity; /* physical capacity; SP_LIMIT remains the FW2 bound */
    uint8_t pins[2], reg426, reg4;
    unsigned cleanup_errors;
} sp_session;
typedef struct { size_t bytes, expanded_bytes; unsigned components; char sha256[65]; const char *stock; } sp_image;
typedef void (*sp_progress)(void *, const char *, uint32_t, uint32_t);
int sp_validate_image(const uint8_t *, size_t, sp_image *, char *, size_t);
int sp_validate_backup(const uint8_t *, size_t, sp_image *, char *, size_t);
uint32_t sp_crc32(const uint8_t *, size_t);
uint16_t sp_crc16(const uint8_t *, size_t);
void sp_sha256(const uint8_t *, size_t, char out[65]);
int sp_request(sp_session *, uint8_t, uint8_t, uint16_t, uint16_t, void *, uint16_t);
int sp_begin(sp_session *);
int sp_finish(sp_session *);
int sp_read(sp_session *, uint32_t, uint8_t *, size_t, sp_progress, void *);
int sp_flash_setup(sp_session *);
/* Caller must already have durably backed up and verified the complete FW2.
 * This engine accepts only the FW2 interval and verifies all erased bytes.
 */
int sp_program(sp_session *, const uint8_t *, size_t, const uint8_t *, size_t, sp_progress, void *);
int sp_restore(sp_session *, const uint8_t *, size_t, sp_progress, void *);
int sp_save_exclusive(const char *, const uint8_t *, size_t, char *, size_t);
int sp_load(const char *, uint8_t **, size_t *, size_t, char *, size_t);
#endif
