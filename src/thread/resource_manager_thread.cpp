#include "resource_manager_thread.h"
#include "vulkan/vulkan_utils.h"
#include <chrono>

ResourceManagerThread::ResourceManagerThread() = default;

ResourceManagerThread::~ResourceManagerThread() {
    Stop();
}

bool ResourceManagerThread::Start() {
    if (m_bRunning.load())
        return false;

    m_bShutdownRequested.store(false);
    m_bRunning.store(true);
    m_pWorkerThread = std::make_unique<std::thread>([this] { this->WorkerThreadMain(); });

    VulkanUtils::LogInfo("ResourceManagerThread: started worker thread");
    return true;
}

void ResourceManagerThread::Stop() {
    if (!m_bRunning.load())
        return;

    VulkanUtils::LogInfo("ResourceManagerThread: requesting shutdown");
    m_bShutdownRequested.store(true);

    // Enqueue shutdown command to wake up worker if it's waiting
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_commandQueue.push(Command(CommandType::Shutdown));
    }

    // Wait for thread to finish
    if (m_pWorkerThread && m_pWorkerThread->joinable()) {
        m_pWorkerThread->join();
        m_pWorkerThread.reset();
    }

    m_bRunning.store(false);
    VulkanUtils::LogInfo("ResourceManagerThread: worker thread stopped");
}

void ResourceManagerThread::EnqueueCommand(const Command& cmd) {
    if (!m_bRunning.load())
        return;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_commandQueue.push(cmd);
    }
}

void ResourceManagerThread::EnqueueCommands(const std::vector<Command>& commands) {
    if (!m_bRunning.load())
        return;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (const auto& cmd : commands)
            m_commandQueue.push(cmd);
    }
}

bool ResourceManagerThread::IsRunning() const {
    return m_bRunning.load();
}

void ResourceManagerThread::WorkerThreadMain() {
    VulkanUtils::LogTrace("ResourceManagerThread::WorkerThreadMain: started");

    while (!m_bShutdownRequested.load()) {
        Command cmd;
        bool bHasCommand = false;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_commandQueue.empty()) {
                cmd = m_commandQueue.front();
                m_commandQueue.pop();
                bHasCommand = true;
            }
        }

        if (bHasCommand) {
            ExecuteCommand(cmd);
        } else {
            // Sleep briefly to avoid spinning (10ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Drain remaining commands before shutdown
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_commandQueue.empty()) {
            Command cmd = m_commandQueue.front();
            m_commandQueue.pop();
            if (cmd.type != CommandType::Shutdown) {
                ExecuteCommand(cmd);
            }
        }
    }

    VulkanUtils::LogTrace("ResourceManagerThread::WorkerThreadMain: exited");
}

void ResourceManagerThread::ExecuteCommand(const Command& cmd) {
    switch (cmd.type) {
    case CommandType::TrimMaterials:
    case CommandType::TrimMeshes:
    case CommandType::TrimTextures:
    case CommandType::TrimPipelines:
    case CommandType::ProcessDestroys:
        if (cmd.callback)
            cmd.callback();
        break;
    case CommandType::Shutdown:
        break;
    }
}
