// PS5 OpenGL - OpenGL implementation for PlayStation 5.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern int sceKernelUsleep(uint32_t microseconds);

__attribute__((constructor)) static void ps5_opengl_open_log(void) {
  FILE *stream = freopen("/download0/ps5-opengl.log", "w", stdout);
  /* Start a fresh receipt, then make both independent streams append-only. */
  if (stream != NULL)
    stream = freopen("/download0/ps5-opengl.log", "a", stdout);
  if (stream != NULL)
    setvbuf(stream, NULL, _IONBF, 0);
  stream = freopen("/download0/ps5-opengl.log", "a", stderr);
  if (stream != NULL)
    setvbuf(stream, NULL, _IONBF, 0);
}

__attribute__((noreturn)) void catchReturnFromMain(int status) {
  printf("[ps5-opengl-native] gate completed status=%d\n", status);
  fflush(NULL);
  for (;;)
    sceKernelUsleep(100000);
}

__attribute__((noreturn)) void __assert(const char *function, const char *file,
                                        int line, const char *expression) {
  fprintf(stderr, "assertion failed: %s (%s:%d, %s)\n", expression, file, line,
          function);
  abort();
}

int mkstemps(char *template_name, int suffix_length) {
  (void)template_name;
  (void)suffix_length;
  errno = ENOSYS;
  return -1;
}

void openlog(const char *identifier, int option, int facility) {
  (void)identifier;
  (void)option;
  (void)facility;
}

FILE *popen(const char *command, const char *mode) {
  (void)command;
  (void)mode;
  errno = ENOSYS;
  return NULL;
}

int pclose(FILE *stream) {
  (void)stream;
  errno = ENOSYS;
  return -1;
}
