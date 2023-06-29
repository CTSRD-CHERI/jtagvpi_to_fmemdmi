#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "fmemdmi.h"

#define DEBUG

#ifdef DEBUG
#define DEBUG_FPRINTF(stream, ...)  ({\
  fprintf ( stream, "DEBUG(%s:%s:l%d)"\
          , __FILE__, __PRETTY_FUNCTION__, __LINE__ );\
  fprintf (stream, __VA_ARGS__);\
  fflush (stream);\
})
#define DEBUG_PRINTF(...)  DEBUG_FPRINTF(stderr, __VA_ARGS__)
#else
#define DEBUG_FPRINTF(...)
#define DEBUG_PRINTF(...)
#endif

static inline void fprintf_hex (FILE* stream, uint8_t* buff, int length)
{
  int i;
  for (i = 0; i < length; i++) {
    fprintf(stream, " %02x", buff[i]);
  }
}

struct fmem_request {
  uint32_t offset;
  uint32_t data;
  uint32_t access_width;
};

#define FMEM_READ  _IOWR('X', 1, struct fmem_request)
#define FMEM_WRITE _IOWR('X', 2, struct fmem_request)

static int fmem_read(int fd, uint32_t offset) {
  struct fmem_request req;
  int error;

  req.offset = offset;
  req.access_width = 4;

  DEBUG_PRINTF(": fmem read ioctl, offset = 0x%0x\n", req.offset);
  error = ioctl(fd, FMEM_READ, &req);
  DEBUG_PRINTF( ": fmem read ioctl returned %d, req.data = 0x%0x\n"
              , error, req.data);

  if (error == 0) return req.data;

  return 0;
}

static int fmem_write(int fd, uint32_t offset, uint32_t data) {
  struct fmem_request req;
  int error;

  req.offset = offset;
  req.data = data;
  req.access_width = 4;

  DEBUG_PRINTF( ": fmem write ioctl, offset = 0x%0x, data = 0x%0x\n"
              , req.offset, req.data);
  error = ioctl(fd, FMEM_WRITE, &req);
  DEBUG_PRINTF( ": fmem write ioctl returned %d\n", error);

  return error;
}

static int fmem_open (const char * fname)
{
  DEBUG_PRINTF("\n");
  return open(fname, O_RDWR);
}

static int fmem_close (int fd)
{
  DEBUG_PRINTF("\n");
  return close (fd);
}

// fmemdmi public API
/////////////////////

static int fmemdmi_fd;

void fmemdmi_init (const char * fname) {
  DEBUG_PRINTF("\n");
  fmemdmi_fd = fmem_open (fname);
  if (fmemdmi_fd < 0) {
    perror("open() failed");
    DEBUG_PRINTF ("fmemdmi_fd: 0x%0x\n", fmemdmi_fd);
    abort ();
  }
}

void fmemdmi_deinit () {
  DEBUG_PRINTF("\n");
  fmem_close (fmemdmi_fd);
}

void fmemdmi_write (uint16_t addr, uint32_t data)
{
  DEBUG_PRINTF ("addr %x: data %x\n", addr, data);
  //fflush(stdout);
  fmem_write (fmemdmi_fd, addr * 4, data);
}

uint32_t fmemdmi_read (uint16_t addr)
{
  DEBUG_PRINTF("addr %x\n", addr);
  uint32_t reg;
  reg = fmem_read (fmemdmi_fd, addr * 4);
  DEBUG_PRINTF (", val %x\n", reg);
  //fflush(stdout);
  return reg;
}

uint32_t fmemdmi_req (dmi_op_t op, uint16_t addr, uint32_t data)
{
  DEBUG_PRINTF("DMI req @0x%0x\n", addr);
  uint32_t ret = 0;
  switch (op) {
    case DMI_OP_READ:
      DEBUG_PRINTF("DMI read req\n");
      ret = fmemdmi_read (addr);
      break;
    case DMI_OP_WRITE:
      DEBUG_PRINTF("DMI write req (data: 0x%0x)\n", data);
      fmemdmi_write (addr, data);
      break;
  }
  return ret;
}
