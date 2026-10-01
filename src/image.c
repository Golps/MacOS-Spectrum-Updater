#include "spectrum.h"
#include <CommonCrypto/CommonDigest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
static uint32_t le32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint16_t be16(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
uint32_t sp_crc32(const uint8_t *p,size_t n){uint32_t c=0;while(n--){c^=(uint32_t)*p++<<24;for(unsigned i=0;i<8;i++)c=(c<<1)^((c&0x80000000u)?0x04c11db7u:0);}return c;}
uint16_t sp_crc16(const uint8_t *p,size_t n){uint16_t c=0;while(n--){c^=(uint16_t)*p++<<8;for(unsigned i=0;i<8;i++)c=(uint16_t)((c<<1)^((c&0x8000)?0x8005:0));}return c;}
void sp_sha256(const uint8_t *p,size_t n,char out[65]){uint8_t h[CC_SHA256_DIGEST_LENGTH];CC_SHA256(p,(CC_LONG)n,h);for(unsigned i=0;i<sizeof h;i++)snprintf(out+i*2,3,"%02x",h[i]);out[64]=0;}
static int bad(char *e,size_t z,const char *m){snprintf(e,z,"%s",m);return -1;}
/* Expanded bytes are discarded; never allocate according to an untrusted size. */
static int raw_stream(const uint8_t *p,size_t n,size_t *consumed,size_t *expanded){
    z_stream z={0};uint8_t out[32768];if(n>UINT32_MAX||inflateInit2(&z,-15)!=Z_OK)return -1;
    z.next_in=(Bytef *)p;z.avail_in=(uInt)n;int r=Z_OK;
    while(r==Z_OK){z.next_out=out;z.avail_out=sizeof out;r=inflate(&z,Z_NO_FLUSH);if(z.total_out>16u*1024u*1024u){r=Z_MEM_ERROR;break;}}
    *consumed=z.total_in;*expanded=z.total_out;inflateEnd(&z);return r==Z_STREAM_END?0:-1;
}
int sp_validate_image(const uint8_t *d,size_t n,sp_image *o,char *e,size_t z){
    const size_t table=0x32080,main=0x33880;const uint8_t magic[]={0x54,0x45,0x4c,9,0x58,0x33,0x69,0};
    if(n<=main+6||n>SP_LIMIT-SP_BASE)return bad(e,z,"Scaler image size invalid (maximum 4 MiB).");
    if(memcmp(d+0xff00,"MST9U4",6)||memcmp(d+table,magic,8))return bad(e,z,"Not a Spectrum MST9U4 update container.");
    uint32_t start=le32(d+0x10000),end=le32(d+0x1000c),footer=le32(d+0x10020);
    if(start!=0x30080||end<=main||end>=footer||footer!=n-4||le32(d+0x10064)!=SP_BASE)return bad(e,z,"Unexpected application, flash base or footer layout.");
    unsigned count=be16(d+table+12);if(!count||count>64||table+14+11*count>main)return bad(e,z,"Invalid component table count.");
    uint32_t offsets[64],sizes[64];uint16_t ids[64];size_t first=footer;
    for(unsigned i=0;i<count;i++){
        const uint8_t *p=d+table+14+11*i;ids[i]=be16(p);offsets[i]=be32(p+2);sizes[i]=be32(p+6);uint8_t flags=p[10];
        if(sizes[i]<2||offsets[i]<end||(uint64_t)offsets[i]+sizes[i]>footer||(flags!=0&&flags!=4))return bad(e,z,"Invalid component range or flags.");
        for(unsigned j=0;j<i;j++)if(ids[i]==ids[j]||((uint64_t)offsets[i]< (uint64_t)offsets[j]+sizes[j]&&(uint64_t)offsets[j]<(uint64_t)offsets[i]+sizes[i]))return bad(e,z,"Duplicate or overlapping components.");
        const uint8_t *c=d+offsets[i];size_t size=sizes[i];if(sp_crc16(c,size-2)!=be16(c+size-2))return bad(e,z,"Component CRC16 mismatch.");
        if(flags==4){size_t used,expanded;if(size<8||raw_stream(c,size,&used,&expanded)||used>size-8||le32(c+used)!=expanded||c[used+4]!=0xbe||c[used+5]!=0xef)return bad(e,z,"Invalid compressed component or trailer.");for(size_t j=used+6;j<size-2;j++)if(c[j])return bad(e,z,"Unknown compressed-component trailer bytes.");}
        if(offsets[i]<first)first=offsets[i];
    }
    if(sp_crc32(d+start,end-start)!=le32(d+0x10014)||sp_crc32(d,n-4)!=be32(d+n-4))return bad(e,z,"Application or whole-image CRC32 mismatch.");
    uint16_t staged=0;
    for(size_t i=0x10000;i<end-2;i++){
        uint8_t b=((i>=0x10014&&i<0x10018)||(i>=0x10060&&i<0x10068))?0:d[i];
        staged^=(uint16_t)b<<8;for(unsigned bit=0;bit<8;bit++)staged=(uint16_t)((staged<<1)^((staged&0x8000)?0x8005:0));
    }
    if(staged!=be16(d+end-2))return bad(e,z,"Staged main-build CRC16 mismatch.");
    size_t used,expanded;if(raw_stream(d+main,first-main,&used,&expanded)||main+used+6!=end||le32(d+main+used)!=expanded)return bad(e,z,"Main stream length or trailer invalid.");
    memset(o,0,sizeof *o);o->bytes=n;o->expanded_bytes=expanded;o->components=count;sp_sha256(d,n,o->sha256);
    struct {const char *hash,*name;} known[]={
      {"5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162","ES07D02 V101"},
      {"e319da9490df8cec4ff94677385e57a560899a9ef335b6dc7e9b052bf3bcc9f0","ES07D02 Beta01"},
      {"0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9","ES07DC9 V101"},
      {"d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3","Beta03"},
      {"136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757","V108"},
      {"199bb51c3d31023ffb37542acc250af4bd66cc7059f9a221697ce9c6c5b27ab1","V106"},
      {"536d94f761d84fa7d8b616dbf5442b81a2d470f2cb7112cba213bc4fbd10add9","V105"}};
    for(unsigned i=0;i<sizeof known/sizeof known[0];i++)if(!strcmp(o->sha256,known[i].hash))o->stock=known[i].name;
    return 0;
}
int sp_validate_backup(const uint8_t *d,size_t n,sp_image *o,char *e,size_t z){
    if(n!=SP_LIMIT-SP_BASE)return bad(e,z,"Full FW2 backup must be exactly 4 MiB.");
    uint32_t footer=le32(d+0x10020);
    if(footer>n-4||footer<0x33880)return bad(e,z,"Backup's embedded firmware footer is invalid.");
    if(sp_validate_image(d,(size_t)footer+4,o,e,z))return -1;
    /* Preserve the container length in bytes; SHA identifies the complete raw
       backup, including prior-package tails and opaque configuration data. */
    sp_sha256(d,n,o->sha256);return 0;
}
