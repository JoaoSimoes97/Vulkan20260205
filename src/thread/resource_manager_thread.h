#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>

/**
 * ResourceManagerThread — manages resource lifecycle asynchronously.
 * Main thread enqueues commands (TrimUnused, ProcessDestroys).
 * Worker thread executes them without blocking the main frame loop.
 */
class ResourceManagerThread {
public:
    enum class CommandType {
        TrimMaterials,
        TrimMeshes,
        TrimTextures,
        TrimPipelines,
        ProcessDestroys,  // Run all ProcessPendingDestroys
        Shutdown,
    };

    struct Command {
        CommandType type;
        std::function<void()> callback;  // Execute callback when ready

        Command() = default;
        Command(CommandType t, std::function<void()> cb = nullptr)
            : type(t), callback(cb) {}
    };

    ResourceManagerThread();
    ~ResourceManagerThread();

    /** Start the worker thread. Returns false if already running. */
    bool Start();

    /** Stop the worker thread and wait for it to finish. */
    void Stop();

    /** Enqueue a command to be executed by worker thread. Thread-safe. */
    void EnqueueCommand(const Command& cmd);

    /** Enqueue multiple commands. Thread-safe. */
    void EnqueueCommands(const std::vector<Command>& commands);

    /** Check if worker thread is running. */
    bool IsRunning() const;

private:
    void WorkerThreadMain();
    void ExecuteCommand(const Command& cmd);

    std::unique_ptr<std::thread> m_pWorkerThread;
    std::queue<Command> m_commandQueue;
    std::mutex m_queueMutex;
    std::atomic<bool> m_bRunning{false};
    std::atomic<bool> m_bShutdownRequested{false};
};
