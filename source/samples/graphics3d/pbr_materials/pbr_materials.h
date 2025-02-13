// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../../sample.h"

namespace dviglo
{

class Drawable;
class Node;
class Scene;
class Zone;

}

/// PBR materials example.
/// This sample demonstrates:
///      - Loading a scene that showcases physically based materials & shaders
///
/// To use with deferred rendering, a PBR deferred renderpath should be chosen:
/// core_data/render_paths/pbr_deferred.xml or core_data/render_paths/pbr_deferred_hw_depth.xml
class PBRMaterials : public Sample
{
    DV_OBJECT(PBRMaterials);

public:
    /// Construct.
    explicit PBRMaterials();

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

private:
    /// Construct the scene content.
    void create_scene();
    /// Construct user interface elements.
    void create_ui();
    /// Set up a viewport for displaying the scene.
    void setup_viewport();
    /// Subscribe to application-wide logic update event.
    void subscribe_to_events();
    /// Reads input and moves the camera.
    void move_camera(float timeStep);
    /// Handle the logic update event.
    void handle_update(StringHash eventType, VariantMap& eventData);
    /// Construct an instruction text to the UI.
    void create_instructions();
    /// Handle the roughness slider drag event.
    void HandleRoughnessSliderChanged(StringHash eventType, VariantMap& eventData);
    /// Handle the metallic slider drag event.
    void HandleMetallicSliderChanged(StringHash eventType, VariantMap& eventData);
    /// Handle the ambient HDR scale slider drag event.
    void HandleAmbientSliderChanged(StringHash eventType, VariantMap& eventData);

    /// Dynamic material.
    Material* dynamicMaterial_;
    /// Roughness label.
    Text* roughnessLabel_;
    /// Metallic label.
    Text* metallicLabel_;
    /// Ambient HDR scale label.
    Text* ambientLabel_;
    /// Zone component in scene.
    WeakPtr<Zone> zone_;
};
