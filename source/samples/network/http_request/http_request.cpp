// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include <dviglo/core/core_events.h>
#include <dviglo/core/process_utils.h>
#include <dviglo/input/input.h>
#include <dviglo/network/network.h>
#include <dviglo/network/http_request.h>
#include <dviglo/ui/font.h>
#include <dviglo/ui/text.h>
#include <dviglo/ui/ui.h>

#include "http_request.h"

#include <dviglo/common/debug_new.h>

DV_DEFINE_APPLICATION_MAIN(HttpRequestDemo)

HttpRequestDemo::HttpRequestDemo()
{
}

void HttpRequestDemo::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the user interface
    create_ui();

    // Subscribe to basic events such as update
    subscribe_to_events();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_FREE);
}

void HttpRequestDemo::create_ui()
{
    // Construct new Text object
    text_ = new Text();

    // Set font and text color
    text_->SetFont(DV_RES_CACHE->GetResource<Font>("fonts/anonymous pro.ttf"), 15);
    text_->SetColor(Color(1.0f, 1.0f, 0.0f));

    // Align Text center-screen
    text_->SetHorizontalAlignment(HA_CENTER);
    text_->SetVerticalAlignment(VA_CENTER);

    // Add Text instance to the UI root element
    DV_UI->GetRoot()->AddChild(text_);
}

void HttpRequestDemo::subscribe_to_events()
{
    // Subscribe handle_update() function for processing HTTP request
    subscribe_to_event(E_UPDATE, DV_HANDLER(HttpRequestDemo, handle_update));
}

void HttpRequestDemo::handle_update(StringHash eventType, VariantMap& eventData)
{
    if (httpRequest_.Null())
#ifdef DV_SSL
        httpRequest_ = DV_NET->MakeHttpRequest("https://api.ipify.org/?format=json");
#else
        httpRequest_ = DV_NET->MakeHttpRequest("http://httpbin.org/ip");
#endif
    else
    {
        // Initializing HTTP request
        if (httpRequest_->GetState() == HTTP_INITIALIZING)
            return;
        // An error has occurred
        else if (httpRequest_->GetState() == HTTP_ERROR)
        {
            text_->SetText("An error has occurred: " + httpRequest_->GetError());
            unsubscribe_from_event("Update");
            DV_LOGERRORF("HttpRequest error: %s", httpRequest_->GetError().c_str());
        }
        // Get message data
        else
        {
            if (httpRequest_->GetAvailableSize() > 0)
                message_ += httpRequest_->ReadLine();
            else
            {
                text_->SetText("Processing...");

                SharedPtr<JSONFile> json(new JSONFile());
                json->FromString(message_);

#ifdef DV_SSL
                JSONValue val = json->GetRoot().Get("ip");
#else
                JSONValue val = json->GetRoot().Get("origin");
#endif

                if (val.IsNull())
                    text_->SetText("Invalid JSON response retrieved!");
                else
                    text_->SetText("Your IP is: " + val.GetString());

                unsubscribe_from_event("Update");
            }
        }
    }
}
