#pragma once

void jtagvpi_init (int port);
void jtagvpi_deinit ();
int jtagvpi_receive_request (int * addr, int * data, int * op);
void jtagvpi_send_response (int data);
