#pragma once

#include <stdint.h>

typedef struct Token_t *Token;
typedef void (*JobFunc)(int64_t userIndex, void *userData);

void initJobSystem();

void terminateJobSystem();

Token createToken();

void destroyToken(Token token);

void enqueueJob(JobFunc func, int64_t userIndex, void *userData, Token token);

void waitForToken(Token token);