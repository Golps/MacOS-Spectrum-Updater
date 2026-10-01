#include "spectrum.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
int sp_load(const char *path,uint8_t **out,size_t *n,size_t limit,char *err,size_t z){
    int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0){snprintf(err,z,"Open failed: %s",strerror(errno));return -1;}
    struct stat s;if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_size<=0||(uint64_t)s.st_size>limit){close(fd);snprintf(err,z,"Invalid regular file size.");return -1;}
    size_t size=(size_t)s.st_size;uint8_t *d=malloc(size);if(!d){close(fd);snprintf(err,z,"Out of memory.");return -1;}
    size_t at=0;while(at<size){ssize_t got=read(fd,d+at,size-at);if(got<0&&errno==EINTR)continue;if(got<=0){free(d);close(fd);snprintf(err,z,"File read failed.");return -1;}at+=(size_t)got;}
    uint8_t extra;ssize_t tail=read(fd,&extra,1);if(tail!=0){free(d);close(fd);snprintf(err,z,"File changed or read error.");return -1;}close(fd);*out=d;*n=size;return 0;
}
int sp_save_exclusive(const char *path,const uint8_t *d,size_t n,char *err,size_t z){
    int fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0){snprintf(err,z,"Backup creation failed: %s",strerror(errno));return -1;}
    size_t at=0;int rc=0;while(at<n){ssize_t put=write(fd,d+at,n-at);if(put<0&&errno==EINTR)continue;if(put<=0){rc=-1;break;}at+=(size_t)put;}
    /* Request the strongest macOS file flush before erase. Actual storage
       hardware can still fail; durability is a best-effort OS guarantee. */
    if(!rc&&(fsync(fd)||fcntl(fd,F_FULLFSYNC)))rc=-1;
    struct stat created,current;if(!rc&&fstat(fd,&created))rc=-1;
    if(!rc&&lseek(fd,0,SEEK_SET)<0)rc=-1;
    uint8_t chunk[8192];at=0;while(!rc&&at<n){size_t take=n-at<sizeof chunk?n-at:sizeof chunk;ssize_t got=read(fd,chunk,take);if(got<0&&errno==EINTR)continue;if(got<=0||memcmp(chunk,d+at,(size_t)got)){rc=-1;break;}at+=(size_t)got;}
    if(!rc&&(lstat(path,&current)||created.st_dev!=current.st_dev||created.st_ino!=current.st_ino||!S_ISREG(current.st_mode)||current.st_nlink!=1||(uint64_t)current.st_size!=n))rc=-1;
    char *copy=strdup(path);if(!copy)rc=-1;
    if(!rc){int dir=open(dirname(copy),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dir<0)rc=-1;else{if(fsync(dir))rc=-1;close(dir);}}
    free(copy);if(close(fd))rc=-1;
    if(rc){snprintf(err,z,"Backup write/synchronization failed; file may be incomplete.");return -1;}
    return 0;
}
