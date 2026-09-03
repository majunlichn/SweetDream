#pragma once

#include <rad/Core/RefCounted.h>

#include <SDL3/SDL_events.h>

namespace sd
{

class EventHandler : public rad::RefCounted<EventHandler>
{
public:
    EventHandler() = default;
    virtual ~EventHandler();

    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    // Return true to stop forwarding this event to later handlers.
    virtual bool OnEvent(const SDL_Event& event) = 0;

    // Called once per main-loop iteration after queued events have been dispatched.
    virtual void OnFrame() {}
}; // class EventHandler

} // namespace sd
