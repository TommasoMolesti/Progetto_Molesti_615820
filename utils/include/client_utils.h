#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include "constants.h"

void send_msg(int sd, char* buffer);
void recv_msg(int sd, char* buffer);

#endif