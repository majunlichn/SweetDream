#pragma once

#include <SweetDream/Core/Common/String.h>
#include <SweetDream/Platform/EventHandler.h>

#include <SDL3/SDL_init.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace sd
{

class Application
{
public:
    enum class Status
    {
        Uninitialized,
        Initialized,
        Running,
        Exiting,
        Exited
    };

    Application() = default;
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Owns initialization, the event loop, and orderly shutdown.
    [[nodiscard]] int Run(int argc, char** argv);
    // Callback-driven alternative to Run().
    [[nodiscard]] bool Init(int argc, char** argv);
    void DispatchEvent(const SDL_Event& event);
    void DispatchFrame();
    // If still Running, call RequestExit first so teardown actually runs.
    void Shutdown();
    void RequestExit(int exitCode = 0) noexcept;

    bool RegisterEventHandler(rad::Ref<EventHandler> handler);
    bool UnregisterEventHandler(const EventHandler* handler);
    [[nodiscard]] bool IsEventHandlerRegistered(const EventHandler* handler) const;

    [[nodiscard]] Status GetStatus() const noexcept { return m_status.load(); }
    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] bool IsRunning() const noexcept { return GetStatus() == Status::Running; }
    [[nodiscard]] int GetExitCode() const noexcept { return m_exitCode.load(); }

    // A zero timeout never waits and is appropriate for continuously rendered applications.
    void SetEventWaitTimeout(uint32_t milliseconds) noexcept { m_eventWaitTimeout = milliseconds; }
    [[nodiscard]] uint32_t GetEventWaitTimeout() const noexcept { return m_eventWaitTimeout; }
    [[nodiscard]] StringSet GetVulkanInstanceExtensions() const;

protected:
    virtual bool OnInitialize() { return true; }
    virtual void OnShutdown() {}
    virtual bool OnEvent(const SDL_Event& event) { return false; }
    virtual void OnFrame() {}

private:
    std::atomic<Status> m_status = Status::Uninitialized;
    std::atomic<int> m_exitCode = 0;
    uint32_t m_eventWaitTimeout = 0;
    bool m_sdlInitialized = false;
    std::vector<rad::Ref<EventHandler>> m_eventHandlers;
}; // class Application

} // namespace sd
