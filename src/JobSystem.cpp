#include "JobSystem.hpp"
#include "Utils.hpp"

#include <condition_variable>
#include <queue>
#include <thread>
#include <vector>

#include <tracy/Tracy.hpp>

struct Job
{
    JobFunc func;
    int64_t userIndex;
    void *userData;
    Token token;
};

static std::vector<std::thread> threads;
static TracyLockable(std::mutex, mutex);
static std::condition_variable_any cond;
static std::queue<Job> jobs;
static bool shouldStop;

static void threadFunc(uint8_t index)
{
    char name[10];
    snprintf(name, sizeof(name), "thread#%02d", index);
    tracy::SetThreadName(name);

    while (true)
    {
        std::unique_lock<LockableBase(std::mutex)> lock(mutex);
        while (!shouldStop && jobs.empty())
        {
            cond.wait(lock);
        }

        if (shouldStop)
            return;

        Job job = jobs.front();
        jobs.pop();
        lock.unlock();

        job.func(job.userIndex, job.userData);

        if (job.token)
            (*(std::atomic_int32_t *)job.token)--;
    }
}

void initJobSystem()
{
    if (!threads.empty())
        return;

    shouldStop = false;
    uint8_t threadCount = (uint8_t)std::thread::hardware_concurrency(); // yes, this is bad
    threads.reserve(threadCount);

    for (uint8_t i = 0; i < threadCount; i++)
    {
        threads.emplace_back(&threadFunc, i);
    }
}

Token createToken()
{
    // TODO: pool allocator
    return (Token)new std::atomic_int32_t(0);
}

void destroyToken(Token token)
{
    ASSERT(!*(std::atomic_int32_t *)token);
    delete (std::atomic_int32_t *)token;
}

void enqueueJob(JobInfo jobInfo, Token token)
{
    ASSERT(jobInfo.func);
    ASSERT(threads.size());
    if (token)
        (*(std::atomic_int32_t *)token)++;

    std::unique_lock<LockableBase(std::mutex)> lock(mutex);
    jobs.push({ jobInfo.func, jobInfo.userIndex, jobInfo.userData, token });
    lock.unlock();
    cond.notify_one();
}

void enqueueJobs(JobInfo *jobInfos, uint32_t jobsCount, Token token)
{
    ASSERT(jobInfos);
    ASSERT(jobsCount);

    if (token)
        (*(std::atomic_int32_t *)token) += jobsCount;

    std::unique_lock<LockableBase(std::mutex)> lock(mutex);
    for (uint32_t i = 0; i < jobsCount; i++)
    {
        ASSERT(jobInfos[i].func);
        jobs.push({ jobInfos[i].func, jobInfos[i].userIndex, jobInfos[i].userData, token });
    }
    lock.unlock();
    cond.notify_all();
}

void waitForToken(Token token)
{
    ASSERT(token);
    while (*(std::atomic_int32_t *)token)
        std::this_thread::yield(); // yes, this is also bad
}

void terminateJobSystem()
{
    if (threads.empty())
        return;

    ASSERT(jobs.empty());
    std::unique_lock<LockableBase(std::mutex)> lock(mutex);
    shouldStop = true;
    lock.unlock();
    cond.notify_all();

    for (std::thread &thread : threads)
    {
        thread.join();
    }
    threads.clear();
}