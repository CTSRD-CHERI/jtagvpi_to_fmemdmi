#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>

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

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static inline void fprintf_hex (FILE* stream, uint8_t* buff, int length)
{
  int i;
  for (i = 0; i < length; i++) {
    fprintf(stream, " %02x", buff[i]);
  }
}

#ifdef __cplusplus
extern "C" {
#endif

// socket helpers
////////////////////////////////////////////////////////////////////////////////

int socket_open(int port) {
  DEBUG_PRINTF("\n");
  int ret;
  int s;
  int c;
  struct sockaddr_in sockaddr;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket() failed");
    abort();
  }
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port == -1 ? 0 : port);
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ret = bind(s, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
  if (ret < 0) {
    perror("bind() failed");
    abort();
  }
  ret = listen(s, 1);
  if (ret < 0) {
    perror("listen() failed");
    abort();
  }
  return s;
}

int socket_accept(int fd) {
  assert(fd >= 0);
  DEBUG_PRINTF("\n");
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;
  int ret = poll(fds, 1, 0);
  if (ret < 0) {
    perror("poll() failed");
    abort();
  }
  if (ret == 0) return -1;
  return accept(fd, NULL, 0);
}

// jtagvpi API
////////////////////////////////////////////////////////////////////////////////

static int jtagvpi_fd;
static int jtagvpi_port = 5555;

void jtagvpi_init (int port) {
  int socket_port = port;
  if (port == 0) socket_port = jtagvpi_port;
  int temp_fd = socket_open (socket_port);
  while ((jtagvpi_fd = socket_accept (temp_fd)) <= 0);
}

void jtagvpi_deinit () {
  close (jtagvpi_fd);
}

#define BUF_SIZE                512

#define CMD_RESET               0
#define CMD_TMS_SEQ             1
#define CMD_SCAN_CHAIN          2
#define CMD_SCAN_CHAIN_FLIP_TMS 3
#define CMD_STOP_SIMU           4

static inline void fprintf_vpi_cmd (FILE* stream, int cmd)
{
  switch (cmd)
  {
    case CMD_RESET:
      fprintf(stream, "CMD_RESET"); break;
    case CMD_TMS_SEQ:
      fprintf(stream, "CMD_TMS_SEQ"); break;
    case CMD_SCAN_CHAIN:
      fprintf(stream, "CMD_SCAN_CHAIN"); break;
    case CMD_SCAN_CHAIN_FLIP_TMS:
      fprintf(stream, "CMD_SCAN_CHAIN_FLIP_TMS"); break;
    case CMD_STOP_SIMU:
      fprintf(stream, "CMD_STOP_SIMU"); break;
    default: fprintf(stream, "UNKNOWN"); break;
  }
}

struct vpi_cmd {
    int cmd;
    unsigned char buffer_out[BUF_SIZE];
    unsigned char buffer_in[BUF_SIZE];
    int length;
    int nb_bits;
};

#define JTAG_STATES \
  X(TEST_LOGIC_RESET) \
  X(RUN_TEST_IDLE) \
  X(SELECT_DR_SCAN) \
  X(CAPTURE_DR) \
  X(SHIFT_DR) \
  X(EXIT1_DR) \
  X(PAUSE_DR) \
  X(EXIT2_DR) \
  X(UPDATE_DR) \
  X(SELECT_IR_SCAN) \
  X(CAPTURE_IR) \
  X(SHIFT_IR) \
  X(EXIT1_IR) \
  X(PAUSE_IR) \
  X(EXIT2_IR) \
  X(UPDATE_IR)

typedef enum {
  #define X(name) name,
  JTAG_STATES
  #undef X
} jtag_state_t;

static inline void fprintf_jtag_state (FILE* stream, jtag_state_t s) {
  switch (s) {
    #define X(name) case name: fprintf (stream, STRINGIFY(name)); break;
    JTAG_STATES
    #undef X
    default:
      fprintf (stream, "UNKNOWN");
      break;
  }
}

const jtag_state_t jtag_state_next[16][2] = {
    /* TEST_LOGIC_RESET */    { RUN_TEST_IDLE, TEST_LOGIC_RESET },
    /* RUN_TEST_IDLE */       { RUN_TEST_IDLE, SELECT_DR_SCAN },
    /* SELECT_DR_SCAN */      { CAPTURE_DR, SELECT_IR_SCAN },
    /* CAPTURE_DR */          { SHIFT_DR, EXIT1_DR },
    /* SHIFT_DR */            { SHIFT_DR, EXIT1_DR },
    /* EXIT1_DR */            { PAUSE_DR, UPDATE_DR },
    /* PAUSE_DR */            { PAUSE_DR, EXIT2_DR },
    /* EXIT2_DR */            { SHIFT_DR, UPDATE_DR },
    /* UPDATE_DR */           { RUN_TEST_IDLE, SELECT_DR_SCAN },
    /* SELECT_IR_SCAN */      { CAPTURE_IR, TEST_LOGIC_RESET },
    /* CAPTURE_IR */          { SHIFT_IR, EXIT1_IR },
    /* SHIFT_IR */            { SHIFT_IR, EXIT1_IR },
    /* EXIT1_IR */            { PAUSE_IR, UPDATE_IR },
    /* PAUSE_IR */            { PAUSE_IR, EXIT2_IR },
    /* EXIT2_IR */            { SHIFT_IR, UPDATE_IR },
    /* UPDATE_IR */           { RUN_TEST_IDLE, SELECT_DR_SCAN }
};

#define IR_NAMES \
  X(IR_IDCODE, 1) \
  X(IR_DTMCONTROL, 0x10) \
  X(IR_DBUS, 0x11) \
  X(IR_MASK, 0x1f)

enum {
  #define X(name, code) name = code,
  IR_NAMES
  #undef X
};

static inline void fprintf_ir_name (FILE* stream, int ir_name) {
  switch (ir_name) {
    #define X(name, ...) case name: fprintf(stream, STRINGIFY(name)); break;
    IR_NAMES
    #undef X
    default:
      fprintf (stream, "UNKNOWN");
      break;
  }
}

#define IRBITS  5

static jtag_state_t state;
static jtag_state_t next_state;
static uint32_t ir;
static uint32_t dr;
static uint32_t dbus_last_data;
static bool busy = false;
struct vpi_cmd vpi;

static const uint32_t idcode = 0x00000ffd;

static uint32_t array_to_word(const void * bytes) {
  assert(bytes != NULL);

  const uint8_t * ptr = (const uint8_t *)bytes;

  return ptr[0] | ((uint32_t)ptr[1] << 8)
                | ((uint32_t)ptr[2] << 16)
                | ((uint32_t)ptr[3] << 24);
}

static void word_to_array(void * bytes, const uint32_t word) {
  assert(bytes != NULL);

  uint8_t * ptr = (uint8_t *)bytes;
  ptr[0] = word;
  ptr[1] = word >> 8;
  ptr[2] = word >> 16;
  ptr[3] = word >> 24;
}

static int do_scan(struct vpi_cmd * vpi, int * paddr, int * pdata, int * pop) {
  assert(vpi != NULL);
  assert(paddr != NULL);
  assert(pdata != NULL);
  assert(pop != NULL);

  switch (state) {
    case SHIFT_DR:
      #ifdef DEBUG
      DEBUG_PRINTF("SHIFT_DR, IR = ");
      fprintf_ir_name(stderr, ir);
      fprintf(stderr, "\n");
      #endif
      if (ir == IR_IDCODE) {
        memcpy(vpi->buffer_in, vpi->buffer_out, vpi->length);
        word_to_array(vpi->buffer_in, idcode);
      } else if (ir == IR_DTMCONTROL) {
        uint32_t dtmcs;
        dtmcs = array_to_word(vpi->buffer_out);
        if (dtmcs & (3 << 16))
            DEBUG_PRINTF(": reset (nyi)\n");
        // abits=6, version=1
        dtmcs = 0x0061;
        word_to_array(vpi->buffer_in, dtmcs);
      } else if (ir == IR_DBUS) {
        uint32_t op = vpi->buffer_out[0] & 3;
        uint32_t data = array_to_word(vpi->buffer_out);
        data = data >> 2 | ((uint32_t)vpi->buffer_out[4] << 30);
        uint32_t addr = (vpi->buffer_out[4] >> 2);
        *pop = op;
        *pdata = data;
        *paddr = addr;
        if (op == 0) {
          // openocd seems to expect last read data on NOP
          *pdata = dbus_last_data;
          data = dbus_last_data;
        } else if (op == 1 || op == 2) {
          DEBUG_PRINTF(": dmi request addr=0x%04x, data=0x%08x, op=%d)\n", addr, data, op);
          busy = true;
          return 1;
        } else {
          DEBUG_PRINTF(": reserved dbus op (%d)\n", op);
        }
        word_to_array(vpi->buffer_in, data << 2);
        vpi->buffer_in[4] = (data >> 30) & 3;
      } else DEBUG_PRINTF(": unknown register (%d)\n", ir);
      break;
    case SHIFT_IR:
      if (vpi->nb_bits == IRBITS) {
        vpi->buffer_in[0] = ir & IR_MASK;
        ir = vpi->buffer_out[0] & IR_MASK;
      } else if (vpi->nb_bits == (IRBITS + 2)) {
        // openocd IR capture validation
        vpi->buffer_in[0] = (ir & IR_MASK) | ((vpi->buffer_out[0] & 0x3) << IRBITS);
        ir = (vpi->buffer_out[0] >> 2) & IR_MASK;
      } else DEBUG_PRINTF(": IR scan NYI\n");
      break;
    default:
      DEBUG_PRINTF(": unknown scan state (%d)\n", state);
  }
  return 0;
}

static void jtag_vpi_response(int fd) {
  #ifdef DEBUG
  DEBUG_FPRINTF(stderr, "new jtag state=");
  fprintf_jtag_state(stderr, state);
  fprintf(stderr, "\n");
  #endif
  if (vpi.cmd == CMD_SCAN_CHAIN || vpi.cmd == CMD_SCAN_CHAIN_FLIP_TMS) {
    #ifdef DEBUG
    {
      DEBUG_PRINTF("vpi_cmd: cmd=");
      fprintf_vpi_cmd(stderr, vpi.cmd);
      fprintf(stderr, ", length=%d, nb_bits=%d", vpi.length, vpi.nb_bits);
      fprintf(stderr, ", buffer_in = ");
      fprintf_hex(stderr, vpi.buffer_in, vpi.length);
      fprintf(stderr, ", buffer_out = ");
      fprintf_hex(stderr, vpi.buffer_out, vpi.length);
      fprintf(stderr, "\n");
    }
    #endif
    DEBUG_PRINTF(": write vpi back to fd\n");
    int c = write(fd, &vpi, sizeof(struct vpi_cmd));
    DEBUG_PRINTF(": writen %0d byte(s)\n", c);
    if (c >= 0 && c != sizeof(struct vpi_cmd)) DEBUG_PRINTF(": short write\n");
    else if (c < 0) {
      perror("read() failed");
      abort();
    }
  }
}

static int jtag_vpi_request(int fd, int * addr, int * data, int * op) {
  assert(fd >= 0);
  assert(addr != NULL);
  assert(data != NULL);
  assert(op != NULL);
  if (busy) return 0;
  int ret;
  DEBUG_PRINTF("\n");
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = POLLIN;
  while (true) {
    ret = poll(fds, 1, 0);
    if (ret < 0) {
      perror("poll() failed");
      abort();
    }
    if (ret == 0) break;
    int c = recv(fd, &vpi, sizeof(struct vpi_cmd), MSG_WAITALL);
    if (((c < 0) && (errno == EAGAIN)) || (c == 0)) {
      ret = 0;
      break;
    } else if (c < 0) {
      perror("recv() failed");
      abort();
    } else if (c != sizeof(struct vpi_cmd)) {
      // probably eof or an error
      abort();
    }
    DEBUG_PRINTF("\n");
    #ifdef DEBUG
    {
      DEBUG_PRINTF("vpi_cmd: cmd=");
      fprintf_vpi_cmd(stderr, vpi.cmd);
      fprintf(stderr, ", length=%d, nb_bits=%d", vpi.length, vpi.nb_bits);
      fprintf(stderr, ", buffer_in = ");
      fprintf_hex(stderr, vpi.buffer_in, vpi.length);
      fprintf(stderr, ", buffer_out = ");
      fprintf_hex(stderr, vpi.buffer_out, vpi.length);
      fprintf(stderr, "\n");
    }

    DEBUG_FPRINTF(stderr, "jtag state=");
    fprintf_jtag_state(stderr, state);
    fprintf(stderr, ", ir=");
    fprintf_ir_name(stderr, ir);
    fprintf(stderr, "\n");
    #endif
    switch ((uint32_t)vpi.cmd) {
    case CMD_RESET:
      DEBUG_PRINTF(": CMD_RESET\n");
      state = TEST_LOGIC_RESET;
      ir = IR_IDCODE;
      break;
    case CMD_TMS_SEQ:
      DEBUG_PRINTF(": CMD_TMS_SEQ\n");
      {
          int i;
          for (i = 0; i < vpi.nb_bits; i++) {
              uint32_t mask = 1 << (i % 8);
              uint32_t tms = vpi.buffer_out[i / 8] & mask;
              #ifdef DEBUG
              DEBUG_FPRINTF(stderr, "jtag state transition: ");
              fprintf_jtag_state(stderr, state);
              #endif
              state = jtag_state_next[state][(tms == 0) ? 0 : 1];
              #ifdef DEBUG
              fprintf(stderr, " -> ");
              fprintf_jtag_state(stderr, state);
              fprintf(stderr, "\n");
              #endif
          }
      }
      break;
    case CMD_SCAN_CHAIN:
      DEBUG_PRINTF(": CMD_SCAN_CHAIN\n");
      next_state = jtag_state_next[state][0];
      ret = do_scan(&vpi, addr, data, op);
      if (ret > 0) return ret;
      #ifdef DEBUG
      DEBUG_FPRINTF(stderr, "jtag state transition: ");
      fprintf_jtag_state(stderr, state);
      #endif
      state = next_state;
      #ifdef DEBUG
      fprintf(stderr, " -> ");
      fprintf_jtag_state(stderr, state);
      fprintf(stderr, "\n");
      #endif
      break;
    case CMD_SCAN_CHAIN_FLIP_TMS:
      DEBUG_PRINTF(": CMD_SCAN_CHAIN_FLIP_TMS\n");
      next_state = jtag_state_next[state][1];
      ret = do_scan(&vpi, addr, data, op);
      if (ret > 0) return ret;
      #ifdef DEBUG
      DEBUG_FPRINTF(stderr, "jtag state transition: ");
      fprintf_jtag_state(stderr, state);
      #endif
      state = next_state;
      #ifdef DEBUG
      fprintf(stderr, " -> ");
      fprintf_jtag_state(stderr, state);
      fprintf(stderr, "\n");
      #endif
      break;
    }
    jtag_vpi_response(fd);
  }
  return ret;
}

int jtagvpi_receive_request (int * addr, int * data, int * op) {
  DEBUG_PRINTF("running sanity assertions...\n");
  assert(jtagvpi_fd >= 0);
  assert(addr != NULL);
  assert(data != NULL);
  assert(op != NULL);
  DEBUG_PRINTF("sanity assertions ok.\n");
  return jtag_vpi_request (jtagvpi_fd, addr, data, op);
}

int jtagvpi_send_response(int data) {
  assert(jtagvpi_fd >= 0);
  if (!busy) {
    DEBUG_PRINTF(": unexpected dmi response\n");
    return -1;
  }
  busy = false;
  DEBUG_PRINTF(": dmi response data=0x%08x)\n", data);
  dbus_last_data = data;
  word_to_array(vpi.buffer_in, data << 2);
  vpi.buffer_in[4] = (data >> 30) & 3;
  state = next_state;
  jtag_vpi_response(jtagvpi_fd);
  return 0;
}

#ifdef __cplusplus
}
#endif
