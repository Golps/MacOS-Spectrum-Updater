#ifndef SP_MACOS_H
#define SP_MACOS_H
#include "spectrum.h"
#include <IOKit/usb/IOUSBLib.h>
typedef struct { IOUSBDeviceInterface500 **usb; uint64_t registry; } sp_macos;
int sp_macos_list(void);
int sp_macos_open(uint64_t,sp_macos *,sp_transport *,char *,size_t);
int sp_macos_open_paired_hub(uint64_t,sp_macos *,sp_transport *,char *,size_t);
uint32_t sp_macos_keep_awake(char *,size_t);
void sp_macos_release_awake(uint32_t);
void sp_macos_close(sp_macos *);
#endif
