#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

/*
 * Job type for loader work. LoadFile = read binary file; LoadMesh/LoadTexture reserved for later.
 */
enum class LoadJobType {
    LoadFile,
    LoadMesh,
    LoadTexture,
};

/*
 * Result of a load-file job. Worker fills vecData and sets bDone; caller may wait on cv until bDone.
 */
struct LoadFileResult {
    std::vector<uint8_t> vecData;
    bool                 bDone = false;
    std::mutex           mtx;
    std::condition_variable cv;
};

/*
 * One completed load job: type, path, and data. Main thread drains these via ProcessCompletedJobs.
 */
struct CompletedLoadJob {
    LoadJobType         eType = LoadJobType::LoadFile;
    std::string         sPath;
    std::vector<uint8_t> vecData;
};

/*
 * Job queue for loader work. Multiple worker threads run load jobs in parallel (use available cores).
 * SubmitLoadFile() posts a job and returns a result handle; caller may wait on result until bDone.
 * Workers push completed jobs to a queue; main thread calls ProcessCompletedJobs(handler) to drain and dispatch by type.
 * All Vulkan/engine work stays on the calling thread; workers only do I/O (and later: parse/decode).
 */
class JobQueue {
public:
    JobQueue() = default;
    ~JobQueue();

    void Start();
    void Stop();

    /* Post a load-file job; returns shared result. Caller may wait on result->cv until result->bDone, then use result->vecData. */
    std::shared_ptr<LoadFileResult> SubmitLoadFile(const std::string& sPath);

    /* Drain completed jobs and call handler for each (type, path, data). Call from main thread; handler may create Vulkan objects. */
    using CompletedJobHandler = std::function<void(LoadJobType, const std::string&, std::vector<uint8_t>)>;
    void ProcessCompletedJobs(CompletedJobHandler handler);

private:
    struct Job {
        LoadJobType eType = LoadJobType::LoadFile;
        std::string sPath;
        std::shared_ptr<LoadFileResult> pResult;
    };

    void WorkerLoop();
    static std::vector<uint8_t> ReadFileBinary(const std::string& sPath);
    static unsigned int GetWorkerCount();

    std::queue<Job>            m_queue;
    std::mutex                 m_mutex;
    std::condition_variable    m_cv;
    std::queue<CompletedLoadJob> m_completed;
    std::mutex                 m_completedMutex;
    std::atomic<bool>          m_stop{false};
    std::vector<std::thread>   m_workers;
};
