#include "../src/spectrum.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {
    uint8_t *flash,gpio[2],reg426,reg4,status;
    uint32_t read_address,program_address;
    unsigned read_chunk,calls,fail_at,erases,pages,invalid_writes,terminators;
    uint16_t debug_reg;
    bool reg4_read,programming,short_reply,busy,dirty_erase,corrupt_program;
    bool reading_id, transient426, disconnected, disconnect_on_exit, blocked_enable;
    unsigned enable_failures, setup_read_count, enable_reads, exit_gpio_calls;
    uint16_t setup_reads[64];
    uint8_t debug_commands[128];unsigned debug_count;
    uint8_t debug_frames[512][7],debug_lengths[512];unsigned debug_frames_count;
    bool second_step_bank,debug_address_selected,debug_ready;
    uint8_t jedec[3];
    uint8_t fail_request;
    unsigned fail_request_number,request_count,erase_attempts,fail_erase_number;
    bool fail_erase_poll,fail_verify_read;
    uint64_t ms;
    size_t image_bytes;
} mock;
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);abort();}}while(0)
static uint32_t address(const uint8_t *b){return ((uint32_t)b[3]<<16)|((uint32_t)b[4]<<8)|b[5];}
static ssize_t control(void *context,uint8_t type,uint8_t req,uint16_t val,uint16_t index,void *data,uint16_t n,uint32_t timeout){
    mock *m=context;uint8_t *b=data;m->calls++;CHECK(timeout==1500);
    if(m->disconnected){if(req==0xf6)m->exit_gpio_calls++;return -1;}
    if(m->calls==m->fail_at)return m->short_reply?(n?(ssize_t)n-1:1):-1;
    if(m->fail_request&&req==m->fail_request&&++m->request_count==m->fail_request_number)return m->short_reply?(n?(ssize_t)n-1:1):-1;
    if(m->fail_erase_poll&&req==0xa3&&index==0x93&&m->erases){m->fail_erase_poll=false;return -1;}
    if(m->fail_verify_read&&req==0xa7&&m->pages){m->fail_verify_read=false;return n-1;}
    if(req==0xf6){CHECK(val==0xa0||val==0xa1);if(type==0xc0){CHECK(n==1);b[0]=m->gpio[val-0xa0];}else{CHECK(n==0);m->gpio[val-0xa0]=(uint8_t)index;}return n;}
    if(req==0xb8||req==0xb9){CHECK(type==0x40&&m->programming&&n<=32&&n);if(m->program_address<SP_BASE||m->program_address+n>SP_LIMIT){m->invalid_writes++;return -1;}for(unsigned i=0;i<n;i++)m->flash[m->program_address+i]&=b[i];m->program_address+=n;if(req==0xb9){m->programming=false;m->status=0;m->pages++;if(m->corrupt_program)m->flash[SP_BASE+m->image_bytes+17]=0;}return n;}
    if(req==0xa7||req==0xa8||req==0xa9){CHECK(type==0xc0&&index==0x93&&n==32);CHECK(req==(m->read_chunk==0?0xa7:(m->read_chunk==7?0xa9:0xa8)));CHECK(m->read_address+32<=SP_LIMIT);memcpy(b,m->flash+m->read_address,32);m->read_address+=32;m->read_chunk++;return n;}
    if(req==0xa3){CHECK(type==0xc0);if(index==0xb3){CHECK(n==1);if(m->reg4_read){b[0]=m->debug_ready?m->reg4:0;m->enable_reads++;}
        else{if(m->setup_read_count<64)m->setup_reads[m->setup_read_count++]=m->debug_reg;
        b[0]=m->debug_ready&&m->debug_reg==0x0426?(m->transient426?(m->reg426&0xfe):m->reg426):0;}}else{CHECK(index==0x93);if(m->reading_id){CHECK(n==3);memcpy(b,m->jedec,3);}else{CHECK(n==1);b[0]=m->busy?3:m->status;}}return n;}
    CHECK((req==0xb2||req==0xb7)&&type==0x40&&n>=2);
    if(b[0]==0xb2){
        if(m->debug_frames_count<512){CHECK(n<=7);memcpy(m->debug_frames[m->debug_frames_count],b,n);m->debug_lengths[m->debug_frames_count++]=(uint8_t)n;}
        if(n==2&&m->debug_count<128)m->debug_commands[m->debug_count++]=b[1];
        if(n==5&&b[1]==0x10&&b[2]==0x1f&&b[3]==0xc1&&b[4]==0x53)m->second_step_bank=true;
        if(n==5&&b[1]==0x10&&!b[2]&&!b[3]&&!b[4])m->debug_address_selected=true;
        if(n==2&&b[1]==0x71&&m->second_step_bank&&m->debug_address_selected)m->debug_ready=true;
        if(n==6&&!memcmp(b+1,"SERDB",5))return n;
        if(b[1]==0x10){if(n>=6&&b[2]==0&&b[3]==0x10&&b[4]==0x0f&&b[5]==0xd7){m->reg4_read=true;if(n==7){if(m->debug_ready&&!m->blocked_enable&&!m->enable_failures)m->reg4=b[6];else if(m->enable_failures)m->enable_failures--;}}
        else if(n>=4){m->reg4_read=false;m->debug_reg=(uint16_t)((b[2]<<8)|b[3]);if(n==5&&m->debug_ready&&m->debug_reg==0x0426)m->reg426=b[4];}}return n;
    }
    CHECK(b[0]==0x92);
    if(b[1]==0x12){m->terminators++;if(m->programming){m->programming=false;m->status=0;m->pages++;}return n;}
    if(n==6&&!memcmp(b+1,"MSTAR",5))return n;
    if(b[1]==0x24&&m->disconnect_on_exit){m->disconnected=true;return n;}
    if(b[1]!=0x10)return n;
    switch(b[2]){
      case 0x03: CHECK(n==6);m->reading_id=false;m->read_address=address(b);m->read_chunk=0;break;
      case 0x05: m->reading_id=false;break;
      case 0x9f: m->reading_id=true;break;
      case 0x06: m->status=2;break;
      case 0x04: m->status=0;break;
      case 0xd8:{CHECK(n==6&&m->status==2);if(++m->erase_attempts==m->fail_erase_number)return n-1;uint32_t a=address(b);if(a<SP_BASE||a>SP_LIMIT-SP_BLOCK||a%SP_BLOCK){m->invalid_writes++;return -1;}memset(m->flash+a,0xff,SP_BLOCK);if(m->dirty_erase)m->flash[a+SP_BLOCK/2]=0;m->erases++;m->status=0;break;}
      case 0x02: CHECK(n==6&&m->status==2&&req==0xb7);m->program_address=address(b);m->programming=true;break;
      default: CHECK(false);
    }return n;
}
static uint64_t now(void *context){return ((mock *)context)->ms;}
static void sleep_ms(void *context,uint32_t n){((mock *)context)->ms+=n;}
static void setup(mock *m,sp_session *s,const uint8_t *image,size_t n){memset(m,0,sizeof *m);m->image_bytes=n;m->jedec[0]=0xc2;m->jedec[1]=0x20;m->jedec[2]=0x17;m->flash=malloc(SP_LIMIT);CHECK(m->flash);memset(m->flash,0xff,SP_LIMIT);memcpy(m->flash,image,0x10000);memcpy(m->flash+SP_BASE,image,n);m->gpio[0]=0xa2;m->gpio[1]=0x30;m->reg426=0x22;m->reg4=0x70;memset(s,0,sizeof *s);s->io=(sp_transport){m,control,now,sleep_ms};}
static void cleanup_check(mock *m,sp_session *s){CHECK(sp_finish(s)==0);CHECK(m->gpio[0]==0xa2&&m->gpio[1]==0x30);CHECK(!m->invalid_writes);free(m->flash);}
int main(int argc,char **argv){CHECK(argc==2);uint8_t *image=NULL;size_t n=0;char err[256];CHECK(sp_load(argv[1],&image,&n,SP_LIMIT-SP_BASE,err,sizeof err)==0);sp_image info;CHECK(sp_validate_image(image,n,&info,err,sizeof err)==0);CHECK(info.stock!=NULL);
    uint8_t *malformed=malloc(n);CHECK(malformed);memcpy(malformed,image,n);
    /* A valid empty raw DEFLATE stream plus CRC, but too-short compressed
       component trailer, must fail without an unsigned-size underflow. */
    uint8_t *entry=malformed+0x32080+14;uint32_t tiny=(uint32_t)n-8;entry[2]=(uint8_t)(tiny>>24);entry[3]=(uint8_t)(tiny>>16);entry[4]=(uint8_t)(tiny>>8);entry[5]=(uint8_t)tiny;entry[6]=entry[7]=entry[8]=0;entry[9]=4;entry[10]=4;malformed[tiny]=3;malformed[tiny+1]=0;uint16_t crc=sp_crc16(malformed+tiny,2);malformed[tiny+2]=(uint8_t)(crc>>8);malformed[tiny+3]=(uint8_t)crc;
    CHECK(sp_validate_image(malformed,n,&info,err,sizeof err)<0);CHECK(strstr(err,"compressed component"));free(malformed);
    mock m;sp_session s;setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);uint8_t sample[257];CHECK(sp_read(&s,SP_BASE+5,sample,sizeof sample,NULL,NULL)==0);CHECK(!memcmp(sample,image+5,sizeof sample));CHECK(sp_read(&s,SP_LIMIT-1,sample,2,NULL,NULL)<0);CHECK(sp_read(&s,SP_BASE,sample,0,NULL,NULL)<0);cleanup_check(&m,&s);
    /* Fail every transfer around GPIO/ISP setup. Rollback is attempted even if
       the failed write might actually have reached the bridge. */
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);unsigned begin_calls=m.calls;cleanup_check(&m,&s);
    for(unsigned i=1;i<=begin_calls;i++)for(unsigned short_case=0;short_case<2;short_case++){
      setup(&m,&s,image,n);m.fail_at=i;m.short_reply=short_case;CHECK(sp_begin(&s)<0);CHECK(!m.erases);CHECK(sp_finish(&s)==0);CHECK(m.gpio[0]==0xa2&&m.gpio[1]==0x30);free(m.flash);
    }
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);unsigned setup_calls=m.calls;CHECK(m.reg4==0xf0&&m.reg426==0x23);cleanup_check(&m,&s);
    for(unsigned i=begin_calls+1;i<=setup_calls;i++){
      setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);m.fail_at=i;CHECK(sp_flash_setup(&s)<0);CHECK(!m.erases);CHECK(sp_finish(&s)==0);CHECK(m.reg4==0x70&&m.reg426==0x22);CHECK(m.gpio[0]==0xa2&&m.gpio[1]==0x30);free(m.flash);
    }
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);m.busy=true;CHECK(sp_flash_setup(&s)<0);CHECK(!m.erases&&m.ms>=1250&&m.ms<1400);m.busy=false;cleanup_check(&m,&s);
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);m.dirty_erase=true;CHECK(sp_program(&s,image,n,m.flash+SP_BASE,SP_LIMIT-SP_BASE,NULL,NULL)<0);CHECK(m.erases>0&&!m.pages);CHECK(strstr(s.error,"blank check"));cleanup_check(&m,&s);
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);m.flash[SP_BASE+n+17]=0x42;CHECK(sp_program(&s,image,n,m.flash+SP_BASE,SP_LIMIT-SP_BASE,NULL,NULL)==0);size_t span=(n+SP_BLOCK-1)/SP_BLOCK*SP_BLOCK;CHECK(m.erases==span/SP_BLOCK&&m.pages==span/SP_PAGE);CHECK(!memcmp(m.flash+SP_BASE,image,n));for(size_t i=n;i<span;i++)CHECK(m.flash[SP_BASE+i]==(i==n+17?0x42:0xff));CHECK(!memcmp(m.flash,image,0x10000));CHECK(m.flash[SP_BASE+span]==0xff);cleanup_check(&m,&s);
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);m.corrupt_program=true;CHECK(sp_program(&s,image,n,m.flash+SP_BASE,SP_LIMIT-SP_BASE,NULL,NULL)<0);CHECK(strstr(s.error,"verification"));cleanup_check(&m,&s);
    setup(&m,&s,image,n);m.jedec[0]=0xef;CHECK(sp_begin(&s)<0);CHECK(!m.erases&&!s.geometry_known);cleanup_check(&m,&s);
    setup(&m,&s,image,n);m.jedec[2]=0x19;CHECK(sp_begin(&s)<0);CHECK(!m.erases&&!s.geometry_known);cleanup_check(&m,&s);
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);m.status=0x0c;CHECK(sp_flash_setup(&s)<0);CHECK(!m.erases&&m.status==0x0c);cleanup_check(&m,&s);
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);size_t raw_size=SP_LIMIT-SP_BASE;uint8_t *raw=malloc(raw_size);CHECK(raw);memcpy(raw,m.flash+SP_BASE,raw_size);for(size_t i=n;i<raw_size;i++)raw[i]=(uint8_t)(i*7+3);CHECK(sp_restore(&s,raw,raw_size,NULL,NULL)==0);CHECK(m.erases==raw_size/SP_BLOCK&&m.pages==raw_size/SP_PAGE);CHECK(!memcmp(m.flash+SP_BASE,raw,raw_size));CHECK(!memcmp(m.flash,image,0x10000));free(raw);cleanup_check(&m,&s);
    /* The reported C22018 has twice the physical capacity. Exercise both
       update and full FW2 restore; factory/upper regions must remain intact,
       and the simulator rejects every transfer outside the existing bounds. */
    setup(&m,&s,image,n);m.jedec[2]=0x18;
    uint8_t *larger=realloc(m.flash,0x1000000u);CHECK(larger);m.flash=larger;
    memset(m.flash+SP_LIMIT,0xa5,SP_LIMIT);
    CHECK(sp_begin(&s)==0);CHECK(s.geometry_known&&s.flash_capacity==0x1000000u);
    CHECK(sp_flash_setup(&s)==0);m.flash[SP_BASE+n+17]=0x42;
    CHECK(sp_program(&s,image,n,m.flash+SP_BASE,SP_LIMIT-SP_BASE,NULL,NULL)==0);
    CHECK(!memcmp(m.flash+SP_BASE,image,n));CHECK(m.flash[SP_BASE+n+17]==0x42);
    CHECK(sp_read(&s,SP_LIMIT,sample,1,NULL,NULL)<0);
    raw=malloc(raw_size);CHECK(raw);memcpy(raw,m.flash+SP_BASE,raw_size);
    for(size_t i=n;i<raw_size;i++)raw[i]=(uint8_t)(i*11+9);
    CHECK(sp_restore(&s,raw,raw_size,NULL,NULL)==0);
    CHECK(!memcmp(m.flash+SP_BASE,raw,raw_size));free(raw);
    CHECK(!memcmp(m.flash,image,0x10000));
    for(size_t i=0x10000;i<SP_BASE;i++)CHECK(m.flash[i]==0xff);
    for(size_t i=SP_LIMIT;i<0x1000000u;i++)CHECK(m.flash[i]==0xa5);
    cleanup_check(&m,&s);
    setup(&m,&s,image,n);m.jedec[2]=0x18;CHECK(sp_begin(&s)==0);m.status=0x0c;CHECK(sp_flash_setup(&s)<0);CHECK(!m.erases&&m.status==0x0c);cleanup_check(&m,&s);
    /* A setup command may read back differently; proceed through the exact
       vendor register order, but require the final enable and status proof. */
    setup(&m,&s,image,n);m.transient426=true;m.disconnect_on_exit=true;
    CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);CHECK(s.flash_ready);
    const uint16_t order[]={0x042a,0x0428,0x0426,0x042a,0x0428,0x0426};
    CHECK(m.setup_read_count==6&&!memcmp(m.setup_reads,order,sizeof order));
    CHECK(m.enable_reads==2);CHECK(!m.erases&&!m.pages);
    const uint8_t commands[]={0x80,0x82,0x84,0x51,0x7f,0x37,0x61,0x35,0x71,0x34,0x45,0x80,0x82,0x85,0x53,0x7f,0x35,0x71};
    CHECK(m.debug_count==sizeof commands&&!memcmp(m.debug_commands,commands,sizeof commands));
    /* Independent vendor packet transcript: every debug OUT frame from the
       successful attempt, including both mode helpers, register and tail data. */
    static const uint8_t vendor[][7]={
      {0xb2,'S','E','R','D','B'},{0xb2,0x10,0xc0,0xc1,0x53},{0xb2,0x10,0x1f,0xc1,0x53},
      {0xb2,0x80},{0xb2,0x82},{0xb2,0x84},{0xb2,0x51},{0xb2,0x7f},{0xb2,0x37},{0xb2,0x61},
      {0xb2,0x10,0,0,0},{0xb2,0x35},{0xb2,0x71},
      {0xb2,0x10,4,0x2a},{0xb2,0x10,4,0x28},{0xb2,0x10,4,0x26},{0xb2,0x10,4,0x26,0x23},
      {0xb2,0x10,4,0x2a},{0xb2,0x10,4,0x28},{0xb2,0x10,4,0x26},
      {0xb2,0x34},{0xb2,0x45},{0xb2,'S','E','R','D','B'},
      {0xb2,0x10,0xc0,0xc1,0x53},{0xb2,0x10,0x1f,0xc1,0x53},
      {0xb2,0x80},{0xb2,0x82},{0xb2,0x85},{0xb2,0x53},{0xb2,0x7f},{0xb2,0x35},{0xb2,0x71},
      {0xb2,0x10,0,0x10,0x0f,0xd7},{0xb2,0x10,0,0x10,0x0f,0xd7,0xf0},{0xb2,0x10,0,0x10,0x0f,0xd7}
    };
    static const uint8_t lengths[]={6,5,5,2,2,2,2,2,2,2,5,2,2,4,4,4,5,4,4,4,2,2,6,5,5,2,2,2,2,2,2,2,6,7,6};
    CHECK(m.debug_frames_count==sizeof lengths);
    for(unsigned f=0;f<sizeof lengths;f++){CHECK(m.debug_lengths[f]==lengths[f]);CHECK(!memcmp(m.debug_frames[f],vendor[f],lengths[f]));}

    CHECK(sp_finish(&s)==0);CHECK(m.disconnected&&!m.exit_gpio_calls);
    CHECK(m.gpio[0]==0xa2&&m.gpio[1]==0x30);free(m.flash);
    setup(&m,&s,image,n);m.transient426=true;m.blocked_enable=true;
    CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)<0);
    CHECK(!s.flash_ready&&!m.erases&&!m.pages&&m.enable_reads==10);
    CHECK(strstr(s.error,"Flash-access enable mismatch"));cleanup_check(&m,&s);
    setup(&m,&s,image,n);m.enable_failures=2;
    CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);
    CHECK(s.flash_ready&&m.enable_reads==6&&!s.error[0]);
    CHECK(s.reg4==0x70&&s.reg426==0x22);cleanup_check(&m,&s);
    /* Representative failures after setup: erase command, erase status, mid
       and final page chunks, and a short post-program verification read. */
    for(unsigned fault=0;fault<5;fault++){
      setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);
      if(fault==0)m.fail_erase_number=2;
      if(fault==1)m.fail_erase_poll=true;
      if(fault==2){m.fail_request=0xb8;m.fail_request_number=3;m.short_reply=true;}
      if(fault==3){m.fail_request=0xb9;m.fail_request_number=1;m.short_reply=true;}
      if(fault==4)m.fail_verify_read=true;
      CHECK(sp_program(&s,image,n,m.flash+SP_BASE,SP_LIMIT-SP_BASE,NULL,NULL)<0);
      if(fault<=1)CHECK(m.erases==1&&!m.pages);
      if(fault==2||fault==3)CHECK(m.pages==1);
      unsigned erases=m.erases,pages=m.pages;CHECK(!memcmp(m.flash,image,0x10000));cleanup_check(&m,&s);CHECK(m.erases==erases&&m.pages==pages);
    }
    setup(&m,&s,image,n);CHECK(sp_begin(&s)==0);CHECK(sp_flash_setup(&s)==0);strcpy(s.error,"Primary operation failure");m.fail_at=m.calls+1;CHECK(sp_finish(&s)<0);CHECK(s.cleanup_errors==1);CHECK(!strcmp(s.error,"Primary operation failure"));CHECK(m.gpio[0]==0xa2&&m.gpio[1]==0x30);CHECK(m.reg426==0x22&&m.reg4==0x70);free(m.flash);
    /* Check exclusive durable file creation and refusal to replace backup. */
    char directory[]="/tmp/spectrum-backup-test-XXXXXX";CHECK(mkdtemp(directory));char path[256];snprintf(path,sizeof path,"%s/backup.bin",directory);CHECK(sp_save_exclusive(path,image,n,err,sizeof err)==0);CHECK(sp_save_exclusive(path,image,n,err,sizeof err)<0);uint8_t *disk=NULL;size_t disk_n=0;CHECK(sp_load(path,&disk,&disk_n,n,err,sizeof err)==0);CHECK(disk_n==n&&!memcmp(disk,image,n));free(disk);CHECK(remove(path)==0);CHECK(rmdir(directory)==0);
    free(image);printf("Protocol simulator: %u assertions passed; actual monitor not tested.\n",checks);return 0;
}
