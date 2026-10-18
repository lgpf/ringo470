/* plataforma POSIX (Linux / ESP32 VFS): arquivos e pastas */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ringo_plat.h"
#include "ringo_core.h"
#ifndef PLAT_ROOT
#define PLAT_ROOT "/SD"
#endif
#ifndef PLAT_CFG
#define PLAT_CFG "/SD/RINGO.CFG"
#endif
int plat_list(const char*dir,void(*cb)(const char*,int,void*),void*u){DIR*d=opendir(dir[0]?dir:PLAT_ROOT);struct dirent*e;char p[512];
  if(!d)return 0;while((e=readdir(d))){struct stat st;if(e->d_name[0]=='.')continue;snprintf(p,sizeof p,"%s/%s",dir,e->d_name);
    cb(e->d_name,stat(p,&st)==0&&S_ISDIR(st.st_mode),u);}closedir(d);return 1;}
long plat_load(const char*path,uint8_t*buf,long max){FILE*f=fopen(path,"rb");long n;if(!f)return -1;n=(long)fread(buf,1,max,f);fclose(f);return n;}
int plat_save(const char*path,const uint8_t*buf,long len){FILE*f=fopen(path,"wb");if(!f)return 0;fwrite(buf,1,len,f);fclose(f);return 1;}
int plat_del(const char*path){return remove(path)==0;}
char plat_sep(void){return '/';}
int plat_isroot(const char*dir){return !strcmp(dir,PLAT_ROOT);}
int plat_cfg_read(char*b,int max){FILE*f=fopen(PLAT_CFG,"rb");int n;if(!f)return 0;n=(int)fread(b,1,max,f);b[n]=0;fclose(f);return n;}
int plat_cfg_write(const char*b){FILE*f=fopen(PLAT_CFG,"wb");if(!f)return 0;fputs(b,f);fclose(f);return 1;}
