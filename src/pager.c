#include "pager.h"
#include <stdlib.h>
#include <string.h>

struct frame_data
{
    pid_t pid;
    int page;
    int prot;
    int dirty;
};

struct page_data
{
    int block;
    int on_disk;
    int frame;
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

    // Retorna o endereço virtual da nova página (representado aqui como NULL)
    return NULL;
}

void pager_fault(pid_t pid, void *addr)
{
}

int pager_syslog(pid_t pid, void *addr, size_t len)
{
    return -1;
}

void pager_destroy(pid_t pid)
{
}