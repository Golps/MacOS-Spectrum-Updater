#include "macos.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint32_t number(io_service_t service,const char *key){CFStringRef k=CFStringCreateWithCString(NULL,key,kCFStringEncodingUTF8);CFTypeRef v=IORegistryEntryCreateCFProperty(service,k,NULL,0);CFRelease(k);int32_t n=0;if(v&&CFGetTypeID(v)==CFNumberGetTypeID())CFNumberGetValue(v,kCFNumberSInt32Type,&n);if(v)CFRelease(v);return (uint32_t)n;}
static void text_property(io_service_t service,const char *key,char *out,size_t n){out[0]=0;CFStringRef k=CFStringCreateWithCString(NULL,key,kCFStringEncodingUTF8);CFTypeRef v=IORegistryEntryCreateCFProperty(service,k,NULL,0);CFRelease(k);if(v&&CFGetTypeID(v)==CFStringGetTypeID())CFStringGetCString(v,out,(CFIndex)n,kCFStringEncodingUTF8);if(v)CFRelease(v);}
static io_iterator_t iterator(void){io_iterator_t it=0;CFMutableDictionaryRef match=IOServiceMatching("IOUSBHostDevice");if(!match)return 0;if(IOServiceGetMatchingServices(kIOMainPortDefault,match,&it)!=KERN_SUCCESS)return 0;return it;}
static bool target(io_service_t s){return number(s,"idVendor")==0x2109&&number(s,"idProduct")==0x8886;}
static void json_string(const char *p){putchar('"');for(;*p;p++){unsigned char c=(unsigned char)*p;if(c=='"'||c=='\\'){putchar('\\');putchar(c);}else if(c<32)printf("\\u%04x",c);else putchar(c);}putchar('"');}
int sp_macos_list(void){
    io_iterator_t it=iterator();if(!it){fprintf(stderr,"Cannot enumerate macOS USB registry.\n");return -1;}io_service_t s;bool comma=false;printf("[");
    while((s=IOIteratorNext(it))){if(target(s)){uint64_t id=0;IORegistryEntryGetRegistryEntryID(s,&id);char product[256],serial[256];text_property(s,"USB Product Name",product,sizeof product);text_property(s,"USB Serial Number",serial,sizeof serial);if(comma)printf(",");comma=true;printf("{\"registry_id\":\"%llu\",\"location_id\":\"%08x\",\"vid\":\"2109\",\"pid\":\"8886\",\"product\":",(unsigned long long)id,number(s,"locationID"));json_string(product);printf(",\"serial\":");json_string(serial);printf("}");}IOObjectRelease(s);}IOObjectRelease(it);printf("]\n");return 0;
}
static ssize_t transfer(void *context,uint8_t type,uint8_t request,uint16_t value,uint16_t index,void *data,uint16_t n,uint32_t ms){
    sp_macos *m=context;IOUSBDevRequestTO r={0};r.bmRequestType=type;r.bRequest=request;r.wValue=value;r.wIndex=index;r.wLength=n;r.pData=data;r.noDataTimeout=ms;r.completionTimeout=ms;
    IOReturn result=(*m->usb)->DeviceRequestTO(m->usb,&r);if(result!=kIOReturnSuccess){fprintf(stderr,"IOKit USB failure 0x%08x on request %02x\n",(unsigned)result,request);return -1;}return (ssize_t)r.wLenDone;
}
static uint64_t now(void *context){(void)context;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static void sleep_ms(void *context,uint32_t ms){(void)context;struct timespec t={(time_t)(ms/1000),(long)(ms%1000)*1000000};while(nanosleep(&t,&t)&&t.tv_sec>=0){} }
static int open_role(uint64_t requested,uint16_t expected_pid,sp_macos *m,sp_transport *t,char *err,size_t z){
    memset(m,0,sizeof *m);io_iterator_t it=iterator();if(!it){snprintf(err,z,"Cannot enumerate USB registry.");return -1;}io_service_t s,chosen=0;
    while((s=IOIteratorNext(it))){uint64_t id=0;IORegistryEntryGetRegistryEntryID(s,&id);if(id==requested&&number(s,"idVendor")==0x2109&&number(s,"idProduct")==expected_pid){chosen=s;break;}IOObjectRelease(s);}IOObjectRelease(it);
    if(!chosen){snprintf(err,z,"Selected VIA bridge is absent. Re-enumerate; registry IDs change after reconnect.");return -1;}
    IOCFPlugInInterface **plugin=NULL;SInt32 score=0;IOReturn r=IOCreatePlugInInterfaceForService(chosen,kIOUSBDeviceUserClientTypeID,kIOCFPlugInInterfaceID,&plugin,&score);IOObjectRelease(chosen);
    if(r||!plugin){snprintf(err,z,"Cannot create macOS USB device interface (0x%08x).",(unsigned)r);return -1;}
    HRESULT hr=(*plugin)->QueryInterface(plugin,CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID500),(LPVOID *)&m->usb);(*plugin)->Release(plugin);
    if(hr||!m->usb){snprintf(err,z,"IOKit USB device interface unavailable.");return -1;}
    UInt16 vid=0,pid=0;(*m->usb)->GetDeviceVendor(m->usb,&vid);(*m->usb)->GetDeviceProduct(m->usb,&pid);if(vid!=0x2109||pid!=expected_pid){(*m->usb)->Release(m->usb);m->usb=NULL;snprintf(err,z,"USB identity changed.");return -1;}
    r=(*m->usb)->USBDeviceOpen(m->usb);if(r){(*m->usb)->Release(m->usb);m->usb=NULL;snprintf(err,z,"Cannot open VIA bridge (0x%08x). Check cable, selected hub source, and macOS USB permissions. The app never seizes the device.",(unsigned)r);return -1;}
    m->registry=requested;*t=(sp_transport){m,transfer,now,sleep_ms};return 0;
}
void sp_macos_close(sp_macos *m){if(m->usb){(*m->usb)->USBDeviceClose(m->usb);(*m->usb)->Release(m->usb);m->usb=NULL;}}

int sp_macos_open(uint64_t requested,sp_macos *m,sp_transport *t,char *err,size_t z){return open_role(requested,0x8886,m,t,err,z);}
int sp_macos_open_paired_hub(uint64_t requested,sp_macos *m,sp_transport *t,char *err,size_t z){
    io_iterator_t it=iterator();io_service_t s=0,bridge=0;
    if(!it){snprintf(err,z,"Cannot enumerate monitor USB hierarchy.");return -1;}
    while((s=IOIteratorNext(it))){uint64_t id=0;IORegistryEntryGetRegistryEntryID(s,&id);if(id==requested&&target(s)){bridge=s;break;}IOObjectRelease(s);}IOObjectRelease(it);
    if(!bridge){snprintf(err,z,"Selected monitor bridge is absent. Refresh Connection.");return -1;}
    uint64_t hub=0;io_registry_entry_t child=bridge;
    /* Only an actual ancestor of the selected billboard can supply shared SPI.
     * Never select another VIA hub by serial (many use placeholder serials). */
    for(unsigned depth=0;depth<16;depth++){
        io_registry_entry_t parent=0;IOReturn r=IORegistryEntryGetParentEntry(child,kIOServicePlane,&parent);IOObjectRelease(child);child=0;
        if(r||!parent)break;
        if(IOObjectConformsTo(parent,"IOUSBHostDevice")){
            if(number(parent,"idVendor")==0x2109&&number(parent,"idProduct")==0x2822)IORegistryEntryGetRegistryEntryID(parent,&hub);
            IOObjectRelease(parent);break; /* reject a different immediate USB parent */
        }
        child=parent;
    }
    if(child)IOObjectRelease(child);
    if(!hub){snprintf(err,z,"The selected monitor has no directly paired VIA 2109:2822 USB2 hub. Check the upstream cable and hub source. Nothing sent.");return -1;}
    return open_role(hub,0x2822,m,t,err,z);
}

uint32_t sp_macos_keep_awake(char *err,size_t z){IOPMAssertionID id=kIOPMNullAssertionID;
    IOReturn r=IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,kIOPMAssertionLevelOn,CFSTR("Spectrum firmware maintenance"),&id);
    if(r!=kIOReturnSuccess||id==kIOPMNullAssertionID){snprintf(err,z,"Cannot keep the Mac awake for firmware maintenance. Nothing sent.");return 0;}
    return id;
}
void sp_macos_release_awake(uint32_t id){if(id)IOPMAssertionRelease(id);}
