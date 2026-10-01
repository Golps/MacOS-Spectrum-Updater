#include "vli-cli.h"
#include "vli.h"
#include "macos.h"
#include "known-firmware.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *opt(int argc,char **argv,const char *key){for(int i=3;i+1<argc;i++)if(!strcmp(argv[i],key))return argv[i+1];return NULL;}
static void progress(void *ctx,const char *phase,uint32_t done,uint32_t total){(void)ctx;printf("%s %u/%u\n",phase,done,total);fflush(stdout);}
static uint32_t le32(const uint8_t *d){return d[0]|((uint32_t)d[1]<<8)|((uint32_t)d[2]<<16)|((uint32_t)d[3]<<24);}
static int prove_model(uint64_t id,const char *model,char *err,size_t z){
    sp_macos native;sp_session s={0};if(sp_macos_open(id,&native,&s.io,err,z))return -1;
    int rc=-1;uint8_t *raw=NULL;
    if(sp_begin(&s)){snprintf(err,z,"Scaler model check: %s",s.error);goto cleanup;}
    raw=malloc(SP_LIMIT-SP_BASE);if(!raw){snprintf(err,z,"Out of memory during model verification.");goto cleanup;}
    if(sp_read(&s,SP_BASE,raw,SP_LIMIT-SP_BASE,progress,NULL)){snprintf(err,z,"%s",s.error);goto cleanup;}
    uint32_t footer=le32(raw+0x10020);sp_image info;
    if(footer<0x33880||footer>SP_LIMIT-SP_BASE-4||sp_validate_image(raw,footer+4,&info,err,z)||!sp_known_model(info.sha256,model)){snprintf(err,z,"Installed scaler is not a known compatible %s image. USB flash stopped before erase.",model);goto cleanup;}
    printf("Model profile verified: %s (exact compatible installed scaler match).\n",model);rc=0;
cleanup:free(raw);if(sp_finish(&s)){snprintf(err,z,"Scaler model-check cleanup failed. Power-cycle before retrying.");rc=-1;}sp_macos_close(&native);return rc;
}
static int receipt(const char *file,const char *sha,const sp_vli_session *s,char *err,size_t z){char path[4096],json[384];int len=snprintf(path,sizeof path,"%s.receipt.json",file);if(len<0||(size_t)len>=sizeof path){snprintf(err,z,"USB backup receipt path too long.");return -1;}len=snprintf(json,sizeof json,"{\"raw_sha256\":\"%s\",\"bytes\":%u,\"controller\":\"VL822Q7+VL103\",\"jedec\":\"%02X%02X%02X\"}\n",sha,s->capacity,s->jedec[0],s->jedec[1],s->jedec[2]);if(len<0||(size_t)len>=sizeof json)return -1;return sp_save_exclusive(path,(uint8_t*)json,(size_t)len,err,z);}
int sp_vli_cli(int argc,char **argv){
    if(argc<2)return -1;bool inspect=!strcmp(argv[1],"inspect-usb"),flash=!strcmp(argv[1],"flash-usb"),verify=!strcmp(argv[1],"verify-usb");
    if(!inspect&&!flash&&!verify)return -1;if(argc<3){fprintf(stderr,"USB command requires a vendor binary.\n");return 2;}
    uint8_t *image=NULL;size_t n=0;char err[256]={0};sp_vli_image info;
    if(sp_load(argv[2],&image,&n,0x100000,err,sizeof err)||sp_vli_validate(image,n,&info,err,sizeof err)){fprintf(stderr,"%s\n",err);free(image);return 1;}
    printf("Image: %s\nBytes: %zu\nSHA256: %s\nStock release: %s\nComponent: %s\nValidated: exact vendor SHA256 and %s\n",argv[2],n,info.sha256,info.version,info.kind==SP_VLI_PD?"VL103 Power Delivery":"VL822Q7 USB hub",info.kind==SP_VLI_PD?"VL103 identity/CRC16":"VL822 header/CRC8");fflush(stdout);
    if(inspect){free(image);return 0;}
    const char *device=opt(argc,argv,"--device"),*model=opt(argc,argv,"--auto-model"),*expected=opt(argc,argv,"--sha256"),*backup=opt(argc,argv,"--backup");
    if(!device||!sp_supported_model(model)||opt(argc,argv,"--confirm")||(flash&&(!expected||strcmp(expected,info.sha256)||!backup))){fprintf(stderr,"USB operation requires an explicit bridge registry ID, supported automatic model profile and, for writes, exact target hash and new backup path.\n");free(image);return 2;}
    errno=0;char *tail;uint64_t id=strtoull(device,&tail,10);if(errno||!*device||*tail||!id||(flash&&access(backup,F_OK)==0)){fprintf(stderr,"Invalid device or existing backup destination. Nothing sent.\n");free(image);return 2;}
    char lock[4096];size_t len=confstr(_CS_DARWIN_USER_TEMP_DIR,lock,sizeof lock);if(!len||len>=sizeof lock-24){free(image);return 1;}strcat(lock,"spectrum-updater.lock");int fd=open(lock,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);struct stat st;
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||st.st_nlink!=1||flock(fd,LOCK_EX|LOCK_NB)){fprintf(stderr,"Another updater may be active, or private session lock failed.\n");if(fd>=0)close(fd);free(image);return 1;}
    struct sigaction guard={0},old_int,old_term,old_pipe;guard.sa_handler=SIG_IGN;sigaction(SIGINT,&guard,&old_int);sigaction(SIGTERM,&guard,&old_term);sigaction(SIGPIPE,&guard,&old_pipe);
    int rc=1;bool model_session=false;sp_macos native;sp_vli_session s={0};uint8_t *saved=NULL,*second=NULL,*planned=NULL;
    uint32_t awake=sp_macos_keep_awake(err,sizeof err);if(!awake){fprintf(stderr,"%s\n",err);goto unlocked;}
    model_session=true;if(prove_model(id,model,err,sizeof err)){fprintf(stderr,"%s\n",err);goto unlocked;}
    if(sp_macos_open_paired_hub(id,&native,&s.io,err,sizeof err)){fprintf(stderr,"%s\n",err);goto unlocked;}
    if(sp_vli_begin(&s)){fprintf(stderr,"%s\n",s.error);goto cleanup;}
    printf("USB flash profile: VL822Q7, JEDEC %02X%02X%02X, %u bytes.\n",s.jedec[0],s.jedec[1],s.jedec[2],s.capacity);fflush(stdout);
    saved=malloc(s.capacity);second=malloc(s.capacity);planned=malloc(s.capacity);if(!saved||!second||!planned){fprintf(stderr,"Out of memory. Nothing erased.\n");goto cleanup;}
    if(sp_vli_read(&s,0,saved,s.capacity,progress,NULL)||sp_vli_read(&s,0,second,s.capacity,progress,NULL)){fprintf(stderr,"%s\n",s.error);goto cleanup;}
    if(memcmp(saved,second,s.capacity)){fprintf(stderr,"Independent shared-SPI reads differ. Nothing erased.\n");goto cleanup;}
    if(sp_vli_plan(saved,s.capacity,image,n,planned,&info,err,sizeof err)){fprintf(stderr,"%s\n",err);goto cleanup;}
    if(verify){if(memcmp(saved,planned,s.capacity)){fprintf(stderr,"Installed USB component differs from selected file.\n");goto cleanup;}puts("Byte-exact component verification succeeded.");rc=0;goto cleanup;}
    if(sp_save_exclusive(backup,saved,s.capacity,err,sizeof err)){fprintf(stderr,"%s\n",err);goto cleanup;}char sha[65];sp_sha256(saved,s.capacity,sha);
    if(receipt(backup,sha,&s,err,sizeof err)){fprintf(stderr,"USB backup receipt failed: %s\nNothing erased.\n",err);goto cleanup;}
    printf("Backup: %s\nBackup bytes: %u\nBackup SHA256: %s\n",backup,s.capacity,sha);fflush(stdout);
    if(!memcmp(saved,planned,s.capacity)){puts("Requested firmware is already installed. No erase performed.");rc=0;goto cleanup;}
    if(sp_vli_program(&s,saved,planned,s.capacity,progress,NULL)){fprintf(stderr,"%s\n",s.error);goto cleanup;}
    puts("USB programming and complete shared-SPI readback succeeded.");rc=0;
cleanup:free(saved);free(second);free(planned);if(sp_vli_finish(&s)){fprintf(stderr,"USB session cleanup errors: %u.\n",s.cleanup_errors);rc=1;}sp_macos_close(&native);
unlocked:sp_macos_release_awake(awake);if(model_session)puts("ISP session ended. Unplug monitor DC power for 10 seconds, then reconnect. Keep the backup and log if any step failed.");sigaction(SIGINT,&old_int,NULL);sigaction(SIGTERM,&old_term,NULL);sigaction(SIGPIPE,&old_pipe,NULL);close(fd);free(image);return rc;
}
