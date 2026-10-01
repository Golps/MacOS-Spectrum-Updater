#include "macos.h"
#include "vli-cli.h"
#include "known-firmware.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
static void usage(void){puts("Spectrum Updater 1.0 — macOS IPS firmware tool\n"
"  spectrum-updater inspect IMAGE.bin\n"
"  spectrum-updater inspect-usb HUB_OR_PD.bin\n"
"  spectrum-updater flash-usb HUB_OR_PD.bin --device REGISTRY_ID\n"
"        --auto-model MODEL --sha256 EXPECTED_HASH --backup NEW_BACKUP.bin\n"
"  spectrum-updater verify-usb HUB_OR_PD.bin --device REGISTRY_ID --auto-model MODEL\n"
"  spectrum-updater inspect-backup RAW_BACKUP.bin\n"
"  spectrum-updater devices\n"
"  spectrum-updater backup OUT.bin --device REGISTRY_ID --confirm ES07D03\n"
"  spectrum-updater verify IMAGE.bin --device REGISTRY_ID --confirm ES07D03\n"
"  spectrum-updater flash IMAGE.bin --device REGISTRY_ID --confirm ES07D03\n"
"                   --sha256 EXPECTED_HASH --backup NEW_BACKUP.bin\n"
"  spectrum-updater restore RAW_BACKUP.bin --device REGISTRY_ID --confirm ES07D03\n"
"                   --sha256 RAW_BACKUP_HASH --backup NEW_BACKUP.bin\n"
"App operations use --auto-model ES07D03 instead of a manual confirmation.\n"
"Automatic model verification requires a known compatible installed scaler image.\n"
"inspect is offline; devices reads the OS registry. backup/verify enter ISP and\n"
"can interrupt the display. flash writes scaler FW2; flash-usb writes the paired hub/PD SPI.\n"
"No automatic device choice, driver seizure, privileged helper or root elevation.");}
static const char *option(int argc,char **argv,const char *name){for(int i=3;i<argc;i++)if(!strcmp(argv[i],name)&&i+1<argc)return argv[i+1];return NULL;}

static void progress(void *context,const char *phase,uint32_t done,uint32_t total){(void)context;printf("%s %u/%u\n",phase,done,total);fflush(stdout);}
static uint32_t le32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int current_image(const uint8_t *d,sp_image *info,char *err,size_t z){uint32_t footer=le32(d+0x10020);if(footer>SP_LIMIT-SP_BASE-4||footer<0x33880){snprintf(err,z,"Current FW2 footer invalid; cannot establish the read path. Nothing erased.");return -1;}return sp_validate_image(d,footer+4,info,err,z);}
static int backup_receipt(const char *path,const char *raw_hash,const char *source_hash,char *err,size_t z){
    char receipt_path[4096],receipt[256];
    int length=snprintf(receipt_path,sizeof receipt_path,"%s.receipt.json",path);
    if(length<0||(size_t)length>=sizeof receipt_path){snprintf(err,z,"Backup receipt path is too long. Nothing erased.");return -1;}
    length=snprintf(receipt,sizeof receipt,"{\"raw_sha256\":\"%s\",\"source_sha256\":\"%s\",\"bytes\":%u}\n",raw_hash,source_hash,SP_LIMIT-SP_BASE);
    if(length<0||(size_t)length>=sizeof receipt){snprintf(err,z,"Backup receipt could not be encoded. Nothing erased.");return -1;}
    return sp_save_exclusive(receipt_path,(const uint8_t *)receipt,(size_t)length,err,z);
}
int main(int argc,char **argv){
    int usb_result=sp_vli_cli(argc,argv);if(usb_result>=0)return usb_result;
    if(argc==2&&!strcmp(argv[1],"devices"))return sp_macos_list()?1:0;
    if(argc<3){usage();return 2;}const char *cmd=argv[1],*file=argv[2];bool inspect=!strcmp(cmd,"inspect"),inspect_backup=!strcmp(cmd,"inspect-backup"),backup=!strcmp(cmd,"backup"),verify=!strcmp(cmd,"verify"),flash=!strcmp(cmd,"flash"),restore=!strcmp(cmd,"restore");
    if(!(inspect||inspect_backup||backup||verify||flash||restore)){usage();return 2;}char err[256]={0};uint8_t *image=NULL;size_t n=0;sp_image info={0};
    if(!backup){if(sp_load(file,&image,&n,SP_LIMIT-SP_BASE,err,sizeof err)||((restore||inspect_backup)?sp_validate_backup(image,n,&info,err,sizeof err):sp_validate_image(image,n,&info,err,sizeof err))){fprintf(stderr,"%s\n",err);free(image);return 1;}
        printf("Image: %s\nBytes: %zu\nSHA256: %s\nStock release: %s\nValidated: outer/application CRC32, staged main-build CRC16 and %u component CRC16 values\nFlash interval: 0x%06X–0x%06X; erase span %zu bytes\n",file,n,info.sha256,info.stock?info.stock:"experimental/unrecognized",info.components,SP_BASE,SP_BASE+(unsigned)n,(n+SP_BLOCK-1)/SP_BLOCK*SP_BLOCK);fflush(stdout);}
    if(inspect||inspect_backup){free(image);return 0;}
    const char *device=option(argc,argv,"--device"),*manual_model=option(argc,argv,"--confirm"),*auto_model=option(argc,argv,"--auto-model"),*expected=option(argc,argv,"--sha256"),*out=option(argc,argv,"--backup");
    const char *model=auto_model?auto_model:manual_model;
    if(!device||!model||!sp_supported_model(model)||(auto_model&&manual_model)){fprintf(stderr,"An explicit registry ID and one supported IPS model-verification mode are required.\n");free(image);return 2;}
    if(!backup){sp_image target_info=info;
        if(restore&&current_image(image,&target_info,err,sizeof err)){fprintf(stderr,"Saved backup has no valid embedded firmware.\n");free(image);return 2;}
        if(!sp_known_model(target_info.sha256,model)){fprintf(stderr,"This target image is not in the selected IPS model catalog. Nothing sent.\n");free(image);return 2;}}
    errno=0;char *tail;unsigned long long id=strtoull(device,&tail,10);if(errno||!*device||*tail||!id){fprintf(stderr,"Invalid registry ID.\n");free(image);return 2;}
    if((flash||restore)&&(!expected||strcmp(expected,info.sha256)||!out||(flash&&!info.stock))){fprintf(stderr,"Flash/restore requires the exact SHA256, a new backup path, and a stock image for installation.\n");free(image);return 2;}
    /* Reserve an exclusive backup path before touching hardware. Defer content
       creation to the durable exclusive writer; never overwrite an old backup. */
    if(backup||flash||restore){const char *p=backup?file:out;if(access(p,F_OK)==0){fprintf(stderr,"Backup destination already exists. Nothing sent.\n");free(image);return 2;}}
    /* Serialize every ISP operation across app and CLI. The per-user lock is
       opened without following symlinks; flock automatically releases on exit. */
    char lock[4096];size_t dirlen=confstr(_CS_DARWIN_USER_TEMP_DIR,lock,sizeof lock);
    if(!dirlen||dirlen>=sizeof lock-24){fprintf(stderr,"Private macOS temporary directory unavailable.\n");free(image);return 1;}
    strcat(lock,"spectrum-updater.lock");int lockfd=open(lock,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK,0600);struct stat lockstat;
    if(lockfd<0||fstat(lockfd,&lockstat)||!S_ISREG(lockstat.st_mode)||lockstat.st_uid!=getuid()||lockstat.st_nlink!=1||flock(lockfd,LOCK_EX|LOCK_NB)){fprintf(stderr,"Another updater may be active, or private session lock failed.\n");if(lockfd>=0)close(lockfd);free(image);return 1;}
    /* Do not exit midway through an erase/program on terminal interrupt. A
       process kill, host crash or loss of power still cannot be prevented. */
    struct sigaction guarded={0},old_int,old_term,old_pipe;guarded.sa_handler=SIG_IGN;sigaction(SIGINT,&guarded,&old_int);sigaction(SIGTERM,&guarded,&old_term);sigaction(SIGPIPE,&guarded,&old_pipe);
    sp_macos native;sp_session s={0};int rc=1;
    uint32_t awake=sp_macos_keep_awake(err,sizeof err);if(!awake){fprintf(stderr,"%s\n",err);goto unlocked;}
    if(sp_macos_open((uint64_t)id,&native,&s.io,err,sizeof err)){fprintf(stderr,"%s\n",err);goto unlocked;}
    if(sp_begin(&s)){fprintf(stderr,"%s\n",s.error);goto cleanup;}
    printf("Flash profile: JEDEC %02X%02X%02X, %u MiB; firmware window 0x400000–0x800000.\n",s.jedec[0],s.jedec[1],s.jedec[2],s.flash_capacity/1048576u);fflush(stdout);
    uint8_t *saved=NULL;
    if(backup||flash||restore){size_t size=SP_LIMIT-SP_BASE;saved=malloc(size);uint8_t *second=malloc(size);if(!saved||!second){fprintf(stderr,"Out of memory.\n");free(second);goto free_backup;}
        if(sp_read(&s,SP_BASE,saved,size,progress,NULL)||sp_read(&s,SP_BASE,second,size,progress,NULL)){fprintf(stderr,"%s\n",s.error);free(second);goto free_backup;}
        if(memcmp(saved,second,size)){fprintf(stderr,"Independent FW2 reads differ; nothing erased.\n");free(second);goto free_backup;}free(second);
        sp_image installed={0};bool current_valid=current_image(saved,&installed,err,sizeof err)==0;
        if(!current_valid&&!restore){fprintf(stderr,"Current FW2 validation failed: %s\n",err);goto free_backup;}
        if(auto_model&&(!current_valid||!sp_known_model(installed.sha256,model))){fprintf(stderr,"Automatic model identification could not recognize the installed firmware as a known compatible IPS release. Nothing erased. Keep the current firmware and retain this log.\n");goto free_backup;}
        if(auto_model)printf("Model profile verified: %s (exact compatible installed firmware match).\n",model);
        if(current_valid)printf("Current FW2: %s, SHA256 %s\n",installed.stock?installed.stock:"unrecognized but validated container",installed.sha256);
        else printf("Current FW2 is invalid. A complete rescue backup will be saved before restoration.\n");
        if(sp_save_exclusive(backup?file:out,saved,size,err,sizeof err)){fprintf(stderr,"%s\n",err);goto free_backup;}char h[65];sp_sha256(saved,size,h);
        if(backup_receipt(backup?file:out,h,current_valid?installed.sha256:"",err,sizeof err)){fprintf(stderr,"Backup receipt failed: %s\nNothing erased.\n",err);goto free_backup;}
        printf("Backup: %s\nBackup bytes: %zu\nBackup SHA256: %s\n",backup?file:out,size,h);fflush(stdout);
        if(backup){rc=0;goto free_backup;}
        if((restore&&!memcmp(saved,image,size))||(!restore&&!strcmp(installed.sha256,info.sha256))){printf("Requested firmware is already installed. No erase performed.\n");rc=0;goto free_backup;}
    }
    if(verify){uint8_t *readback=malloc(n);if(!readback){fprintf(stderr,"Out of memory.\n");goto free_backup;}if(sp_read(&s,SP_BASE,readback,n,progress,NULL)){fprintf(stderr,"%s\n",s.error);free(readback);goto free_backup;}if(memcmp(image,readback,n)){fprintf(stderr,"Installed FW2 differs from selected file.\n");free(readback);goto free_backup;}free(readback);puts("Byte-exact verification succeeded.");rc=0;goto free_backup;}
    if(sp_flash_setup(&s)||(restore?sp_restore(&s,image,n,progress,NULL):sp_program(&s,image,n,saved,SP_LIMIT-SP_BASE,progress,NULL))){fprintf(stderr,"%s\n",s.error);goto free_backup;}puts("Programming and full erase-span readback succeeded.");rc=0;
free_backup: free(saved);
cleanup: if(sp_finish(&s)){fprintf(stderr,"Session cleanup errors: %u.\n",s.cleanup_errors);rc=1;}sp_macos_close(&native);puts("ISP session ended. Unplug monitor DC power for 10 seconds, then reconnect. If a secondary update appears, let it finish and power-cycle again. Factory fallback behavior has not been verified on this monitor.");
unlocked: sp_macos_release_awake(awake);sigaction(SIGINT,&old_int,NULL);sigaction(SIGTERM,&old_term,NULL);sigaction(SIGPIPE,&old_pipe,NULL);close(lockfd);free(image);return rc;
}
