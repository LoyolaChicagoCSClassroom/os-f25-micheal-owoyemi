// fat.c
// FAT12/FAT16 driver: fatInit, fatOpen, fatRead

#include <stdint.h>
#include <stddef.h>
#include "fat.h"
#include "ide.h"

#define DEFAULT_SECTOR_SIZE 512

/* ------------------------------------------------------------------ */
/* Minimal bump allocator + libc helpers (freestanding). */
static unsigned char heap_area[8192];
static unsigned int heap_off = 0;
static void *malloc(size_t n){ if(!n) return NULL; n=(n+3)&~3; if(heap_off+n>sizeof(heap_area)) return NULL; void *p=&heap_area[heap_off]; heap_off+=n; return p; }
static void free(void *p){ (void)p; }
static void *memcpy_local(void *d,const void *s,size_t n){ unsigned char *D=d; const unsigned char *S=s; while(n--) *D++=*S++; return d; }
static void *memset_local(void *d,int v,size_t n){ unsigned char *D=d; while(n--) *D++=(unsigned char)v; return d; }
static int memcmp_local(const void *a,const void *b,size_t n){ const unsigned char *x=a,*y=b; while(n--){ if(*x!=*y) return (int)*x-(int)*y; x++; y++; } return 0; }
static size_t strlen_local(const char *s){ size_t n=0; while(s[n]) n++; return n; }
static const char *strchr_local(const char *s,int c){ while(*s){ if(*s==(char)c) return s; s++; } return NULL; }
static int toupper_local(int c){ return (c>='a'&&c<='z')?c-'a'+'A':c; }

/* ------------------------------------------------------------------ */
/* Driver state */
static struct boot_sector bootsector_buf __attribute__((aligned(4)));
static unsigned char *fat_table = NULL;
static unsigned int bytes_per_sector = DEFAULT_SECTOR_SIZE;
static unsigned int reserved_sectors = 0;
static unsigned int num_fats = 0;
static unsigned int sectors_per_fat = 0;
static unsigned int root_dir_entries = 0;
static unsigned int root_dir_sectors = 0;
static unsigned int root_dir_start_sector = 0;
static unsigned int sectors_per_cluster = 0;
static unsigned int first_data_sector = 0;
static unsigned int total_sectors = 0;
static unsigned int fat_sectors = 0;
static int fat_type = 16; /* 12 or 16; reject 32 */

/* ------------------------------------------------------------------ */
static int read_sectors(unsigned int lba, unsigned char *buf, unsigned int count){ return (ata_lba_read(lba, buf, count)<0)?-1:0; }

static void determine_fat_type(void){ unsigned int data_sectors = total_sectors - first_data_sector; unsigned int total_clusters = data_sectors / sectors_per_cluster; if(total_clusters < 4085) fat_type=12; else if(total_clusters < 65525) fat_type=16; else fat_type=32; }

static void compute_layout(void){
    reserved_sectors    = bootsector_buf.num_reserved_sectors;
    num_fats            = bootsector_buf.num_fat_tables;
    root_dir_entries    = bootsector_buf.num_root_dir_entries;
    sectors_per_fat     = bootsector_buf.num_sectors_per_fat;
    sectors_per_cluster = bootsector_buf.num_sectors_per_cluster;
    total_sectors = bootsector_buf.total_sectors ? bootsector_buf.total_sectors : bootsector_buf.total_sectors_in_fs;
    root_dir_sectors = ((root_dir_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    root_dir_start_sector = bootsector_buf.num_hidden_sectors + reserved_sectors + (num_fats * sectors_per_fat);
    first_data_sector = root_dir_start_sector + root_dir_sectors;
}

static uint32_t get_fat_entry(uint32_t cluster){
    if(!fat_table) return 0xFFFFFFFF;
    if(fat_type==16){ const uint16_t *ft=(const uint16_t*)fat_table; return ft[cluster]; }
    if(fat_type==12){ uint32_t off=(cluster*3)/2; uint16_t val=fat_table[off] | (fat_table[off+1]<<8); if(cluster&1) val=(val>>4)&0x0FFF; else val &=0x0FFF; return val; }
    return 0xFFFFFFFF;
}

static void make_8dot3(const char *in,char out11[11]){
    for(int i=0;i<11;i++) out11[i]=' ';
    const char *dot=strchr_local(in,'.');
    int nname = dot ? (int)(dot - in) : (int)strlen_local(in);
    if(nname>8) nname=8;
    for(int i=0;i<nname;i++) out11[i]=(char)toupper_local((unsigned char)in[i]);
    if(dot){ const char *ext=dot+1; int e=0; while(e<3 && ext[e]){ out11[8+e]=(char)toupper_local((unsigned char)ext[e]); e++; } }
}

int fatInit(void){
    unsigned char sector[DEFAULT_SECTOR_SIZE];
    if(read_sectors(0, sector, 1)<0) return -1; /* try LBA0 */
    memcpy_local(&bootsector_buf, sector, sizeof(struct boot_sector));
    int ok=0;
    if(bootsector_buf.boot_signature==0xAA55){ if(!memcmp_local(bootsector_buf.fs_type,"FAT12",5) || !memcmp_local(bootsector_buf.fs_type,"FAT16",5)) ok=1; }
    if(!ok){ /* fallback typical partition start */
        if(read_sectors(2048, sector, 1)<0) return -1;
        memcpy_local(&bootsector_buf, sector, sizeof(struct boot_sector));
        if(bootsector_buf.boot_signature!=0xAA55) return -1;
        if(memcmp_local(bootsector_buf.fs_type,"FAT12",5) && memcmp_local(bootsector_buf.fs_type,"FAT16",5)) return -1;
    }
    bytes_per_sector = bootsector_buf.bytes_per_sector;
    if(!(bytes_per_sector==512 || bytes_per_sector==1024 || bytes_per_sector==2048 || bytes_per_sector==4096)) return -1;
    compute_layout();
    determine_fat_type();
    if(fat_type==32) return -1;
    fat_sectors = sectors_per_fat;
    size_t fat_bytes = (size_t)sectors_per_fat * bytes_per_sector;
    fat_table = (unsigned char*)malloc(fat_bytes);
    if(!fat_table) return -1;
    unsigned int fat_lba = bootsector_buf.num_hidden_sectors + reserved_sectors;
    if(read_sectors(fat_lba, fat_table, sectors_per_fat)<0){ free(fat_table); fat_table=NULL; return -1; }
    return 0;
}

struct file *fatOpen(const char *filename){
    if(!filename || !fat_table) return NULL;
    char want[11]; make_8dot3(filename,want);
    size_t dir_bytes = (size_t)root_dir_sectors * bytes_per_sector;
    unsigned char *dir = (unsigned char*)malloc(dir_bytes);
    if(!dir) return NULL;
    if(read_sectors(root_dir_start_sector, dir, root_dir_sectors)<0){ free(dir); return NULL; }
    int entries = (int)root_dir_entries;
    for(int i=0;i<entries;i++){
        unsigned char *e = dir + i*32;
        unsigned char first = e[0];
        if(first==0x00) break; /* end */
        if(first==0xE5) continue; /* deleted */
        uint8_t attr = e[11];
        if(attr & 0x08) continue; /* volume label */
        if(attr & FILE_ATTRIBUTE_SUBDIRECTORY) continue; /* skip directories */
        if(!memcmp_local(e, want, 11)){
            struct file *f = (struct file*)malloc(sizeof(struct file));
            if(!f){ free(dir); return NULL; }
            memset_local(f,0,sizeof(*f));
            memcpy_local(&f->rde, e, sizeof(struct root_directory_entry));
            f->start_cluster = f->rde.cluster;
            free(dir);
            return f;
        }
    }
    free(dir);
    return NULL;
}

int fatRead(struct file *f, void *buf, uint32_t count, uint32_t offset){
    if(!f || !buf || !fat_table) return -1;
    uint32_t size = f->rde.file_size;
    if(offset >= size) return 0;
    if(offset + count > size) count = size - offset;
    uint8_t *out = (uint8_t*)buf;
    uint32_t bpc = sectors_per_cluster * bytes_per_sector;
    uint32_t cluster = f->start_cluster; if(cluster < 2) return -1;
    uint32_t cluster_index = offset / bpc;
    uint32_t cluster_offset = offset % bpc;
    for(uint32_t i=0;i<cluster_index;i++){
        uint32_t next = get_fat_entry(cluster);
        if((fat_type==16 && next>=0xFFF8) || (fat_type==12 && next>=0xFF8)) return 0; /* offset past EOF */
        if(next==0x0000 || next==0xFFFFFFFF) return -1;
        cluster = next;
    }
    uint32_t read = 0;
    while(read < count){
        uint32_t first_sector = first_data_sector + (cluster - 2) * sectors_per_cluster;
        for(uint32_t s=0; s<sectors_per_cluster && read < count; s++){
            unsigned char *sec = (unsigned char*)malloc(bytes_per_sector);
            if(!sec) return -1;
            if(read_sectors(first_sector + s, sec, 1)<0){ free(sec); return -1; }
            uint32_t in_off = cluster_offset;
            uint32_t avail = bytes_per_sector - in_off;
            uint32_t need  = count - read;
            uint32_t copy  = (avail < need) ? avail : need;
            memcpy_local(out + read, sec + in_off, copy);
            read += copy;
            free(sec);
            cluster_offset = 0; /* after first sector */
        }
        if(read >= count) break;
        uint32_t next = get_fat_entry(cluster);
        if(fat_type==16){ if(next>=0xFFF8) break; }
        else if(fat_type==12){ if(next>=0xFF8) break; }
        else return -1;
        if(next==0x0000 || next==0xFFFFFFFF) return -1;
        cluster = next;
    }
    return (int)read;
}

int fatReadCompat(struct file *f, void *buf, uint32_t count){ return fatRead(f, buf, count, 0); }

/* Demo helper: call from kernel main to show file contents (TESTFILE.TXT). */
int fatDemo(void){
    if(fatInit()<0) return -1;
    struct file *f = fatOpen("TESTFILE.TXT");
    if(!f) return -1;
    unsigned char tmp[256];
    int n = fatRead(f, tmp, sizeof(tmp)-1, 0);
    if(n<0) return -1; tmp[n]='\0';
    volatile uint16_t *vga=(uint16_t*)0xB8000; uint8_t attr=0x07; for(int i=0; i<n && i<80*24; i++) vga[i]=(attr<<8)|tmp[i];
    return 0;
}

void fatShutdown(void){ if(fat_table){ free(fat_table); fat_table=NULL; } }
