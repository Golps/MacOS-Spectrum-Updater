/* Actual USB CLI orchestration with memory-only native/flash interfaces. */
#include "../src/vli-cli.c"
#include <assert.h>
static uint8_t *scaler,*spi;static unsigned opens,hub_opens,writes,checks,cases;static bool pair_fail,bad_read,cleanup_fail;static const char *destination;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"USB CLI line %d: %s\n",__LINE__,#x);abort();}}while(0)
uint32_t sp_macos_keep_awake(char *e,size_t z){(void)e;(void)z;return 1;}
void sp_macos_release_awake(uint32_t id){CHECK(id==1);}
int sp_macos_open(uint64_t id,sp_macos *m,sp_transport *t,char *e,size_t z){(void)m;(void)t;(void)e;(void)z;CHECK(id==1);opens++;return 0;}
int sp_macos_open_paired_hub(uint64_t id,sp_macos *m,sp_transport *t,char *e,size_t z){(void)m;(void)t;CHECK(id==1);hub_opens++;if(pair_fail){snprintf(e,z,"Wrong USB parent");return -1;}return 0;}
void sp_macos_close(sp_macos *m){(void)m;}
int sp_begin(sp_session *s){(void)s;return 0;}int sp_finish(sp_session *s){(void)s;return cleanup_fail?-1:0;}
int sp_read(sp_session *s,uint32_t a,uint8_t *d,size_t n,sp_progress p,void *c){(void)s;(void)p;(void)c;CHECK(a==SP_BASE&&n==SP_LIMIT-SP_BASE);memcpy(d,scaler,n);return 0;}
int sp_vli_begin(sp_vli_session *s){s->capacity=0x40000;s->jedec[0]=0xef;s->jedec[1]=0x30;s->jedec[2]=0x12;return 0;}
int sp_vli_finish(sp_vli_session *s){(void)s;return 0;}
int sp_vli_read(sp_vli_session *s,uint32_t a,uint8_t *d,size_t n,sp_progress p,void *c){(void)s;(void)p;(void)c;CHECK(a==0&&n==0x40000);memcpy(d,spi,n);static unsigned reads;reads++;if(bad_read&&reads%2==0)d[0]^=1;return 0;}
int sp_vli_program(sp_vli_session *s,const uint8_t *old,const uint8_t *plan,size_t n,sp_progress p,void *c){(void)s;(void)p;(void)c;CHECK(n==0x40000&&!memcmp(old,spi,n));uint8_t *raw;size_t count;char err[256],receipt_path[4096];CHECK(!sp_load(destination,&raw,&count,n,err,sizeof err));CHECK(count==n&&!memcmp(raw,old,n));free(raw);snprintf(receipt_path,sizeof receipt_path,"%s.receipt.json",destination);CHECK(!sp_load(receipt_path,&raw,&count,4096,err,sizeof err));CHECK(count>128);free(raw);memcpy(spi,plan,n);writes++;return 0;}
/* Rename only transport operations in portable planner, so this test exercises
 * the real SHA/CRC, partition and layout implementation. */
#define sp_vli_begin unused_vli_begin
#define sp_vli_finish unused_vli_finish
#define sp_vli_read unused_vli_read
#define sp_vli_program unused_vli_program
#include "../src/vli.c"
#undef sp_vli_begin
#undef sp_vli_finish
#undef sp_vli_read
#undef sp_vli_program
static int invoke(const char *cmd,const char *file,const char *model,const char *hash,const char *backup){char *argv[]={"test",(char*)cmd,(char*)file,"--device","1","--auto-model",(char*)model,"--sha256",(char*)hash,"--backup",(char*)backup};opens=hub_opens=writes=0;destination=backup;cases++;return sp_vli_cli(11,argv);}
static void clear(const char *backup){char p[4096];snprintf(p,sizeof p,"%s.receipt.json",backup);if(access(backup,F_OK)==0)CHECK(!unlink(backup));if(access(p,F_OK)==0)CHECK(!unlink(p));}
int main(int argc,char **argv){CHECK(argc==6);char err[256],dir[]="build/usb-cli-XXXXXX";CHECK(mkdtemp(dir));char backup[4096];snprintf(backup,sizeof backup,"%s/backup.bin",dir);uint8_t *simage,*hub,*pd,*old;size_t sn,hn,pn,on;CHECK(!sp_load(argv[1],&simage,&sn,0x400000,err,sizeof err));CHECK(!sp_load(argv[2],&hub,&hn,0x100000,err,sizeof err));CHECK(!sp_load(argv[3],&pd,&pn,0x100000,err,sizeof err));CHECK(!sp_load(argv[4],&old,&on,0x100000,err,sizeof err));scaler=malloc(0x400000);spi=malloc(0x40000);CHECK(scaler&&spi);memset(scaler,255,0x400000);memcpy(scaler,simage,sn);memset(spi,255,0x40000);memcpy(spi,hub,hn);memcpy(spi+0x20000,old,on);sp_vli_image info;CHECK(!sp_vli_validate(pd,pn,&info,err,sizeof err));
 CHECK(invoke("flash-usb",argv[3],"OLED",info.sha256,backup)==2);CHECK(!opens&&!writes);
 CHECK(invoke("flash-usb",argv[3],"ES07D03","wrong",backup)==2);CHECK(!opens&&!writes);
 CHECK(invoke("flash-usb",argv[3],"ES07D02",info.sha256,backup)==1);CHECK(opens==1&&!hub_opens&&!writes);
 scaler[1]^=1;CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(!hub_opens&&!writes);scaler[1]^=1;
 pair_fail=true;CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(hub_opens==1&&!writes);pair_fail=false;
 cleanup_fail=true;CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(!hub_opens&&!writes);cleanup_fail=false;
 bad_read=true;CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(!writes&&access(backup,F_OK));bad_read=false;
 spi[31]^=1;CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(!writes&&access(backup,F_OK));spi[31]^=1;
 char receipt[4096];CHECK(snprintf(receipt,sizeof receipt,"%s.receipt.json",backup)>0);FILE *f=fopen(receipt,"w");CHECK(f);fputs("existing",f);CHECK(!fclose(f));CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==1);CHECK(!writes);clear(backup);
 CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==0);CHECK(opens==1&&hub_opens==1&&writes==1);clear(backup);
 CHECK(invoke("flash-usb",argv[3],"ES07D03",info.sha256,backup)==0);CHECK(!writes);clear(backup);
 CHECK(invoke("verify-usb",argv[3],"ES07D03",info.sha256,backup)==0);CHECK(!writes);
 CHECK(invoke("verify-usb",argv[4],"ES07D03",info.sha256,backup)==1);CHECK(!writes);
 CHECK(!sp_vli_validate(hub,hn,&info,err,sizeof err));CHECK(invoke("flash-usb",argv[2],"ES07D03",info.sha256,backup)==0);CHECK(writes==1);clear(backup);
 CHECK(invoke("verify-usb",argv[2],"ES07D03",info.sha256,backup)==0);CHECK(!writes);
 f=fopen(argv[5],"w");CHECK(f);fprintf(f,"{\"scenarios_passed\":%u,\"assertions\":%u,\"hardware_access\":false}\n",cases,checks);CHECK(!fclose(f));free(simage);free(hub);free(pd);free(old);free(scaler);free(spi);CHECK(!rmdir(dir));return 0;
}
