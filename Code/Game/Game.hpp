#pragma once

#include "Game/GameCommon.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Stopwatch.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Plane3.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Spritesheet.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/UI/UIWidget.hpp"

#include <vector>


class Map;
class Player;


enum class GameState
{
	NONE = -1,

	ATTRACT,
	MENU,
	MAP_SELECT,
	HOW_TO_PLAY,
	CREDITS,
	PERFORCE,
	GAME,
	PAUSE,
	LEVEL_IMAGE,
	LEVEL_COMPLETE,

	COUNT
};


class Game
{
public:
	~Game();
	Game();
	
	void Update();
	void FixedUpdate(float deltaSeconds);
	void ClearScreen();
	void Render() const;
	void RenderScreen() const;
	void RenderCustomScreens() const;
	ArchiLeapRaycastResult3D RaycastVsScreen(Vec3 const& startPosition, Vec3 const& fwdNormal, float maxDistance) const;

public:
	static bool Event_Navigate(EventArgs& args);
	static bool Event_SetHowToPlayTab(EventArgs& args);
	static bool Event_ToggleShowInstructions(EventArgs& args);
	static bool Event_TogglePause(EventArgs& args);
	static bool Event_UndoLeftControllerAction(EventArgs& args);
	static bool Event_RedoLeftControllerAction(EventArgs& args);
	static bool Event_UndoRightControllerAction(EventArgs& args);
	static bool Event_RedoRightControllerAction(EventArgs& args);
	static bool Event_EditMap(EventArgs& args);
	static bool Event_PlayMap(EventArgs& args);
	static bool Event_ConnectToPerforce(EventArgs& args);
	static bool Event_ToggleInGameMapImage(EventArgs& args);
	static bool Event_StartTutorial(EventArgs& args);

public:
	static 	constexpr float SCREEN_QUAD_DISTANCE = 2.f;

	GameState m_state = GameState::NONE;
	GameState m_nextState = GameState::ATTRACT;
	float m_timeInState = 0.f;

	Clock m_clock = Clock();

	VertexBuffer* m_gridVBO = nullptr;

	Player* m_player = nullptr;
	Map* m_currentMap = nullptr;

	Texture* m_gameLogoTexture = nullptr;

	Mat44 m_screenBillboardMatrix = Mat44::IDENTITY;

	UIWidget* m_attractWidget = nullptr;

	UIWidget* m_menuWidget = nullptr;

	UIWidget* m_mapSelectWidget = nullptr;
	UIWidget* m_noSavedMapsWidget = nullptr;
	UIWidget* m_createMapWidget = nullptr;
	UIWidget* m_savedMapsListWidget = nullptr;
	UIWidget* m_connectToPerforceMessageWidget = nullptr;

	UIWidget* m_controlsWidget = nullptr;
	UIWidget* m_controlsWidgetTabButtons[4] = {nullptr, nullptr, nullptr, nullptr};
	UIWidget* m_controlsWidgetTabContainers[4] = {nullptr, nullptr, nullptr, nullptr};

	UIWidget* m_creditsWidget = nullptr;

	UIWidget* m_perforceWidget = nullptr;
	UIWidget* m_perforceUserTextInputFieldWidget = nullptr;
	UIWidget* m_perforceWorkspaceTextInputFieldWidget = nullptr;
	UIWidget* m_perforceServerTextInputFieldWidget = nullptr;
	UIWidget* m_perforceStatusTextWidget = nullptr;
	UIWidget* m_perforceErrorMessageTextWidget = nullptr;

	UIWidget* m_gameWidget = nullptr;
	UIWidget* m_gamePlayerStateWidget = nullptr;
	UIWidget* m_instructionsWidget = nullptr;
	UIWidget* m_saveButtonWidget = nullptr;
	UIWidget* m_coinsCollectedWidget = nullptr;
	UIWidget* m_coinsCollectedTextWidget = nullptr;
	UIWidget* m_leftUndoButton = nullptr;
	UIWidget* m_leftRedoButton = nullptr;
	UIWidget* m_rightUndoButton = nullptr;
	UIWidget* m_rightRedoButton = nullptr;
	UIWidget* m_toggleMapImageButton = nullptr;
	UIWidget* m_tutorialTextWidget = nullptr;

	UIWidget* m_pauseWidget = nullptr;
	UIWidget* m_pausePlayerStateWidget = nullptr;
	UIWidget* m_togglePlayPositionWidget = nullptr;
	UIWidget* m_toggleInstructionsWidget = nullptr;
	UIWidget* m_toggleLinkLinesWidget = nullptr;
	UIWidget* m_pauseSaveMapButton = nullptr;
	UIWidget* m_mapNameInputField = nullptr;

	UIWidget* m_levelImageWidget = nullptr;

	UIWidget* m_levelCompleteWidget = nullptr;
	UIWidget* m_levelCompleteContinueEditingButton = nullptr;
	UIWidget* m_levelCompleteCoinsCollectedWidget = nullptr;
	UIWidget* m_levelCompleteCoinsCollectedTextWidget = nullptr;

	//Texture* m_skyboxTextures[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	int m_instructionIndex = -1;
	bool m_showInstructions = true;
	std::string m_instructionsText = "";

	std::string m_p4User = "";
	std::string m_p4Server = "";
	std::string m_p4Workspace = "";
	std::string m_currentDir = "";

	bool m_isConnectedToPerforce = false;

	int m_controlsTabIndex = 0;

	bool m_isMapImageVisible = false;
	Texture* m_mapImageTexture = nullptr;

	bool m_isTutorial = false;
	std::map<std::string, AABB3> m_tutorialTriggerBoxesByText;

private:
	void LoadAssets();
	void InitializeGrid();

	void InitializeUI();
	void InitializeAttractUI();
	void InitializeMenuUI();
	void InitializeMapSelectUI();
	void InitializeHowToPlayUI();
	void InitializeCreditsUI();
	void InitializePerforceUI();
	void InitializeGameUI();
	void InitializePauseUI();
	void InitializeLevelImageUI();
	void InitializeLevelCompleteUI();

	void UpdateAttract();
	void UpdateMenu();
	void UpdateMapSelect();
	void UpdateHowToPlay();
	void UpdateCredits();
	void UpdatePerforce();
	void UpdateGame();
	void UpdatePause();
	void UpdateLevelImage();
	void UpdateLevelComplete();

	void RenderAttract() const;
	void RenderScreenAttract() const;
	
	void RenderMenu() const;
	void RenderScreenMenu() const;

	void RenderMapSelect() const;
	void RenderScreenMapSelect() const;

	void RenderHowToPlay() const;
	void RenderScreenHowToPlay() const;

	void RenderCredits() const;
	void RenderScreenCredits() const;

	void RenderPerforce() const;
	void RenderScreenPerforce() const;

	void RenderGame() const;
	void RenderScreenGame() const;

	void RenderPause() const;
	void RenderScreenPause() const;

	void RenderLevelImage() const;
	void RenderScreenLevelImage() const;
	
	void RenderLevelComplete() const;
	void RenderScreenLevelComplete() const;

	void RenderGrid() const;
	void RenderWorldScreenQuad() const;

	void HandleKeyboardInput();
	void HandleVRInput();

	void RenderOutroTransition() const;
	void RenderIntroTransition() const;

	void HandleStateChange();

	void EnterAttract();
	void ExitAttract();

	void EnterMenu();
	void ExitMenu();

	void EnterMapSelect();
	void ExitMapSelect();

	void EnterHowToPlay();
	void ExitHowToPlay();

	void EnterCredits();
	void ExitCredits();

	void EnterPerforce();
	void ExitPerforce();

	void EnterGame();
	void ExitGame();

	void EnterPause();
	void ExitPause();

	void EnterLevelImage();
	void ExitLevelImage();

	void EnterLevelCompelte();
	void ExitLevelComplete();

	void RenderSkybox() const;

	void UpdateTutorialInstructions(std::string const& tutorialText);
	void UpdateInGameInstruction();

private:
	static constexpr int NUM_HOW_TO_PLAY_TABS = 4;

	VertexBuffer* m_transitionSphereVBO = nullptr;
	Stopwatch m_transitionTimer = Stopwatch(0.25f);
	//Stopwatch m_logoAnimationTimer = Stopwatch(0.5f);

private:
	void ReadPerforceSettings();
};
