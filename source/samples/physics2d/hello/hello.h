// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../../sample.h"

namespace dviglo
{
    class Node;
    class Scene;
}

/// Urho2D and Physics2D sample.
/// This sample demonstrates:
///     - Creating both static and moving 2D physics objects to a scene
///     - Displaying physics debug geometry
class Urho2DPhysics : public Sample
{
    DV_OBJECT(Urho2DPhysics);

public:
    /// Construct.
    explicit Urho2DPhysics();

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

private:
    /// Construct the scene content.
    void create_scene();
    /// Construct an instruction text to the UI.
    void create_instructions();
    /// Set up a viewport for displaying the scene.
    void setup_viewport();
    /// Read input and moves the camera.
    void move_camera(float timeStep);
    /// Subscribe to application-wide logic update events.
    void subscribe_to_events();
    /// Handle the logic update event.
    void handle_update(StringHash eventType, VariantMap& eventData);
};
