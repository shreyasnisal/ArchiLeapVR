#pragma once

#include "Game/GameCommon.hpp"

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/UI/UIWidget.hpp"

#include <stack>
#include <vector>


class Game;
class PlayerPawn;
class HandController;


class Player
{
public:
	~Player();
	explicit Player(Game* game, Vec3 const& position, EulerAngles const& orientation);

	void Update();
	void FixedUpdate(float deltaSeconds);
	void Render() const;
	void RenderScreen() const;

	void UpdateCameras();

	void UpdateMovementInput();
	void UpdateFreeFlyInput();
	void UpdateFreeFlyKeyboardInput();
	void UpdateFreeFlyVRInput();

	void UpdateFirstPersonInput();
	void UpdateFirstPersonKeyboardInput();
	void UpdateFirstPersonVRInput();

	void HandleKeyboardMouseEditing();
	void HandleKeyboardMouseEditing_Create();
	void HandleKeyboardMouseEditing_Edit();
	void SpawnEntities();
	void TranslateEntity(Entity* entity, Vec3 const& translation) const;
	void SnapEntityToGrid(Entity* entity, Vec3& controllerRaycast) const;
	void RenderFakeEntitiesForSpawn() const;

	void UpdateVRControllers();
	void RenderVRControllers() const;
	void RenderLinkingArrows() const;

	Vec3 const GetPlayerPosition() const;
	EulerAngles const GetPlayerOrientation() const;
	Mat44 const GetModelMatrix() const;

	std::string GetCurrentStateStr() const;

	void UndoLastAction();
	void RedoLastAction();

	void ChangeState(PlayerState prevState, PlayerState newState);

public:
	static bool Event_ChangeState(EventArgs& args);
	static bool Event_TogglePlayStartLocation(EventArgs& args);
	static bool Event_LinkEntity(EventArgs& args);

public:
	static constexpr float PLAYER_EYE_HEIGHT = 1.55f;
	static constexpr float FREEFLY_SPEED = 4.f;
	static constexpr float FREEFLY_SPRINT_FACTOR = 2.5f;
	static constexpr float CONTROLLER_RADIUS = 0.2f;
	static constexpr float TURN_RATE_PER_SECOND = 90.f;
	static constexpr float ENTITY_DISTANCE_ADJUST_SPEED = 5.f;
	static constexpr float RAYCAST_DISTANCE = 10.f;
	static constexpr float ENTITY_DISTANCE_ADJUST_PER_MOUSE_WHEEL_DELTA = 0.25f;

	Game* m_game = nullptr;
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;
	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
	EulerAngles m_angularVelocity = EulerAngles::ZERO;

	PlayerPawn* m_pawn = nullptr;

	Vec3 m_leftEyeLocalPosition = Vec3::ZERO;
	Vec3 m_rightEyeLocalPosition = Vec3::ZERO;
	EulerAngles m_hmdOrientation = EulerAngles::ZERO;

	std::vector<std::pair<Vec3, Vec3>> m_linkingArrows;

	bool m_isStartPlayAtCameraPosition = false;

	mutable UIWidget* m_leftHoveredWidget = nullptr;
	mutable UIWidget* m_rightHoveredWidget = nullptr;

	PlayerState m_state = PlayerState::NONE;

	HandController* m_leftController = nullptr;
	HandController* m_rightController = nullptr;

	Entity* m_linkingEntity = nullptr;

	// Variables for keyboard-mouse editing
	EntityType m_selectedEntityType = EntityType::NONE;
	Entity* m_hoveredEntity = nullptr;
	Entity* m_selectedEntity = nullptr;
	Vec3 m_selectedEntityPosition = Vec3::ZERO;
	EulerAngles m_selectedEntityOrientation = EulerAngles::ZERO;
	float m_selectedEntityScale = 1.f;
	Vec3 m_raycastPosition = Vec3::ZERO;
	Vec3 m_raycastPositionLastFrame = Vec3::ZERO;
	Vec3 m_raycastDirection = Vec3::ZERO;
	float m_entityDistance = 0.f;
	AxisLockDirection m_axisLockDirection = AxisLockDirection::NONE;
	Vec3 m_entitySpawnStartPosition = Vec3::ZERO;
	Vec3 m_entitySpawnEndPosition = Vec3::ZERO;
	ActionType m_mouseActionState = ActionType::NONE;
	std::stack<Action> m_undoActionStack;
	std::stack<Action> m_redoActionStack;

	// Drop shadow verts
	std::vector<Vertex_PCU> m_dropShadowVerts;
};
