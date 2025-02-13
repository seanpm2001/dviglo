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

/// Ragdoll example.
/// This sample demonstrates:
///     - Detecting physics collisions
///     - Moving an AnimatedModel's bones with physics and connecting them with constraints
///     - Using rolling friction to stop rolling objects from moving infinitely
class Ragdolls : public Sample
{
    DV_OBJECT(Ragdolls);

public:
    /// Construct.
    explicit Ragdolls();

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

private:
    /// Construct the scene content.
    void create_scene();
    /// Construct an instruction text to the UI.
    void create_instructions();
    /// Set up a viewport for displaying the scene.
    void setup_viewport();
    /// Subscribe to application-wide logic update and post-render update events.
    void subscribe_to_events();
    /// Read input and moves the camera.
    void move_camera(float timeStep);
    /// Spawn a physics object from the camera position.
    void SpawnObject();
    /// Handle the logic update event.
    void handle_update(StringHash eventType, VariantMap& eventData);
    /// Handle the post-render update event.
    void handle_post_render_update(StringHash eventType, VariantMap& eventData);

    /// Flag for drawing debug geometry.
    bool draw_debug_;
};
