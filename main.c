#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <getopt.h>

#include "jtagvpi.h"
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

void cleanup ()
{
    DEBUG_PRINTF ("cleaning up\n");
    fmemdmi_deinit ();
    jtagvpi_deinit ();
}

int main (int argc , char** argv)
{
    int jtagvpi_port = 5555;
    char fmemdmi_filename[256] = "/dev/fmem_sys0_debug_unit";
    // gather command line options
    int opt;
    while ((opt = getopt(argc, argv, "p:f:")) != -1) {
        switch (opt) {
            case 'p': jtagvpi_port = atoi (optarg); break;
            case 'f':
                strncpy (fmemdmi_filename, optarg, 255);
                fmemdmi_filename[255] = '\0';
                break;
            default:
                fprintf ( stderr
                        , "Usage: %s [-p JTAG_PORT] [-f DMI_FILENAME]\n"
                        , argv[0] );
                exit(EXIT_FAILURE);
        }
    }
    DEBUG_PRINTF ( "after getopt - jtagvpi_port: %d, fmemdmi_filename: %s\n"
                 , jtagvpi_port, fmemdmi_filename );
    // initialise
    DEBUG_PRINTF ("initializing\n");
    jtagvpi_init (jtagvpi_port);
    fmemdmi_init (fmemdmi_filename);
    signal (SIGINT, cleanup);
    // main loop
    DEBUG_PRINTF ("main loop\n");
    bool done = false; // currently never stops ...
    int addr;
    int data;
    int op;
    while (!done)
    {
        DEBUG_PRINTF ( "-------------------------------------------\n" );
        DEBUG_PRINTF ( "pre-jtagvpi_receive_request, "
                       "addr: 0x%0x, data: 0x%0x, op: %0d\n"
                     , addr, data, op );
        int ret = jtagvpi_receive_request (&addr, &data, &op);
        if (ret < 0)
        {
          fprintf(stderr, "ERROR: jtagvpi_receive_request() returned %d", ret);
          exit (ret);
        }
        else if (ret > 0) {
          DEBUG_PRINTF ( "pre-fmemdmi_req, "
                         "addr: 0x%0x, data: 0x%0x, op: %0d\n"
                       , addr, data, op );
          data = fmemdmi_req (op, addr, data);
          DEBUG_PRINTF ( "pre-jtagvpi_send_response, "
                         "addr: 0x%0x, data: 0x%0x, op: %0d\n"
                       , addr, data, op );
          if (op == DMI_OP_READ || op == DMI_OP_WRITE)
              jtagvpi_send_response (data);
        }
    }
    // clean up
    cleanup ();
    return 0;
}
