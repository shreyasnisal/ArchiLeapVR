#pragma once

#include "Game/GameCommon.hpp"

#include "Engine/VirtualReality/VRController.hpp"

#include <stack>


class Player;


class HandController
{
public:
	~HandController();
	HandController(XRHand hand, Player* owner);

	void UpdateTransform();
	void HandleInput();
	void Render() const;

	void UndoLastAction();
	void RedoLastAction();


	VRController& GetController();
	VRController const& GetController() const;
	VRController& GetOtherController();
	VRController const& GetOtherController() const;
	HandController* GetOtherHandController() const;

	Vec3 const GetLinearVelocity() const;

public:
	Vec3 m_worldPosition = Vec3::ZERO;
	Vec3 m_worldPositionLastFrame = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;
	EulerAngles m_orientationLastFrame = EulerAngles::ZERO;

private:
	void HandleCreateInput();
	void HandleEditInput();
	void HandlePlayInput();

	void HandleRaycastVsMapAndUI();
	void HandleRaycastVsScreen();
	void HandleButtonClicks();
	void HandleRaycastVsMap();

	void SpawnEntities();
	void TranslateEntity(Entity* entity, Vec3 const& translation) const;
	void SnapEntityToGrid(Entity* entity, Vec3& controllerRaycast) const;

	void RenderFakeEntitiesForSpawn() const;

public:
	static constexpr float CONTROLLER_RAYCAST_DISTANCE = 10.f;
	static constexpr float ENTITY_DISTANCE_ADJUST_SPEED = 5.f;
	static constexpr float CONTROLLER_SCALE = 0.03f;
	static constexpr float CONTROLLER_VIBRATION_AMPLITUDE = 0.1f;
	static constexpr float CONTROLLER_VIBRATION_DURATION = 0.1f;

	Player* m_player = nullptr;
	XRHand m_hand = XRHand::NONE;

	Vec3 m_localPosition = Vec3::ZERO;
	Vec3 m_raycastPosition = Vec3::ZERO;
	Vec3 m_raycastPositionLastFrame = Vec3::ZERO;
	Vec3 m_raycastDirection = Vec3::ZERO;
	float m_entityDistance = 0.f;
	Vec3 m_velocity = Vec3::ZERO;

	Shader* m_diffuseShader = nullptr;
	VertexBuffer* m_sphereVBO = nullptr;

	// Create mode variables
	EntityType m_selectedEntityType = EntityType::NONE;
	Vec3 m_entitySpawnStartPosition = Vec3::ZERO;
	Vec3 m_entitySpawnEndPosition = Vec3::ZERO;

	// Edit mode variables
	Entity* m_hoveredEntity = nullptr;
	Entity* m_selectedEntity = nullptr;
	Vec3 m_selectedEntityPosition = Vec3::ZERO;
	EulerAngles m_selectedEntityOrientation = EulerAngles::ZERO;
	float m_selectedEntityScale = 1.f;
	AxisLockDirection m_axisLockDirection = AxisLockDirection::NONE;
	float m_controllerBaselineDistanceForScaling = 0.f;
	bool m_isResponsibleForScaling = false;

	// UI variables
	UIWidget* m_hoveredWidget = nullptr;

	// Undo-Redo stacks
	std::stack<Action> m_undoActionStack;
	std::stack<Action> m_redoActionStack;

	ActionType m_actionState = ActionType::NONE;

	Model* m_model = nullptr;

	std::vector<Vertex_PCU> m_dropShadowVerts;

	Stopwatch m_redoDoubleTapTimer;
};

