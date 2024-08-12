#include "pager.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/mman.h>

#include "mmu.h"

struct frame_data
{
    pid_t pid;
    int page;
    int prot;
    int dirty;
    int secondChance;
};

struct page_data
{
    int block;
    int on_disk;
    int frame;
    intptr_t bl_addr;
};

struct proc
{
    pid_t pid;
    int npages;
    int maxpages;
    struct page_data *pages;
};

struct pager
{
    struct frame_data *frames;
    int nframes;
    int nblocks;
    int free_blocks;
    struct proc *procs;
    int nprocs;
    int max_procs;
};

struct pager pager;

void pager_init(int nframes, int nblocks)
{
    pager.frames = (struct frame_data *)malloc(sizeof(struct frame_data) * nframes);
    pager.nframes = nframes;
    pager.nblocks = nblocks;
    pager.free_blocks = nblocks;

    // Inicializando os frames com valores padrão
    for (int i = 0; i < nframes; i++)
    {
        pager.frames[i].pid = -1;
        pager.frames[i].page = -1;
        pager.frames[i].prot = 0;
        pager.frames[i].dirty = 0;
    }

    pager.procs = NULL;
    pager.nprocs = 0;
    pager.max_procs = 0;
}

void pager_create(pid_t pid)
{   
    if (pager.nprocs == pager.max_procs)
    {
        pager.max_procs = pager.max_procs == 0 ? 4 : pager.max_procs * 2;
        pager.procs = (struct proc *)realloc(pager.procs, sizeof(struct proc) * pager.max_procs);
    }

    struct proc *new_proc = &pager.procs[pager.nprocs++];
    new_proc->pid = pid;
    new_proc->npages = 0;
    new_proc->maxpages = 4; // Capacidade inicial de páginas
    new_proc->pages = (struct page_data *)malloc(sizeof(struct page_data) * new_proc->maxpages);

    // Inicializando os dados das páginas com valores padrão
    for (int i = 0; i < new_proc->maxpages; i++)
    {
        new_proc->pages[i].block = -1;
        new_proc->pages[i].on_disk = 0;
        new_proc->pages[i].frame = -1;
    }
}

void *pager_extend(pid_t pid)
{
    struct proc *proc = NULL;
    for (int i = 0; i < pager.nprocs; i++) {
        if (pager.procs[i].pid == pid) {
            proc = &pager.procs[i];
            break;
        }
    }
    if (!proc) return NULL;

    if (pager.free_blocks == 0) {
        return NULL;  // Sem blocos de disco disponíveis
    }
    
    if (proc->npages == proc->maxpages) {
        proc->maxpages *= 2;
        proc->pages = (struct page_data *)realloc(proc->pages, sizeof(struct page_data) * proc->maxpages);
    }

    struct page_data *page = &proc->pages[proc->npages++];
    page->block = pager.nblocks - pager.free_blocks--;  // Atribui um bloco de disco
    page->on_disk = 1;  // Marca como estando no disco
    page->frame = -1;   // Ainda não alocou um frame de memória física

    void *virtual_address = (void *)(UVM_BASEADDR + (page->block*sysconf(_SC_PAGESIZE)));
    page->bl_addr = (intptr_t)virtual_address;
    // Retorna o endereço virtual da nova página (representado aqui como NULL)
    return virtual_address;
}

int findAndUpdate(){
    for(int i = 0; i < pager.nframes; i++) 
    { 
         
        if(pager.frames[i].dirty == 0) 
        {
            pager.frames[i].dirty = 1;
            pager.frames[i].secondChance = 1;
             
            return i; 
        } 
    } 
     
    return -1; 
}

int findAndReplase(){
    for(int i = 0; i < pager.nframes; i++) 
    { 
         
        if(pager.frames[i].secondChance == 0) 
        {
             
            return i; 
        } 
    } 
     
    return -1; 
}

void pager_fault(pid_t pid, void *addr)
{
    struct proc *proc = NULL;
    for (int i = 0; i < pager.nprocs; i++) {
        if (pager.procs[i].pid == pid) {
            proc = &pager.procs[i];
            break;
        }
    }

    int page;


    for (int y = 0; y < pager.nprocs; y++) {
        if(proc->pages[y].bl_addr == addr){

            page = y;

            break;
        }else if(y == pager.nprocs){
            int frame = findAndUpdate();

            if(frame == -1){
                frame = findAndReplase();
            }

            proc->pages[y + 1].frame = frame;
            //proc->pages[y + 1].bl_addr = addr;
            
            mmu_zero_fill(frame);
            mmu_resident(pid, addr, frame, PROT_READ);
        }
    }

    if(proc->pages[page].frame != 0){
        mmu_chprot(pid, addr, PROT_READ | PROT_WRITE);

        pager.frames[proc->pages[page].frame].secondChance = 0;
    }


}

int pager_syslog(pid_t pid, void *addr, size_t len)
{
    struct proc *proc = NULL;
    for (int i = 0; i < pager.nprocs; i++) {
        if (pager.procs[i].pid == pid) {
            proc = &pager.procs[i];
            break;
        }
    }

    int *buf = addr;

    for (int y = 0; y < pager.nprocs; y++) {
        if(((proc->pages[y].bl_addr <= *buf) && *buf <= (proc->pages[y].bl_addr + 0xFFF)) && ((proc->pages[y].bl_addr <= (*buf + len)) && (*buf + len) <= proc->pages[y].bl_addr + 0xFFF)){
            for(int i = 0; i < len; i++) {        // len é o número de bytes a imprimir
                printf("%02x", (unsigned)buf[i]); // buf contém os dados a serem impressos
            }

            return 0;
        }
    }

    return -1;
}

void pager_destroy(pid_t pid)
{
    for(int i = 0; i < pager.nprocs; i++){
        if (pager.procs[i].pid == pid) {
            free(pager.procs[i].pages);
            break;
        }
    }
}