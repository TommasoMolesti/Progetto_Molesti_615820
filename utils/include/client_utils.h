#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include "constants.h"

void send_message(int sd, char* buffer);
void recv_message(int sd, char* buffer);
void reset(char* buffer);

#endif