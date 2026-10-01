/* Original macOS/portable implementation of VIA's documented USB SPI route.
 * Protocol reference: fwupd plugins/vli (LGPL-2.1-or-later), see PROTOCOL.txt.
 * This file contains independently written code, not copied fwupd routines. */
#include "vli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t be16(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|(p[1]<<8));}
static uint8_t crc8(const uint8_t *p,size_t n){uint8_t c=0;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(uint8_t)((c<<1)^((c&0x80)?7:0));}return c;}
static uint16_t usb_crc(const uint8_t *p,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(uint16_t)((c>>1)^((c&1)?0xa001:0));}return (uint16_t)(c^0xffff);}
static int fail(char *e,size_t z,const char *message){snprintf(e,z,"%s",message);return -1;}
static uint32_t hdr_addr(const uint8_t *h){return be16(h+4)|((uint32_t)h[12]<<16);}
static uint32_t hdr_size(const uint8_t *h){return be16(h+6)|((uint32_t)h[13]<<16);}
static bool valid_header(const uint8_t *h){return be16(h)==0x0518&&crc8(h,31)==h[31]&&be16(h+10)==0&&h[16]==0&&hdr_addr(h)==0x2000&&hdr_size(h)>0&&hdr_size(h)<=0xf000&&h[28]==0xff&&(h[29]==0xff||h[29]==0x80);}
static bool pd_header(const uint8_t *p){return le16(p+0x1007)==0x2109&&le16(p+0x1009)==0x0103&&p[0x1003]==0x0a&&p[0x1004]==0x89&&p[0x1006]==2;}
int sp_vli_validate(const uint8_t *d,size_t n,sp_vli_image *out,char *err,size_t z){
    if(!d||!out)return fail(err,z,"Missing component image.");
    memset(out,0,sizeof *out);sp_sha256(d,n,out->sha256);
    if(n==42556&&!strcmp(out->sha256,"ec6699214c621671449ec941ab4ccd8c413cb79b6e369d99a654c12beb3a3356")){
        if(!valid_header(d)||d[29]!=0xff||d[30]!=2||hdr_size(d)!=0x8630||hdr_addr(d)+hdr_size(d)>n)return fail(err,z,"VL822 firmware header/CRC8 is invalid.");
        out->kind=SP_VLI_HUB;out->version="06A4";out->payload_offset=hdr_addr(d);out->payload_bytes=hdr_size(d);return 0;
    }
    if(n==SP_VLI_PD_BYTES&&(!strcmp(out->sha256,"62879652a96b8098cb240c4a4290828304afc6709b3948066ccb0d1de49e0fe1")||!strcmp(out->sha256,"3e3c4c2224c676b1e4971280f5678f65b37ddb8609c3afdfa1df6f72e14b44b1"))){
        if(!pd_header(d)||usb_crc(d,n-2)!=le16(d+n-2))return fail(err,z,"VL103 identity/CRC16 is invalid.");
        out->kind=SP_VLI_PD;out->version=d[0x1005]==0x19?"0A.89.19.02":"0A.89.17.02";out->payload_bytes=(uint32_t)n;return 0;
    }
    return fail(err,z,"Unknown USB firmware. Only exact cataloged vendor hub/PD binaries are accepted.");
}
int sp_vli_plan(const uint8_t *saved,size_t capacity,const uint8_t *file,size_t n,uint8_t *planned,sp_vli_image *info,char *err,size_t z){
    if(!saved||!planned||saved==planned||capacity<0x40000||capacity>0x100000||(capacity&(capacity-1)))return fail(err,z,"Unsupported shared-SPI backup geometry.");
    if(sp_vli_validate(file,n,info,err,z))return -1;
    const uint8_t *root=saved;
    if(!valid_header(root))return fail(err,z,"Factory hub header is invalid. No blind recovery or erase is permitted.");
    if(!pd_header(saved+SP_VLI_PD_BASE)||usb_crc(saved+SP_VLI_PD_BASE,SP_VLI_PD_BYTES-2)!=le16(saved+SP_VLI_PD_BASE+SP_VLI_PD_BYTES-2))return fail(err,z,"Installed VL103 image/CRC is not a supported shared-SPI layout. Nothing erased.");
    /* Legacy PD area must not masquerade as a second active partition. */
    if(le16(saved+0x14007)==0x2109)return fail(err,z,"Ambiguous legacy and current PD layouts. Nothing erased.");
    if(root[29]==0x80){const uint8_t *h=saved+0x1000;uint32_t a=hdr_addr(h),b=hdr_size(h);
        if(be16(h)!=0x0518||crc8(h,31)!=h[31]||h[28]!=0||h[29]!=0xff||!b||b>0xf000||a<0x2000+((hdr_size(root)+0xfff)&~0xfffu)||a>=SP_VLI_PD_BASE||b>SP_VLI_PD_BASE-a||be16(h+10)||h[16])return fail(err,z,"Installed update hub header is invalid or overlaps another component.");
    }
    memcpy(planned,saved,capacity);
    if(info->kind==SP_VLI_PD){memcpy(planned+SP_VLI_PD_BASE,file,n);return 0;}
    uint32_t start=0x2000+((hdr_size(root)+0xfff)&~0xfffu);
    uint32_t bytes=info->payload_bytes;
    if(start>=SP_VLI_PD_BASE||((bytes+0xfff)&~0xfffu)>SP_VLI_PD_BASE-start)return fail(err,z,"Hub update overlaps PD firmware. Nothing erased.");
    memcpy(planned+start,file+info->payload_offset,bytes);
    uint8_t *h=planned+0x1000;memcpy(h,file,32);h[4]=(uint8_t)(start>>8);h[5]=(uint8_t)start;h[12]=(uint8_t)(start>>16);h[28]=0;h[29]=0xff;h[31]=crc8(h,31);
    /* Preserve all other bytes in header sector, including factory recovery. */
    uint8_t factory[32];memcpy(factory,root,32);factory[29]=0xff;factory[31]=crc8(factory,31);memcpy(planned+0x1800,factory,32);
    planned[29]=0x80;planned[31]=crc8(planned,31);
    return 0;
}
static int req(sp_vli_session *s,uint8_t type,uint8_t cmd,uint16_t value,uint16_t index,void *d,uint16_t n){
    ssize_t actual=s->io.control(s->io.context,type,cmd,value,index,d,n,3000);
    if(actual!=n){snprintf(s->error,sizeof s->error,"VIA USB request %02X failed or returned a short transfer (%zd/%u).",cmd,actual,n);return -1;}return 0;
}
static int reg_read(sp_vli_session *s,uint16_t a,uint8_t *v){return req(s,0xc0,(uint8_t)(a>>8),a&0xff,0,v,1);}
static int reg_write(sp_vli_session *s,uint16_t a,uint8_t v){return req(s,0x40,(uint8_t)(a>>8),a&0xff,v,NULL,0);}
static const uint16_t regs[4]={0xf8a2,0xf832,0xf920,0xf836};
static int status(sp_vli_session *s,uint8_t *v){return req(s,0xc0,0xc1,5,0,v,1);}
static int idle(sp_vli_session *s){uint64_t start=s->io.clock_ms(s->io.context);unsigned consecutive=0;
    for(unsigned i=0;i<1000;i++){uint8_t v;if(status(s,&v))return -1;
        if(v&0xfc)return fail(s->error,sizeof s->error,"SPI status contains protection/configuration bits; update stopped.");
        consecutive=(v&3)?0:consecutive+1;if(consecutive==3)return 0;
        if(s->io.clock_ms(s->io.context)-start>=10000)break;s->io.sleep_ms(s->io.context,10);
    }return fail(s->error,sizeof s->error,"Shared SPI busy timeout. Do not disconnect until cleanup finishes.");
}
int sp_vli_begin(sp_vli_session *s){
    if(!s||!s->io.control||!s->io.clock_ms||!s->io.sleep_ms)return -1;
    uint8_t version,id1,id2,pkg;
    if(reg_read(s,0xf88c,&version)||reg_read(s,0xf88e,&id1)||reg_read(s,0xf88f,&id2)||reg_read(s,0xf651,&pkg))return -1;
    if(version!=0xf0||id1!=0x18||id2!=0x35||((pkg>>1)&7)!=0)return fail(s->error,sizeof s->error,"Hub is not the supported VL822Q7 silicon. Nothing erased.");
    for(unsigned i=0;i<4;i++){if(reg_read(s,regs[i],s->registers+i))return -1;s->register_saved[i]=true;uint8_t value=s->registers[i];value=i==3?(uint8_t)(value|8):(uint8_t)(value&(i==0?0x77:i==1?0xfc:0xf9));if(reg_write(s,regs[i],value))return -1;}
    uint8_t id[4]={0};if(req(s,0xc0,0xc8,0x9f,0,id,4))return -1;memcpy(s->jedec,id,3);
    /* Standard 4-KiB sector/page geometry only. Other SPI variants are rejected. */
    bool maker=(id[0]==0xc2&&id[1]==0x20)||(id[0]==0xef&&(id[1]==0x30||id[1]==0x40));
    if(!maker||id[2]<0x12||id[2]>0x14){snprintf(s->error,sizeof s->error,"Shared-SPI JEDEC %02X%02X%02X has no supported 4-KiB profile. Nothing erased.",id[0],id[1],id[2]);return -1;}
    s->capacity=1u<<id[2];if(idle(s))return -1;s->ready=true;return 0;
}
int sp_vli_finish(sp_vli_session *s){s->ready=false;for(int i=3;i>=0;i--)if(s->register_saved[i]){if(reg_write(s,regs[i],s->registers[i]))s->cleanup_errors++;s->register_saved[i]=false;}return s->cleanup_errors?-1:0;}
static uint16_t addr_value(uint32_t a,uint8_t command){return (uint16_t)(((a>>8)&0xff00)|command);}
static uint16_t addr_index(uint32_t a){return (uint16_t)(((a<<8)&0xff00)|((a>>8)&0xff));}
int sp_vli_read(sp_vli_session *s,uint32_t start,uint8_t *data,size_t n,sp_progress progress,void *ctx){
    if(!s->ready||!data||start>s->capacity||n>s->capacity-start)return fail(s->error,sizeof s->error,"Shared-SPI read outside the identified flash.");
    for(size_t done=0;done<n;){uint16_t amount=(uint16_t)((n-done)>32?32:n-done);uint32_t a=start+(uint32_t)done;if(req(s,0xc0,0xc4,addr_value(a,3),addr_index(a),data+done,amount))return -1;done+=amount;if(progress&&(done%4096==0||done==n))progress(ctx,"Reading",(uint32_t)done,(uint32_t)n);}return 0;
}
static int wren(sp_vli_session *s){if(req(s,0x40,0xd1,6,0,NULL,0))return -1;uint8_t v;if(status(s,&v))return -1;if(v!=2)return fail(s->error,sizeof s->error,"SPI write-enable did not latch cleanly. Nothing further written.");return 0;}
static bool all_ff(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)if(p[i]!=0xff)return false;return true;}
static int write32(sp_vli_session *s,uint32_t a,const uint8_t *p){
    if(all_ff(p,32))return 0;
    if(wren(s)||req(s,0x40,0xd4,addr_value(a,2),addr_index(a),(void *)p,32))return -1;
    s->io.sleep_ms(s->io.context,1);if(idle(s))return -1;uint8_t check[32];if(sp_vli_read(s,a,check,32,NULL,NULL))return -1;
    if(memcmp(check,p,32))return fail(s->error,sizeof s->error,"Shared-SPI programmed chunk failed readback.");return 0;
}
static int sector(sp_vli_session *s,uint32_t a,const uint8_t *before,const uint8_t *after,bool defer_first,sp_progress p,void *ctx,uint32_t ordinal,uint32_t total){
    uint8_t check[SP_VLI_SECTOR];if(sp_vli_read(s,a,check,sizeof check,NULL,NULL))return -1;
    if(memcmp(before,check,sizeof check))return fail(s->error,sizeof s->error,"Shared SPI changed since its backup. Nothing further erased.");
    if(wren(s)||req(s,0x40,0xd4,addr_value(a,0x20),addr_index(a),NULL,0)||idle(s))return -1;
    if(sp_vli_read(s,a,check,sizeof check,NULL,NULL))return -1;if(!all_ff(check,sizeof check))return fail(s->error,sizeof s->error,"Shared-SPI sector did not erase completely.");
    if(p)p(ctx,"Erasing",ordinal+1,total);
    /* Boot/header entry chunk is the last programmed chunk in each sector. */
    for(unsigned offset=32;offset<SP_VLI_SECTOR;offset+=32)if(write32(s,a+offset,after+offset))return -1;
    if(!defer_first&&write32(s,a,after))return -1;
    if(sp_vli_read(s,a,check,sizeof check,NULL,NULL))return -1;if((defer_first&&(!all_ff(check,32)||memcmp(check+32,after+32,sizeof check-32)))||(!defer_first&&memcmp(check,after,sizeof check)))return fail(s->error,sizeof s->error,"Shared-SPI sector readback differs.");
    if(p)p(ctx,"Programming",ordinal+1,total);return 0;
}
int sp_vli_program(sp_vli_session *s,const uint8_t *saved,const uint8_t *planned,size_t n,sp_progress p,void *ctx){
    if(!s->ready||!saved||!planned||n!=s->capacity)return fail(s->error,sizeof s->error,"Shared-SPI program needs a complete verified backup.");
    bool pd_diff=memcmp(saved+SP_VLI_PD_BASE,planned+SP_VLI_PD_BASE,SP_VLI_PD_BYTES)!=0;
    if(pd_diff){
        if(memcmp(saved,planned,SP_VLI_PD_BASE)||memcmp(saved+SP_VLI_PD_BASE+SP_VLI_PD_BYTES,planned+SP_VLI_PD_BASE+SP_VLI_PD_BYTES,n-SP_VLI_PD_BASE-SP_VLI_PD_BYTES))return fail(s->error,sizeof s->error,"PD plan alters another partition. Nothing erased.");
    }else if(memcmp(saved,planned,n)){
        const uint8_t *h=planned+0x1000;uint32_t start=hdr_addr(h),bytes=hdr_size(h);
        if(!valid_header(saved)||planned[29]!=0x80||planned[31]!=crc8(planned,31)||be16(h)!=0x0518||h[31]!=crc8(h,31)||h[28]!=0||h[29]!=0xff||start<0x2000+((hdr_size(saved)+0xfff)&~0xfffu)||!bytes||bytes>0xf000||start>=SP_VLI_PD_BASE||bytes>SP_VLI_PD_BASE-start)return fail(s->error,sizeof s->error,"Hub plan has invalid headers or overlaps a partition.");
        for(size_t i=0;i<n;i++){
            bool allowed=i==29||i==31||(i>=0x1000&&i<0x1020)||(i>=0x1800&&i<0x1820)||(i>=start&&i<start+bytes);
            if(!allowed&&saved[i]!=planned[i])return fail(s->error,sizeof s->error,"Hub plan alters factory firmware or another partition. Nothing erased.");
        }
    }
    /* Data first, component entry chunk after all its sectors, update header
     * next, root link last. An interrupted payload has an erased entry chunk. */
    bool pd_changed=memcmp(saved+SP_VLI_PD_BASE,planned+SP_VLI_PD_BASE,SP_VLI_PD_BYTES)!=0;
    uint32_t first=pd_changed?SP_VLI_PD_BASE:hdr_addr(planned+0x1000);
    uint32_t bytes=pd_changed?SP_VLI_PD_BYTES:hdr_size(planned+0x1000);
    bool changed=memcmp(saved,planned,n)!=0;
    if(changed){
        if(first<0x2000||first%SP_VLI_SECTOR||!bytes||first>=n||bytes>n-first)return fail(s->error,sizeof s->error,"Invalid component write plan.");
        uint32_t end=first+((bytes+SP_VLI_SECTOR-1)&~(SP_VLI_SECTOR-1));
        if(end>n)return fail(s->error,sizeof s->error,"Component erase plan exceeds flash.");
        bool update_header=!pd_changed&&memcmp(saved+0x1000,planned+0x1000,SP_VLI_SECTOR)!=0;
        bool root_header=!pd_changed&&memcmp(saved,planned,SP_VLI_SECTOR)!=0;
        uint32_t data_sectors=(end-first)/SP_VLI_SECTOR,total=data_sectors+update_header+root_header;
        for(uint32_t a=first;a<end;a+=SP_VLI_SECTOR)if(sector(s,a,saved+a,planned+a,a==first,p,ctx,(a-first)/SP_VLI_SECTOR,total))return -1;
        if(write32(s,first,planned+first))return -1;
        if(!pd_changed){
            if(memcmp(saved+0x1000,planned+0x1000,SP_VLI_SECTOR)&&sector(s,0x1000,saved+0x1000,planned+0x1000,false,p,ctx,data_sectors,total))return -1;
            if(memcmp(saved,planned,SP_VLI_SECTOR)&&sector(s,0,saved,planned,false,p,ctx,data_sectors+update_header,total))return -1;
        }
    }
    uint8_t check[SP_VLI_SECTOR];for(uint32_t a=0;a<n;a+=SP_VLI_SECTOR){if(sp_vli_read(s,a,check,sizeof check,NULL,NULL))return -1;if(memcmp(check,planned+a,sizeof check))return fail(s->error,sizeof s->error,"Complete shared-SPI verification differs; retain the backup and log.");if(p)p(ctx,"Verifying",a+SP_VLI_SECTOR,(uint32_t)n);}return 0;
}
