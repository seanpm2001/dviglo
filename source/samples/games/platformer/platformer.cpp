// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#include <dviglo/audio/audio.h>
#include <dviglo/core/core_events.h>
#include <dviglo/core/string_utils.h>
#include <dviglo/engine/engine.h>
#include <dviglo/graphics/camera.h>
#include <dviglo/graphics/debug_renderer.h>
#include <dviglo/graphics/graphics.h>
#include <dviglo/graphics/graphics_events.h>
#include <dviglo/graphics/octree.h>
#include <dviglo/graphics/renderer.h>
#include <dviglo/graphics/zone.h>
#include <dviglo/input/input.h>
#include <dviglo/physics_2d/collision_box_2d.h>
#include <dviglo/physics_2d/collision_chain_2d.h>
#include <dviglo/physics_2d/collision_circle_2d.h>
#include <dviglo/physics_2d/collision_polygon_2d.h>
#include <dviglo/physics_2d/physics_events_2d.h>
#include <dviglo/physics_2d/physics_world_2d.h>
#include <dviglo/physics_2d/rigid_body_2d.h>
#include <dviglo/resource/resource_cache.h>
#include <dviglo/scene/scene.h>
#include <dviglo/scene/scene_events.h>
#include <dviglo/ui/button.h>
#include <dviglo/ui/font.h>
#include <dviglo/ui/text.h>
#include <dviglo/ui/ui.h>
#include <dviglo/ui/ui_events.h>
#include <dviglo/urho_2d/animated_sprite_2d.h>
#include <dviglo/urho_2d/animation_set_2d.h>
#include <dviglo/urho_2d/tilemap_2d.h>
#include <dviglo/urho_2d/tilemap_layer_2d.h>
#include <dviglo/urho_2d/tmx_file_2d.h>

#include <dviglo/common/debug_new.h>

#include "character2d.h"
#include "platformer.h"
#include "../../utilities2d/sample2d.h"
#include "../../utilities2d/mover.h"

DV_DEFINE_APPLICATION_MAIN(Urho2DPlatformer)

Urho2DPlatformer::Urho2DPlatformer()
{
    // Register factory for the Character2D component so it can be created via create_component
    Character2D::register_object();
    // Register factory and attributes for the Mover component so it can be created via create_component, and loaded / saved
    Mover::register_object();
}

void Urho2DPlatformer::Setup()
{
    Sample::Setup();
    engineParameters_[EP_SOUND] = true;
}

void Urho2DPlatformer::Start()
{
    // Execute base class startup
    Sample::Start();

    sample2D_ = new Sample2D();

    // Set filename for load/save functions
    sample2D_->demoFilename_ = "platformer2d";

    // Create the scene content
    create_scene();

    // Create the UI content
    sample2D_->CreateUIContent("PLATFORMER 2D DEMO", character2D_->remainingLifes_, character2D_->remainingCoins_);
    Button* playButton = static_cast<Button*>(DV_UI->GetRoot()->GetChild("PlayButton", true));
    subscribe_to_event(playButton, E_RELEASED, DV_HANDLER(Urho2DPlatformer, HandlePlayButton));

    // Hook up to the frame update events
    subscribe_to_events();
}

void Urho2DPlatformer::create_scene()
{
    scene_ = new Scene();
    sample2D_->scene_ = scene_;

    // Create the Octree, DebugRenderer and PhysicsWorld2D components to the scene
    scene_->create_component<Octree>();
    scene_->create_component<DebugRenderer>();
    /*PhysicsWorld2D* physicsWorld =*/ scene_->create_component<PhysicsWorld2D>();

    // Create camera
    cameraNode_ = scene_->create_child("Camera");
    auto* camera = cameraNode_->create_component<Camera>();
    camera->SetOrthographic(true);

    camera->SetOrthoSize((float)DV_GRAPHICS->GetHeight() * PIXEL_SIZE);
    camera->SetZoom(2.0f * Min((float)DV_GRAPHICS->GetWidth() / 1280.0f, (float)DV_GRAPHICS->GetHeight() / 800.0f)); // Set zoom according to user's resolution to ensure full visibility (initial zoom (2.0) is set for full visibility at 1280x800 resolution)

    // Setup the viewport for displaying the scene
    SharedPtr<Viewport> viewport(new Viewport(scene_, camera));
    DV_RENDERER->SetViewport(0, viewport);

    // Set background color for the scene
    Zone* zone = DV_RENDERER->GetDefaultZone();
    zone->SetFogColor(Color(0.2f, 0.2f, 0.2f));

    // Create tile map from tmx file
    SharedPtr<Node> tileMapNode(scene_->create_child("TileMap"));
    auto* tileMap = tileMapNode->create_component<TileMap2D>();
    tileMap->SetTmxFile(DV_RES_CACHE->GetResource<TmxFile2D>("sprites/tilesets/ortho.tmx"));
    const TileMapInfo2D& info = tileMap->GetInfo();

    // Create Spriter Imp character (from sample 33_SpriterAnimation)
    Node* spriteNode = sample2D_->CreateCharacter(info, 0.8f, Vector3(1.0f, 8.0f, 0.0f), 0.2f);
    character2D_ = spriteNode->create_component<Character2D>(); // Create a logic component to handle character behavior

    // Generate physics collision shapes from the tmx file's objects located in "Physics" (top) layer
    TileMapLayer2D* tileMapLayer = tileMap->GetLayer(tileMap->GetNumLayers() - 1);
    sample2D_->CreateCollisionShapesFromTMXObjects(tileMapNode, tileMapLayer, info);

    // Instantiate enemies and moving platforms at each placeholder of "MovingEntities" layer (placeholders are Poly Line objects defining a path from points)
    sample2D_->PopulateMovingEntities(tileMap->GetLayer(tileMap->GetNumLayers() - 2));

    // Instantiate coins to pick at each placeholder of "Coins" layer (placeholders for coins are Rectangle objects)
    TileMapLayer2D* coinsLayer = tileMap->GetLayer(tileMap->GetNumLayers() - 3);
    sample2D_->PopulateCoins(coinsLayer);

    // Init coins counters
    character2D_->remainingCoins_ = coinsLayer->GetNumObjects();
    character2D_->maxCoins_ = coinsLayer->GetNumObjects();

    //Instantiate triggers (for ropes, ladders, lava, slopes...) at each placeholder of "Triggers" layer (placeholders for triggers are Rectangle objects)
    sample2D_->PopulateTriggers(tileMap->GetLayer(tileMap->GetNumLayers() - 4));

    // Create background
    sample2D_->CreateBackgroundSprite(info, 3.5, "textures/heightmap.png", true);

    // Check when scene is rendered
    subscribe_to_event(E_ENDRENDERING, DV_HANDLER(Urho2DPlatformer, HandleSceneRendered));
}

void Urho2DPlatformer::HandleSceneRendered(StringHash eventType, VariantMap& eventData)
{
    unsubscribe_from_event(E_ENDRENDERING);
    // Save the scene so we can reload it later
    sample2D_->SaveScene(true);
    // Pause the scene as long as the UI is hiding it
    scene_->SetUpdateEnabled(false);
}

void Urho2DPlatformer::subscribe_to_events()
{
    // Subscribe handle_update() function for processing update events
    subscribe_to_event(E_UPDATE, DV_HANDLER(Urho2DPlatformer, handle_update));

    // Subscribe HandlePostUpdate() function for processing post update events
    subscribe_to_event(E_POSTUPDATE, DV_HANDLER(Urho2DPlatformer, HandlePostUpdate));

    // Subscribe to PostRenderUpdate to draw debug geometry
    subscribe_to_event(E_POSTRENDERUPDATE, DV_HANDLER(Urho2DPlatformer, handle_post_render_update));

    // Subscribe to Box2D contact listeners
    subscribe_to_event(E_PHYSICSBEGINCONTACT2D, DV_HANDLER(Urho2DPlatformer, HandleCollisionBegin));
    subscribe_to_event(E_PHYSICSENDCONTACT2D, DV_HANDLER(Urho2DPlatformer, HandleCollisionEnd));

    // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
    unsubscribe_from_event(E_SCENEUPDATE);
}

void Urho2DPlatformer::HandleCollisionBegin(StringHash eventType, VariantMap& eventData)
{
    // Get colliding node
    auto* hitNode = static_cast<Node*>(eventData[PhysicsBeginContact2D::P_NODEA].GetPtr());
    if (hitNode->GetName() == "Imp")
        hitNode = static_cast<Node*>(eventData[PhysicsBeginContact2D::P_NODEB].GetPtr());
    String nodeName = hitNode->GetName();
    Node* character2DNode = scene_->GetChild("Imp", true);

    // Handle ropes and ladders climbing
    if (nodeName == "Climb")
    {
        if (character2D_->isClimbing_) // If transition between rope and top of rope (as we are using split triggers)
            character2D_->climb2_ = true;
        else
        {
            character2D_->isClimbing_ = true;
            auto* body = character2DNode->GetComponent<RigidBody2D>();
            body->SetGravityScale(0.0f); // Override gravity so that the character doesn't fall
            // Clear forces so that the character stops (should be performed by setting linear velocity to zero, but currently doesn't work)
            body->SetLinearVelocity(Vector2(0.0f, 0.0f));
            body->SetAwake(false);
            body->SetAwake(true);
        }
    }

    if (nodeName == "CanJump")
        character2D_->aboveClimbable_ = true;

    // Handle coins picking
    if (nodeName == "Coin")
    {
        hitNode->Remove();
        character2D_->remainingCoins_ -= 1;
        if (character2D_->remainingCoins_ == 0)
        {
            Text* instructions = static_cast<Text*>(DV_UI->GetRoot()->GetChild("Instructions", true));
            instructions->SetText("!!! Go to the Exit !!!");
        }
        Text* coinsText = static_cast<Text*>(DV_UI->GetRoot()->GetChild("CoinsText", true));
        coinsText->SetText(String(character2D_->remainingCoins_)); // Update coins UI counter
        sample2D_->PlaySoundEffect("powerup.wav");
    }

    // Handle interactions with enemies
    if (nodeName == "Enemy" || nodeName == "Orc")
    {
        auto* animatedSprite = character2DNode->GetComponent<AnimatedSprite2D>();
        float deltaX = character2DNode->GetPosition().x - hitNode->GetPosition().x;

        // Orc killed if character is fighting in its direction when the contact occurs (flowers are not destroyable)
        if (nodeName == "Orc" && animatedSprite->GetAnimation() == "attack" && (deltaX < 0 == animatedSprite->GetFlipX()))
        {
            static_cast<Mover*>(hitNode->GetComponent<Mover>())->emitTime_ = 1;
            if (!hitNode->GetChild("Emitter", true))
            {
                hitNode->GetComponent("RigidBody2D")->Remove(); // Remove Orc's body
                sample2D_->SpawnEffect(hitNode);
                sample2D_->PlaySoundEffect("big_explosion.wav");
            }
        }
        // Player killed if not fighting in the direction of the Orc when the contact occurs, or when colliding with a flower
        else
        {
            if (!character2DNode->GetChild("Emitter", true))
            {
                character2D_->wounded_ = true;
                if (nodeName == "Orc")
                {
                    auto* orc = static_cast<Mover*>(hitNode->GetComponent<Mover>());
                    orc->fightTimer_ = 1;
                }
                sample2D_->SpawnEffect(character2DNode);
                sample2D_->PlaySoundEffect("big_explosion.wav");
            }
        }
    }

    // Handle exiting the level when all coins have been gathered
    if (nodeName == "Exit" && character2D_->remainingCoins_ == 0)
    {
        // Update UI
        Text* instructions = static_cast<Text*>(DV_UI->GetRoot()->GetChild("Instructions", true));
        instructions->SetText("!!! WELL DONE !!!");
        instructions->SetPosition(IntVector2(0, 0));
        // Put the character outside of the scene and magnify him
        character2DNode->SetPosition(Vector3(-20.0f, 0.0f, 0.0f));
        character2DNode->SetScale(1.5f);
    }

    // Handle falling into lava
    if (nodeName == "Lava")
    {
        auto* body = character2DNode->GetComponent<RigidBody2D>();
        body->ApplyForceToCenter(Vector2(0.0f, 1000.0f), true);
        if (!character2DNode->GetChild("Emitter", true))
        {
            character2D_->wounded_ = true;
            sample2D_->SpawnEffect(character2DNode);
            sample2D_->PlaySoundEffect("big_explosion.wav");
        }
    }

    // Handle climbing a slope
    if (nodeName == "Slope")
        character2D_->onSlope_ = true;
}

void Urho2DPlatformer::HandleCollisionEnd(StringHash eventType, VariantMap& eventData)
{
    // Get colliding node
    auto* hitNode = static_cast<Node*>(eventData[PhysicsEndContact2D::P_NODEA].GetPtr());
    if (hitNode->GetName() == "Imp")
        hitNode = static_cast<Node*>(eventData[PhysicsEndContact2D::P_NODEB].GetPtr());
    String nodeName = hitNode->GetName();
    Node* character2DNode = scene_->GetChild("Imp", true);

    // Handle leaving a rope or ladder
    if (nodeName == "Climb")
    {
        if (character2D_->climb2_)
            character2D_->climb2_ = false;
        else
        {
            character2D_->isClimbing_ = false;
            auto* body = character2DNode->GetComponent<RigidBody2D>();
            body->SetGravityScale(1.0f); // Restore gravity
        }
    }

    if (nodeName == "CanJump")
        character2D_->aboveClimbable_ = false;

    // Handle leaving a slope
    if (nodeName == "Slope")
    {
        character2D_->onSlope_ = false;
        // Clear forces (should be performed by setting linear velocity to zero, but currently doesn't work)
        auto* body = character2DNode->GetComponent<RigidBody2D>();
        body->SetLinearVelocity(Vector2::ZERO);
        body->SetAwake(false);
        body->SetAwake(true);
    }
}

void Urho2DPlatformer::handle_update(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Zoom in/out
    if (cameraNode_)
        sample2D_->Zoom(cameraNode_->GetComponent<Camera>());

    // Toggle debug geometry with 'Z' key.
    // Используем скан-код, а не код клавиши, иначе не будет работать в Linux, когда включена русская раскладка клавиатуры
    if (DV_INPUT->GetScancodePress(SCANCODE_Z))
        draw_debug_ = !draw_debug_;

    // Check for loading / saving the scene
    if (DV_INPUT->GetKeyPress(KEY_F5))
        sample2D_->SaveScene(false);
    if (DV_INPUT->GetKeyPress(KEY_F7))
        ReloadScene(false);
}

void Urho2DPlatformer::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{
    if (!character2D_)
        return;

    Node* character2DNode = character2D_->GetNode();
    cameraNode_->SetPosition(Vector3(character2DNode->GetPosition().x, character2DNode->GetPosition().y, -10.0f)); // Camera tracks character
}

void Urho2DPlatformer::handle_post_render_update(StringHash eventType, VariantMap& eventData)
{
    if (draw_debug_)
    {
        auto* physicsWorld = scene_->GetComponent<PhysicsWorld2D>();
        physicsWorld->draw_debug_geometry();

        Node* tileMapNode = scene_->GetChild("TileMap", true);
        auto* map = tileMapNode->GetComponent<TileMap2D>();
        map->draw_debug_geometry(scene_->GetComponent<DebugRenderer>(), false);
    }
}

void Urho2DPlatformer::ReloadScene(bool reInit)
{
    String filename = sample2D_->demoFilename_;
    if (!reInit)
        filename += "_in_game";

    File loadFile(DV_FILE_SYSTEM->GetProgramDir() + "data/scenes/" + filename + ".xml", FILE_READ);
    scene_->load_xml(loadFile);
    // After loading we have to reacquire the weak pointer to the Character2D component, as it has been recreated
    // Simply find the character's scene node by name as there's only one of them
    Node* character2DNode = scene_->GetChild("Imp", true);
    if (character2DNode)
        character2D_ = character2DNode->GetComponent<Character2D>();

    // Set what number to use depending whether reload is requested from 'PLAY' button (reInit=true) or 'F7' key (reInit=false)
    int lifes = character2D_->remainingLifes_;
    int coins = character2D_->remainingCoins_;
    if (reInit)
    {
        lifes = LIFES;
        coins = character2D_->maxCoins_;
    }

    // Update lifes UI
    Text* lifeText = static_cast<Text*>(DV_UI->GetRoot()->GetChild("LifeText", true));
    lifeText->SetText(String(lifes));

    // Update coins UI
    Text* coinsText = static_cast<Text*>(DV_UI->GetRoot()->GetChild("CoinsText", true));
    coinsText->SetText(String(coins));
}

void Urho2DPlatformer::HandlePlayButton(StringHash eventType, VariantMap& eventData)
{
    // Remove fullscreen UI and unfreeze the scene
    UI* ui = DV_UI;
    if (static_cast<Text*>(ui->GetRoot()->GetChild("FullUI", true)))
    {
        ui->GetRoot()->GetChild("FullUI", true)->Remove();
        scene_->SetUpdateEnabled(true);
    }
    else
    {
        // Reload scene
        ReloadScene(true);
    }

    // Hide Instructions and Play/Exit buttons
    Text* instructionText = static_cast<Text*>(ui->GetRoot()->GetChild("Instructions", true));
    instructionText->SetText("");
    Button* exitButton = static_cast<Button*>(ui->GetRoot()->GetChild("ExitButton", true));
    exitButton->SetVisible(false);
    Button* playButton = static_cast<Button*>(ui->GetRoot()->GetChild("PlayButton", true));
    playButton->SetVisible(false);

    // Hide mouse cursor
    DV_INPUT->SetMouseVisible(false);
}
