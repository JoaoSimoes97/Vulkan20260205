/*
 * JobQueue — worker threads for async file loads. SubmitLoadFile() enqueues; workers call ReadFileBinary
 * and set result; main thread drains completed jobs via ProcessCompletedJobs(). Used by VulkanShaderManager.
 */
#include "job_queue.h"
#include "vulkan/vulkan_utils.h"
#include <fstream>

std::vector<uint8_t> JobQueue::ReadFileBinary(const std::string& sPath) {
    std::ifstream stm(sPath, std::ios::binary | std::ios::ate);
    if (stm.is_open() == false)
        return std::vector<uint8_t>();
    std::streamsize zSize = stm.tellg();
    if (zSize <= static_cast<std::streamsize>(0))
        return std::vector<uint8_t>();
    stm.seekg(static_cast<std::streamoff>(0), std::ios::beg);
    std::vector<uint8_t> vecData(static_cast<size_t>(zSize));
    stm.read(reinterpret_cast<char*>(vecData.data()), zSize);
    return vecData;
}

void JobQueue::WorkerLoop() {
    while (true) {
        Job stJob;
        {
            std::unique_lock<std::mutex> lock(this->m_mutex);
            while ((this->m_queue.empty() == true) && (this->m_stop.load() == false))
                this->m_cv.wait(lock);
            if (this->m_stop.load() == true)
                break;
            stJob = this->m_queue.front();
            this->m_queue.pop();
        }
        std::vector<uint8_t> vecData = ReadFileBinary(stJob.sPath);

        if (stJob.pResult != nullptr) {
            std::lock_guard<std::mutex> lock(stJob.pResult->mtx);
            stJob.pResult->vecData = vecData;
            stJob.pResult->bDone = true;
            stJob.pResult->cv.notify_all();
        }

        {
            CompletedLoadJob stCompleted;
            stCompleted.eType = stJob.eType;
            stCompleted.sPath = stJob.sPath;
            stCompleted.vecData = std::move(vecData);
            std::lock_guard<std::mutex> clock(this->m_completedMutex);
            this->m_completed.push(std::move(stCompleted));
        }
    }
}

unsigned int JobQueue::GetWorkerCount() {
    unsigned int n = static_cast<unsigned int>(std::thread::hardware_concurrency());
    if (n == static_cast<unsigned int>(0))
        n = static_cast<unsigned int>(2);
    return (n > static_cast<unsigned int>(16)) ? static_cast<unsigned int>(16) : n;
}

void JobQueue::Start() {
    this->m_stop.store(false);
    unsigned int n = this->GetWorkerCount();
    this->m_workers.reserve(n);
    for (unsigned int i = static_cast<unsigned int>(0); i < n; ++i)
        this->m_workers.emplace_back(&JobQueue::WorkerLoop, this);
}

void JobQueue::Stop() {
    this->m_stop.store(true);
    this->m_cv.notify_all();
    for (std::thread& t : this->m_workers) {
        if (t.joinable() == true)
            t.join();
    }
    this->m_workers.clear();
}

std::shared_ptr<LoadFileResult> JobQueue::SubmitLoadFile(const std::string& sPath) {
    auto pResult = std::make_shared<LoadFileResult>();
    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        Job stJob;
        stJob.eType = LoadJobType::LoadFile;
        stJob.sPath = sPath;
        stJob.pResult = pResult;
        this->m_queue.push(std::move(stJob));
    }
    this->m_cv.notify_all();
    return pResult;
}

void JobQueue::SubmitLoadTexture(const std::string& sPath) {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    Job stJob;
    stJob.eType = LoadJobType::LoadTexture;
    stJob.sPath = sPath;
    stJob.pResult = nullptr;  /* no wait handle for texture loads */
    this->m_queue.push(std::move(stJob));
    this->m_cv.notify_all();
}

void JobQueue::ProcessCompletedJobs(const CompletedJobHandler& pHandler_ic) {
    std::queue<CompletedLoadJob> vecBatch;
    {
        std::lock_guard<std::mutex> lock(this->m_completedMutex);
        while (this->m_completed.empty() == false) {
            vecBatch.push(std::move(this->m_completed.front()));
            this->m_completed.pop();
        }
    }
    while (vecBatch.empty() == false) {
        CompletedLoadJob& st = vecBatch.front();
        pHandler_ic(st.eType, st.sPath, std::move(st.vecData));
        vecBatch.pop();
    }
}

JobQueue::~JobQueue() {
    this->Stop();
}
