/* One optional package directory, with explicit MCC-root resource fallback. */
#include "ae_mcc_boot.h" /* (Arena Evolved) */
#include "mcc_package.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int mcc_package_name(char const *name, char canonical[64])
{
    size_t i, start=9, separators=0, length;
    static char const prefix[]="mcc_maps";
    if (!name) return 0;
    /* (Arena Evolved) MCC's own maps in place keep their names: ui, <name>@mcc (ae_mcc_boot.h) */
    if (ae_mcc_boot_ui(name)) {strcpy(canonical,"ui");return 1;}
    if (ae_mcc_install_level(name)) {snprintf(canonical,64,"%s",ae_mcc_install_level_identity(name));return 1;}
    for (length=0;length<64 && name[length];length++) {}
    if (length<10 || length>63) return 0;
    for (i=0;i<8;i++) {
        unsigned char c=(unsigned char)name[i];
        if (c>='A' && c<='Z') c+='a'-'A';
        if (c!=prefix[i]) return 0;
    }
    if (name[8]!='/' && name[8]!='\\') return 0;
    memcpy(canonical,"mcc_maps\\",9);
    for (i=9;i<=length;i++) {
        unsigned char c=(unsigned char)name[i];
        if (!c || c=='/' || c=='\\') {
            if (i==start || name[i-1]=='.' || name[i-1]==' ') return 0;
            if (c && ++separators>1) return 0;
            start=i+1;
            canonical[i]=c ? '\\' : 0;
        } else {
            if (c<32 || strchr(":*?\"<>|",c)) return 0;
            canonical[i]=(char)c;
        }
    }
    return 1;
}

int mcc_package_map_path(char const *name, char path[128])
{
    char canonical[64];
    if (!mcc_package_name(name,canonical)) return 0;
    if (ae_mcc_install_level(name)) return ae_mcc_install_level_path(name,path,128); /* (Arena Evolved) */
    sprintf(path,"d:\\%s.map",canonical);
    return 1;
}

int mcc_package_resource_path(char const *name, unsigned type, char path[128])
{
    char canonical[64], *separator;
    size_t i;
    if (type<1 || type>2 || ae_mcc_install_level(name) || !mcc_package_name(name,canonical)) return 0; /* (AE: no package) */
    separator=strrchr(canonical,'\\');
    *separator=0;
    for (i=0;canonical[i];i++) if (canonical[i]=='\\') canonical[i]='/';
    sprintf(path,"%s/%s.map",canonical,type==1 ? "bitmaps" : "sounds");
    return 1;
}

FILE *mcc_package_resource_open(char const *name,unsigned type,char path[128])
{
    FILE *file;
    char root[128];
    path[0]=0;
    if (ae_mcc_install_level(name)) {errno=ENOENT;return NULL;} /* (Arena Evolved) no package: the install's shared files */
    if (!mcc_package_resource_path(name,type,path) ||
        !mcc_package_resource_path("mcc_maps/root",type,root)) {errno=EINVAL;return NULL;}
    errno=0;
    file=fopen(path,"rb");
    /* A present but unreadable or invalid override must never be replaced. */
    if (!file && errno==ENOENT && strcmp(path,root)) {
        strcpy(path,root);
        file=fopen(path,"rb");
    }
    return file;
}
