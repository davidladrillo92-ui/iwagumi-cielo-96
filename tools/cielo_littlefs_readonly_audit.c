#ifndef LFS_READONLY
#error Compile this audit with -DLFS_READONLY
#endif
#include "lfs.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
typedef struct {FILE* f;unsigned reads,denied,files,dirs;uint64_t bytes;} Context;
static int rd(const struct lfs_config*c,lfs_block_t b,lfs_off_t o,void*out,lfs_size_t n){
 Context*x=c->context;x->reads++;
 if(b>=384||o+n>4096)return LFS_ERR_IO;
 if(fseek(x->f,0x670000L+(long)b*4096+(long)o,SEEK_SET))return LFS_ERR_IO;
 return fread(out,1,n,x->f)==n?0:LFS_ERR_IO;
}
static int prog(const struct lfs_config*c,lfs_block_t b,lfs_off_t o,const void*p,lfs_size_t n){(void)b;(void)o;(void)p;(void)n;((Context*)c->context)->denied++;return LFS_ERR_IO;}
static int erase(const struct lfs_config*c,lfs_block_t b){(void)b;((Context*)c->context)->denied++;return LFS_ERR_IO;}
static int sync_ro(const struct lfs_config*c){(void)c;return 0;}
static int walk(lfs_t*l,Context*x,const char*path,unsigned depth){
 if(depth>8||x->files+x->dirs>512)return LFS_ERR_INVAL;
 lfs_dir_t d;int err=lfs_dir_open(l,&d,path);if(err)return err;
 struct lfs_info info;
 while((err=lfs_dir_read(l,&d,&info))>0){
  if(!strcmp(info.name,".")||!strcmp(info.name,".."))continue;
  if(info.type==LFS_TYPE_REG){x->files++;x->bytes+=info.size;}
  else if(info.type==LFS_TYPE_DIR){
   char next[1024];if(snprintf(next,sizeof(next),"%s/%s",path,info.name)>=(int)sizeof(next)){err=LFS_ERR_INVAL;break;}
   x->dirs++;err=walk(l,x,next,depth+1);if(err)break;
  }
 }
 int close=lfs_dir_close(l,&d);return err<0?err:close;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 Context x={0};x.f=fopen(argv[1],"rb");if(!x.f)return 3;
 if(fseek(x.f,0,SEEK_END)||ftell(x.f)!=0x800000L){fclose(x.f);return 4;}
 struct lfs_config c={0};c.context=&x;c.read=rd;c.prog=prog;c.erase=erase;c.sync=sync_ro;
 c.read_size=16;c.prog_size=16;c.block_size=4096;c.block_count=384;c.block_cycles=500;c.cache_size=256;c.lookahead_size=32;
 lfs_t l;int mounted=lfs_mount(&l,&c);struct lfs_fsinfo info={0};int status=-1,listed=-1;int used=-1;
 if(!mounted){status=lfs_fs_stat(&l,&info);listed=walk(&l,&x,"/",0);used=lfs_fs_size(&l);lfs_unmount(&l);}
 fclose(x.f);
 printf("{\"readonly_build\":true,\"mount_result\":%d,\"stat_result\":%d,\"list_result\":%d,\"disk_version\":%u,\"block_size\":%u,\"block_count\":%u,\"used_blocks\":%d,\"files\":%u,\"directories\":%u,\"file_bytes\":%"PRIu64",\"read_calls\":%u,\"write_or_erase_callbacks\":%u}\n",mounted,status,listed,info.disk_version,info.block_size,info.block_count,used,x.files,x.dirs,x.bytes,x.reads,x.denied);
 return mounted||status||listed||x.denied?1:0;
}
