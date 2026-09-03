#include <SweetDream/Platform/Application.h>

#include <SweetDream/Core/IO/Logging.h>

#include <rad/System/Application.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <limits>
#include <utility>

namespace sd
{

Application::~Application()
{
    Shutdown();
}

StringSet Application::GetVulkanInstanceExtensions() const
{
    assert(m_sdlInitialized);

    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensionNames == nullptr)
    {
        SD_LOG(err, "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }

    StringSet extensions;
    for (Uint32 index = 0; index < extensionCount; ++index)
    {
        assert(extensionNames[index] != nullptr);
        extensions.emplace(extensionNames[index]);
    }

    return extensions;
}

bool Application::Init(int argc, char** argv)
{
    const Status status = GetStatus();
    if (status != Status::Uninitialized)
    {
        SD_LOG(err, "Application::Init called in an invalid state");
        m_exitCode = EXIT_FAILURE;
        return false;
    }

    rad::Application::Instance().Init(argc, argv);

    m_exitCode = 0;
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SD_LOG(err, "SDL_Init failed: {}", SDL_GetError());
        m_exitCode = EXIT_FAILURE;
        return false;
    }

    m_sdlInitialized = true;
    m_status = Status::Initialized;

    try
    {
        if (!OnInitialize())
        {
            SD_LOG(err, "Application initialization was rejected by OnInitialize");
            m_exitCode = EXIT_FAILURE;
            Shutdown();
            return false;
        }
    }
    catch (const std::exception& error)
    {
        SD_LOG(err, "Application initialization failed: {}", error.what());
        m_exitCode = EXIT_FAILURE;
        Shutdown();
        return false;
    }
    catch (...)
    {
        SD_LOG(err, "Application initialization failed with an unknown exception");
        m_exitCode = EXIT_FAILURE;
        Shutdown();
        return false;
    }

    if (GetStatus() == Status::Initialized)
    {
        m_status = Status::Running;
    }
    return true;
}

void Application::Shutdown()
{
    const Status status = GetStatus();
    if (status == Status::Running)
    {
        RequestExit(m_exitCode.load());
        return;
    }
    if (status == Status::Uninitialized && !m_sdlInitialized)
    {
        return;
    }
    m_status = Status::Exiting;

    if (m_sdlInitialized)
    {
        try
        {
            OnShutdown();
        }
        catch (const std::exception& error)
        {
            SD_LOG(err, "Application shutdown failed: {}", error.what());
            m_exitCode = EXIT_FAILURE;
        }
        catch (...)
        {
            SD_LOG(err, "Application shutdown failed with an unknown exception");
            m_exitCode = EXIT_FAILURE;
        }
    }

    m_eventHandlers.clear();
    if (m_sdlInitialized)
    {
        SDL_Quit();
        m_sdlInitialized = false;
    }
    m_status = Status::Exited;
}

int Application::Run(int argc, char** argv)
{
    if (!Init(argc, argv))
    {
        return m_exitCode;
    }

    try
    {
        while (GetStatus() == Status::Running)
        {
            SDL_Event event;
            if (m_eventWaitTimeout > 0 &&
                SDL_WaitEventTimeout(
                    &event, static_cast<Sint32>(std::min<uint32_t>(
                                m_eventWaitTimeout,
                                static_cast<uint32_t>(std::numeric_limits<Sint32>::max())))))
            {
                DispatchEvent(event);
            }

            while (GetStatus() == Status::Running && SDL_PollEvent(&event))
            {
                DispatchEvent(event);
            }

            if (GetStatus() == Status::Running)
            {
                DispatchFrame();
            }
        }
    }
    catch (const std::exception& error)
    {
        SD_LOG(err, "Application main loop failed: {}", error.what());
        m_exitCode = EXIT_FAILURE;
        m_status = Status::Exiting;
    }
    catch (...)
    {
        SD_LOG(err, "Application main loop failed with an unknown exception");
        m_exitCode = EXIT_FAILURE;
        m_status = Status::Exiting;
    }

    RequestExit(m_exitCode);
    Shutdown();
    return m_exitCode;
}

void Application::RequestExit(int exitCode) noexcept
{
    m_exitCode = exitCode;

    const Status status = GetStatus();
    if (status == Status::Initialized || status == Status::Running)
    {
        m_status = Status::Exiting;
    }
}

bool Application::RegisterEventHandler(rad::Ref<EventHandler> handler)
{
    const Status status = GetStatus();
    if (!handler || status == Status::Exiting || status == Status::Exited ||
        IsEventHandlerRegistered(handler.get()))
    {
        return false;
    }

    m_eventHandlers.push_back(std::move(handler));
    return true;
}

bool Application::UnregisterEventHandler(const EventHandler* handler)
{
    const auto iterator = std::find_if(m_eventHandlers.begin(), m_eventHandlers.end(),
                                       [handler](const rad::Ref<EventHandler>& item)
                                       { return item.get() == handler; });
    if (iterator == m_eventHandlers.end())
    {
        return false;
    }

    m_eventHandlers.erase(iterator);
    return true;
}

bool Application::IsEventHandlerRegistered(const EventHandler* handler) const
{
    return std::ranges::any_of(m_eventHandlers, [handler](const rad::Ref<EventHandler>& item)
                               { return item.get() == handler; });
}

bool Application::IsInitialized() const noexcept
{
    const Status status = GetStatus();
    return status == Status::Initialized || status == Status::Running || status == Status::Exiting;
}

void Application::DispatchEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        RequestExit();
    }

    const std::vector<rad::Ref<EventHandler>> handlers = m_eventHandlers;
    for (const rad::Ref<EventHandler>& handler : handlers)
    {
        if (handler->OnEvent(event))
        {
            return;
        }
    }

    static_cast<void>(OnEvent(event));
}

void Application::DispatchFrame()
{
    OnFrame();
    if (GetStatus() != Status::Running)
    {
        return;
    }

    const std::vector<rad::Ref<EventHandler>> handlers = m_eventHandlers;
    for (const rad::Ref<EventHandler>& handler : handlers)
    {
        handler->OnFrame();
        if (GetStatus() != Status::Running)
        {
            return;
        }
    }
}

} // namespace sd
