#include "common.h"
#include "fs.h"
#define DEFAULT_ENTRY ((void *)0x8048000)
#include "memory.h"
extern void ramdisk_read(void *buf, off_t offset, size_t len);
extern size_t get_ramdisk_size();
uintptr_t loader(_Protect *as, const char *filename) {
 // ramdisk_read(DEFAULT_ENTRY,0,get_ramdisk_size());
 int fd = fs_open(filename,0,0);
 size_t lens= fs_filesz(fd);
 void *pa;
 void *end = DEFAULT_ENTRY+lens;
 void *va=DEFAULT_ENTRY;
 int page_count=lens/PGSIZE+1;
 for(int i=0;i<page_count;i++)
 {
	pa = new_page();
    Log("Map va to pa: 0x%08x to 0x%08x", va, pa);
	 _map(as,va,pa);
	fs_read(fd,pa,(end-va)<PGSIZE ?(end - va):PGSIZE);
 }
 fs_close(fd);
return (uintptr_t)DEFAULT_ENTRY;
}
