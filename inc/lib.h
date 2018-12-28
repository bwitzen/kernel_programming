/*
 * Main public header file for our user-land support library,
 * whose code lives in the lib directory.
 * This library is roughly our OS's version of a standard C library,
 * and is intended to be linked into all user-mode applications
 * (NOT the kernel or boot loader).
 */

#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H 1

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/trap.h>

#define USED(x)     (void)(x)

/* main user program */
void    umain(int argc, char **argv);

/* libmain.c or entry.S */
extern const char *binaryname;
extern const volatile struct env *thisenv;
extern const volatile struct env envs[NENV];
extern const volatile struct page_info pages[];

/* exit.c */
void    exit(void);

/* readline.c */
char*   readline(const char *buf);

/* syscall.c */
void    sys_cputs(const char *string, size_t len);
int sys_cgetc(void);
envid_t sys_getenvid(void);
int sys_env_destroy(envid_t);
void    sys_yield(void);
int     sys_wait(envid_t);
envid_t sys_fork(void);
envid_t sys_ipc_recv(void *dst, size_t *sz);
int sys_ipv_send(envid_t envid, void *src, size_t sz);
void *sys_shmem_alloc(size_t sz, int key);
void *sys_shmem_attach(int key);

/* fork.c */
envid_t fork(void);
void *sys_vma_create(size_t size, int perm, int flags);
int sys_vma_destroy(void *va, size_t size);


/* File open modes */
#define O_RDONLY    0x0000      /* open for reading only */
#define O_WRONLY    0x0001      /* open for writing only */
#define O_RDWR      0x0002      /* open for reading and writing */
#define O_ACCMODE   0x0003      /* mask for above modes */

#define O_CREAT     0x0100      /* create if nonexistent */
#define O_TRUNC     0x0200      /* truncate to zero length */
#define O_EXCL      0x0400      /* error if already exists */
#define O_MKDIR     0x0800      /* create directory, not regular file */

/* Virtual Memory Area permissions */
#define PERM_R	    0x0001
#define PERM_W	    0x0002

/* Virtual Memory Area flags */
#define MAP_POPULATE    0x0001

#endif  /* !JOS_INC_LIB_H */
