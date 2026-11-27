/* One optional package directory, with explicit MCC-root resource fallback. */
#include "ae_mcc_boot.h" /* (Arena Evolved) */
#include "mcc_package.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HALO_WEB
#include "../../web/src/web_mcc_storage.h"
#endif

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

/* While a downloaded package is checked in its staging folder, its own files are read
 * from there (mcc_share.c); nothing else is redirected, and the redirect is cleared
 * as soon as the check is done. */
static char mcc_package_staged_name[64];
static char mcc_package_staged_folder[96];

void mcc_package_stage_redirect(char const *name, char const *folder)
{
    char canonical[64];
    mcc_package_staged_name[0]=0;
    mcc_package_staged_folder[0]=0;
    if (!name || !folder || strlen(folder)>=sizeof(mcc_package_staged_folder) || !mcc_package_name(name,canonical)) return;
    strcpy(mcc_package_staged_name,canonical);
    strcpy(mcc_package_staged_folder,folder);
}

static int mcc_package_staged(char const *canonical)
{
    size_t i;
    if (!mcc_package_staged_name[0]) return 0;
    for (i=0;canonical[i] && mcc_package_staged_name[i];i++) {
        unsigned char a=(unsigned char)canonical[i],b=(unsigned char)mcc_package_staged_name[i];
        if (a>='A' && a<='Z') a=(unsigned char)(a+'a'-'A');
        if (b>='A' && b<='Z') b=(unsigned char)(b+'a'-'A');
        if (a!=b) return 0;
    }
    return canonical[i]==mcc_package_staged_name[i];
}

int mcc_package_map_path(char const *name, char path[128])
{
    char canonical[64];
    if (!mcc_package_name(name,canonical)) return 0;
    if (ae_mcc_install_level(name)) return ae_mcc_install_level_path(name,path,128); /* (Arena Evolved) */
    if (mcc_package_staged(canonical)) {
        char const *level=strrchr(canonical,'\\')+1;
        size_t i;
        int length=snprintf(path,128,"d:\\%s\\%s.map",mcc_package_staged_folder,level);
        if (length<0 || length>=128) return 0;
        for (i=3;i<(size_t)length;i++) if (path[i]=='/') path[i]='\\';
        return 1;
    }
#ifdef HALO_WEB
    if (!web_mcc_package_visible(canonical)) return 0;
#endif
    sprintf(path,"d:\\%s.map",canonical);
    return 1;
}

/* whether `canonical` is a level of the Forge package (mcc_maps\forge\<name>) */
static int mcc_package_forge_level(char const *canonical)
{
    static char const forge[]="mcc_maps\\" MCC_PACKAGE_FORGE "\\";
    size_t i;
    for (i=0;forge[i];i++) {
        unsigned char a=(unsigned char)canonical[i];
        if (a>='A' && a<='Z') a=(unsigned char)(a+'a'-'A');
        if (a!=(unsigned char)forge[i]) return 0;
    }
    return canonical[i]!=0;
}

/* (the last map asked about: the loader and the lobby's readiness polls ask
 * often, and the trailer changes only as Forge saves, which forgets it) */
static struct { char name[64], base[64]; int found, known; } mcc_package_forge_last;

void mcc_package_forge_forget(void)
{
    memset(&mcc_package_forge_last,0,sizeof(mcc_package_forge_last));
}

int mcc_package_forge_base(char const *name, char base[64])
{
    char canonical[64], path[128], text[1025], *line;
    unsigned char header[0x200];
    uint32_t state_offset, state_size, signature;
    size_t length, i;
    FILE *file;
    base[0]=0;
    if (!mcc_package_name(name,canonical) || !mcc_package_forge_level(canonical)) return 0;
    if (mcc_package_forge_last.known && !strcmp(mcc_package_forge_last.name,canonical)) {
        strcpy(base,mcc_package_forge_last.base);
        return mcc_package_forge_last.found;
    }
    mcc_package_forge_forget();
    strcpy(mcc_package_forge_last.name,canonical);
    mcc_package_forge_last.known=1;
    if (!mcc_package_map_path(canonical,path) || !(file=fopen(path,"rb"))) return 0;
    length=fread(header,1,sizeof(header),file);
    /* (Forge's trailer: its state's offset and size at 0x1E4, 'FLG1' at 0x1FC) */
    state_offset=(uint32_t)header[0x1E4]|(uint32_t)header[0x1E5]<<8|(uint32_t)header[0x1E6]<<16|(uint32_t)header[0x1E7]<<24;
    state_size=(uint32_t)header[0x1E8]|(uint32_t)header[0x1E9]<<8|(uint32_t)header[0x1EA]<<16|(uint32_t)header[0x1EB]<<24;
    signature=(uint32_t)header[0x1FC]|(uint32_t)header[0x1FD]<<8|(uint32_t)header[0x1FE]<<16|(uint32_t)header[0x1FF]<<24;
    if (length!=sizeof(header) || signature!=0x464C4731u || state_offset<0x800u || !state_size ||
        fseek(file,(long)state_offset,SEEK_SET)) {fclose(file);return 0;}
    length=fread(text,1,state_size<sizeof(text)-1 ? state_size : sizeof(text)-1,file);
    fclose(file);
    text[length]=0;
    if (!(line=strstr(text,"Base Map: "))) return 0;
    line+=10;
    for (i=0;line[i] && line[i]!='\r' && line[i]!='\n';i++) {}
    line[i]=0;
    /* (a base map in the Forge package would read its own trailer for ever) */
    if (!mcc_package_name(line,base) || mcc_package_forge_level(base)) {base[0]=0;return 0;}
    strcpy(mcc_package_forge_last.base,base);
    mcc_package_forge_last.found=1;
    return 1;
}

int mcc_package_resource_path(char const *name, unsigned type, char path[128])
{
    char canonical[64], *separator;
    size_t i;
    int staged,length;
    if (type<1 || type>2 || ae_mcc_install_level(name) || !mcc_package_name(name,canonical)) return 0; /* (AE: no package) */
    staged=mcc_package_staged(canonical);
#ifdef HALO_WEB
    if (!staged && !web_mcc_package_visible(canonical)) return 0;
#endif
    separator=strrchr(canonical,'\\');
    *separator=0;
    for (i=0;canonical[i];i++) if (canonical[i]=='\\') canonical[i]='/';
    length=snprintf(path,128,"%s/%s.map",staged ? mcc_package_staged_folder : canonical,type==1 ? "bitmaps" : "sounds");
    return length>0 && length<128;
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

int mcc_package_base_resource_path(unsigned type,unsigned set,char path[128])
{
    static char const *folders[]={"", "_shared/singleplayer/", "_shared/multiplayer/"};
    if (type<1 || type>2 || set>=MCC_PACKAGE_BASE_SETS) return 0;
    snprintf(path,128,"mcc_maps/%s%s.map",folders[set],type==1 ? "bitmaps" : "sounds");
    return 1;
}

int mcc_package_shared_resource(char const *path,unsigned type)
{
    char expected[128];
    unsigned set;
    if (!path) return 0;
    for (set=0;set<MCC_PACKAGE_BASE_SETS;set++)
        if (mcc_package_base_resource_path(type,set,expected) && !strcmp(path,expected)) return 1;
    return 0;
}

int mcc_package_resource_candidate(char const *name,unsigned type,unsigned set,char path[128])
{
    FILE *file;
    if (set>=MCC_PACKAGE_BASE_SETS || !mcc_package_resource_path(name,type,path)) return 0;
    if (!mcc_package_shared_resource(path,type)) {
        errno=0;
        file=fopen(path,"rb");
        if (file) {fclose(file);return 1;}
        if (errno!=ENOENT) return 0;
    }
    return mcc_package_base_resource_path(type,set,path);
}
