// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include <dviglo/math/plane.h>

#include "../../sample.h"

namespace dviglo
{

class Node;
class Scene;

}

/// Water example.
/// This sample demonstrates:
///     - Creating a large plane to represent a water body for rendering
///     - Setting up a second camera to render reflections on the water surface
class Water : public Sample
{
    DV_OBJECT(Water);

public:
    /// Construct.
    explicit Water();

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

private:
    /// Construct the scene content.
    void create_scene();
    /// Construct an instruction text to the UI.
    void create_instructions();
    /// Set up a viewport for displaying the scene.
    void setup_viewport();
    /// Subscribe to the logic update event.
    void subscribe_to_events();
    /// Read input and moves the camera.
    void move_camera(float timeStep);
    /// Handle the logic update event.
    void handle_update(StringHash eventType, VariantMap& eventData);

    /// Reflection camera scene node.
    SharedPtr<Node> reflectionCameraNode_;
    /// Water body scene node.
    SharedPtr<Node> waterNode_;
    /// Reflection plane representing the water surface.
    Plane waterPlane_;
    /// Clipping plane for reflection rendering. Slightly biased downward from the reflection plane to avoid artifacts.
    Plane waterClipPlane_;
};
