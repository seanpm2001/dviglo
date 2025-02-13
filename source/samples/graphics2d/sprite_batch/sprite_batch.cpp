#include <dviglo/dviglo_all.h>

#include <dviglo/graphics/sprite_batch.h>

using namespace dviglo;

class Game : public Application
{
    DV_OBJECT(Game);

public:
    SharedPtr<Scene> scene_;
    SharedPtr<Node> cameraNode_;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    SharedPtr<SpriteBatch> worldSpaceSpriteBatch_;
    SharedPtr<SpriteBatch> screenSpaceSpriteBatch_;
    SharedPtr<SpriteBatch> virtualSpriteBatch_;
    float fpsTimeCounter_ = 0.0f;
    i32 fpsFrameCounter_ = 0;
    i32 fpsValue_ = 0;
    i32 currentTest_ = 1;
    float angle_ = 0.0f;
    float scale_ = 0.0f;

    Game()
    {
    }

    void Setup()
    {
        engineParameters_[EP_WINDOW_RESIZABLE] = true;
        engineParameters_[EP_FULL_SCREEN] = false;
        engineParameters_[EP_WINDOW_WIDTH] = 1200;
        engineParameters_[EP_WINDOW_HEIGHT] = 900;
        engineParameters_[EP_FRAME_LIMITER] = false;
        engineParameters_[EP_LOG_NAME] = get_pref_path("dviglo", "logs") + "56_SpriteBatch.log";
    }

    void Start()
    {
        //DV_ENGINE->SetMaxFps(10);

        create_scene();
        setup_viewport();
        subscribe_to_events();

        XmlFile* xmlFile = DV_RES_CACHE->GetResource<XmlFile>("ui/default_style.xml");
        DV_DEBUG_HUD->SetDefaultStyle(xmlFile);

        screenSpaceSpriteBatch_ = new SpriteBatch();
        
        worldSpaceSpriteBatch_ = new SpriteBatch();
        worldSpaceSpriteBatch_->camera = cameraNode_->GetComponent<Camera>();
        worldSpaceSpriteBatch_->compare_mode = CMP_LESSEQUAL;

        virtualSpriteBatch_ = new SpriteBatch();
        virtualSpriteBatch_->virtual_screen_size = IntVector2(700, 600);
    }

    void setup_viewport()
    {
        SharedPtr<Viewport> viewport(new Viewport(scene_, cameraNode_->GetComponent<Camera>()));
        DV_RENDERER->SetViewport(0, viewport);
    }

    void create_scene()
    {
        ResourceCache* cache = DV_RES_CACHE;

        scene_ = new Scene();
        scene_->create_component<Octree>();

        Node* planeNode = scene_->create_child("Plane");
        planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
        StaticModel* planeObject = planeNode->create_component<StaticModel>();
        planeObject->SetModel(cache->GetResource<Model>("models/plane.mdl"));
        planeObject->SetMaterial(cache->GetResource<Material>("materials/stone_tiled.xml"));

        Node* lightNode = scene_->create_child("DirectionalLight");
        lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f));
        Light* light = lightNode->create_component<Light>();
        light->SetColor(Color(0.5f, 0.5f, 0.5f));
        light->SetLightType(LIGHT_DIRECTIONAL);
        light->SetCastShadows(true);
        light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
        light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
        //light->SetShadowIntensity(0.5f);

        Node* zoneNode = scene_->create_child("Zone");
        Zone* zone = zoneNode->create_component<Zone>();
        zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
        zone->SetAmbientColor(Color(0.5f, 0.5f, 0.5f));
        zone->SetFogColor(Color(0.4f, 0.5f, 0.8f));
        zone->SetFogStart(100.0f);
        zone->SetFogEnd(300.0f);

        constexpr i32 NUM_OBJECTS = 20;
        for (i32 i = 0; i < NUM_OBJECTS; ++i)
        {
            Node* mushroomNode = scene_->create_child("Mushroom");
            mushroomNode->SetPosition(Vector3(Random(90.0f) - 45.0f, 0.0f, Random(90.0f) - 45.0f));
            mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
            mushroomNode->SetScale(0.5f + Random(2.0f));
            StaticModel* mushroomObject = mushroomNode->create_component<StaticModel>();
            mushroomObject->SetModel(cache->GetResource<Model>("models/mushroom.mdl"));
            mushroomObject->SetMaterial(cache->GetResource<Material>("materials/mushroom.xml"));
            mushroomObject->SetCastShadows(true);
        }

        cameraNode_ = scene_->create_child("Camera");
        cameraNode_->create_component<Camera>();
        cameraNode_->SetPosition(Vector3(0.0f, 2.0f, -5.0f));
    }

    void move_camera(float timeStep)
    {
        constexpr float MOVE_SPEED = 20.0f;
        constexpr float MOUSE_SENSITIVITY = 0.1f;

        Input* input = DV_INPUT;

        IntVector2 mouseMove = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * mouseMove.x;
        pitch_ += MOUSE_SENSITIVITY * mouseMove.y;
        pitch_ = Clamp(pitch_, -90.0f, 90.0f);
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

        if (input->GetKeyDown(KEY_W))
            cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
        if (input->GetKeyDown(KEY_S))
            cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
        if (input->GetKeyDown(KEY_A))
            cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
        if (input->GetKeyDown(KEY_D))
            cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

    }

    void subscribe_to_events()
    {
        subscribe_to_event(E_UPDATE, DV_HANDLER(Game, handle_update));
        subscribe_to_event(E_ENDVIEWRENDER, DV_HANDLER(Game, HandleEndViewRender));
    }

    void handle_update(StringHash eventType, VariantMap& eventData)
    {
        using namespace Update;
        float timeStep = eventData[P_TIMESTEP].GetFloat();

        Input* input = DV_INPUT;

        if (input->GetKeyPress(KEY_1))
            currentTest_ = 1;
        else if (input->GetKeyPress(KEY_2))
            currentTest_ = 2;
        else if (input->GetKeyPress(KEY_3))
            currentTest_ = 3;
        else if (input->GetKeyPress(KEY_ESCAPE))
            DV_ENGINE->Exit();

        if (input->GetMouseButtonDown(MOUSEB_RIGHT))
        {
            input->SetMouseVisible(false);
            move_camera(timeStep);
        }
        else
        {
            input->SetMouseVisible(true);
        }

        fpsTimeCounter_ += timeStep;
        fpsFrameCounter_++;

        if (fpsTimeCounter_ >= 1.0f)
        {
            fpsTimeCounter_ = 0.0f;
            fpsValue_ = fpsFrameCounter_;
            fpsFrameCounter_ = 0;
        }

        angle_ += timeStep * 100.0f;
        angle_ = fmod(angle_, 360.0f);

        scale_ = 1.0f + Sin(DV_TIME->GetElapsedTime() * 200.0f) * 0.3f;
    }

    bool collide_ = true;

    void HandleEndViewRender(StringHash eventType, VariantMap& eventData)
    {
        ResourceCache* cache = DV_RES_CACHE;
        Graphics* graphics = DV_GRAPHICS;

        // Размер текстуры должен быть степенью двойки (64, 128, 256, ...),
        // иначе она не будет работать в GL ES 1.0 (в вебе).
        // В некоторых случаях (например для "sprites/stretchable.png" - 200x200) может помочь
        // Texture2D* head = cache->GetResource<Texture2D>("sprites/stretchable.png");
        // if (head->GetAddressMode(COORD_U) == ADDRESS_WRAP)
        // {
        //     head->SetAddressMode(COORD_U, ADDRESS_CLAMP);
        //     head->SetAddressMode(COORD_V, ADDRESS_CLAMP);
        // }
        // как это сделано в Sprite2D.cpp.
        // В других случаях ("sprites/imp/imp_head.png" - 238x149) и это не помогает

        Texture2D* head = cache->GetResource<Texture2D>("textures/fish_bone_logo.png");
        Font* font = cache->GetResource<Font>("fonts/anonymous pro.ttf");

        // Очистка экрана. Если сцена пустая, то можно просто задать цвет зоны
        //graphics->Clear(CLEAR_COLOR, Color::GREEN);

        String str;

        if (currentTest_ == 1)
        {
            screenSpaceSpriteBatch_->set_shape_color(0xFFFF0000);
            screenSpaceSpriteBatch_->draw_circle(500.f, 200.f, 200.f);

            Vector2 origin = Vector2(head->GetWidth() * 0.5f, head->GetHeight() * 0.5f);
            screenSpaceSpriteBatch_->draw_sprite(head, Vector2(200.0f, 200.0f), nullptr, 0xFFFFFFFF, angle_, origin, Vector2(scale_, scale_));

            str = "QqWЙйр";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 60.f), 0xFF00FF00, 0.0f, Vector2::ZERO, Vector2::ONE, FlipModes::none);
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(104.0f, 60.f), 0xFF00FF00, 0.0f, Vector2::ZERO, Vector2::ONE, FlipModes::horizontally);
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 100.f), 0xFF00FF00, 0.0f, Vector2::ZERO, Vector2::ONE, FlipModes::vertically);
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(104.0f, 100.f), 0xFF00FF00, 0.0f, Vector2::ZERO, Vector2::ONE, FlipModes::both);

            // screenSpaceSpriteBatch_->flush(); не вызываем, так как ещё текст будем выводить
        }
        else if (currentTest_ == 2)
        {
            worldSpaceSpriteBatch_->draw_sprite(head, Vector2(0.0f, 0.0f), nullptr, 0xFFFFFFFF, 0.0f, Vector2::ZERO, Vector2(0.01f, 0.01f));
            worldSpaceSpriteBatch_->flush();
        }
        else if (currentTest_ == 3)
        {
            virtualSpriteBatch_->set_shape_color(0x90FF0000);
            virtualSpriteBatch_->draw_aabb_solid(Vector2::ZERO, (Vector2)virtualSpriteBatch_->virtual_screen_size);
            virtualSpriteBatch_->draw_sprite(head, Vector2(200.0f, 200.0f));
            // Преобразуем координаты мыши из оконных координат в виртуальные координаты
            Vector2 virtualMousePos = virtualSpriteBatch_->to_virtual_pos(Vector2(DV_INPUT->GetMousePosition()));
            virtualSpriteBatch_->set_shape_color(0xFFFFFFFF);
            virtualSpriteBatch_->draw_arrow({100.0f, 100.f}, virtualMousePos, 10);
            virtualSpriteBatch_->flush();
        }

        // Выводим индекс текущего теста
        str = "Текущий тест: " + String(currentTest_) + " (используйте 1, 2, 3 для переключения)";
        screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 0.f), 0xFFFFFFFF);

        // Выводим описание текущего теста
        if (currentTest_ == 1)
        {
            str = "Рисование в экранном пространстве";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 24.f), 0xFFFFFFFF);
        }
        else if (currentTest_ == 2)
        {
            str = "Рисование в пространстве сцены";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 24.f), 0xFFFFFFFF);
        }
        else if (currentTest_ == 3)
        {
            str = "Использование виртуальных координат";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 24.f), 0xFFFFFFFF);

            str = "Синий прямоугольник - границы виртуального экрана (700x600 виртуальных пикселей)";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 48.f), 0xFFFFFFFF);

            str = "Меняйте размеры окна, чтобы увидеть, как виртуальный экран вписывается в окно";
            screenSpaceSpriteBatch_->draw_string(str, font, 20.f, Vector2(4.0f, 72.f), 0xFFFFFFFF);
        }

        // Выводим подсказку про ПКМ
        str = "Зажмите ПКМ для перемещения по сцене";
        Vector2 pos{graphics->GetWidth() - 550.f, graphics->GetHeight() - 36.f}; // TODO: Добавить MeasureString
        screenSpaceSpriteBatch_->draw_string(str, font, 20.f, pos, 0xFFFFFFFF);

        // Выводим FPS
        str = "FPS: " + String(fpsValue_);
        pos = {10.f, graphics->GetHeight() - 56.f};
        screenSpaceSpriteBatch_->draw_string(str, font, 40.f, pos, 0xFF0000FF);

        screenSpaceSpriteBatch_->flush();
    }
};

DV_DEFINE_APPLICATION_MAIN(Game)
