#include "Game/App.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/UI/UISystem.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"


App* g_app = nullptr;
Renderer* g_renderer = nullptr;
Window* g_window = nullptr;
BitmapFont* g_squirrelFont = nullptr;
ModelLoader* g_modelLoader = nullptr;
RandomNumberGenerator* g_rng = nullptr;
AudioSystem* g_audio = nullptr;

constexpr float WINDOW_ASPECT = 1.f;
//constexpr float WINDOW_ASPECT = 0.954167f;


bool App::HandleQuitRequested(EventArgs& args)
{
	UNUSED(args);
	g_app->HandleQuitRequested();
	return true;
}

bool App::ShowControls(EventArgs& args)
{
	UNUSED(args);

	// Add controls to DevConsole
	g_console->AddLine(Rgba8::STEEL_BLUE, "Editor controls", false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Move", "WASD"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Look Around", "Mouse"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Release Mouse Cursor", "RMB (Hold)"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Undo", "Ctrl+Z"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Redo", "Ctrl+Shift+Z/Ctrl+Y"), false);
	g_console->AddLine(Rgba8::STEEL_BLUE, "Create Mode", false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Cycle Entities", "Q/E"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Spawn Entities", "LMB (Hold)"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Change Entity Distance", "Mouse Wheel"), false);
	g_console->AddLine(Rgba8::STEEL_BLUE, "Edit Mode", false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Translate Entity", "LMB (Hold)"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Clone Entity", "LAlt + LMB (Hold)"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Clone Entity", "LAlt + LMB (Hold)"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Change Entity Distance", "Mouse Wheel"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Rotate Entity", "Left/Right Arrow"), false);
	g_console->AddLine(Rgba8::MAGENTA, Stringf("%-20s : Scale Entity", "Up/Down Arrow"), false);

	return true;
}

App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	EventSystemConfig eventSystemConfig;
	g_eventSystem = new EventSystem(eventSystemConfig);

	InputConfig inputConfig;
	g_input = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_inputSystem = g_input;
	windowConfig.m_windowTitle = "ArchiLeapVR";
	windowConfig.m_clientAspect = WINDOW_ASPECT;
	g_window = new Window(windowConfig);

	RenderConfig renderConfig;
	renderConfig.m_window = g_window;
	g_renderer = new Renderer(renderConfig);

	Camera devConsoleCamera;
	devConsoleCamera.SetOrthoView(Vec2::ZERO, Vec2(WINDOW_ASPECT, 1.f));
	devConsoleCamera.SetViewport(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * WINDOW_ASPECT, SCREEN_SIZE_Y));
	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_camera = devConsoleCamera;
	devConsoleConfig.m_consoleFontFilePathWithNoExtension = "Data/Images/SquirrelFixedFont";
	devConsoleConfig.m_renderer = g_renderer;
	devConsoleConfig.m_overlayColor = Rgba8(0, 0, 0, 200);
	devConsoleConfig.m_linesToShow = 50.5f;
	devConsoleConfig.m_fontAspect = 0.7f;
	g_console = new DevConsole(devConsoleConfig);

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_renderer;
	debugRenderConfig.m_startVisible = false;
	debugRenderConfig.m_messageHeightFractionOfScreenHeight = 0.02f;
	debugRenderConfig.m_bitmapFontFilePathWithNoExtension = "Data/Images/SquirrelFixedFont";

	OpenXRConfig openXRConfig;
	openXRConfig.m_renderer = g_renderer;
	g_openXR = new OpenXR(openXRConfig);

	ModelLoaderConfig modelLoaderConfig;
	modelLoaderConfig.m_renderer = g_renderer;
	g_modelLoader = new ModelLoader(modelLoaderConfig);

	UISystemConfig uiSystemConfig;
	uiSystemConfig.m_fontFileNameWithNoExtension = "Data/Fonts/RobotoMonoSemiBold128";
	uiSystemConfig.m_input = g_input;
	uiSystemConfig.m_renderer = g_renderer;
	uiSystemConfig.m_supportKeyboard = false;
	Camera uiSystemCamera;
	uiSystemCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * WINDOW_ASPECT, SCREEN_SIZE_Y));
	uiSystemCamera.SetViewport(Vec2(0.f, 0.f), Vec2(SCREEN_SIZE_Y * WINDOW_ASPECT, SCREEN_SIZE_Y));
	uiSystemConfig.m_camera = uiSystemCamera;
	g_ui = new UISystem(uiSystemConfig);

	AudioConfig audioConfig;
	g_audio = new AudioSystem(audioConfig);

	g_rng = new RandomNumberGenerator();

	g_window->Startup();
	g_renderer->Startup();
	g_eventSystem->Startup();
	g_console->Startup();
	g_ui->Startup();
	g_input->Startup();
	DebugRenderSystemStartup(debugRenderConfig);
	g_openXR->Startup();
	g_modelLoader->Startup();
	g_audio->Startup();

	m_game = new Game();
	InitializeCameras();

	SubscribeEventCallbackFunction("Quit", HandleQuitRequested, "Exit the application");
	SubscribeEventCallbackFunction("Controls", ShowControls, "Show controls");

	EventArgs emptyArgs;
	FireEvent("Controls", emptyArgs);
}

void App::Run()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

double renderTime_ms = 0.f;
void App::RunFrame()
{
	BeginFrame();
	
	double updateStartTimeSeconds = GetCurrentTimeSeconds();
	Update();
	double updateEndTimeSeconds = GetCurrentTimeSeconds();
	double updateTime_ms = (updateEndTimeSeconds - updateStartTimeSeconds) * 1000.f;
	DebugAddScreenText(Stringf("Update: %.0f ms", updateTime_ms), Vec2(48.f, 384.f), 192.f, Vec2(0.f, 0.f), 0.f);

	double renderStartTimeSecodns = GetCurrentTimeSeconds();
	m_currentEye = XREye::NONE;
	g_renderer->BeginRenderForEye(XREye::NONE);

	RenderCustomScreens();

	g_renderer->BeginRenderEvent("Screen to Texture");
	RenderScreen();
	g_renderer->EndRenderEvent("Screen to Texture");

	m_game->ClearScreen();
	g_renderer->BeginRenderEvent("Desktop Single View");
	Render();
	g_renderer->EndRenderEvent("Desktop Single View");
	
	if (g_openXR->IsInitialized())
	{
		m_currentEye = XREye::LEFT;
		g_renderer->BeginRenderForEye(XREye::LEFT);
		g_renderer->BeginRenderEvent("HMD Left Eye");
		m_game->ClearScreen();
		Render();
		g_renderer->EndRenderEvent("HMD Left Eye");

		m_currentEye = XREye::RIGHT;
		g_renderer->BeginRenderForEye(XREye::RIGHT);
		g_renderer->BeginRenderEvent("HMD Right Eye");
		m_game->ClearScreen();
		Render();
		g_renderer->EndRenderEvent("HMD Right Eye");
	}
	double renderEndTimeSeconds = GetCurrentTimeSeconds();
	renderTime_ms = (renderEndTimeSeconds - renderStartTimeSecodns) * 1000.f;

	EndFrame();
}

bool App::HandleQuitRequested()
{
	m_isQuitting = true;

	return true;
}

XREye App::GetCurrentEye() const
{
	return m_currentEye;
}

Camera const App::GetCurrentCamera() const
{
	if (m_currentEye == XREye::LEFT)
	{
		return m_leftEyeCamera;
	}

	if (m_currentEye == XREye::RIGHT)
	{
		return m_rightEyeCamera;
	}

	// m_currentEye is NONE
	if (m_currentEyeForScreen == XREye::LEFT)
	{
		return m_leftWorldCamera;
	}
	else if (m_currentEyeForScreen == XREye::RIGHT)
	{
		return m_rightWorldCamera;
	}

	return m_worldCamera;
}

void App::InitializeCameras()
{
	m_worldCamera.SetRenderBasis(Vec3::SKYWARD, Vec3::WEST, Vec3::NORTH);
	m_worldCamera.SetPerspectiveView(g_window->GetAspect(), 60.f, NEAR_PLANE_DISTANCE, FAR_PLANE_DISTANCE);
	m_worldCamera.SetTransform(Vec3::ZERO, EulerAngles::ZERO);

	m_leftWorldCamera.SetRenderBasis(Vec3::SKYWARD, Vec3::WEST, Vec3::NORTH);
	m_rightWorldCamera.SetRenderBasis(Vec3::SKYWARD, Vec3::WEST, Vec3::NORTH);

	m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
	m_screenCamera.SetViewport(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
	m_screenRTVTexture = g_renderer->CreateRenderTargetTexture("ScreenTexture", IntVec2(int(SCREEN_SIZE_Y * WINDOW_ASPECT), int(SCREEN_SIZE_Y)));

	m_leftEyeCamera.SetRenderBasis(Vec3::GROUNDWARD, Vec3::WEST, Vec3::NORTH);
	m_rightEyeCamera.SetRenderBasis(Vec3::GROUNDWARD, Vec3::WEST, Vec3::NORTH);
}

void App::BeginFrame()
{
	Clock::TickSystemClock();

	g_eventSystem->BeginFrame();
	g_console->BeginFrame();
	g_input->BeginFrame();
	g_window->BeginFrame();
	g_renderer->BeginFrame();
	DebugRenderBeginFrame();
	g_openXR->BeginFrame();
	g_modelLoader->BeginFrame();
	g_ui->BeginFrame();
	g_audio->BeginFrame();
}

void App::Update()
{
	HandleDevInput();
	m_game->Update();

	DebugAddScreenText(Stringf("FPS: %.0f", 1.f / Clock::GetSystemClock().GetDeltaSeconds()), Vec2(SCREEN_SIZE_Y * g_window->GetAspect() - 48.f, 0.f), 96.f, Vec2(1.f, 0.f), 0.f);
}

void App::Render() const
{
	if (m_currentEye == XREye::NONE)
	{
		g_renderer->BeginCamera(m_worldCamera);
	}
	else if (m_currentEye == XREye::LEFT)
	{
		g_renderer->BeginCamera(m_leftEyeCamera);
	}
	else if (m_currentEye == XREye::RIGHT)
	{
		g_renderer->BeginCamera(m_rightEyeCamera);
	}

	// Render game
	m_game->Render();

	if (m_currentEye == XREye::NONE)
	{
		g_renderer->EndCamera(m_worldCamera);
		DebugRenderWorld(m_worldCamera);
	}
	else if (m_currentEye == XREye::LEFT)
	{
		g_renderer->EndCamera(m_leftEyeCamera);
		DebugRenderWorld(m_leftEyeCamera);
	}
	else if (m_currentEye == XREye::RIGHT)
	{
		g_renderer->EndCamera(m_rightEyeCamera);
		DebugRenderWorld(m_rightEyeCamera);
	}
}

void App::EndFrame()
{
	g_audio->EndFrame();
	g_ui->EndFrame();
	g_modelLoader->EndFrame();
	g_openXR->EndFrame();
	DebugRenderEndFrame();
	g_renderer->EndFrame();
	g_window->EndFrame();
	g_input->EndFrame();
	g_console->EndFrame();
	g_eventSystem->EndFrame();
}

void App::RenderScreen() const
{
	g_renderer->BindTexture(nullptr);

	g_renderer->SetRTV(m_screenRTVTexture);
	g_renderer->ClearRTV(Rgba8::TRANSPARENT_BLACK, m_screenRTVTexture);

	g_renderer->BeginCamera(m_screenCamera);
	m_game->RenderScreen();
	g_renderer->EndCamera(m_screenCamera);

	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::BILINEAR_CLAMP);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetModelConstants();
	g_renderer->BindShader(nullptr);
	g_ui->Render();
	
	DebugAddScreenText(Stringf("Render: %.0f ms", renderTime_ms), Vec2(48.f, 128.f), 192.f, Vec2(0.f, 0.f), 0.f);
	DebugRenderScreen(m_screenCamera);

	// Render Dev Console
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetModelConstants();
	g_renderer->BindShader(nullptr);
	g_console->Render(AABB2(Vec2(0.f, 0.f), Vec2(WINDOW_ASPECT, 1.f)));
	

	g_renderer->SetRTV();
}

void App::RenderCustomScreens() const
{
	g_renderer->BeginRenderEvent("Custom Screens");
	m_game->RenderCustomScreens();
	g_renderer->EndRenderEvent("Custom Screens");
}

void App::HandleDevInput()
{
	if (g_console->GetMode() == DevConsoleMode::HIDDEN && g_window->HasFocus() && m_game->m_state == GameState::GAME && !g_input->IsKeyDown(KEYCODE_RMB))
	{
		g_input->SetCursorMode(true, true);
	}
	else
	{
		g_input->SetCursorMode(false, false);
	}

	if (g_input->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_console->ToggleMode(DevConsoleMode::OPENFULL);
		g_ui->SetFocus(g_console->GetMode() == DevConsoleMode::HIDDEN);
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F1))
	{
		FireEvent("DebugRenderToggle");
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F8))
	{
		delete m_game;
		m_game = new Game();
	}
}

void App::Shutdown()
{
	delete m_game;
	m_game = nullptr;

	g_audio->Shutdown();
	g_ui->Shutdown();
	g_modelLoader->Shutdown();
	g_openXR->Shutdown();
	DebugRenderSystemShutdown();
	g_renderer->Shutdown();
	g_input->Shutdown();
	g_window->Shutdown();
	g_eventSystem->Shutdown();
}

