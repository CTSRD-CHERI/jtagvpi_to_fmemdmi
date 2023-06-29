#pragma once

typedef enum {
    DMI_OP_NOP = 0
  , DMI_OP_READ = 1
  , DMI_OP_WRITE = 2
  , DMI_OP_RESERVED = 3
  , DMI_OP_SUCCESS = 0
  , DMI_OP_FAILED = 2
  , DMI_OP_BUSY = 3
} dmi_op_t;

void fmemdmi_init (const char * fname);
void fmemdmi_deinit ();
void fmemdmi_write (uint16_t addr, uint32_t data);
uint32_t fmemdmi_read (uint16_t addr);
uint32_t fmemdmi_req (dmi_op_t op, uint16_t addr, uint32_t data);
