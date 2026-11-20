#pragma once
// Test-only prelude. Load production/system headers BEFORE installing macros.
#include "Server.hpp"
#include "Configs.hpp"
#include "CGI.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int ioAuditPoll(struct pollfd*, nfds_t, int, const char*, int);
ssize_t ioAuditRecv(int, void*, size_t, int, const char*, int);
ssize_t ioAuditSend(int, const void*, size_t, int, const char*, int);
ssize_t ioAuditRead(int, void*, size_t, const char*, int);
ssize_t ioAuditWrite(int, const void*, size_t, const char*, int);
int& ioAuditErrno(const char*, int);
int ioAuditClose(int);
int ioAuditOpen(const char*, int);
int ioAuditOpen(const char*, int, mode_t);
int ioAuditStat(const char*, struct stat*);
int ioAuditAccept(int, struct sockaddr*, socklen_t*);
DIR* ioAuditOpendir(const char*);
struct dirent* ioAuditReaddir(DIR*);
int ioAuditClosedir(DIR*);
int ioAuditRemove(const char*);
