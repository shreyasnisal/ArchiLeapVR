#include "Game/Game.hpp"

#include "Game/App.hpp"
#include "Game/GameCommon.hpp"
#include "Game/HandController.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"
#include "Game/TileDefinition.hpp"

#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/UI/UISystem.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"


Game::~Game()
{
	delete m_player;
	m_player = nullptr;
}

Game::Game()
{
	LoadAssets();
	InitializeUI();

	g_audio->SetNumListeners(1);
	m_player = new Player(this, Vec3(0.f, 0.f, 1.f), EulerAngles::ZERO);
	PlayerPawn* playerPawn = new PlayerPawn(m_player, Vec3(0.f, 0.f, 1.f), EulerAngles::ZERO);
	m_player->m_pawn = playerPawn;

	// Create Transition sphere VBO
	std::vector<Vertex_PCU> transitionSphereVerts;
	AddVertsForSphere3D(transitionSphereVerts, Vec3::ZERO, 10.f, Rgba8::WHITE, AABB2::ZERO_TO_ONE, 16, 32);
	m_transitionSphereVBO = g_renderer->CreateVertexBuffer(transitionSphereVerts.size() * sizeof(Vertex_PCU));
	g_renderer->CopyCPUToGPU(transitionSphereVerts.data(), transitionSphereVerts.size() * sizeof(Vertex_PCU), m_transitionSphereVBO);

	SubscribeEventCallbackFunction("Navigate", Event_Navigate, "Navigation between screens");
	SubscribeEventCallbackFunction("SetHowToPlayTab", Event_SetHowToPlayTab, "Switch between how-to-play tabs");
	SubscribeEventCallbackFunction("ToggleInstructions", Event_ToggleShowInstructions, "Toggle Instructions");
	SubscribeEventCallbackFunction("TogglePause", Event_TogglePause, "Toggle pause");
	SubscribeEventCallbackFunction("LeftControllerUndo", Event_UndoLeftControllerAction, "Undo left controller action");
	SubscribeEventCallbackFunction("LeftControllerRedo", Event_RedoLeftControllerAction, "Redo left controller action");
	SubscribeEventCallbackFunction("RightControllerUndo", Event_UndoRightControllerAction, "Undo right controller action");
	SubscribeEventCallbackFunction("RightControllerRedo", Event_RedoRightControllerAction, "Redo right controller action");
	SubscribeEventCallbackFunction("PlayMap", Event_PlayMap, "Play a saved map");
	SubscribeEventCallbackFunction("EditMap", Event_EditMap, "Edit a saved map");
	SubscribeEventCallbackFunction("ToggleMapImage", Event_ToggleInGameMapImage, "Toggles the in-game map image");
	SubscribeEventCallbackFunction("ConnectToPerforce", Event_ConnectToPerforce, "Connect to perforce");
	SubscribeEventCallbackFunction("StartTutorial", Event_StartTutorial, "Starts the tutorial");
}

void Game::Update()
{
	float deltaSeconds = m_clock.GetDeltaSeconds();
	m_timeInState += deltaSeconds;
	m_player->Update();

	Mat44 billboardTargetMatrix = Mat44::CreateTranslation3D(m_player->m_position);
	billboardTargetMatrix.Append((m_player->m_orientation + m_player->m_hmdOrientation).GetAsMatrix_iFwd_jLeft_kUp());

	m_screenBillboardMatrix = GetBillboardMatrix(BillboardType::FULL_FACING, billboardTargetMatrix, m_player->GetPlayerPosition() + (m_player->m_orientation + m_player->m_hmdOrientation).GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D() * SCREEN_QUAD_DISTANCE);

	switch (m_state)
	{
		case GameState::ATTRACT:			UpdateAttract();			break;
		case GameState::MENU:				UpdateMenu();				break;
		case GameState::HOW_TO_PLAY:		UpdateHowToPlay();			break;
		case GameState::CREDITS:			UpdateCredits();			break;
		case GameState::MAP_SELECT:			UpdateMapSelect();			break;
		case GameState::GAME:				UpdateGame();				break;
		case GameState::PAUSE:				UpdatePause();				break;
		case GameState::LEVEL_COMPLETE:		UpdateLevelComplete();		break;
	}

	HandleStateChange();
}

void Game::FixedUpdate(float deltaSeconds)
{
	m_player->FixedUpdate(deltaSeconds);
}

void Game::ClearScreen()
{
	Rgba8 clearColor = Rgba8::GRAY;
	g_renderer->ClearScreen(clearColor);
}

void Game::Render() const
{
	RenderSkybox();
	switch (m_state)
	{
		case GameState::ATTRACT:			RenderAttract();			break;
		case GameState::MENU:				RenderMenu();				break;
		case GameState::HOW_TO_PLAY:		RenderHowToPlay();			break;
		case GameState::CREDITS:			RenderCredits();			break;
		case GameState::MAP_SELECT:			RenderMapSelect();			break;
		case GameState::GAME:				RenderGame();				break;
		case GameState::PAUSE:				RenderPause();				break;
		case GameState::LEVEL_COMPLETE:		RenderLevelComplete();		break;
	}

	RenderWorldScreenQuad();
	m_player->Render();

	RenderIntroTransition();
	RenderOutroTransition();
}

void Game::RenderScreen() const
{
	switch (m_state)
	{
		case GameState::ATTRACT:			RenderScreenAttract();			break;
		case GameState::MENU:				RenderScreenMenu();				break;
		case GameState::HOW_TO_PLAY:		RenderScreenHowToPlay();		break;
		case GameState::CREDITS:			RenderScreenCredits();			break;
		case GameState::MAP_SELECT:			RenderScreenMapSelect();		break;
		case GameState::GAME:				RenderScreenGame();				break;
		case GameState::PAUSE:				RenderScreenPause();			break;
		case GameState::LEVEL_COMPLETE:		RenderScreenLevelComplete();	break;
	}
}

void Game::RenderCustomScreens() const
{
	if (m_currentMap)
	{
		m_currentMap->RenderCustomScreens();
	}
}

ArchiLeapRaycastResult3D Game::RaycastVsScreen(Vec3 const& startPosition, Vec3 const& fwdNormal, float maxDistance) const
{
	ArchiLeapRaycastResult3D result;
	result.m_rayStartPosition = startPosition;
	result.m_rayForwardNormal = fwdNormal;
	result.m_rayMaxLength = maxDistance;

	float quadHeight = SCREEN_QUAD_DISTANCE / TanDegrees(60.f) * 0.5f;
	float quadWidth = quadHeight * g_window->GetAspect();

	Vec3 screenRight = m_screenBillboardMatrix.GetJBasis3D().GetNormalized() * (quadWidth);
	Vec3 screenUp = m_screenBillboardMatrix.GetKBasis3D().GetNormalized() * (quadHeight);
	Vec3 screenCenter = m_screenBillboardMatrix.GetTranslation3D();

	Vec3 topLeft = screenCenter - screenRight + screenUp;
	Vec3 topRight = screenCenter + screenRight + screenUp;
	Vec3 bottomLeft = screenCenter - screenRight - screenUp;
	Vec3 bottomRight = screenCenter + screenRight - screenUp;

	Plane3 screenPlane(m_screenBillboardMatrix.GetIBasis3D().GetNormalized(), GetProjectedLength3D(screenCenter, m_screenBillboardMatrix.GetIBasis3D().GetNormalized()));

	RaycastResult3D raycastVsPlaneResult = RaycastVsPlane3(startPosition, fwdNormal, maxDistance, screenPlane);
	if (!raycastVsPlaneResult.m_didImpact)
	{
		return result;
	}

	Vec3 rightVector = bottomRight - bottomLeft;
	Vec3 upVector = topLeft - bottomLeft;
	Vec3 intersectionToBottomLeft = raycastVsPlaneResult.m_impactPosition - bottomLeft;

	float rightDot = DotProduct3D(intersectionToBottomLeft, rightVector);
	float upDot = DotProduct3D(intersectionToBottomLeft, upVector);
	float rightLenSq = DotProduct3D(rightVector, rightVector);
	float upLenSq = DotProduct3D(upVector, upVector);

	Vec2 impactScreenCoords;
	if (rightDot >= 0 && rightDot <= rightLenSq && upDot >= 0 && upDot <= upLenSq)
	{
		impactScreenCoords.x = rightDot / rightLenSq;
		impactScreenCoords.y = upDot / upLenSq;

		result.m_didImpact = true;
		result.m_impactPosition = raycastVsPlaneResult.m_impactPosition;
		result.m_impactDistance = raycastVsPlaneResult.m_impactDistance;
		result.m_impactNormal = raycastVsPlaneResult.m_impactNormal;
		result.m_screenImpactCoords = impactScreenCoords;
		result.m_impactWidget = g_ui->GetWidgetAtNormalizedCoords(impactScreenCoords);
		return result;
	}

	return result;
}

bool Game::Event_Navigate(EventArgs& args)
{
	if (!g_app->m_game)
	{
		return false;
	}

	GameState nextState = (GameState)args.GetValue("target", (int)GameState::NONE);
	PlayerState playerState = (PlayerState)args.GetValue("playerState", (int)PlayerState::NONE);

	if (playerState != PlayerState::NONE)
	{
		PlayerState prevPlayerState = g_app->m_game->m_player->m_state;
		g_app->m_game->m_player->ChangeState(prevPlayerState, playerState);
	}

	g_app->m_game->m_nextState = nextState;
	return true;
}

bool Game::Event_SetHowToPlayTab(EventArgs& args)
{
	int tabIndex = args.GetValue("tab", -1);
	if (tabIndex == -1)
	{
		return false;
	}

	g_app->m_game->m_controlsTabIndex = tabIndex;
	return true;
}

void Game::LoadAssets()
{
	TileDefinition::CreateFromXml();
	g_squirrelFont = g_renderer->CreateBitmapFromFile("Data/Images/SquirrelFixedFont");
	m_gameLogoTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/ArchiLeap_Temp_Logo.png");
	m_logoTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Logo.png");
	m_logoSpriteSheet = new SpriteSheet(m_logoTexture, IntVec2(15, 19));

	m_mapImageTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/LevelImage.jpg");

	m_skyboxTextures[0] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap+X.png");
	m_skyboxTextures[1] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap-X.png");
	m_skyboxTextures[2] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap+Y.png");
	m_skyboxTextures[3] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap-Y.png");
	m_skyboxTextures[4] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap+Z.png");
	m_skyboxTextures[5] = g_renderer->CreateOrGetTextureFromFile("Data/Images/Cubemap-Z.png");
}

void Game::InitializeUI()
{
	InitializeAttractUI();
	InitializeMenuUI();
	InitializeMapSelectUI();
	InitializeHowToPlayUI();
	InitializeCreditsUI();
	InitializePerforceUI();
	InitializeGameUI();
	InitializePauseUI();
	InitializeLevelImageUI();
	InitializeLevelCompleteUI();

	m_attractWidget->SetFocus(false);
	m_menuWidget->SetFocus(false);
	m_mapSelectWidget->SetFocus(false);
	m_controlsWidget->SetFocus(false);
	m_creditsWidget->SetFocus(false);
	m_perforceWidget->SetFocus(false);
	m_gameWidget->SetFocus(false);
	m_pauseWidget->SetFocus(false);
	m_levelImageWidget->SetFocus(false);
	m_levelCompleteWidget->SetFocus(false);
}

void Game::InitializeAttractUI()
{
	m_attractWidget = g_ui->CreateWidget();
	m_attractWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* attractTitleWidget = g_ui->CreateWidget(m_attractWidget);
	attractTitleWidget->SetText("ArchiLeapVR")
		->SetPosition(Vec2(0.5f, 0.85f))
		->SetDimensions(Vec2(0.5f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(2.f)
		->SetRaycastTarget(false);

	UIWidget* infoWidget = g_ui->CreateWidget(m_attractWidget);
	infoWidget->SetText("Press any VR Button or Space to Continue...")
		->SetPosition(Vec2(0.5f, 0.15f))
		->SetDimensions(Vec2(0.75f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetBorderRadius(2.f)
		->SetRaycastTarget(false);

}

void Game::InitializeMenuUI()
{
	m_menuWidget = g_ui->CreateWidget();
	m_menuWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* menuTitleWidget = g_ui->CreateWidget(m_menuWidget);
	menuTitleWidget->SetText("ArchiLeapVR")
		->SetPosition(Vec2(0.5f, 0.85f))
		->SetDimensions(Vec2(0.5f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(2.f)
		->SetRaycastTarget(false);

	UIWidget* newMapButtonWidget = g_ui->CreateWidget(m_menuWidget);
	newMapButtonWidget->SetText("New Map")
		->SetPosition(Vec2(0.05f, 0.6f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(4.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::GAME));

	UIWidget* savedMapsButtonWidget = g_ui->CreateWidget(m_menuWidget);
	savedMapsButtonWidget->SetText("Saved Maps")
		->SetPosition(Vec2(0.05f, 0.525f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(4.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MAP_SELECT));

	UIWidget* tutorialButtonWidget = g_ui->CreateWidget(m_menuWidget);
	tutorialButtonWidget->SetText("Tutorial")
		->SetPosition(Vec2(0.05f, 0.45f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(4.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("StartTutorial"));

	UIWidget* controlsButtonWidget = g_ui->CreateWidget(m_menuWidget);
	controlsButtonWidget->SetText("Controls")
		->SetPosition(Vec2(0.05f, 0.375f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::HOW_TO_PLAY));

	UIWidget* creditsButtonWidget = g_ui->CreateWidget(m_menuWidget);
	creditsButtonWidget->SetText("Credits")
		->SetPosition(Vec2(0.05f, 0.3f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::CREDITS));

	UIWidget* perforceButtonWidget = g_ui->CreateWidget(m_menuWidget);
	perforceButtonWidget->SetText("Perforce")
		->SetPosition(Vec2(0.05f, 0.225f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::PERFORCE));

	UIWidget* exitButtonWidget = g_ui->CreateWidget(m_menuWidget);
	exitButtonWidget->SetText("Exit")
		->SetPosition(Vec2(0.05f, 0.15f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Quit"));
}

void Game::InitializeMapSelectUI()
{
	m_mapSelectWidget = g_ui->CreateWidget();
	m_mapSelectWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* backButtonWidget = g_ui->CreateWidget(m_mapSelectWidget);
	backButtonWidget->SetImage("Data/Images/Home.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));

	UIWidget* mapSelectTitleWidget = g_ui->CreateWidget(m_mapSelectWidget);
	mapSelectTitleWidget->SetText("Select a Map")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false);

	m_noSavedMapsWidget = g_ui->CreateWidget(m_mapSelectWidget);
	m_noSavedMapsWidget->SetText("No Saved Maps!")
		->SetPosition(Vec2(0.5f, 0.5f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetFontSize(8.f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false)
		->SetVisible(false);

	m_createMapWidget = g_ui->CreateWidget(m_mapSelectWidget);
	m_createMapWidget->SetText("Create a Map")
		->SetPosition(Vec2(0.5f, 0.425f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false)
		->SetVisible(false)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::GAME));

	m_connectToPerforceMessageWidget = g_ui->CreateWidget(m_mapSelectWidget);
	m_connectToPerforceMessageWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.875f))
		->SetDimensions(Vec2(0.9f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false)
		->SetVisible(false)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::PERFORCE));
}

void Game::InitializeHowToPlayUI()
{
	m_controlsWidget = g_ui->CreateWidget();
	m_controlsWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* backButtonWidget = g_ui->CreateWidget(m_controlsWidget);
	backButtonWidget->SetImage("Data/Images/Home.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));

	UIWidget* controlsTitleWidget = g_ui->CreateWidget(m_controlsWidget);
	controlsTitleWidget->SetText("Controls")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false);

	UIWidget* controlsTabsContainer = g_ui->CreateWidget(m_controlsWidget);
	controlsTabsContainer->SetPosition(Vec2(0.05f, 0.85f))
		->SetDimensions(Vec2(0.9f, 0.05f))
		->SetRaycastTarget(false);

	//---------------------------------------------------------------------------------------
	m_controlsWidgetTabButtons[0] = g_ui->CreateWidget(controlsTabsContainer);
	m_controlsWidgetTabButtons[0]->SetText("Basic")
		->SetPosition(Vec2(0.125f, 0.5f))
		->SetDimensions(Vec2(0.2f, 1.f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(3.f)
		->SetBorderRadius(0.5f)
		->SetBorderWidth(0.1f)
		->SetClickEventName(Stringf("SetHowToPlayTab tab=%d", 0));

	m_controlsWidgetTabContainers[0] = g_ui->CreateWidget(m_controlsWidget);
	m_controlsWidgetTabContainers[0]->SetPosition(Vec2(0.05f, 0.f))
		->SetDimensions(Vec2(0.9f, 0.8f))
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* controlsBasicControllerLayoutTitleWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	controlsBasicControllerLayoutTitleWidget->SetText("Controller Layout")
		->SetPosition(Vec2(0.5f, 0.9f))
		->SetDimensions(Vec2(1.f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetFontSize(8.f);

	UIWidget* moveTextWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	moveTextWidget->SetPosition(Vec2(0.1f, 0.8f))
		->SetDimensions(Vec2(0.1f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(1.f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetText("Move")
		->SetFontSize(4.f);

	UIWidget* leftJoystickWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	leftJoystickWidget->SetImage("Data/Images/InputPrompts/Controllers/generic_stick.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.2f, 0.8f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* leftMenuButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	leftMenuButtonWidget->SetImage("Data/Images/InputPrompts/Controllers/xbox_button_menu_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.15f, 0.65f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* leftXButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	leftXButtonWidget->SetImage("Data/Images/InputPrompts/Controllers/xbox_button_x_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.29f, 0.69f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* leftYButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	leftYButtonWidget->SetImage("Data/Images/InputPrompts/Controllers/xbox_button_y_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.35f, 0.8f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));


	//---------------------------------------------------------------------------------------

	UIWidget* lookAroundTextWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	lookAroundTextWidget->SetPosition(Vec2(0.6f, 0.8f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(1.f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetText("Look Around (Yaw)")
		->SetFontSize(4.f);

	UIWidget* rightJoystickWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	rightJoystickWidget->SetImage("Data/Images/InputPrompts/Controllers/generic_stick.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.8f, 0.8f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* rightAButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	rightAButtonWidget->SetImage("Data/Images/InputPrompts/Controllers/xbox_button_a_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.89f, 0.69f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* rightBButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	rightBButtonWidget->SetImage("Data/Images/InputPrompts/Controllers/xbox_button_b_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.95f, 0.8f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	//---------------------------------------------------------------------------------------

	UIWidget* controlsBasicKeyboardMouseTitleWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	controlsBasicKeyboardMouseTitleWidget->SetText("Keyboard + Mouse")
		->SetPosition(Vec2(0.5f, 0.45f))
		->SetDimensions(Vec2(1.f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetFontSize(8.f);

	UIWidget* keyboardWButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	keyboardWButtonWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/keyboard_w_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.2f, 0.3f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* keyboardAButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	keyboardAButtonWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/keyboard_a_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.1f, 0.2f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* keyboardSButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	keyboardSButtonWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/keyboard_s_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.2f, 0.2f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* keyboardDButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	keyboardDButtonWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/keyboard_d_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.3f, 0.2f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* kbmMoveTextWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	kbmMoveTextWidget->SetPosition(Vec2(0.2f, 0.1f))
		->SetDimensions(Vec2(0.2f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetText("Move")
		->SetFontSize(4.f);

	UIWidget* kbmMouseMoveWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	kbmMouseMoveWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/mouse_move.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.525f, 0.3f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* kbmLookAroundTextWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	kbmLookAroundTextWidget->SetPosition(Vec2(0.8f, 0.3f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetText("Look Around (Yaw + Pitch)")
		->SetFontSize(4.f);

	UIWidget* kbmShiftButtonWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	kbmShiftButtonWidget->SetImage("Data/Images/InputPrompts/KeyboardMouse/keyboard_shift_outline.png")
		->SetColor(Rgba8::WHITE)
		->SetPosition(Vec2(0.525f, 0.15f))
		->SetDimensions(Vec2(0.1f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f));

	UIWidget* kbmSprintTextWidget = g_ui->CreateWidget(m_controlsWidgetTabContainers[0]);
	kbmSprintTextWidget->SetPosition(Vec2(0.8f, 0.15f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetText("Sprint")
		->SetFontSize(4.f);

	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------

	m_controlsWidgetTabButtons[1] = g_ui->CreateWidget(controlsTabsContainer);
	m_controlsWidgetTabButtons[1]->SetText("Gameplay")
		->SetPosition(Vec2(0.375f, 0.5f))
		->SetDimensions(Vec2(0.2f, 1.f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(3.f)
		->SetBorderRadius(0.5f)
		->SetBorderWidth(0.1f)
		->SetClickEventName(Stringf("SetHowToPlayTab tab=%d", 1));

	m_controlsWidgetTabContainers[1] = g_ui->CreateWidget(m_controlsWidget);
	m_controlsWidgetTabContainers[1]->SetPosition(Vec2(0.05f, 0.f))
		->SetDimensions(Vec2(0.9f, 0.8f))
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false)
		->SetScrollable(true)
		->SetScrollBuffer(200.f);

	//UIWidget* controlsWidgetGameplayText = g_ui->CreateWidget(m_controlsWidgetTabContainers[1]);
	//controlsWidgetGameplayText->SetText(howToPlayGameplayText)
	//	->SetPosition(Vec2(0.5f, 1.f))
	//	->SetDimensions(Vec2(1.f, 2.f))
	//	->SetPivot(Vec2(0.5f, 1.f))
	//	->SetAlignment(Vec2(0.5f, 1.f))
	//	->SetColor(Rgba8::WHITE)
	//	->SetFontSize(4.f);

	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------

	m_controlsWidgetTabButtons[2] = g_ui->CreateWidget(controlsTabsContainer);
	m_controlsWidgetTabButtons[2]->SetText("Editor Create")
		->SetPosition(Vec2(0.625f, 0.5f))
		->SetDimensions(Vec2(0.2f, 1.f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(3.f)
		->SetBorderRadius(0.5f)
		->SetBorderWidth(0.1f)
		->SetClickEventName(Stringf("SetHowToPlayTab tab=%d", 2));

	m_controlsWidgetTabContainers[2] = g_ui->CreateWidget(m_controlsWidget);
	m_controlsWidgetTabContainers[2]->SetPosition(Vec2(0.05f, 0.f))
		->SetDimensions(Vec2(0.9f, 0.8f))
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------


	m_controlsWidgetTabButtons[3] = g_ui->CreateWidget(controlsTabsContainer);
	m_controlsWidgetTabButtons[3]->SetText("Editor Edit")
		->SetPosition(Vec2(0.875f, 0.5f))
		->SetDimensions(Vec2(0.2f, 1.f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(3.f)
		->SetBorderRadius(0.5f)
		->SetBorderWidth(0.1f)
		->SetClickEventName(Stringf("SetHowToPlayTab tab=%d", 3));

	m_controlsWidgetTabContainers[3] = g_ui->CreateWidget(m_controlsWidget);
	m_controlsWidgetTabContainers[3]->SetPosition(Vec2(0.05f, 0.f))
		->SetDimensions(Vec2(0.9f, 0.8f))
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	controlsTabsContainer->SetVisible(false);
}

void Game::InitializeCreditsUI()
{
	m_creditsWidget = g_ui->CreateWidget();
	m_creditsWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* backButtonWidget = g_ui->CreateWidget(m_creditsWidget);
	backButtonWidget->SetImage("Data/Images/Home.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));

	UIWidget* controlsTitleWidget = g_ui->CreateWidget(m_creditsWidget);
	controlsTitleWidget->SetText("Credits")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false);

	std::string creditsText;
	FileReadToString(creditsText, "Data/Credits.txt");
	UIWidget* creditsWidget = g_ui->CreateWidget(m_creditsWidget);
	creditsWidget->SetText(Stringf("%s", creditsText.c_str()))
		->SetPosition(Vec2(0.5f, 0.5f))
		->SetDimensions(Vec2(0.6f, 0.75f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetFontSize(4.f);

	std::string infoText;
	FileReadToString(infoText, "Data/Info.txt");
	UIWidget* infoWidget = g_ui->CreateWidget(m_creditsWidget);
	infoWidget->SetText(Stringf("%s", infoText.c_str()))
		->SetPosition(Vec2(0.5f, 0.15f))
		->SetDimensions(Vec2(0.9f, 0.2f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetFontSize(8.f);
}

void Game::InitializePerforceUI()
{
	m_perforceWidget = g_ui->CreateWidget();
	m_perforceWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* backButtonWidget = g_ui->CreateWidget(m_perforceWidget);
	backButtonWidget->SetImage("Data/Images/Home.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));

	UIWidget* perforceTitleWidget = g_ui->CreateWidget(m_perforceWidget);
	perforceTitleWidget->SetText("Perforce")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false);

	//---------------------------------------------------------------------------------------
	UIWidget* userTextWidget = g_ui->CreateWidget(m_perforceWidget);
	userTextWidget->SetText("User: ")
		->SetPosition(Vec2(0.05f, 0.6f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetHoverColor(Rgba8::WHITE)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false)
		->SetClickEventName("");

	m_perforceUserTextInputFieldWidget = g_ui->CreateWidget(m_perforceWidget);
	m_perforceUserTextInputFieldWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.6f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.5f)
		->SetTextInputField(true)
		->SetRaycastTarget(true)
		->SetTextInputFieldInfoText("Enter P4 Username...");

	UIWidget* serverTextWidget = g_ui->CreateWidget(m_perforceWidget);
	serverTextWidget->SetText("Server: ")
		->SetPosition(Vec2(0.05f, 0.525f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetHoverColor(Rgba8::WHITE)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false)
		->SetClickEventName("");

	m_perforceServerTextInputFieldWidget = g_ui->CreateWidget(m_perforceWidget);
	m_perforceServerTextInputFieldWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.525f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.5f)
		->SetTextInputField(true)
		->SetRaycastTarget(true)
		->SetTextInputFieldInfoText("Enter P4 Server...");

	UIWidget* workspaceTextWidget = g_ui->CreateWidget(m_perforceWidget);
	workspaceTextWidget->SetText("Workspace: ")
		->SetPosition(Vec2(0.05f, 0.45f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(Rgba8::WHITE)
		->SetHoverColor(Rgba8::WHITE)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false)
		->SetClickEventName("");

	m_perforceWorkspaceTextInputFieldWidget = g_ui->CreateWidget(m_perforceWidget);
	m_perforceWorkspaceTextInputFieldWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.45f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.5f)
		->SetTextInputField(true)
		->SetRaycastTarget(true)
		->SetTextInputFieldInfoText("Enter P4 Workspace...");

	//---------------------------------------------------------------------------------------
	m_perforceErrorMessageTextWidget = g_ui->CreateWidget(m_perforceWidget);
	m_perforceErrorMessageTextWidget->SetText("ERR")
		->SetPosition(Vec2(0.5f, 0.375f))
		->SetDimensions(Vec2(0.9f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8::RED)
		->SetHoverColor(Rgba8::RED)
		->SetFontSize(8.f)
		->SetFocus(false)
		->SetVisible(false);

	m_perforceStatusTextWidget = g_ui->CreateWidget(m_perforceWidget);
	m_perforceStatusTextWidget->SetText("Status: Not connected")
		->SetPosition(Vec2(0.5f, 0.3f))
		->SetDimensions(Vec2(0.9f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8::YELLOW)
		->SetHoverColor(Rgba8::YELLOW)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetFocus(false);

	UIWidget* connectButtonWidget = g_ui->CreateWidget(m_perforceWidget);
	connectButtonWidget->SetText("Connect")
		->SetPosition(Vec2(0.5f, 0.2f))
		->SetDimensions(Vec2(0.9f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("ConnectToPerforce"));
}

void Game::InitializeGameUI()
{
	m_gameWidget = g_ui->CreateWidget();
	m_gameWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	m_gamePlayerStateWidget = g_ui->CreateWidget(m_gameWidget);
	m_gamePlayerStateWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(1.f);

	m_instructionsWidget = g_ui->CreateWidget(m_gameWidget);
	m_instructionsWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.875f))
		->SetDimensions(Vec2(0.8f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetBorderRadius(1.f);

	m_tutorialTextWidget = g_ui->CreateWidget(m_gameWidget);
	m_tutorialTextWidget->SetText("")
		->SetPosition(Vec2(0.5f, 0.75f))
		->SetDimensions(Vec2(0.8f, 0.15f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(Rgba8(PRIMARY_COLOR.r, PRIMARY_COLOR.g, PRIMARY_COLOR.b, 195))
		->SetHoverColor(Rgba8(PRIMARY_COLOR.r, PRIMARY_COLOR.g, PRIMARY_COLOR.b, 195))
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetBorderRadius(1.f)
		->SetRaycastTarget(false)
		->SetVisible(false)
		->SetFocus(false);

	m_toggleMapImageButton = g_ui->CreateWidget(m_gameWidget);
	m_toggleMapImageButton->SetImage("Data/Images/Image.png")
		->SetPosition(Vec2(0.875f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.4f)
		->SetClickEventName("ToggleMapImage");

	UIWidget* pauseButton = g_ui->CreateWidget(m_gameWidget);
	pauseButton->SetImage("Data/Images/Pause.png")
		->SetPosition(Vec2(0.95f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.4f)
		->SetClickEventName("TogglePause");

	m_saveButtonWidget = g_ui->CreateWidget(m_gameWidget);
	m_saveButtonWidget->SetImage("Data/Images/Save.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetClickEventName(Stringf("SaveMap"));

	m_coinsCollectedWidget = g_ui->CreateWidget(m_gameWidget);
	m_coinsCollectedWidget->SetPosition(Vec2(0.1f, 0.95f))
		->SetDimensions(Vec2(0.1f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetRaycastTarget(false);

	UIWidget* coinsCollectedImageWidget = g_ui->CreateWidget(m_coinsCollectedWidget);
	coinsCollectedImageWidget
		->SetImage("Data/Images/Entities/coinGold.png")
		->SetPosition(Vec2(-0.5f, -0.5f))
		->SetDimensions(Vec2(0.5f, 1.f))
		->SetPivot(Vec2(0.f, 0.f))
		->SetAlignment(Vec2(0.f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetHoverColor(Rgba8::WHITE)
		->SetRaycastTarget(false);

	m_coinsCollectedTextWidget = g_ui->CreateWidget(m_coinsCollectedWidget);
	m_coinsCollectedTextWidget->SetText("0")
		->SetPosition(Vec2(0.f, -0.5f))
		->SetDimensions(Vec2(0.5f, 1.f))
		->SetPivot(Vec2(0.f, 0.f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	m_coinsCollectedWidget->SetVisible(false);
	m_coinsCollectedWidget->SetFocus(false);

	// Controller Undo/Redo buttons
	if (g_openXR && g_openXR->IsInitialized())
	{
		m_leftUndoButton = g_ui->CreateWidget(m_gameWidget);
		m_leftUndoButton->SetImage("Data/Images/Undo.png")
			->SetPosition(Vec2(0.05f, 0.05f))
			->SetDimensions(Vec2(0.05f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.4f)
			->SetClickEventName("LeftControllerUndo");

		m_leftRedoButton = g_ui->CreateWidget(m_gameWidget);
		m_leftRedoButton->SetImage("Data/Images/Redo.png")
			->SetPosition(Vec2(0.105f, 0.05f))
			->SetDimensions(Vec2(0.05f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.4f)
			->SetClickEventName("LeftControllerRedo");

		m_rightUndoButton = g_ui->CreateWidget(m_gameWidget);
		m_rightUndoButton->SetImage("Data/Images/Undo.png")
			->SetPosition(Vec2(0.895f, 0.05f))
			->SetDimensions(Vec2(0.05f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.4f)
			->SetClickEventName("RightControllerUndo");

		m_rightRedoButton = g_ui->CreateWidget(m_gameWidget);
		m_rightRedoButton->SetImage("Data/Images/Redo.png")
			->SetPosition(Vec2(0.95f, 0.05f))
			->SetDimensions(Vec2(0.05f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.4f)
			->SetClickEventName("RightControllerRedo");
	}
}

void Game::InitializePauseUI()
{
	m_pauseWidget = g_ui->CreateWidget();
	m_pauseWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* pauseTitleWidget = g_ui->CreateWidget(m_pauseWidget);
	pauseTitleWidget->SetText("Paused")
		->SetPosition(Vec2(0.5f, 0.85f))
		->SetDimensions(Vec2(0.4f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(2.f)
		->SetRaycastTarget(false);

	UIWidget* resumeButtonWidget = g_ui->CreateWidget(m_pauseWidget);
	resumeButtonWidget->SetText("Resume")
		->SetPosition(Vec2(0.05f, 0.6f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::GAME));

	m_pausePlayerStateWidget = g_ui->CreateWidget(m_pauseWidget);
	m_pausePlayerStateWidget->SetText("")
		->SetPosition(Vec2(0.05f, 0.525f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f);

	m_togglePlayPositionWidget = g_ui->CreateWidget(m_pauseWidget);
	m_togglePlayPositionWidget->SetText("")
		->SetPosition(Vec2(0.05f, 0.45f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName("TogglePlayStartLocation");

	m_toggleInstructionsWidget = g_ui->CreateWidget(m_pauseWidget);
	m_toggleInstructionsWidget->SetText("")
		->SetPosition(Vec2(0.05f, 0.375f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName("ToggleInstructions");

	m_toggleLinkLinesWidget = g_ui->CreateWidget(m_pauseWidget);
	m_toggleLinkLinesWidget->SetText("")
		->SetPosition(Vec2(0.05f, 0.3f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName("ToggleLinkLines");

	m_pauseSaveMapButton = g_ui->CreateWidget(m_pauseWidget);
	m_pauseSaveMapButton->SetText("Save Map -- Map Name:")
		->SetPosition(Vec2(0.05f, 0.225f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName("SaveMap");

	Strings unusedMapNames;
	int mapIndex = ListAllFilesInDirectory("Saved", unusedMapNames) - 1;
	m_mapNameInputField = g_ui->CreateWidget(m_pauseWidget);
	m_mapNameInputField->SetText(Stringf("Map%d", mapIndex))
		->SetPosition(Vec2(0.5f, 0.225f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(PRIMARY_COLOR)
		->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(SECONDARY_COLOR)
		->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.5f)
		->SetTextInputField(true)
		->SetRaycastTarget(true)
		->SetTextInputFieldInfoText("Enter Map Name...");

	UIWidget* levelImageButtonWidget = g_ui->CreateWidget(m_pauseWidget);
	levelImageButtonWidget->SetText("Show Level Design")
		->SetPosition(Vec2(0.05f, 0.15f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::LEVEL_IMAGE));

	UIWidget* returnToMenuButtonWidget = g_ui->CreateWidget(m_pauseWidget);
	returnToMenuButtonWidget->SetText("Menu")
		->SetPosition(Vec2(0.05f, 0.075f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));
}

void Game::InitializeLevelImageUI()
{
	m_levelImageWidget = g_ui->CreateWidget();
	m_levelImageWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* backButtonWidget = g_ui->CreateWidget(m_levelImageWidget);
	backButtonWidget->SetImage("Data/Images/arrowLeft.png")
		->SetPosition(Vec2(0.05f, 0.95f))
		->SetDimensions(Vec2(0.05f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::PAUSE));

	UIWidget* levelImageTitleWidget = g_ui->CreateWidget(m_levelImageWidget);
	levelImageTitleWidget->SetText("Level Design")
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetRaycastTarget(false);

	UIWidget* levelImageWidget = g_ui->CreateWidget(m_levelImageWidget);
	levelImageWidget->SetImage("Data/Images/LevelImage.jpg")
		->SetPosition(Vec2(0.075f, 0.025f))
		->SetDimensions(Vec2(0.85f, 0.85f))
		->SetColor(Rgba8::WHITE)
		->SetRaycastTarget(false);
}

void Game::InitializeLevelCompleteUI()
{
	m_levelCompleteWidget = g_ui->CreateWidget();
	m_levelCompleteWidget->SetPosition(Vec2::ZERO)
		->SetDimensions(Vec2::ONE)
		->SetVisible(false)
		->SetBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetHoverBackgroundColor(Rgba8::TRANSPARENT_BLACK)
		->SetRaycastTarget(false);

	UIWidget* levelCompleteTitleWidget = g_ui->CreateWidget(m_levelCompleteWidget);
	levelCompleteTitleWidget->SetText("Level Complete!")
		->SetPosition(Vec2(0.5f, 0.85f))
		->SetDimensions(Vec2(0.4f, 0.1f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(2.f)
		->SetRaycastTarget(false);

	m_levelCompleteCoinsCollectedWidget = g_ui->CreateWidget(m_levelCompleteWidget);
	m_levelCompleteCoinsCollectedWidget->SetPosition(Vec2(0.5f, 0.525f))
		->SetDimensions(Vec2(0.1f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetBorderRadius(0.4f)
		->SetRaycastTarget(false);

	UIWidget* levelCompleteCoinsCollectedImageWidget = g_ui->CreateWidget(m_levelCompleteCoinsCollectedWidget);
	levelCompleteCoinsCollectedImageWidget
		->SetImage("Data/Images/Entities/coinGold.png")
		->SetPosition(Vec2(-0.5f, -0.5f))
		->SetDimensions(Vec2(0.5f, 1.f))
		->SetPivot(Vec2(0.f, 0.f))
		->SetAlignment(Vec2(0.f, 0.f))
		->SetColor(Rgba8::WHITE)
		->SetHoverColor(Rgba8::WHITE)
		->SetRaycastTarget(false);

	m_levelCompleteCoinsCollectedTextWidget = g_ui->CreateWidget(m_levelCompleteCoinsCollectedWidget);
	m_levelCompleteCoinsCollectedTextWidget->SetText("0")
		->SetPosition(Vec2(0.f, -0.5f))
		->SetDimensions(Vec2(0.5f, 1.f))
		->SetPivot(Vec2(0.f, 0.f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	UIWidget* playAgainButton = g_ui->CreateWidget(m_levelCompleteWidget);
	playAgainButton->SetText("Play Again")
		->SetPosition(Vec2(0.5f, 0.45f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d playerState=%d", (int)GameState::GAME, (int)PlayerState::PLAY));

	m_levelCompleteContinueEditingButton = g_ui->CreateWidget(m_levelCompleteWidget);
	m_levelCompleteContinueEditingButton->SetText("Continue Editing")
		->SetPosition(Vec2(0.5f, 0.375f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d playerState=%d", (int)GameState::GAME, (int)PlayerState::EDITOR_CREATE));

	UIWidget* returnToMenuButtonWidget = g_ui->CreateWidget(m_levelCompleteWidget);
	returnToMenuButtonWidget->SetText("Menu")
		->SetPosition(Vec2(0.5f, 0.3f))
		->SetDimensions(Vec2(0.4f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(8.f)
		->SetBorderRadius(0.5f)
		->SetClickEventName(Stringf("Navigate target=%d", (int)GameState::MENU));
}

void Game::InitializeGrid()
{
	std::vector<Vertex_PCU> gridVerts;

	for (int y = -50; y <= 50; y++)
	{
		Rgba8 lineColor = Interpolate(Rgba8::WHITE, Rgba8::TRANSPARENT_WHITE, fabsf((float)y) / 50.f);

		gridVerts.push_back(Vertex_PCU(Vec3::EAST * 50.f + Vec3::NORTH * (float)y, lineColor, Vec2::ZERO));
		gridVerts.push_back(Vertex_PCU(Vec3::WEST * 50.f + Vec3::NORTH * (float)y, lineColor, Vec2::ZERO));
	}
	for (int x = -50; x <= 50; x++)
	{
		Rgba8 lineColor = Interpolate(Rgba8::WHITE, Rgba8::TRANSPARENT_WHITE, fabsf((float)x) / 50.f);

		gridVerts.push_back(Vertex_PCU(Vec3::EAST * (float)x + Vec3::NORTH * 50.f, lineColor, Vec2::ZERO));
		gridVerts.push_back(Vertex_PCU(Vec3::EAST * (float)x + Vec3::SOUTH * 50.f, lineColor, Vec2::ZERO));
	}

	m_gridVBO = g_renderer->CreateVertexBuffer(gridVerts.size() * sizeof(Vertex_PCU), VertexType::VERTEX_PCU, true);
	g_renderer->CopyCPUToGPU(gridVerts.data(), gridVerts.size() * sizeof(Vertex_PCU), m_gridVBO);
}

void Game::UpdateAttract()
{
	VRController leftController = g_openXR->GetLeftController();
	VRController rightController = g_openXR->GetRightController();

	if (g_input->WasKeyJustPressed(KEYCODE_SPACE) || leftController.WasAnyKeyJustPressed() || rightController.WasAnyKeyJustPressed())
	{
		m_nextState = GameState::MENU;
	}

	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		FireEvent("Quit");
	}
}

void Game::UpdateMenu()
{
	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_app->HandleQuitRequested();
	}
}

void Game::UpdateMapSelect()
{
	float deltaSeconds = m_clock.GetDeltaSeconds();

	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_nextState = GameState::MENU;
	}

	if (g_openXR && g_openXR->IsInitialized())
	{
		VRController const& leftController = g_openXR->GetLeftController();
		m_savedMapsListWidget->AddScroll(leftController.GetJoystick().GetPosition().y * 200.f * deltaSeconds);
	}
}

void Game::UpdateHowToPlay()
{
	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_nextState = GameState::MENU;
	}

	//for (int tabIndex = 0; tabIndex < NUM_HOW_TO_PLAY_TABS; tabIndex++)
	//{
	//	if (m_controlsTabIndex == tabIndex)
	//	{
	//		m_controlsWidgetTabButtons[tabIndex]->SetBackgroundColor(SECONDARY_COLOR)
	//			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
	//			->SetColor(PRIMARY_COLOR)
	//			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
	//			->SetBorderColor(PRIMARY_COLOR)
	//			->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT);

	//		m_controlsWidgetTabContainers[tabIndex]->SetVisible(true)->SetFocus(true);
	//	}
	//	else
	//	{
	//		m_controlsWidgetTabButtons[tabIndex]->SetBackgroundColor(PRIMARY_COLOR)
	//			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
	//			->SetColor(SECONDARY_COLOR)
	//			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
	//			->SetBorderColor(SECONDARY_COLOR)
	//			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);

	//		m_controlsWidgetTabContainers[tabIndex]->SetVisible(false)->SetFocus(false);
	//	}
	//}
}

void Game::UpdateCredits()
{
	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_nextState = GameState::MENU;
	}
}

void Game::UpdatePerforce()
{
}

void Game::UpdateGame()
{
	if (m_currentMap)
	{
		m_currentMap->Update();

		if (m_currentMap->m_mode == MapMode::PLAY)
		{
			m_gamePlayerStateWidget->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
		}
		else
		{
			m_gamePlayerStateWidget->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT);
		}

		if (m_isTutorial)
		{
			UpdateTutorialInstructions("");

			for (auto tutorialTriggerBoxMapIter = m_tutorialTriggerBoxesByText.begin(); tutorialTriggerBoxMapIter != m_tutorialTriggerBoxesByText.end(); ++tutorialTriggerBoxMapIter)
			{
				DebugAddWorldWireBox(tutorialTriggerBoxMapIter->second, 0.f, Rgba8::MAGENTA, Rgba8::MAGENTA, DebugRenderMode::USE_DEPTH);

				if (DoZCylinderAndAABB3Overlap(m_player->m_pawn->m_position, m_player->m_pawn->m_position + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT, PlayerPawn::PLAYER_RADIUS, tutorialTriggerBoxMapIter->second))
				{
					UpdateTutorialInstructions(tutorialTriggerBoxMapIter->first);
				}
			}
		}
	}

	if (m_player->m_state != PlayerState::PLAY)
	{
		m_saveButtonWidget->SetVisible(true);
		m_saveButtonWidget->SetFocus(true);
		m_coinsCollectedWidget->SetVisible(false);
		m_coinsCollectedWidget->SetFocus(false);

		if (g_openXR && g_openXR->IsInitialized())
		{
			m_leftUndoButton->SetFocus(true)->SetVisible(true);
			m_leftRedoButton->SetFocus(true)->SetVisible(true);
			m_rightUndoButton->SetFocus(true)->SetVisible(true);
			m_rightRedoButton->SetFocus(true)->SetVisible(true);
		}
	}
	else if (m_player->m_state == PlayerState::PLAY)
	{
		m_saveButtonWidget->SetVisible(false);
		m_saveButtonWidget->SetFocus(false);
		m_coinsCollectedWidget->SetVisible(true);
		m_coinsCollectedWidget->SetFocus(true);

		if (g_openXR && g_openXR->IsInitialized())
		{
			m_leftUndoButton->SetFocus(false)->SetVisible(false);
			m_leftRedoButton->SetFocus(false)->SetVisible(false);
			m_rightUndoButton->SetFocus(false)->SetVisible(false);
			m_rightRedoButton->SetFocus(false)->SetVisible(false);
		}
	}

	m_gamePlayerStateWidget->SetText(Stringf("Mode: %s", m_player->GetCurrentStateStr().c_str()));
	m_gamePlayerStateWidget->SetClickEventName(Stringf("ChangePlayerState newState=%d", ((int)m_player->m_state + 1) % (int)PlayerState::NUM));

	HandleKeyboardInput();
	HandleVRInput();

	UpdateInGameInstruction();
}

void Game::UpdatePause()
{
	if (m_player->m_state == PlayerState::PLAY)
	{
		m_togglePlayPositionWidget->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
		m_toggleLinkLinesWidget->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
		m_pauseSaveMapButton->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
	}
	else
	{
		m_togglePlayPositionWidget->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR);
		m_toggleLinkLinesWidget->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR);
		m_pauseSaveMapButton->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR);
	}

	if (m_currentMap->m_mode == MapMode::PLAY)
	{
		m_pausePlayerStateWidget->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
	}
	else
	{
		m_pausePlayerStateWidget->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT);;
	}

	m_pausePlayerStateWidget->SetText(Stringf("Mode: %s", m_player->GetCurrentStateStr().c_str()));
	m_pausePlayerStateWidget->SetClickEventName(Stringf("ChangePlayerState newState=%d", ((int)m_player->m_state + 1) % (int)PlayerState::NUM));
	m_togglePlayPositionWidget->SetText(Stringf("Play: %s", m_player->m_isStartPlayAtCameraPosition ? "Camera Position" : "Player Start"));
	m_toggleInstructionsWidget->SetText(Stringf("Instructions: %s", m_showInstructions ? "On" : "Off"));
	m_toggleLinkLinesWidget->SetText(Stringf("Link Lines: %s", m_currentMap->m_renderLinkLines ? "On" : "Off"));

	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_nextState = GameState::GAME;
	}
}

void Game::UpdateLevelImage()
{
}

void Game::UpdateLevelComplete()
{
	if (m_currentMap->m_mode == MapMode::PLAY)
	{
		m_levelCompleteContinueEditingButton->SetFocus(false)->SetBackgroundColor(SECONDARY_COLOR_VARIANT_DARK)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_DARK);
	}
	else
	{
		m_levelCompleteContinueEditingButton->SetFocus(true)->SetBackgroundColor(SECONDARY_COLOR)->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT);
	}
}

void Game::RenderAttract() const
{
}

void Game::RenderScreenAttract() const
{
	g_renderer->BeginRenderEvent("Attract Screen");
	{
		const float SCREEN_SIZE_X = SCREEN_SIZE_Y * g_window->GetAspect();
		const float SCREEN_CENTER_X = SCREEN_SIZE_X * 0.5f;
		const float SCREEN_CENTER_Y = SCREEN_SIZE_Y * 0.5f;

		std::vector<Vertex_PCU> attractScreenVerts;
		std::vector<Vertex_PCU> attractScreenTextVerts;
		std::vector<Vertex_PCU> attractScreenBackgroundVerts;
		AddVertsForAABB2(attractScreenBackgroundVerts, AABB2(Vec2(-0.25f, -0.25f), Vec2(0.25f, 0.25f)), Rgba8::WHITE);
		TransformVertexArrayXY3D(attractScreenBackgroundVerts, SCREEN_SIZE_Y, 0.f, Vec2(SCREEN_CENTER_X, SCREEN_CENTER_Y));

		g_renderer->SetBlendMode(BlendMode::OPAQUE);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetModelConstants();
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindShader(nullptr);

		g_renderer->BindTexture(m_gameLogoTexture);
		g_renderer->DrawVertexArray(attractScreenBackgroundVerts);

		g_renderer->BindTexture(nullptr);
		g_renderer->DrawVertexArray(attractScreenVerts);
	}
	g_renderer->EndRenderEvent("Attract Screen");
}

void Game::RenderMenu() const
{
}

void Game::RenderScreenMenu() const
{
	AABB2 screenBounds(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
	AABB2 logoBounds = screenBounds.GetBoxAtUVs(Vec2(0.25f, 0.25f), Vec2(1.05f, 0.75f));
	SpriteAnimDefinition logoBlinkAnimation(m_logoSpriteSheet, 270, 271, 1.f, SpriteAnimPlaybackType::LOOP);
	SpriteDefinition currentSprite = logoBlinkAnimation.GetSpriteDefAtTime(m_timeInState);
	std::vector<Vertex_PCU> logoVerts;
	AddVertsForAABB2(logoVerts, logoBounds, Rgba8::WHITE, currentSprite.GetUVs().m_mins, currentSprite.GetUVs().m_maxs);
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetModelConstants();
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindShader(nullptr);
	g_renderer->BindTexture(m_logoTexture);
	g_renderer->DrawVertexArray(logoVerts);
}

void Game::RenderMapSelect() const
{
}

void Game::RenderScreenMapSelect() const
{
}

void Game::RenderHowToPlay() const
{
}

void Game::RenderScreenHowToPlay() const
{
}

void Game::RenderCredits() const
{
}

void Game::RenderScreenCredits() const
{
}

void Game::RenderPerforce() const
{
}

void Game::RenderScreenPerforce() const
{
}

void Game::RenderGame() const
{
	if (m_currentMap)
	{
		m_currentMap->Render();
		if (m_player->m_state == PlayerState::EDITOR_CREATE || m_player->m_state == PlayerState::EDITOR_EDIT)
		{
			RenderGrid();
		}

		return;
	}
}

void Game::RenderScreenGame() const
{
	g_renderer->BeginRenderEvent("Game Screen");
	{
		if (!g_openXR || !g_openXR->IsInitialized())
		{
			Vec2 screenCenter(SCREEN_SIZE_Y * g_window->GetAspect() * 0.5f, SCREEN_SIZE_Y * 0.5f);
			std::vector<Vertex_PCU> reticleVerts;
			AddVertsForDisc2D(reticleVerts, screenCenter, 5.f, Rgba8::RED, Vec2::ZERO, Vec2::ONE, 32);
			g_renderer->SetBlendMode(BlendMode::ALPHA);
			g_renderer->SetDepthMode(DepthMode::DISABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->SetModelConstants();
			g_renderer->BindShader(nullptr);
			g_renderer->BindTexture(nullptr);
			g_renderer->DrawVertexArray(reticleVerts);
		}

		if (m_player->m_state == PlayerState::PLAY)
		{
			std::vector<Vertex_PCU> healthBarVerts;
			AABB2 healthBarBounds(Vec2(SCREEN_SIZE_Y * g_window->GetAspect() * 0.05f, SCREEN_SIZE_Y * 0.05f), Vec2(SCREEN_SIZE_Y * g_window->GetAspect() * 0.25f, SCREEN_SIZE_Y * 0.075f));
			AddVertsForAABB2(healthBarVerts, healthBarBounds, Rgba8::RED);
			AddVertsForAABB2(healthBarVerts, healthBarBounds.GetBoxAtUVs(Vec2::ZERO, Vec2((float)m_player->m_pawn->m_health / 100.f, 1.f)), Rgba8::GREEN);
			AddVertsForLineSegment2D(healthBarVerts, healthBarBounds.m_mins, Vec2(healthBarBounds.m_maxs.x, healthBarBounds.m_mins.y), 2.f, PRIMARY_COLOR);
			AddVertsForLineSegment2D(healthBarVerts, Vec2(healthBarBounds.m_maxs.x, healthBarBounds.m_mins.y), healthBarBounds.m_maxs, 2.f, PRIMARY_COLOR);
			AddVertsForLineSegment2D(healthBarVerts, healthBarBounds.m_maxs, Vec2(healthBarBounds.m_mins.x, healthBarBounds.m_maxs.y), 2.f, PRIMARY_COLOR);
			AddVertsForLineSegment2D(healthBarVerts, Vec2(healthBarBounds.m_mins.x, healthBarBounds.m_maxs.y), healthBarBounds.m_mins, 2.f, PRIMARY_COLOR);
			g_renderer->SetBlendMode(BlendMode::ALPHA);
			g_renderer->SetDepthMode(DepthMode::DISABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->SetModelConstants();
			g_renderer->BindShader(nullptr);
			g_renderer->BindTexture(nullptr);
			g_renderer->DrawVertexArray(healthBarVerts);
		}

		if (m_isMapImageVisible)
		{
			std::vector<Vertex_PCU> imageVerts;
			AABB2 screenBounds(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
			AddVertsForAABB2(imageVerts, screenBounds.GetBoxAtUVs(Vec2(0.55f, 0.45f), Vec2(0.95f, 0.85f)), Rgba8::WHITE);
			g_renderer->SetBlendMode(BlendMode::ALPHA);
			g_renderer->SetDepthMode(DepthMode::DISABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->SetModelConstants();
			g_renderer->BindShader(nullptr);
			g_renderer->BindTexture(m_mapImageTexture);
			g_renderer->DrawVertexArray(imageVerts);
		}
	}
	g_renderer->EndRenderEvent("Game Screen");
}

void Game::RenderPause() const
{
}

void Game::RenderScreenPause() const
{
}

void Game::RenderLevelImage() const
{
}

void Game::RenderScreenLevelImage() const
{
}

void Game::RenderLevelComplete() const
{
}

void Game::RenderScreenLevelComplete() const
{
}

void Game::RenderGrid() const
{
	g_renderer->BeginRenderEvent("Grid");
	{
		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetModelConstants();
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(nullptr);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexBuffer(m_gridVBO, (int)(m_gridVBO->m_size / sizeof(Vertex_PCU)));
	}
	g_renderer->EndRenderEvent("Grid");
}

void Game::RenderWorldScreenQuad() const
{
	g_renderer->BeginRenderEvent("World Screen Quad");
	{
		// Math for rendering screen quad
		XREye currentEye = g_app->GetCurrentEye();
		float quadHeight = SCREEN_QUAD_DISTANCE * TanDegrees(30.f) * (currentEye == XREye::NONE ? 1.f : 0.5f);
		float quadWidth = quadHeight * g_window->GetAspect();

		// Render screen quad
		std::vector<Vertex_PCU> screenVerts;
		AddVertsForQuad3D(screenVerts, Vec3(0.f, quadWidth, -quadHeight), Vec3(0.f, -quadWidth, -quadHeight), Vec3(0.f, -quadWidth, quadHeight), Vec3(0.f, quadWidth, quadHeight), Rgba8::WHITE, AABB2(Vec2(1.f, 1.f), Vec2(0.f, 0.f)));
		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::DISABLED);
		g_renderer->SetModelConstants(m_screenBillboardMatrix);
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(g_app->m_screenRTVTexture);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexArray(screenVerts);
	}
	g_renderer->EndRenderEvent("World Screen Quad");
}

void Game::HandleKeyboardInput()
{
	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_nextState = GameState::PAUSE;
	}
	if (g_openXR && g_openXR->IsInitialized())
	{
		VRController leftController = g_openXR->GetLeftController();
		if (leftController.WasMenuButtonJustPressed())
		{
			m_nextState = GameState::PAUSE;
		}
	}

	if (g_input->WasKeyJustPressed('F') && m_currentMap->m_mode != MapMode::PLAY)
	{
		FireEvent(Stringf("ChangePlayerState newState=%d", ((int)m_player->m_state + 1) % (int)PlayerState::NUM));
	}
}

void Game::HandleVRInput()
{
}

void Game::RenderOutroTransition() const
{
	if (m_nextState == GameState::NONE)
	{
		return;
	}

	float t = EaseOutQuadratic(m_transitionTimer.GetElapsedFraction());
	Rgba8 transitionColor = Interpolate(Rgba8::TRANSPARENT_BLACK, Rgba8::BLACK, t);
	Mat44 modelTransform = Mat44::CreateTranslation3D(m_player->m_position);
	g_renderer->BeginRenderEvent("Transition Sphere");
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetModelConstants(modelTransform, transitionColor);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_FRONT);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);
	g_renderer->DrawVertexBuffer(m_transitionSphereVBO, (int)m_transitionSphereVBO->m_size / sizeof(Vertex_PCU));
	g_renderer->EndRenderEvent("Transition Sphere");
}

void Game::RenderIntroTransition() const
{
	if (m_timeInState > m_transitionTimer.m_duration)
	{
		return;
	}

	float t = EaseOutQuadratic(m_timeInState * 2.f);
	Rgba8 transitionColor = Interpolate(Rgba8::BLACK, Rgba8::TRANSPARENT_BLACK, t);
	Mat44 modelTransform = m_player->GetModelMatrix();
	g_renderer->BeginRenderEvent("Transition Sphere");
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetModelConstants(modelTransform, transitionColor);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_FRONT);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);
	g_renderer->DrawVertexBuffer(m_transitionSphereVBO, (int)m_transitionSphereVBO->m_size / sizeof(Vertex_PCU));
	g_renderer->EndRenderEvent("Transition Sphere");
}

void Game::HandleStateChange()
{
	if (m_nextState == GameState::NONE)
	{
		return;
	}

	if (m_transitionTimer.IsStopped())
	{
		m_transitionTimer.Start();
	}
	if (m_transitionTimer.HasDurationElapsed())
	{
		switch (m_state)
		{
			case GameState::ATTRACT:		ExitAttract();			break;
			case GameState::MENU:			ExitMenu();				break;
			case GameState::MAP_SELECT:		ExitMapSelect();		break;
			case GameState::HOW_TO_PLAY:	ExitHowToPlay();		break;
			case GameState::CREDITS:		ExitCredits();			break;
			case GameState::PERFORCE:		ExitPerforce();			break;
			case GameState::GAME:			ExitGame();				break;
			case GameState::PAUSE:			ExitPause();			break;
			case GameState::LEVEL_IMAGE:	ExitLevelImage();		break;
			case GameState::LEVEL_COMPLETE:	ExitLevelComplete();	break;
		}

		m_state = m_nextState;
		m_nextState = GameState::NONE;

		switch (m_state)
		{
			case GameState::ATTRACT:		EnterAttract();			break;
			case GameState::MENU:			EnterMenu();			break;
			case GameState::MAP_SELECT:		EnterMapSelect();		break;
			case GameState::HOW_TO_PLAY:	EnterHowToPlay();		break;
			case GameState::CREDITS:		EnterCredits();			break;
			case GameState::PERFORCE:		EnterPerforce();		break;
			case GameState::GAME:			EnterGame();			break;
			case GameState::PAUSE:			EnterPause();			break;
			case GameState::LEVEL_IMAGE:	EnterLevelImage();		break;
			case GameState::LEVEL_COMPLETE:	EnterLevelCompelte();	break;
		}

		m_timeInState = 0.f;
		m_transitionTimer.Stop();
	}
}

void Game::EnterAttract()
{
	m_attractWidget->SetFocus(true);
	m_attractWidget->SetVisible(true);
}

void Game::ExitAttract()
{
	m_attractWidget->SetFocus(false);
	m_attractWidget->SetVisible(false);
}

void Game::EnterMenu()
{
	m_menuWidget->SetFocus(true);
	m_menuWidget->SetVisible(true);
	m_logoAnimationTimer.Start();
}

void Game::ExitMenu()
{
	m_menuWidget->SetFocus(false);
	m_menuWidget->SetVisible(false);
	m_logoAnimationTimer.Stop();
}

void Game::EnterMapSelect()
{
	m_mapSelectWidget->SetFocus(true);
	m_mapSelectWidget->SetVisible(true);


	Strings mapNames;
	int numSavedMaps = ListAllFilesInDirectory("Saved", mapNames);

	if (numSavedMaps == 1)
	{
		m_noSavedMapsWidget->SetVisible(true);
		m_noSavedMapsWidget->SetFocus(true);
		m_createMapWidget->SetVisible(true);
		m_createMapWidget->SetFocus(true);
	}
	else
	{
		if (!m_isConnectedToPerforce)
		{
			m_connectToPerforceMessageWidget->SetText("Connect to perforce to start read-only editing maps")->SetFocus(true)->SetVisible(true);
		}
		else
		{
			m_connectToPerforceMessageWidget->SetVisible(false)->SetFocus(false);
		}

		m_noSavedMapsWidget->SetVisible(false);
		m_noSavedMapsWidget->SetFocus(false);
		m_createMapWidget->SetVisible(false);
		m_createMapWidget->SetFocus(false);

		m_savedMapsListWidget = g_ui->CreateWidget(m_mapSelectWidget);
		m_savedMapsListWidget->SetPosition(Vec2(0.05f, 0.f))
			->SetDimensions(Vec2(0.9f, 0.8f))
			->SetPivot(Vec2(0.f, 0.f))
			->SetAlignment(Vec2(0.f, 0.f))
			->SetRaycastTarget(false)
			->SetScrollable(true)
			->SetScrollBuffer(200.f);
	}

	float widgetPositionY = 1.f;
	for (int savedMapIndex = 0; savedMapIndex < numSavedMaps; savedMapIndex++)
	{
		if (!strcmp(mapNames[savedMapIndex].c_str(), "placeholder.txt"))
		{
			// Skip the placeholder.txt created for perforce
			continue;
		}

		std::string mapDisplayName = "";
		Strings splitMapName;
		int numSplits = SplitStringOnDelimiter(splitMapName, mapNames[savedMapIndex], '.');
		for (int splitIndex = 0; splitIndex < numSplits - 1; splitIndex++)
		{
			mapDisplayName += splitMapName[splitIndex];
		}
		
		UIWidget* savedMapWidget = g_ui->CreateWidget(m_savedMapsListWidget);
		savedMapWidget->SetText(mapDisplayName)
			->SetPosition(Vec2(0.f, widgetPositionY))
			->SetDimensions(Vec2(1.f, 0.05f))
			->SetPivot(Vec2(0.f, 0.5f))
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR)
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR)
			->SetBorderRadius(0.5f)
			->SetFontSize(8.f)
			->SetRaycastTarget(false);

		UIWidget* playMapButton = g_ui->CreateWidget(savedMapWidget);
		playMapButton->SetText("Play")
			->SetPosition(Vec2(0.8f, 0.f))
			->SetDimensions(Vec2(0.1f, 0.9f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.5f)
			->SetFontSize(8.f)
			->SetRaycastTarget(true)
			->SetClickEventName(Stringf("PlayMap name=Saved/%s", mapNames[savedMapIndex].c_str()));

		bool isFileReadOnly = false;
		if (!m_isConnectedToPerforce)
		{
			isFileReadOnly = IsFileReadOnly((Stringf("Saved/%s", mapNames[savedMapIndex].c_str())));
		}

		UIWidget* editMapButton = g_ui->CreateWidget(savedMapWidget);
		editMapButton->SetText("Edit")
			->SetPosition(Vec2(0.925f, 0.f))
			->SetDimensions(Vec2(0.1f, 0.9f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetBackgroundColor(!isFileReadOnly ? PRIMARY_COLOR : PRIMARY_COLOR_VARIANT_DARK)
			->SetHoverBackgroundColor(!isFileReadOnly ? PRIMARY_COLOR_VARIANT_LIGHT : PRIMARY_COLOR_VARIANT_DARK)
			->SetColor(!isFileReadOnly ? SECONDARY_COLOR : SECONDARY_COLOR_VARIANT_DARK)
			->SetHoverColor(!isFileReadOnly ? SECONDARY_COLOR_VARIANT_LIGHT : SECONDARY_COLOR_VARIANT_DARK)
			->SetBorderRadius(0.5f)
			->SetFontSize(8.f)
			->SetRaycastTarget(true)
			->SetFocus(!isFileReadOnly)
			->SetClickEventName(Stringf("EditMap name=Saved/%s", mapNames[savedMapIndex].c_str()));

		widgetPositionY -= 0.075f;
	}
}

void Game::ExitMapSelect()
{
	if (m_savedMapsListWidget)
	{
		delete m_savedMapsListWidget;
	}

	m_mapSelectWidget->SetFocus(false);
	m_mapSelectWidget->SetVisible(false);
}

void Game::EnterHowToPlay()
{
	m_controlsWidget->SetFocus(true);
	m_controlsWidget->SetVisible(true);
}

void Game::ExitHowToPlay()
{
	m_controlsWidget->SetFocus(false);
	m_controlsWidget->SetVisible(false);
}

void Game::EnterCredits()
{
	m_creditsWidget->SetFocus(true);
	m_creditsWidget->SetVisible(true);
}

void Game::ExitCredits()
{
	m_creditsWidget->SetFocus(false);
	m_creditsWidget->SetVisible(false);
}

void Game::EnterPerforce()
{
	m_perforceWidget->SetFocus(true);
	m_perforceWidget->SetVisible(true);
	ReadPerforceSettings();
	m_perforceUserTextInputFieldWidget->SetText(m_p4User);
	m_perforceServerTextInputFieldWidget->SetText(m_p4Server);
	m_perforceWorkspaceTextInputFieldWidget->SetText(m_p4Workspace);
}

void Game::ExitPerforce()
{
	m_perforceWidget->SetFocus(false);
	m_perforceWidget->SetVisible(false);
}

void Game::EnterGame()
{
	if (!m_gridVBO)
	{
		InitializeGrid();
	}
	if (!m_currentMap)
	{
		m_currentMap = new Map(this);
		m_player->m_position = Vec3(-3.f, 2.f, 2.f);
		m_player->m_orientation = EulerAngles(-45.f, 0.f, 0.f);
	}

	if (m_isTutorial)
	{
		m_tutorialTextWidget->SetVisible(true);

		std::string tutorialTextMovement = "Use the left controller joystick to move.\nUse the right controller joystick to look around.";
		AABB3 tutorialTriggerBoxMovement(Vec3(-5.f, -5.5f, 0.f), Vec3(2.f, 5.5f, 2.f));
		m_tutorialTriggerBoxesByText[tutorialTextMovement] = tutorialTriggerBoxMovement;

		std::string tutorialTextJump = "Use A on the right controller to jump.";
		AABB3 tutorialTriggerBoxJump(Vec3(4.f, -5.5f, 0.f), Vec3(5.f, 5.5f, 2.f));
		m_tutorialTriggerBoxesByText[tutorialTextJump] = tutorialTriggerBoxJump;

		std::string tutorialTextLedgeGrab = "That jump looks like it's too far.\nAfter jumping, reach out with either hand\nand use the Grip button to grab a ledge.";
		AABB3 tutorialTriggerBoxLedgeGrab(Vec3(8.f, 0.5f, 0.5f), Vec3(10.f, 5.5f, 2.5f));
		m_tutorialTriggerBoxesByText[tutorialTextLedgeGrab] = tutorialTriggerBoxLedgeGrab;

		std::string tutorialTextLever = "Reach out with either hand and use\nthe grip button to grab the lever.\nMove the handle by moving your hand while holding it.";
		AABB3 tutorialTriggerBoxLever(Vec3(15.f, 0.5f, 0.5f), Vec3(18.f, 5.5f, 2.5f));
		m_tutorialTriggerBoxesByText[tutorialTextLever] = tutorialTriggerBoxLever;

		std::string tutorialTextLedgeGrab2 = "Try jumping up and grabbing the ledge again.";
		AABB3 tutorialTriggerBoxLedgeGrab2(Vec3(15.f, 6.5f, 3.5f), Vec3(17.f, 7.5f, 4.5f));
		m_tutorialTriggerBoxesByText[tutorialTextLedgeGrab2] = tutorialTriggerBoxLedgeGrab2;

		std::string tutorialTextEnemy = "Punch an enemy by holding the grip and\ntrigger buttons and swinging your hand.\nGrab an enemy by reaching out and holding the grip button.";
		AABB3 tutorialTriggerBoxEnemy(Vec3(13.f, 10.f, 3.5f), Vec3(19.f, 16.f, 6.5f));
		m_tutorialTriggerBoxesByText[tutorialTextEnemy] = tutorialTriggerBoxEnemy;

		std::string tutorialTextButton = "Stand on a button to open the door.\nWhen you step off, the door will close.";
		AABB3 tutorialTriggerBoxButton(Vec3(6.f, 15.f, 4.5f), Vec3(7.f, 16.5f, 6.5f));
		m_tutorialTriggerBoxesByText[tutorialTextButton] = tutorialTriggerBoxButton;

		std::string tutorialTextCrate = "Push the crate or reach out and use the grip button\nto grab it";
		AABB3 tutorialTriggerBoxCrate(Vec3(7.f, 11.f, 4.5f), Vec3(10.f, 12.f, 6.5f));
		m_tutorialTriggerBoxesByText[tutorialTextCrate] = tutorialTriggerBoxCrate;
	}
	else
	{
		m_tutorialTextWidget->SetVisible(false);
	}

	m_gameWidget->SetFocus(true);
	m_gameWidget->SetVisible(true);
	m_currentMap->m_coinsCollected = 0;
}

void Game::ExitGame()
{
	if (m_nextState != GameState::PAUSE && m_nextState != GameState::LEVEL_COMPLETE)
	{
		delete m_currentMap;
		m_currentMap = nullptr;

		m_player->m_state = PlayerState::NONE;
		m_player->m_mouseActionState = ActionType::NONE;
		m_player->m_leftController->m_actionState = ActionType::NONE;
		m_player->m_rightController->m_actionState = ActionType::NONE;
		m_player->m_position = Vec3::ZERO;
		m_player->m_orientation = EulerAngles::ZERO;
		m_player->m_velocity = Vec3::ZERO;
		m_player->m_acceleration = Vec3::ZERO;
		m_player->m_angularVelocity = EulerAngles::ZERO;
		m_player->m_pawn->m_position = Vec3::ZERO;
		m_player->m_pawn->m_velocity = Vec3::ZERO;
		m_player->m_pawn->m_acceleration = Vec3::ZERO;
		m_player->m_pawn->m_health = PlayerPawn::MAX_HEALTH;
		m_player->m_pawn->m_hasWon = false;
		m_isTutorial = false;
	}
	m_gameWidget->SetFocus(false);
	m_gameWidget->SetVisible(false);
}

void Game::EnterPause()
{
	m_pauseWidget->SetFocus(true);
	m_pauseWidget->SetVisible(true);
}

void Game::ExitPause()
{
	if (m_nextState != GameState::GAME && m_nextState != GameState::LEVEL_IMAGE && m_currentMap)
	{
		delete m_currentMap;
		m_currentMap = nullptr;

		m_player->m_state = PlayerState::NONE;
		m_player->m_mouseActionState = ActionType::NONE;
		m_player->m_leftController->m_actionState = ActionType::NONE;
		m_player->m_rightController->m_actionState = ActionType::NONE;
		m_player->m_position = Vec3::ZERO;
		m_player->m_orientation = EulerAngles::ZERO;
		m_player->m_velocity = Vec3::ZERO;
		m_player->m_acceleration = Vec3::ZERO;
		m_player->m_angularVelocity = EulerAngles::ZERO;
		m_player->m_pawn->m_hasWon = false;
		m_isTutorial = false;
	}
	m_pauseWidget->SetFocus(false);
	m_pauseWidget->SetVisible(false);
}

void Game::EnterLevelImage()
{
	m_levelImageWidget->SetVisible(true);
	m_levelImageWidget->SetFocus(true);
}

void Game::ExitLevelImage()
{
	m_levelImageWidget->SetVisible(false);
	m_levelImageWidget->SetFocus(false);
}

void Game::EnterLevelCompelte()
{
	m_levelCompleteWidget->SetFocus(true);
	m_levelCompleteWidget->SetVisible(true);
	m_levelCompleteCoinsCollectedTextWidget->SetText(Stringf("%d", m_currentMap->m_coinsCollected));
}

void Game::ExitLevelComplete()
{
	if (m_nextState != GameState::GAME && m_currentMap)
	{
		delete m_currentMap;
		m_currentMap = nullptr;

		m_player->m_state = PlayerState::NONE;
		m_player->m_mouseActionState = ActionType::NONE;
		m_player->m_leftController->m_actionState = ActionType::NONE;
		m_player->m_rightController->m_actionState = ActionType::NONE;
		m_player->m_position = Vec3::ZERO;
		m_player->m_orientation = EulerAngles::ZERO;
		m_player->m_velocity = Vec3::ZERO;
		m_player->m_acceleration = Vec3::ZERO;
		m_player->m_angularVelocity = EulerAngles::ZERO;
		m_player->m_pawn->m_hasWon = false;
		m_isTutorial = false;
	}
	m_levelCompleteWidget->SetFocus(false);
	m_levelCompleteWidget->SetVisible(false);
}

void Game::RenderSkybox() const
{
	Vec3 BLF = Vec3(-0.5f, 0.5f, -0.5f);
	Vec3 BRF = Vec3(-0.5f, -0.5f, -0.5f);
	Vec3 TRF = Vec3(-0.5f, -0.5f, 0.5f);
	Vec3 TLF = Vec3(-0.5f, 0.5f, 0.5f);
	Vec3 BLB = Vec3(0.5f, 0.5f, -0.5f);
	Vec3 BRB = Vec3(0.5f, -0.5f, -0.5f);
	Vec3 TRB = Vec3(0.5f, -0.5f, 0.5f);
	Vec3 TLB = Vec3(0.5f, 0.5f, 0.5f);

	g_renderer->BeginRenderEvent("Skybox");
	std::vector<Vertex_PCU> skyboxVerts;
	AddVertsForQuad3D(skyboxVerts, BRB, BLB, TLB, TRB, Rgba8::WHITE); // +X
	Mat44 skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::WEST * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetModelConstants(skyboxTransform);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_FRONT);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->BindShader(nullptr);
	g_renderer->BindTexture(m_skyboxTextures[0]);
	g_renderer->DrawVertexArray(skyboxVerts);

	skyboxVerts.clear();
	AddVertsForQuad3D(skyboxVerts, BLF, BRF, TRF, TLF, Rgba8::WHITE); // -X
	skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::EAST * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->BindTexture(m_skyboxTextures[1]);
	g_renderer->DrawVertexArray(skyboxVerts);

	skyboxVerts.clear();
	AddVertsForQuad3D(skyboxVerts, BLB, BLF, TLF, TLB, Rgba8::WHITE); // +Y
	skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::SOUTH * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->BindTexture(m_skyboxTextures[2]);
	g_renderer->DrawVertexArray(skyboxVerts);

	skyboxVerts.clear();
	AddVertsForQuad3D(skyboxVerts, BRF, BRB, TRB, TRF, Rgba8::WHITE); // -Y
	skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::NORTH * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->BindTexture(m_skyboxTextures[3]);
	g_renderer->DrawVertexArray(skyboxVerts);

	skyboxVerts.clear();
	AddVertsForQuad3D(skyboxVerts, TLF, TRF, TRB, TLB, Rgba8::WHITE); // +Z
	skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::SKYWARD * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->BindTexture(m_skyboxTextures[4]);
	g_renderer->DrawVertexArray(skyboxVerts);

	skyboxVerts.clear();
	AddVertsForQuad3D(skyboxVerts, BLB, BRB, BRF, BLF, Rgba8::WHITE); // -Z
	skyboxTransform = Mat44::CreateTranslation3D(m_player->m_position + Vec3::GROUNDWARD * 0.5f);
	skyboxTransform.AppendScaleUniform3D(100.f);
	g_renderer->BindTexture(m_skyboxTextures[5]);
	g_renderer->DrawVertexArray(skyboxVerts);

	g_renderer->EndRenderEvent("Skybox");
}

void Game::UpdateTutorialInstructions(std::string const& tutorialText)
{
	if (tutorialText.empty())
	{
		m_tutorialTextWidget->SetVisible(false);
		return;
	}

	m_tutorialTextWidget->SetVisible(true)->SetText(tutorialText);
}

void Game::UpdateInGameInstruction()
{
	if (m_player->m_state == PlayerState::NONE)
	{
		m_instructionsText = "Click on the button above to change the current mode";
	}
	else if (m_player->m_state == PlayerState::EDITOR_CREATE)
	{
		if (g_openXR && g_openXR->IsInitialized())
		{
			if (m_player->m_leftController->m_selectedEntityType == EntityType::NONE && m_player->m_rightController->m_selectedEntityType == EntityType::NONE)
			{
				m_instructionsText = "A/B or X/Y to cycle through Entities to spawn";
			}
			else
			{
				m_instructionsText = "Tap trigger to spawn single Entity, hold for multi-spawn";
			}
		}
		else
		{
			if (m_player->m_selectedEntityType == EntityType::NONE)
			{
				m_instructionsText = "Q/E to cycle through Entities to spawn";
			}
			else
			{
				m_instructionsText = "Tap LMB to spawn single Entity, hold for multi-spawn";
			}
		}
	}
	else if (m_player->m_state == PlayerState::EDITOR_EDIT)
	{
		if (m_player->m_mouseActionState == ActionType::LINK)
		{
			if (m_currentMap->m_isPulsingActivators)
			{
				m_instructionsText = "Select Activator";
			}
			else
			{
				m_instructionsText = "Select Activatable";
			}
		}
		else if (g_openXR && g_openXR->IsInitialized())
		{
			if (m_player->m_leftController->m_hoveredEntity == nullptr && m_player->m_rightController->m_hoveredEntity == nullptr)
			{
				m_instructionsText = "Point at an Entity";
			}
			else if (m_player->m_leftController->m_actionState == ActionType::NONE && m_player->m_rightController->m_actionState == ActionType::NONE)
			{
				m_instructionsText = "Grip: Translate -- Trigger: Clone -- A/X: Select -- B/Y: Delete";
			}
			else if (m_player->m_leftController->m_actionState == ActionType::TRANSLATE || m_player->m_rightController->m_actionState == ActionType::TRANSLATE)
			{
				m_instructionsText = "Same controller Trigger: Rotate -- Other Controller Grip: Scale";
			}
		}
		else
		{
			if (m_player->m_hoveredEntity == nullptr)
			{
				m_instructionsText = "Point at an Entity";
			}
			else
			{
				m_instructionsText = "LMB: Translate -- LAlt+LMB: Clone -- Space: Select -- Del: Delete; Arrow Keys: Rotate/Scale";
			}
		}
	}
	else if (m_player->m_state == PlayerState::PLAY)
	{
		m_instructionsText = "Get to the Flag";
	}

	m_instructionsWidget->SetText(m_instructionsText);
}

void Game::ReadPerforceSettings()
{
	m_p4User = "";
	m_p4Workspace = "";
	m_p4Server = "";

	std::string p4Config = RunCommand("p4 set");
	Strings p4ConfigLines;
	int numLinesInP4Config = SplitStringOnDelimiter(p4ConfigLines, p4Config, '\n');
	for (int lineIndex = 0; lineIndex < numLinesInP4Config; lineIndex++)
	{
		Strings keyValuePair;
		int numLineComponents = SplitStringOnDelimiter(keyValuePair, p4ConfigLines[lineIndex], '=');
		if (numLineComponents >= 2 && keyValuePair[0] == "P4USER")
		{
			Strings valueWithTrailingSet;
			SplitStringOnDelimiter(valueWithTrailingSet, keyValuePair[1], ' ');
			m_p4User = valueWithTrailingSet[0];
		}
		else if (numLineComponents >= 2 && keyValuePair[0] == "P4CLIENT")
		{
			Strings valueWithTrailingSet;
			SplitStringOnDelimiter(valueWithTrailingSet, keyValuePair[1], ' ');
			Strings userWorkspaceSplit;
			int numComponentsInWorkspace = SplitStringOnDelimiter(userWorkspaceSplit, valueWithTrailingSet[0], '_');
			for (int userWorkspaceSplitIndex = 1; userWorkspaceSplitIndex < numComponentsInWorkspace; userWorkspaceSplitIndex++)
			{
				m_p4Workspace += userWorkspaceSplit[userWorkspaceSplitIndex] + "_";
			}
			m_p4Workspace.erase(m_p4Workspace.end() - 1);
		}
		else if (numLineComponents >= 2 && keyValuePair[0] == "P4PORT")
		{
			Strings valueWithTrailingSet;
			SplitStringOnDelimiter(valueWithTrailingSet, keyValuePair[1], ' ');
			m_p4Server = valueWithTrailingSet[0];
		}
	}
}

bool Game::Event_ToggleShowInstructions(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_showInstructions = !g_app->m_game->m_showInstructions;
	if (g_app->m_game->m_showInstructions)
	{
		g_app->m_game->m_instructionsWidget->SetVisible(true);
	}
	else
	{
		g_app->m_game->m_instructionsWidget->SetVisible(false);
	}
	return true;
}

bool Game::Event_TogglePause(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_nextState = GameState::PAUSE;
	return true;
}

bool Game::Event_UndoLeftControllerAction(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_player->m_leftController->UndoLastAction();
	return true;
}

bool Game::Event_RedoLeftControllerAction(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_player->m_leftController->RedoLastAction();
	return true;
}

bool Game::Event_UndoRightControllerAction(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_player->m_rightController->UndoLastAction();
	return true;
}

bool Game::Event_RedoRightControllerAction(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_player->m_rightController->RedoLastAction();
	return true;
}

bool Game::Event_EditMap(EventArgs& args)
{
	std::string mapName = args.GetValue("name", "");
	if (mapName.empty())
	{
		return false;
	}

	std::string p4checkoutResult = RunCommand(Stringf("p4 edit %s\\%s", g_app->m_game->m_currentDir.c_str(), mapName.c_str()));
	
	g_app->m_game->m_currentMap = new Map(g_app->m_game, mapName, MapMode::EDIT);
	g_app->m_game->m_nextState = GameState::GAME;
	return true;
}

bool Game::Event_PlayMap(EventArgs& args)
{
	std::string mapName = args.GetValue("name", "");
	if (mapName.empty())
	{
		return false;
	}

	g_app->m_game->m_currentMap = new Map(g_app->m_game, mapName, MapMode::PLAY);
	g_app->m_game->m_nextState = GameState::GAME;
	return true;
}

bool Game::Event_ConnectToPerforce(EventArgs& args)
{
	UNUSED(args);

	std::string const& p4User = g_app->m_game->m_perforceUserTextInputFieldWidget->m_text;
	std::string const& p4Server = g_app->m_game->m_perforceServerTextInputFieldWidget->m_text;
	std::string const& p4Client = p4User + "_" + g_app->m_game->m_perforceWorkspaceTextInputFieldWidget->m_text;

	RunCommand(Stringf("p4 set P4PORT=%s P4USER=%s P4CLIENT=%s", p4Server.c_str(), p4User.c_str(), p4Client.c_str()));

	std::string p4Info = RunCommand("p4 info");
	Strings p4InfoLines;
	int numLines = SplitStringOnDelimiter(p4InfoLines, p4Info, '\n');

	std::string clientRoot = "";
	std::string currentDirectory = "";

	for (int lineIndex = 0; lineIndex < numLines; lineIndex++)
	{
		Strings lineComponents;
		SplitStringOnDelimiter(lineComponents, p4InfoLines[lineIndex], ' ');
		if (lineComponents[0] == "Client" && lineComponents[1] == "root:")
		{
			clientRoot = lineComponents[2];
			std::transform(clientRoot.begin(), clientRoot.end(), clientRoot.begin(), [](unsigned char c) { return (unsigned char)(std::tolower((int)c)); });
		}
		else if (lineComponents[0] == "Current" && lineComponents[1] == "directory:")
		{
			currentDirectory = lineComponents[2];
			std::transform(currentDirectory.begin(), currentDirectory.end(), currentDirectory.begin(), [](unsigned char c){ return (unsigned char)(std::tolower((int)c)); });
		}
	}

	if (clientRoot.empty())
	{
		g_app->m_game->m_perforceErrorMessageTextWidget->SetVisible(true)->SetText("Could not connect to perforce. Invalid configuration!");
		g_app->m_game->m_perforceStatusTextWidget->SetColor(Rgba8::YELLOW)->SetText("Status: Not connected");
		g_app->m_game->m_isConnectedToPerforce = false;
		return false;
	}
	if (currentDirectory.compare(0, clientRoot.length(), clientRoot) != 0)
	{
		g_app->m_game->m_perforceErrorMessageTextWidget->SetVisible(true)->SetText("Current directory is not under workspace root!");
		g_app->m_game->m_perforceStatusTextWidget->SetColor(Rgba8::YELLOW)->SetText("Status: Not connected");
		g_app->m_game->m_isConnectedToPerforce = false;
		return false;
	}

	g_app->m_game->m_currentDir = currentDirectory;
	g_app->m_game->m_perforceErrorMessageTextWidget->SetVisible(false);
	g_app->m_game->m_perforceStatusTextWidget->SetColor(Rgba8::GREEN)->SetText("Status: Connected!");
	g_app->m_game->m_isConnectedToPerforce = true;
	return true;
}

bool Game::Event_ToggleInGameMapImage(EventArgs& args)
{
	UNUSED(args);

	Game*& game = g_app->m_game;

	game->m_isMapImageVisible = !game->m_isMapImageVisible;
	game->m_toggleMapImageButton->SetImage(game->m_isMapImageVisible ? "Data/Images/cross.png" : "Data/Images/Image.png");
	return true;
}

bool Game::Event_StartTutorial(EventArgs& args)
{
	UNUSED(args);

	Game*& game = g_app->m_game;
	game->m_isTutorial = true;
	game->m_currentMap = new Map(game, "Saved/Tutorial.almap", MapMode::PLAY);
	game->m_nextState = GameState::GAME;

	return true;
}
