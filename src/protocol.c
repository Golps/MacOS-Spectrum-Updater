#include "spectrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail(sp_session *s,const char *m){snprintf(s->error,sizeof s->error,"%s",m);return -1;}
int sp_request(sp_session *s,uint8_t type,uint8_t request,uint16_t value,uint16_t index,void *data,uint16_t len){
    ssize_t got=s->io.control(s->io.context,type,request,value,index,data,len,1500);
    if(got!=(ssize_t)len){snprintf(s->error,sizeof s->error,"USB request %02X: expected %u bytes, got %zd. Session aborted.",request,len,got);return -1;}return 0;
}
static int send(sp_session *s,const uint8_t *data,uint16_t n){return sp_request(s,0x40,0xb2,0,0,(void *)data,n);}
static int repeat(sp_session *s,const uint8_t *data,uint16_t n){return sp_request(s,0x40,0xb7,0,0,(void *)data,n);}
static int gpio_read(sp_session *s,uint8_t reg,uint8_t *v){return sp_request(s,0xc0,0xf6,reg,0,v,1);}
static int gpio_write(sp_session *s,uint8_t reg,uint8_t v){uint16_t sign=v<0x80?v:(uint16_t)(0xff00|v);return sp_request(s,0x40,0xf6,reg,sign,NULL,0);}
static int debug_byte(sp_session *s,uint8_t cmd){const uint8_t b[]={0xb2,cmd};return send(s,b,sizeof b);}
static int debug_enter(sp_session *s){const uint8_t b[]={0xb2,'S','E','R','D','B'};s->debug_attempted=true;return send(s,b,sizeof b);}
/* Vendor 401A90 writes BOTH banks before either mode sequence. */
static int step_enter(sp_session *s){
    const uint8_t first[]={0xb2,0x10,0xc0,0xc1,0x53},second[]={0xb2,0x10,0x1f,0xc1,0x53};
    s->step_attempted=true;
    if(send(s,first,sizeof first))return -1;
    return send(s,second,sizeof second);
}
/* Vendor 401C20: full initial debug mode/address setup, distinct from tail. */
static int initial_debug_mode(sp_session *s){
    const uint8_t commands[]={0x80,0x82,0x84,0x51,0x7f,0x37,0x61};
    for(unsigned i=0;i<sizeof commands;i++)if(debug_byte(s,commands[i]))return -1;
    const uint8_t address[]={0xb2,0x10,0,0,0};
    if(send(s,address,sizeof address)||debug_byte(s,0x35))return -1;
    return debug_byte(s,0x71);
}
static int reg_read(sp_session *s,uint16_t reg,uint8_t *v){uint8_t b[]={0xb2,0x10,(uint8_t)(reg>>8),(uint8_t)reg};if(repeat(s,b,sizeof b))return -1;return sp_request(s,0xc0,0xa3,0,0xb3,v,1);}
static int reg_write(sp_session *s,uint16_t reg,uint8_t v){uint8_t b[]={0xb2,0x10,(uint8_t)(reg>>8),(uint8_t)reg,v};return send(s,b,sizeof b);}
static int reg4_read(sp_session *s,uint8_t *v){uint8_t b[]={0xb2,0x10,0,0x10,0x0f,0xd7};if(repeat(s,b,sizeof b))return -1;return sp_request(s,0xc0,0xa3,0,0xb3,v,1);}
static int reg4_write(sp_session *s,uint8_t v){uint8_t b[]={0xb2,0x10,0,0x10,0x0f,0xd7,v};return send(s,b,sizeof b);}
static int spi_end(sp_session *s){const uint8_t b[]={0x92,0x12};return send(s,b,sizeof b);}
static int finish_spi(sp_session *s,int rc){char error[256];snprintf(error,sizeof error,"%s",s->error);int end=spi_end(s);if(rc){snprintf(s->error,sizeof s->error,"%s",error);return -1;}return end;}
static int status(sp_session *s,uint8_t *v){const uint8_t a[]={0x92,0x10,0x05},b[]={0x92,0x11};int rc=send(s,a,sizeof a);if(!rc)rc=send(s,b,sizeof b);if(!rc)rc=sp_request(s,0xc0,0xa3,0,0x93,v,1);return finish_spi(s,rc);}
static int identify(sp_session *s){const uint8_t a[]={0x92,0x10,0x9f},b[]={0x92,0x11};int rc=send(s,a,sizeof a);if(!rc)rc=send(s,b,sizeof b);if(!rc)rc=sp_request(s,0xc0,0xa3,0,0x93,s->jedec,3);if(finish_spi(s,rc))return -1;
    /* Documented Macronix profiles: MX25L6406E PM1577 rev1.9 (C22017)
       and MX25L12835F PM1795 rev1.7, tables 5/6 (C22018).
       Both use 24-bit addresses, D8=64 KiB erase, 02=256-byte program.
       Capacity does not widen the permitted ES07D03 FW2 write interval.
       RDID identifies a compatible profile, not an exact package suffix. */
    s->geometry_known=false;s->flash_capacity=0;
    if(s->jedec[0]==0xc2&&s->jedec[1]==0x20){
        if(s->jedec[2]==0x17)s->flash_capacity=0x800000u;
        if(s->jedec[2]==0x18)s->flash_capacity=0x1000000u;
        s->geometry_known=s->flash_capacity>=SP_LIMIT;
    }
    if(!s->geometry_known){snprintf(s->error,sizeof s->error,"JEDEC %02X%02X%02X lacks a supported geometry profile. Nothing erased.",s->jedec[0],s->jedec[1],s->jedec[2]);return -1;}return 0;
}
static int wait_ready(sp_session *s,uint32_t limit,uint32_t delay){
    uint64_t start=s->io.clock_ms(s->io.context);unsigned count=0;
    do{uint8_t v;if(status(s,&v))return -1;if(v==0xff)return fail(s,"SPI status 0xFF: flash did not respond.");if(!(v&3))return 0;s->io.sleep_ms(s->io.context,delay);if(++count>limit/delay+2)break;}while(s->io.clock_ms(s->io.context)-start<limit);
    return fail(s,"SPI busy/write-enable state did not clear before timeout.");
}
static int settle_for_cleanup(sp_session *s){
    uint64_t started=s->io.clock_ms(s->io.context);unsigned polls=0;
    do{uint8_t v;if(status(s,&v))return -1;if(v==0xff)return fail(s,"SPI did not respond during cleanup.");if(!(v&1)){const uint8_t disable[]={0x92,0x10,0x04};if(finish_spi(s,send(s,disable,sizeof disable)))return -1;if(status(s,&v))return -1;return (v&3)?fail(s,"SPI write latch remained set during cleanup."):0;}s->io.sleep_ms(s->io.context,50);polls++;}while(polls<242&&s->io.clock_ms(s->io.context)-started<12000);
    return fail(s,"SPI remained busy during session cleanup.");
}
static int wren(sp_session *s){const uint8_t b[]={0x92,0x10,0x06};if(finish_spi(s,send(s,b,sizeof b)))return -1;uint8_t v;if(status(s,&v))return -1;if((v&3)!=2)return fail(s,"SPI write-enable latch was not set.");return 0;}
static int read_page(sp_session *s,uint32_t address,uint8_t *out){
    if(address>SP_LIMIT-SP_PAGE)return fail(s,"SPI read exceeds supported firmware layout.");
    const uint8_t a[]={0x92,0x10,0x03,(uint8_t)(address>>16),(uint8_t)(address>>8),(uint8_t)address},b[]={0x92,0x11};
    int rc=send(s,a,sizeof a);if(!rc)rc=send(s,b,sizeof b);
    for(unsigned i=0;!rc&&i<8;i++)rc=sp_request(s,0xc0,i==0?0xa7:(i==7?0xa9:0xa8),0,0x93,out+32*i,32);
    return finish_spi(s,rc);
}
int sp_read(sp_session *s,uint32_t address,uint8_t *out,size_t n,sp_progress progress,void *context){
    if(!n||address>=SP_LIMIT||n>SP_LIMIT-address)return fail(s,"Invalid SPI read interval.");
    uint8_t page[SP_PAGE];for(size_t i=0;i<n;i+=SP_PAGE){size_t take=n-i<SP_PAGE?n-i:SP_PAGE;if(read_page(s,address+(uint32_t)i,page))return -1;memcpy(out+i,page,take);if(progress&&((i%SP_BLOCK)==0||i+take==n))progress(context,"Reading",(uint32_t)(i+take),(uint32_t)n);}return 0;
}
int sp_begin(sp_session *s){
    s->error[0]=0;for(unsigned i=0;i<2;i++){uint8_t reg=(uint8_t)(0xa0+i),v;if(gpio_read(s,reg,&v))return -1;s->pins[i]=v;s->pins_saved[i]=true;if(gpio_write(s,reg,(uint8_t)(v|4))||gpio_read(s,reg,&v))return -1;if(v!=(uint8_t)(s->pins[i]|4))return fail(s,"Hub GPIO did not read back correctly.");}
    s->io.sleep_ms(s->io.context,200);const uint8_t enter[]={0x92,'M','S','T','A','R'};s->isp_attempted=true;if(send(s,enter,sizeof enter))return -1;s->io.sleep_ms(s->io.context,50);
    uint8_t page[SP_PAGE];if(read_page(s,0xff00,page))return -1;if(memcmp(page,"MST9U4",6))return fail(s,"Connected scaler lacks required MST9U4 signature.");return identify(s);
}
int sp_finish(sp_session *s){
    bool write_session=s->flash_ready;
    s->flash_ready=false;
    char saved[256];snprintf(saved,sizeof saved,"%s",s->error);unsigned failures=0;
    if(s->isp_attempted&&spi_end(s))failures++;
    if(write_session&&settle_for_cleanup(s))failures++;
    if(s->reg4_saved&&reg4_write(s,s->reg4))failures++;
    if(s->reg426_saved&&reg_write(s,0x0426,s->reg426))failures++;
    /* Restore bridge pins before ISP exit can disconnect/re-enumerate USB. */
    for(unsigned i=0;i<2;i++)if(s->pins_saved[i]){uint8_t v;if(gpio_write(s,(uint8_t)(0xa0+i),s->pins[i])||gpio_read(s,(uint8_t)(0xa0+i),&v)||v!=s->pins[i])failures++;}
    if(s->step_attempted){const uint8_t b[]={0xb2,0x10,0xc0,0xc1,0xff};if(send(s,b,sizeof b))failures++;}
    if(s->debug_attempted&&debug_byte(s,0x45))failures++;
    if(s->isp_attempted){const uint8_t b[]={0x92,0x24};if(send(s,b,sizeof b))failures++;}
    s->cleanup_errors=failures;
    if(saved[0])snprintf(s->error,sizeof s->error,"%s",saved);else if(failures)snprintf(s->error,sizeof s->error,"%u session cleanup requests failed. Power-cycle the monitor.",failures);
    return failures?-1:0;
}
/* Vendor v0.0.0.5: 0x404E13–0x404EAE and 0x401F60–0x4021BE.
 * 0426 is a setup command, not the final flash-enable proof. Its reread is
 * diagnostic; the vendor compares only the later four-byte flash-access reg.
 * Keep strict transport checks and do not guess that 0426 is a stable latch. */
static int flash_setup_attempt(sp_session *s){
    if(debug_enter(s)||step_enter(s)||initial_debug_mode(s))return -1;
    uint8_t a,b,v;
    if(reg_read(s,0x042a,&a)||reg_read(s,0x0428,&b)||reg_read(s,0x0426,&v))return -1;
    if(!s->reg426_saved){s->reg426=v;s->reg426_saved=true;}
    uint8_t requested=(uint8_t)(v|1);
    if(reg_write(s,0x0426,requested)||reg_read(s,0x042a,&a)||reg_read(s,0x0428,&b)||reg_read(s,0x0426,&v))return -1;
    fprintf(stdout,"Scaler setup: 0426 requested=%02X observed=%02X; 042A=%02X 0428=%02X.\n",requested,v,a,b);fflush(stdout);
    if(debug_byte(s,0x34)||debug_byte(s,0x45)||debug_enter(s)||step_enter(s))return -1;
    /* Vendor 401F60 has its own tail mode; send each command once. */
    const uint8_t cmds[]={0x80,0x82,0x85,0x53,0x7f,0x35,0x71};
    for(unsigned i=0;i<sizeof cmds;i++)if(debug_byte(s,cmds[i]))return -1;
    if(reg4_read(s,&v))return -1;
    if(!s->reg4_saved){s->reg4=v;s->reg4_saved=true;}
    requested=(uint8_t)(v|0x80);
    if(reg4_write(s,requested)||reg4_read(s,&v))return -1;
    fprintf(stdout,"Flash-access enable: requested=%02X observed=%02X.\n",requested,v);fflush(stdout);
    if(v!=requested){snprintf(s->error,sizeof s->error,"Flash-access enable mismatch: requested %02X, observed %02X. Nothing erased.",requested,v);return 1;}
    return 0;
}
int sp_flash_setup(sp_session *s){
    s->flash_ready=false;
    if(!s->geometry_known)return fail(s,"No validated flash geometry profile.");
    int result=-1;
    /* Vendor retries the setup at most five times. Retry only a successfully
       transferred final-register mismatch, never a failed/short USB command. */
    for(unsigned attempt=0;attempt<5;attempt++){
        result=flash_setup_attempt(s);
        if(result<=0)break;
    }
    if(result)return -1;
    s->error[0]=0;
    if(wait_ready(s,1000,10))return -1;uint8_t st;if(status(s,&st))return -1;
    /* No speculative status-register writes. */
    if(st!=0)return fail(s,"Flash status/protection is nonzero; unsupported flash variant. Nothing erased.");
    s->flash_ready=true;return 0;
}
static int erase(sp_session *s,uint32_t a){
    if(a<SP_BASE||a>SP_LIMIT-SP_BLOCK||a%SP_BLOCK)return fail(s,"Rejected erase address.");
    if(wait_ready(s,1000,10)||wren(s))return -1;uint8_t b[]={0x92,0x10,0xd8,(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};if(finish_spi(s,send(s,b,sizeof b)))return -1;return wait_ready(s,12000,50);
}
static int program_page(sp_session *s,uint32_t a,const uint8_t *d,size_t n){
    if(a<SP_BASE||a>=SP_LIMIT||!n||n>SP_PAGE||n>SP_LIMIT-a||a%SP_PAGE+n>SP_PAGE)return fail(s,"Rejected page-program address/length.");
    if(wait_ready(s,1000,2)||wren(s))return -1;uint8_t b[]={0x92,0x10,0x02,(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};int rc=repeat(s,b,sizeof b);
    for(size_t at=0;!rc&&at<n;at+=32){size_t take=n-at<32?n-at:32;rc=sp_request(s,0x40,at+take==n?0xb9:0xb8,0,0,(void *)(d+at),(uint16_t)take);}if(finish_spi(s,rc))return -1;return wait_ready(s,1000,2);
}
static int program_region(sp_session *s,const uint8_t *d,size_t n,sp_progress progress,void *context){
    if(!s->flash_ready)return fail(s,"Flash access setup has not succeeded.");
    if(!s->geometry_known)return fail(s,"No validated flash geometry profile.");
    uint32_t span=(uint32_t)((n+SP_BLOCK-1)/SP_BLOCK*SP_BLOCK);
    if(span>SP_LIMIT-SP_BASE)return fail(s,"Rejected aligned erase span.");
    for(uint32_t at=0;at<span;at+=SP_BLOCK){if(erase(s,SP_BASE+at))return -1;if(progress)progress(context,"Erasing",at+SP_BLOCK,span);}
    uint8_t page[SP_PAGE];for(uint32_t at=0;at<span;at+=SP_PAGE){if(read_page(s,SP_BASE+at,page))return -1;for(unsigned j=0;j<SP_PAGE;j++)if(page[j]!=0xff)return fail(s,"Full-span blank check failed; no programming attempted.");if(progress&&at%SP_BLOCK==0)progress(context,"Checking erase",at+SP_PAGE,span);}
    for(size_t at=0;at<n;at+=SP_PAGE){size_t take=n-at<SP_PAGE?n-at:SP_PAGE;if(program_page(s,SP_BASE+(uint32_t)at,d+at,take))return -1;if(progress&&(at%SP_BLOCK==0||at+take==n))progress(context,"Programming",(uint32_t)(at+take),(uint32_t)n);}
    for(uint32_t at=0;at<span;at+=SP_PAGE){if(read_page(s,SP_BASE+at,page))return -1;for(unsigned j=0;j<SP_PAGE;j++){size_t off=(size_t)at+j;uint8_t expected=off<n?d[off]:0xff;if(page[j]!=expected)return fail(s,"Post-program full-span verification failed.");}if(progress&&(at%SP_BLOCK==0||at+SP_PAGE==span))progress(context,"Verifying",at+SP_PAGE,span);}return 0;
}
int sp_program(sp_session *s,const uint8_t *d,size_t n,const uint8_t *prior,size_t prior_n,sp_progress progress,void *context){
    sp_image info;if(sp_validate_image(d,n,&info,s->error,sizeof s->error)||sp_validate_backup(prior,prior_n,&info,s->error,sizeof s->error))return -1;
    size_t span=(n+SP_BLOCK-1)/SP_BLOCK*SP_BLOCK;uint8_t *target=malloc(span);if(!target)return fail(s,"Out of memory preparing preserved update tail.");
    memcpy(target,prior,span);memcpy(target,d,n);int rc=program_region(s,target,span,progress,context);free(target);return rc;
}
int sp_restore(sp_session *s,const uint8_t *d,size_t n,sp_progress progress,void *context){sp_image info;if(sp_validate_backup(d,n,&info,s->error,sizeof s->error))return -1;return program_region(s,d,n,progress,context);}
