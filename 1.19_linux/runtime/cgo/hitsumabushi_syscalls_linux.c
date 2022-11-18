// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2022 The Hitsumabushi Authors

// This file defines C functions and system calls for Cgo.

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h> // for usleep

#include "libcgo.h"
#include "libcgo_unix.h"

typedef unsigned int gid_t;

extern int hitsumabushi_clock_gettime(clockid_t clk_id, struct timespec *tp);
extern int32_t hitsumabushi_getproccount();

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  abort();
  return NULL;
}

int munmap(void *addr, size_t length) {
  abort();
  return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
  // Do nothing.
  return 0;
}

int setegid(gid_t gid) {
  // Do nothing.
  return 0;
}

int seteuid(uid_t gid) {
  // Do nothing.
  return 0;
}

int setgid(gid_t gid) {
  // Do nothing.
  return 0;
}

int setgroups(size_t size, const gid_t *list) {
  // Do nothing.
  return 0;
}

int setregid(gid_t rgid, gid_t egid) {
  // Do nothing.
  return 0;
}

int setreuid(uid_t ruid, uid_t euid) {
  // Do nothing.
  return 0;
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
  // Do nothing.
  return 0;
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
  // Do nothing.
  return 0;
}

int setuid(uid_t gid) {
  // Do nothing.
  return 0;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  // Do nothing.
  return 0;
}

int sigaddset(sigset_t *set, int signum) {
  // Do nothing.
  return 0;
}

int sigemptyset(sigset_t *set) {
  // Do nothing.
  return 0;
}

int sigfillset(sigset_t *set) {
  // Do nothing.
  return 0;
}

int sigismember(const sigset_t *set, int signum) {
  // Do nothing.
  return 0;
}

static const int kFDOffset = 100;

typedef struct {
  const void* content;
  size_t      content_size;
  size_t      current;
  int32_t     fd;
} pseudo_file;

// TODO: Do we need to protect this by mutex?
static pseudo_file pseudo_files[100];

static pthread_mutex_t* pseudo_file_mutex() {
  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  return &mutex;
}

static int32_t open_pseudo_file(const void* content, size_t content_size) {
  pthread_mutex_lock(pseudo_file_mutex());

  int index = 0;
  int found = 0;
  for (int i = 0; i < sizeof(pseudo_files) / sizeof(pseudo_file); i++) {
    if (pseudo_files[i].fd == 0) {
      index = i;
      found = 1;
      break;
    }
  }
  if (!found) {
    // Too many pseudo files are opened.
    pthread_mutex_unlock(pseudo_file_mutex());
    return -1;
  }
  int32_t fd = index + kFDOffset;
  pseudo_files[index].content = content;
  pseudo_files[index].content_size = content_size;
  pseudo_files[index].current = 0;
  pseudo_files[index].fd = fd;

  pthread_mutex_unlock(pseudo_file_mutex());
  return fd;
}

static size_t read_pseudo_file(int32_t fd, void *p, int32_t n) {
  pthread_mutex_lock(pseudo_file_mutex());

  int32_t index = fd - kFDOffset;
  pseudo_file *file = &pseudo_files[index];
  size_t rest = file->content_size - file->current;
  if (rest < n) {
    n = rest;
  }
  memcpy(p, file->content + file->current, n);
  pseudo_files[index].current += n;

  pthread_mutex_unlock(pseudo_file_mutex());
  return n;
}

static void close_pseudo_file(int32_t fd) {
  pthread_mutex_lock(pseudo_file_mutex());

  int32_t index = fd - kFDOffset;
  pseudo_files[index].content = NULL;
  pseudo_files[index].content_size = 0;
  pseudo_files[index].current = 0;
  pseudo_files[index].fd = 0;

  pthread_mutex_unlock(pseudo_file_mutex());
}

int32_t hitsumabushi_closefd(int32_t fd) {
  if (fd >= kFDOffset) {
    close_pseudo_file(fd);
    return 0;
  }
  fprintf(stderr, "syscall close(%d) is not implemented\n", fd);
  return 0;
}

int64_t hitsumabushi_nanotime1() {
  struct timespec tp;
  hitsumabushi_clock_gettime(CLOCK_MONOTONIC, &tp);
  return (int64_t)(tp.tv_sec) * 1000000000ll + (int64_t)tp.tv_nsec;
}

int32_t hitsumabushi_open(char *name, int32_t mode, int32_t perm) {
  if (strcmp(name, "/proc/self/auxv") == 0) {
    static const char auxv[] =
      "\x06\x00\x00\x00\x00\x00\x00\x00"  // _AT_PAGESZ tag (6)
      "\x00\x10\x00\x00\x00\x00\x00\x00"  // 4096 bytes per page
      "\x00\x00\x00\x00\x00\x00\x00\x00"  // Dummy bytes
      "\x00\x00\x00\x00\x00\x00\x00\x00"; // Dummy bytes
    return open_pseudo_file(auxv, sizeof(auxv) / sizeof(char));
  }
  if (strcmp(name, "/sys/kernel/mm/transparent_hugepage/hpage_pmd_size") == 0) {
    static const char hpage_pmd_size[] =
      "\x30\x5c"; // '0', '\n'
    return open_pseudo_file(hpage_pmd_size, sizeof(hpage_pmd_size) / sizeof(char));
  }
  fprintf(stderr, "syscall open(%s, %d, %d) is not implemented\n", name, mode, perm);
  const static int kENOENT = 0x2;
  return kENOENT;
}

int32_t hitsumabushi_sched_getaffinity(pid_t pid, size_t cpusetsize, void *mask) {
    int32_t numcpu = hitsumabushi_getproccount();
    for (int32_t i = 0; i < numcpu; i += 8)
        ((unsigned char*)mask)[i / 8] = (unsigned char)((1u << (numcpu - i)) - 1);
    // https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html
    // > On success, the raw sched_getaffinity() system call returns the
    // > number of bytes placed copied into the mask buffer;
    return (numcpu + 7) / 8;
}

int32_t hitsumabushi_read(int32_t fd, void *p, int32_t n) {
  if (fd >= kFDOffset) {
    return read_pseudo_file(fd, p, n);
  }
  fprintf(stderr, "syscall read(%d, %p, %d) is not implemented\n", fd, p, n);
  const static int kEBADF = 0x9;
  return kEBADF;
}

void hitsumabushi_usleep(useconds_t usec) {
  usleep(usec);
}

void hitsumabushi_walltime1(int64_t* sec, int32_t* nsec) {
  struct timespec tp;
  hitsumabushi_clock_gettime(CLOCK_REALTIME, &tp);
  *sec = tp.tv_sec;
  *nsec = tp.tv_nsec;
}

int32_t hitsumabushi_write1(uintptr_t fd, void *p, int32_t n) {
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  int32_t ret = 0;
  pthread_mutex_lock(&m);
  switch (fd) {
  case 1:
    ret = fwrite(p, 1, n, stdout);
    fflush(stdout);
    break;
  case 2:
    ret = fwrite(p, 1, n, stderr);
    fflush(stderr);
    break;
  default:
    fprintf(stderr, "syscall write(%lu, %p, %d) is not implemented\n", fd, p, n);
    break;
  }
  pthread_mutex_unlock(&m);
  return ret;
}
