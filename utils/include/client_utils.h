#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include "constants.h"

void sendmsg(int sd, char* buffer);
void recvmsg(int sd, char* buffer);
void reset(char* buffer);

#endif