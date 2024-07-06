#pragma once

#include <stdint.h>

typedef struct Token_t *Token;
typedef void (*JobFunc)(int64_t userIndex, void *userData);

struct JobInfo
{
    JobFunc func;
    int64_t userIndex;
    void *userData;
};

void initJobSystem();

void terminateJobSystem();

Token createToken();

void destroyToken(Token token);

void enqueueJob(JobInfo jobInfo, Token token);

void enqueueJobs(JobInfo *jobInfos, uint32_t jobsCount, Token token);

void waitForToken(Token token);