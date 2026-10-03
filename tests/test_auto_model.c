/* Runs the actual CLI entry point with a memory-only device backend. */
#define main updater_main
#include "../src/main.c"
#undef main
#include <assert.h>

int sp_vli_cli(int argc,char **argv) { (void)argc;(void)argv;return -1; }
static uint8_t *mock_contents;
static unsigned opens, writes, assertions, cases;
static const char *expected_backup;
static const char *selected_model="ES07D03";
#define CHECK(x) do { ++assertions; if (!(x)) { fprintf(stderr,"Model test failed line %d: %s\n",__LINE__,#x); abort(); } } while(0)
int sp_macos_list(void) { CHECK(false); return -1; }
uint32_t sp_macos_keep_awake(char *e,size_t z){(void)e;(void)z;return 1;}
void sp_macos_release_awake(uint32_t id){CHECK(id==1);}
int sp_macos_open(uint64_t id,sp_macos *m,sp_transport *t,char *e,size_t z) { (void)m;(void)t;(void)e;(void)z;CHECK(id==1);opens++;return 0; }
void sp_macos_close(sp_macos *m) { (void)m; }
int sp_begin(sp_session *s) { (void)s;return 0; }
int sp_finish(sp_session *s) { (void)s;return 0; }
int sp_read(sp_session *s,uint32_t address,uint8_t *out,size_t n,sp_progress p,void *c) { (void)s;(void)p;(void)c;CHECK(address==SP_BASE&&n<=SP_LIMIT-SP_BASE);memcpy(out,mock_contents,n);return 0; }
int sp_flash_setup(sp_session *s) { (void)s;uint8_t *backup=NULL;size_t n=0;char e[256],receipt[4096];CHECK(!sp_load(expected_backup,&backup,&n,SP_LIMIT-SP_BASE,e,sizeof e));CHECK(n==SP_LIMIT-SP_BASE&&!memcmp(backup,mock_contents,n));free(backup);snprintf(receipt,sizeof receipt,"%s.receipt.json",expected_backup);CHECK(!sp_load(receipt,&backup,&n,4096,e,sizeof e));CHECK(n>128);free(backup);return 0; }
int sp_program(sp_session *s,const uint8_t *im,size_t n,const uint8_t *backup,size_t size,sp_progress p,void *c) { (void)s;(void)im;(void)n;(void)p;(void)c;CHECK(size==SP_LIMIT-SP_BASE&&!memcmp(backup,mock_contents,size));writes++;return 0; }
int sp_restore(sp_session *s,const uint8_t *im,size_t n,sp_progress p,void *c) { (void)s;(void)im;(void)p;(void)c;CHECK(n==SP_LIMIT-SP_BASE);writes++;return 0; }

static void file_write(const char *path,const uint8_t *d,size_t n) { FILE *f=fopen(path,"wb");CHECK(f!=NULL);CHECK(fwrite(d,1,n,f)==n);CHECK(!fclose(f)); }
static void clear_backup(const char *path) { char receipt[4096];snprintf(receipt,sizeof receipt,"%s.receipt.json",path);CHECK(!unlink(path));CHECK(!unlink(receipt)); }
static int invoke(const char *verb,const char *input,const char *hash,const char *backup,bool experiment,bool both) {
    char *args[]={"test-updater",(char *)verb,(char *)input,"--device","1","--auto-model",(char *)selected_model,"--label-confirm",(char *)selected_model,"--sha256",(char *)hash,"--backup",(char *)backup,"--experimental","--confirm","ES07D03"};
    int count=experiment?14:13;if(both)count=16;
    opens=writes=0;expected_backup=backup;cases++;
    return updater_main(count,args);
}
int main(int argc,char **argv) {
    CHECK(argc==7);char e[256],directory[]="build/model-check-XXXXXX";CHECK(mkdtemp(directory)!=NULL);
    uint8_t *stock=NULL,*target=NULL;size_t sn=0,cn=0;
    CHECK(!sp_load(argv[1],&stock,&sn,SP_LIMIT-SP_BASE,e,sizeof e));
    CHECK(!sp_load(argv[2],&target,&cn,SP_LIMIT-SP_BASE,e,sizeof e));
    sp_image si,ci;CHECK(!sp_validate_image(stock,sn,&si,e,sizeof e));CHECK(!sp_validate_image(target,cn,&ci,e,sizeof e));
    CHECK(sp_known_es07d03(si.sha256));CHECK(sp_known_es07d03(ci.sha256));CHECK(!sp_known_es07d03(NULL));CHECK(!sp_known_es07d03(""));
    uint8_t *unknown=malloc(sn);CHECK(unknown!=NULL);memcpy(unknown,stock,sn);unknown[0]^=1;
    uint32_t crc=sp_crc32(unknown,sn-4);for(unsigned i=0;i<4;i++)unknown[sn-4+i]=(uint8_t)(crc>>(24-8*i));
    sp_image ui;CHECK(!sp_validate_image(unknown,sn,&ui,e,sizeof e));CHECK(!sp_known_es07d03(ui.sha256));
    char unknown_path[256],raw_path[256],backup[256];snprintf(unknown_path,sizeof unknown_path,"%s/unknown.bin",directory);file_write(unknown_path,unknown,sn);
    mock_contents=malloc(SP_LIMIT-SP_BASE);CHECK(mock_contents!=NULL);
    memset(mock_contents,0xa5,SP_LIMIT-SP_BASE);memcpy(mock_contents,stock,sn);
    snprintf(backup,sizeof backup,"%s/backup.bin",directory);
    CHECK(invoke("flash",argv[2],ci.sha256,backup,true,false)==0);CHECK(opens==1&&writes==1);clear_backup(backup);
    memcpy(mock_contents,unknown,sn);
    CHECK(invoke("flash",argv[2],ci.sha256,backup,true,false)==1);CHECK(opens==1&&writes==0&&access(backup,F_OK)!=0);
    mock_contents[1]^=1;
    CHECK(invoke("flash",argv[2],ci.sha256,backup,true,false)==1);CHECK(writes==0&&access(backup,F_OK)!=0);
    memcpy(mock_contents,stock,sn);
    CHECK(invoke("flash",unknown_path,ui.sha256,backup,true,false)==2);CHECK(opens==0&&writes==0);
    CHECK(invoke("flash",argv[2],si.sha256,backup,true,false)==2);CHECK(opens==0&&writes==0);
    CHECK(invoke("flash",argv[2],ci.sha256,backup,false,false)==0);CHECK(opens==1&&writes==1);clear_backup(backup);
    {
        char *args[]={"test-updater","flash",argv[5],"--device","1","--auto-model","ES07DC9","--sha256",(char *)"0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9","--backup",backup};
        selected_model="ES07DC9";opens=writes=0;expected_backup=backup;cases++;
        CHECK(updater_main(11,args)==1);CHECK(opens==1&&writes==0&&access(backup,F_OK)!=0);
        selected_model="ES07D03";
    }
    CHECK(invoke("flash",argv[2],ci.sha256,backup,true,true)==2);CHECK(opens==0&&writes==0);
    CHECK(invoke("flash",argv[1],si.sha256,backup,false,false)==0);CHECK(opens==1&&writes==0);clear_backup(backup);
    snprintf(raw_path,sizeof raw_path,"%s/raw.bin",directory);file_write(raw_path,mock_contents,SP_LIMIT-SP_BASE);
    char raw_hash[65];sp_sha256(mock_contents,SP_LIMIT-SP_BASE,raw_hash);
    memcpy(mock_contents,target,cn);
    CHECK(invoke("restore",raw_path,raw_hash,backup,false,false)==0);CHECK(opens==1&&writes==1);clear_backup(backup);
    mock_contents[0]^=1;
    CHECK(invoke("restore",raw_path,raw_hash,backup,false,false)==1);CHECK(opens==1&&writes==0);
    memcpy(mock_contents,target,cn);
    CHECK(invoke("verify",argv[2],ci.sha256,backup,false,false)==0);CHECK(opens==1&&writes==0);
    memcpy(mock_contents,unknown,sn);file_write(raw_path,mock_contents,SP_LIMIT-SP_BASE);sp_sha256(mock_contents,SP_LIMIT-SP_BASE,raw_hash);
    CHECK(invoke("restore",raw_path,raw_hash,backup,false,false)==2);CHECK(opens==0&&writes==0);
    memcpy(mock_contents,stock,sn);
    char blocked_receipt[4096];snprintf(blocked_receipt,sizeof blocked_receipt,"%s.receipt.json",backup);file_write(blocked_receipt,(const uint8_t *)"existing",8);
    CHECK(invoke("flash",argv[2],ci.sha256,backup,true,false)==1);CHECK(opens==1&&writes==0);clear_backup(backup);
    const char *models[]={"ES07D02","ES07DC9","ES07E30"};
    const char *installed_paths[]={argv[4],argv[6],argv[1]};
    const char *target_paths[]={argv[5],argv[2],argv[2]};
    for(unsigned model_index=0;model_index<3;model_index++){
        uint8_t *old_image=NULL,*new_image=NULL;size_t old_n=0,new_n=0;
        CHECK(!sp_load(installed_paths[model_index],&old_image,&old_n,SP_LIMIT-SP_BASE,e,sizeof e));
        CHECK(!sp_load(target_paths[model_index],&new_image,&new_n,SP_LIMIT-SP_BASE,e,sizeof e));
        sp_image target_info;CHECK(!sp_validate_image(new_image,new_n,&target_info,e,sizeof e));
        memset(mock_contents,0xa5,SP_LIMIT-SP_BASE);memcpy(mock_contents,old_image,old_n);
        selected_model=models[model_index];
        CHECK(invoke("flash",target_paths[model_index],target_info.sha256,backup,false,false)==0);CHECK(opens==1&&writes==1);clear_backup(backup);
        selected_model=model_index==0?"ES07D03":"ES07D02";
        CHECK(invoke("flash",target_paths[model_index],target_info.sha256,backup,false,false)==2);CHECK(opens==0&&writes==0);
        selected_model=models[model_index];file_write(raw_path,mock_contents,SP_LIMIT-SP_BASE);sp_sha256(mock_contents,SP_LIMIT-SP_BASE,raw_hash);
        memset(mock_contents,0xa5,SP_LIMIT-SP_BASE);memcpy(mock_contents,new_image,new_n);
        CHECK(invoke("restore",raw_path,raw_hash,backup,false,false)==0);CHECK(opens==1&&writes==1);clear_backup(backup);
        free(old_image);free(new_image);
    }
    FILE *result=fopen(argv[3],"w");CHECK(result!=NULL);fprintf(result,"{\"scenarios_passed\":%u,\"assertions\":%u,\"real_USB_operations\":false}\n",cases,assertions);CHECK(!fclose(result));
    unlink(unknown_path);unlink(raw_path);rmdir(directory);free(stock);free(target);free(unknown);free(mock_contents);
    printf("Automatic model guard: %u simulated scenarios passed; no real USB backend linked.\n",cases);return 0;
}
