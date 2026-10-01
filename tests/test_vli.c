#include "../src/vli.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CAP 0x40000u
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
typedef struct {
 uint8_t data[CAP], regs[65536], jedec[4];
 bool wel, protected, busy, dirty_erase, corrupt_program;
 unsigned calls, fail_at, short_at, erases, writes;
 uint32_t erased[64], written[8192];
 uint64_t time;
} fake;
static uint64_t now(void *c){return ((fake*)c)->time;}
static void sleep_ms(void *c,uint32_t ms){((fake*)c)->time+=ms;}
static ssize_t control(void *c,uint8_t type,uint8_t cmd,uint16_t value,uint16_t index,void *d,uint16_t n,uint32_t timeout){
 fake *f=c;CHECK(timeout==3000);f->calls++;if(f->calls==f->fail_at)return -1;if(f->calls==f->short_at)return n? n-1:-1;
 uint32_t a=((uint32_t)(value&0xff00)<<8)|((index&0xff)<<8)|(index>>8);
 if(type==0xc0&&cmd==0xc8){CHECK(value==0x9f&&index==0&&n==4);memcpy(d,f->jedec,4);return n;}
 if(type==0xc0&&cmd==0xc1){CHECK(value==5&&n==1);*(uint8_t*)d=(f->wel?2:0)|(f->protected?0x1c:0)|(f->busy?1:0);return 1;}
 if(type==0xc0&&cmd==0xc4){CHECK((value&255)==3&&n<=32&&a+n<=CAP);memcpy(d,f->data+a,n);return n;}
 if(type==0x40&&cmd==0xd1){CHECK(value==6&&n==0);f->wel=true;return 0;}
 if(type==0x40&&cmd==0xd4){CHECK(f->wel&&!f->protected&&a<CAP);f->wel=false;
  if((value&255)==0x20){CHECK(n==0&&a%4096==0);CHECK(f->erases<64);f->erased[f->erases++]=a;memset(f->data+a,255,4096);if(f->dirty_erase)f->data[a+128]=0;return 0;}
  CHECK((value&255)==2&&n==32&&a%32==0&&a+n<=CAP);CHECK(f->writes<8192);f->written[f->writes++]=a;for(unsigned i=0;i<n;i++)f->data[a+i]&=((uint8_t*)d)[i];if(f->corrupt_program)f->data[a]^=1;return n;
 }
 uint16_t reg=(uint16_t)(((uint16_t)cmd<<8)|(value&255));
 if(type==0xc0){CHECK(n==1&&index==0);*(uint8_t*)d=f->regs[reg];return 1;}
 CHECK(type==0x40&&n==0);f->regs[reg]=(uint8_t)index;return 0;
}
static void init(fake *f,const uint8_t *saved){memset(f,0,sizeof *f);memcpy(f->data,saved,CAP);f->regs[0xf88c]=0xf0;f->regs[0xf88e]=0x18;f->regs[0xf88f]=0x35;f->regs[0xf651]=0;f->regs[0xf8a2]=0xff;f->regs[0xf832]=0xfd;f->regs[0xf920]=0xab;f->regs[0xf836]=0x40;f->jedec[0]=0xef;f->jedec[1]=0x30;f->jedec[2]=0x12;}
static sp_vli_session session(fake *f){sp_vli_session s={0};s.io=(sp_transport){f,control,now,sleep_ms};return s;}
static uint8_t crc8(const uint8_t *p){uint8_t c=0;for(unsigned i=0;i<31;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(uint8_t)((c<<1)^((c&128)?7:0));}return c;}
int main(int argc,char **argv){CHECK(argc==4);char err[256];uint8_t *hub,*pd,*old;size_t hn,pn,on;CHECK(!sp_load(argv[1],&hub,&hn,0x100000,err,sizeof err));CHECK(!sp_load(argv[2],&pd,&pn,0x100000,err,sizeof err));CHECK(!sp_load(argv[3],&old,&on,0x100000,err,sizeof err));
 uint8_t *saved=malloc(CAP),*plan=malloc(CAP),*scratch=malloc(CAP);fake *f=malloc(sizeof *f);CHECK(saved&&plan&&scratch&&f);memset(saved,255,CAP);memcpy(saved,hub,32);memcpy(saved+0x2000,hub+0x2000,0x8630);memcpy(saved+0x20000,old,on);
 /* Board-specific bytes and recovery partition must survive every update. */
 for(unsigned i=0x28000;i<CAP;i++)saved[i]=(uint8_t)(i*17);saved[0x700]=0x43;saved[0x1700]=0x78;
 sp_vli_image info;CHECK(!sp_vli_validate(hub,hn,&info,err,sizeof err)&&info.kind==SP_VLI_HUB);CHECK(!sp_vli_validate(pd,pn,&info,err,sizeof err)&&info.kind==SP_VLI_PD);pd[12]^=1;CHECK(sp_vli_validate(pd,pn,&info,err,sizeof err));pd[12]^=1;
 CHECK(!sp_vli_plan(saved,CAP,hub,hn,plan,&info,err,sizeof err));CHECK(!memcmp(saved+0x2000,plan+0x2000,0x9000));CHECK(!memcmp(saved+0x20000,plan+0x20000,CAP-0x20000));CHECK(plan[29]==0x80&&plan[0x1029]!=0x80);CHECK(plan[0x1004]==0xb0&&plan[0x1005]==0);CHECK(plan[0x700]==0x43&&plan[0x1700]==0x78);CHECK(!memcmp(plan+0xb000,hub+0x2000,0x8630));CHECK(plan[31]==crc8(plan));
 init(f,saved);sp_vli_session s=session(f);CHECK(!sp_vli_begin(&s)&&s.capacity==CAP);CHECK(!sp_vli_program(&s,saved,plan,CAP,NULL,NULL));CHECK(!memcmp(f->data,plan,CAP));CHECK(f->erased[f->erases-1]==0&&f->erased[f->erases-2]==0x1000);CHECK(f->written[f->writes-1]==0);CHECK(!sp_vli_finish(&s));CHECK(f->regs[0xf8a2]==0xff&&f->regs[0xf832]==0xfd&&f->regs[0xf920]==0xab&&f->regs[0xf836]==0x40);
 /* A second hub install is a no-op and doesn't erase flash. */
 memcpy(saved,plan,CAP);CHECK(!sp_vli_plan(saved,CAP,hub,hn,scratch,&info,err,sizeof err));CHECK(!memcmp(saved,scratch,CAP));init(f,saved);s=session(f);CHECK(!sp_vli_begin(&s));CHECK(!sp_vli_program(&s,saved,scratch,CAP,NULL,NULL));CHECK(f->erases==0);CHECK(!sp_vli_finish(&s));
 CHECK(!sp_vli_plan(saved,CAP,pd,pn,plan,&info,err,sizeof err));CHECK(!memcmp(plan,saved,0x20000));CHECK(!memcmp(plan+0x28000,saved+0x28000,CAP-0x28000));init(f,saved);s=session(f);CHECK(!sp_vli_begin(&s));CHECK(!sp_vli_program(&s,saved,plan,CAP,NULL,NULL));CHECK(f->erases==8);CHECK(!memcmp(f->data,plan,CAP));CHECK(!sp_vli_finish(&s));
 /* Every setup request can fail or return short; no erase is ever issued. */
 for(unsigned mode=0;mode<2;mode++)for(unsigned step=1;step<=16;step++){init(f,saved);if(mode)f->short_at=step;else f->fail_at=step;s=session(f);int rc=sp_vli_begin(&s);CHECK(rc!=0);CHECK(f->erases==0);sp_vli_finish(&s);}
 init(f,saved);f->jedec[2]=0x18;s=session(f);CHECK(sp_vli_begin(&s));CHECK(f->erases==0);sp_vli_finish(&s);
 init(f,saved);f->regs[0xf651]=2;s=session(f);CHECK(sp_vli_begin(&s));CHECK(f->erases==0);
 init(f,saved);f->protected=true;s=session(f);CHECK(sp_vli_begin(&s));CHECK(f->erases==0);sp_vli_finish(&s);
 init(f,saved);f->busy=true;s=session(f);CHECK(sp_vli_begin(&s));CHECK(f->time<=10000);sp_vli_finish(&s);
 for(unsigned mode=0;mode<3;mode++){init(f,saved);s=session(f);CHECK(!sp_vli_begin(&s));if(mode==0)f->dirty_erase=true;if(mode==1)f->corrupt_program=true;if(mode==2)f->data[0x20000]^=1;CHECK(sp_vli_program(&s,saved,plan,CAP,NULL,NULL));CHECK(f->erases<=1);sp_vli_finish(&s);}
 /* Sweep failure and short-transfer positions through pre-erase, erase,
  * blank check, write-enable, program, busy polling and readback. */
 for(unsigned mode=0;mode<2;mode++)for(unsigned offset=1;offset<=800;offset++){
  init(f,saved);s=session(f);CHECK(!sp_vli_begin(&s));
  if(mode)f->short_at=f->calls+offset;else f->fail_at=f->calls+offset;
  CHECK(sp_vli_program(&s,saved,plan,CAP,NULL,NULL));CHECK(f->erases<=1);
  CHECK(!memcmp(f->data,saved,0x20000));CHECK(!memcmp(f->data+0x28000,saved+0x28000,CAP-0x28000));
  sp_vli_finish(&s);
 }
 /* Backend rejects a hand-crafted plan that changes another component. */
 plan[0x30000]^=1;init(f,saved);s=session(f);CHECK(!sp_vli_begin(&s));CHECK(sp_vli_program(&s,saved,plan,CAP,NULL,NULL));CHECK(f->erases==0);sp_vli_finish(&s);plan[0x30000]^=1;
 /* Root/header/PD damage and ambiguous layouts fail before any USB transfer. */
 for(unsigned mode=0;mode<5;mode++){memcpy(scratch,saved,CAP);if(mode==0)scratch[31]^=1;if(mode==1)scratch[0x1000+31]^=1;if(mode==2)scratch[0x20000+12]^=1;if(mode==3){scratch[0x14007]=9;scratch[0x14008]=0x21;}if(mode==4){scratch[6]=0xf0;scratch[7]=0;scratch[31]=crc8(scratch);}CHECK(sp_vli_plan(scratch,CAP,hub,hn,plan,&info,err,sizeof err));}
 printf("{\"assertions\":%u,\"hardware_access\":false,\"result\":\"passed\"}\n",checks);free(f);free(scratch);free(plan);free(saved);free(hub);free(pd);free(old);return 0;
}
