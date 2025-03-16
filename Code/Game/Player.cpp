#include "Game/Player.hpp"

#include "Game/Activatable.hpp"
#include "Game/App.hpp"
#include "Game/Entity.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Activator.hpp"
#include "Game/Map.hpp"
#include "Game/PlayerPawn.hpp"
#include "Game/Tile.hpp"
#include "Game/HandController.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/UI/UISystem.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"
#include "Engine/VirtualReality/VRController.hpp"


Player::~Player()
{
	delete m_leftController;
	m_leftController = nullptr;
	delete m_rightController;
	m_rightController = nullptr;
}

Player::Player(Game* game, Vec3 const& position, EulerAngles const& orientation)
	: m_game(game)
	, m_position(position)
	, m_orientation(orientation)
{
	m_leftController = new HandController(XRHand::LEFT, this);
	m_rightController = new HandController(XRHand::RIGHT, this);

	SubscribeEventCallbackFunction("ChangePlayerState", Event_ChangeState, "Used to change the player state");
	SubscribeEventCallbackFunction("TogglePlayStartLocation", Event_TogglePlayStartLocation, "Used to change the start position when switching to play mode");
	SubscribeEventCallbackFunction("LinkEntity", Event_LinkEntity, "Used to link entities");
}

void Player::Update()
{
	VRController leftController = g_openXR->GetLeftController();

	UpdateMovementInput();
	if (m_state == PlayerState::PLAY)
	{
		m_position = m_pawn->m_position + PLAYER_EYE_HEIGHT * Vec3::SKYWARD;
		m_orientation = m_pawn->m_orientation;
	}

	if (g_openXR && g_openXR->IsInitialized())
	{
		UpdateVRControllers();
		m_leftController->HandleInput();
		m_rightController->HandleInput();
	}
	else
	{
		HandleKeyboardMouseEditing();
	}

	Vec3 fwd, left, up;
	m_orientation.GetAsVectors_iFwd_jLeft_kUp(fwd, left, up);
	g_audio->UpdateListeners(0, m_position, fwd, up);
	UpdateCameras();
}

void Player::FixedUpdate(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Player::Render() const
{
	RenderVRControllers();
	if (!g_openXR || !g_openXR->IsInitialized())
	{
		RenderFakeEntitiesForSpawn();
		if (!m_dropShadowVerts.empty())
		{
			g_renderer->SetBlendMode(BlendMode::ALPHA);
			g_renderer->SetDepthMode(DepthMode::DISABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->BindShader(nullptr);
			g_renderer->SetModelConstants();
			g_renderer->DrawVertexArray(m_dropShadowVerts);
		}
	}
}

void Player::RenderScreen() const
{
}

void Player::UpdateCameras()
{
	if (g_openXR && g_openXR->IsInitialized())
	{
		Mat44 const playerModelMatrix = GetModelMatrix();

		constexpr float XR_NEAR = NEAR_PLANE_DISTANCE;
		constexpr float XR_FAR = FAR_PLANE_DISTANCE;
		float lFovLeft, lFovRight, lFovUp, lFovDown;
		float rFovLeft, rFovRight, rFovUp, rFovDown;
		Vec3 leftEyePosition, rightEyePosition;
		EulerAngles leftEyeOrientation, rightEyeOrientation;

		g_openXR->GetFovsForEye(XREye::LEFT, lFovLeft, lFovRight, lFovUp, lFovDown);
		g_app->m_leftEyeCamera.SetXRView(lFovLeft, lFovRight, lFovUp, lFovDown, XR_NEAR, XR_FAR);
		Mat44 leftEyeTransform = playerModelMatrix;
		leftEyeTransform.AppendTranslation3D(m_leftEyeLocalPosition);
		leftEyeTransform.Append(m_hmdOrientation.GetAsMatrix_iFwd_jLeft_kUp());
		g_app->m_leftEyeCamera.SetTransform(leftEyeTransform);

		g_openXR->GetFovsForEye(XREye::RIGHT, rFovLeft, rFovRight, rFovUp, rFovDown);
		g_app->m_rightEyeCamera.SetXRView(rFovLeft, rFovRight, rFovUp, rFovDown, XR_NEAR, XR_FAR);
		Mat44 rightEyeTransform = playerModelMatrix;
		rightEyeTransform.AppendTranslation3D(m_rightEyeLocalPosition);
		rightEyeTransform.Append(m_hmdOrientation.GetAsMatrix_iFwd_jLeft_kUp());
		g_app->m_rightEyeCamera.SetTransform(rightEyeTransform);

		float leftEyeFov = ConvertRadiansToDegrees(lFovUp - lFovDown);
		float leftEyeAspect = (lFovRight - lFovLeft) / (lFovUp - lFovDown);

		g_app->m_leftWorldCamera.SetPerspectiveView(leftEyeAspect, leftEyeFov, XR_NEAR, XR_FAR);
		g_app->m_leftWorldCamera.SetNormalizedViewport(Vec2::ZERO, Vec2(0.5f, 1.f));
		g_app->m_leftWorldCamera.SetTransform(GetPlayerPosition() + leftEyePosition, GetPlayerOrientation() + leftEyeOrientation);

		float rightEyeFov = ConvertRadiansToDegrees(rFovUp - rFovDown);
		float rightEyeAspect = (rFovRight - rFovLeft) / (rFovUp - rFovDown);

		g_app->m_rightWorldCamera.SetPerspectiveView(rightEyeAspect, rightEyeFov, XR_NEAR, XR_FAR);
		g_app->m_rightWorldCamera.SetNormalizedViewport(Vec2(0.5f, 0.f), Vec2(0.5f, 1.f));
		g_app->m_rightWorldCamera.SetTransform(GetPlayerPosition() + rightEyePosition, GetPlayerOrientation() + rightEyeOrientation);
	}

	g_app->m_worldCamera.SetTransform(GetPlayerPosition(), GetPlayerOrientation() + m_hmdOrientation);
}

void Player::UpdateMovementInput()
{
	if (m_state == PlayerState::PLAY)
	{
		UpdateFirstPersonInput();
	}
	else
	{
		UpdateFreeFlyInput();
	}

	EulerAngles leftEyeOrientation;
	g_openXR->GetTransformForEye_iFwd_jLeft_kUp(XREye::LEFT, m_leftEyeLocalPosition, leftEyeOrientation);

	EulerAngles rightEyeOrientation;
	g_openXR->GetTransformForEye_iFwd_jLeft_kUp(XREye::RIGHT, m_rightEyeLocalPosition, rightEyeOrientation);

	m_hmdOrientation = rightEyeOrientation;
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -89.f, 89.f);
}

void Player::UpdateFreeFlyInput()
{
	if (g_openXR && g_openXR->IsInitialized())
	{
		UpdateFreeFlyVRInput();
	}
	else
	{
		UpdateFreeFlyKeyboardInput();
	}
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
}

void Player::UpdateFreeFlyKeyboardInput()
{
	float deltaSeconds = m_game->m_clock.GetDeltaSeconds();

	float movementSpeed = FREEFLY_SPEED;
	if (g_input->IsShiftHeld())
	{
		movementSpeed *= FREEFLY_SPRINT_FACTOR;
	}

	Vec3 playerFwd, playerLeft, playerUp;
	m_orientation.GetAsVectors_iFwd_jLeft_kUp(playerFwd, playerLeft, playerUp);

	if (g_input->IsKeyDown('W'))
	{
		m_position += playerFwd * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('S'))
	{
		m_position -= playerFwd * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('A'))
	{
		m_position += playerLeft * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('D'))
	{
		m_position -= playerLeft * movementSpeed * deltaSeconds;
	}

	m_orientation.m_yawDegrees += (float)g_input->GetCursorClientDelta().x * 0.075f;
	m_orientation.m_pitchDegrees -= (float)g_input->GetCursorClientDelta().y * 0.075f;
}

void Player::UpdateFreeFlyVRInput()
{
	VRController leftController = g_openXR->GetLeftController();
	VRController rightController = g_openXR->GetRightController();
	float deltaSeconds = m_game->m_clock.GetDeltaSeconds();
	float movementSpeed = FREEFLY_SPEED;

	if (leftController.GetTrigger() > 0.f)
	{
		movementSpeed *= FREEFLY_SPRINT_FACTOR;
	}

	Vec2 leftJoystickPosition = leftController.GetJoystick().GetPosition();
	Vec3 playerFwd, playerLeft, playerUp;
	EulerAngles cameraOrientation = g_app->GetCurrentCamera().GetOrientation();
	cameraOrientation.GetAsVectors_iFwd_jLeft_kUp(playerFwd, playerLeft, playerUp);

	Vec3 movementFwd = playerFwd;
	Vec3 movementLeft = playerLeft;

	if (!leftController.IsJoystickPressed())
	{
		m_position += leftJoystickPosition.y * movementFwd * movementSpeed * deltaSeconds;
		m_position += -leftJoystickPosition.x * movementLeft * movementSpeed * deltaSeconds;
	}

	if (!rightController.IsJoystickPressed())
	{
		m_orientation.m_yawDegrees -= rightController.GetJoystick().GetPosition().x * TURN_RATE_PER_SECOND * deltaSeconds;
	}
}

void Player::UpdateFirstPersonInput()
{
	if (g_openXR && g_openXR->IsInitialized())
	{
		UpdateFirstPersonVRInput();
	}
	UpdateFirstPersonKeyboardInput();
	m_pawn->m_orientation.m_pitchDegrees = GetClamped(m_pawn->m_orientation.m_pitchDegrees, -85.f, 85.f);
}

void Player::UpdateFirstPersonKeyboardInput()
{
	Vec3 playerFwd, playerLeft, playerUp;
	m_orientation.GetAsVectors_iFwd_jLeft_kUp(playerFwd, playerLeft, playerUp);

	Vec3 movementFwd = playerFwd.GetXY().GetNormalized().ToVec3();
	Vec3 movementLeft = playerLeft.GetXY().GetNormalized().ToVec3();

	if (g_input->IsKeyDown('W'))
	{
		m_pawn->MoveInDirection(movementFwd.GetXY().ToVec3());
	}
	if (g_input->IsKeyDown('S'))
	{
		m_pawn->MoveInDirection(-movementFwd.GetXY().ToVec3());
	}
	if (g_input->IsKeyDown('A'))
	{
		m_pawn->MoveInDirection(movementLeft.GetXY().ToVec3());
	}
	if (g_input->IsKeyDown('D'))
	{
		m_pawn->MoveInDirection(-movementLeft.GetXY().ToVec3());
	}
	if (g_input->WasKeyJustPressed(KEYCODE_SPACE))
	{
		m_pawn->Jump();
	}

	m_pawn->m_orientation.m_yawDegrees += (float)g_input->GetCursorClientDelta().x * 0.075f;
	m_pawn->m_orientation.m_pitchDegrees -= (float)g_input->GetCursorClientDelta().y * 0.075f;
}

void Player::UpdateFirstPersonVRInput()
{
	float deltaSeconds = m_game->m_clock.GetDeltaSeconds();

	VRController leftController = g_openXR->GetLeftController();
	VRController rightController = g_openXR->GetRightController();

	Vec3 playerFwd, playerLeft, playerUp;
	EulerAngles cameraOrientation = g_app->GetCurrentCamera().GetOrientation();
	cameraOrientation.GetAsVectors_iFwd_jLeft_kUp(playerFwd, playerLeft, playerUp);

	Vec3 movementFwd = playerFwd.GetXY().GetNormalized().ToVec3();
	Vec3 movementLeft = playerLeft.GetXY().GetNormalized().ToVec3();

	Vec2 leftJoystickPosition = leftController.GetJoystick().GetPosition();
	m_pawn->MoveInDirection((leftJoystickPosition.y * movementFwd) + (-leftJoystickPosition.x * movementLeft));

	Vec2 rightJoystickPosition = rightController.GetJoystick().GetPosition();
	m_pawn->m_orientation.m_yawDegrees += -rightJoystickPosition.x * PlayerPawn::TURN_RATE * deltaSeconds;

	if (rightController.WasSelectButtonJustPressed())
	{
		m_pawn->Jump();
	}
}

void Player::HandleKeyboardMouseEditing()
{
	m_dropShadowVerts.clear();

	if (!m_game->m_currentMap)
	{
		return;
	}
	if (g_ui->GetLastHoveredWidget())
	{
		return;
	}

	switch (m_state)
	{
		case PlayerState::EDITOR_CREATE:		HandleKeyboardMouseEditing_Create();		break;
		case PlayerState::EDITOR_EDIT:			HandleKeyboardMouseEditing_Edit();			break;
		case PlayerState::PLAY:																break;
	}

	if (m_state != PlayerState::PLAY)
	{
		m_raycastPositionLastFrame = m_raycastPosition;
		m_raycastDirection = m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D();
		m_raycastPosition = m_position + m_raycastDirection * (m_entityDistance ? m_entityDistance : RAYCAST_DISTANCE);
	
		if (g_input->IsKeyDown(KEYCODE_CTRL) && !g_input->IsShiftHeld() && g_input->WasKeyJustPressed('Z'))
		{
			UndoLastAction();
		}
		if (g_input->IsKeyDown(KEYCODE_CTRL) && g_input->WasKeyJustPressed('Y'))
		{
			RedoLastAction();
		}
		if (g_input->IsKeyDown(KEYCODE_CTRL) && g_input->IsShiftHeld() && g_input->WasKeyJustPressed('Z'))
		{
			RedoLastAction();
		}

		if (m_mouseActionState != ActionType::NONE && m_mouseActionState != ActionType::LINK)
		{
			return;
		}

		ArchiLeapRaycastResult3D raycastVsEntitiesResult = m_game->m_currentMap->RaycastVsEntities(m_position, m_raycastDirection, RAYCAST_DISTANCE);
		if (raycastVsEntitiesResult.m_didImpact)
		{
			if (m_state == PlayerState::EDITOR_EDIT)
			{
				m_hoveredEntity = raycastVsEntitiesResult.m_impactEntity;
				m_game->m_currentMap->SetHoveredEntityForHand(XRHand::NONE, m_hoveredEntity);
				m_entityDistance = raycastVsEntitiesResult.m_impactDistance;
			}
			else if (m_state == PlayerState::EDITOR_CREATE)
			{
				m_entityDistance = raycastVsEntitiesResult.m_impactDistance;
			}
		}
		else
		{
			m_hoveredEntity = nullptr;
			m_game->m_currentMap->SetHoveredEntityForHand(XRHand::NONE, nullptr);
			m_entityDistance = RAYCAST_DISTANCE;
			m_selectedEntityOrientation = EulerAngles::ZERO;
		}
	}
	else
	{
		m_selectedEntity = nullptr;
		m_selectedEntityType = EntityType::NONE;
		m_hoveredEntity = nullptr;
		m_game->m_currentMap->SetHoveredEntityForHand(XRHand::NONE, nullptr);
		m_entityDistance = RAYCAST_DISTANCE;
	}
}

void Player::HandleKeyboardMouseEditing_Create()
{
	if (m_selectedEntityType != EntityType::NONE)
	{
		m_entityDistance += g_input->m_cursorState.m_wheelScrollDelta * ENTITY_DISTANCE_ADJUST_PER_MOUSE_WHEEL_DELTA;
		m_entityDistance = GetClamped(m_entityDistance, 0.5f, 10.f);
	}

	if (g_input->WasKeyJustPressed('E'))
	{
		m_selectedEntityType = EntityType(((int)m_selectedEntityType + 1) % (int)EntityType::NUM);
		m_selectedEntity = m_game->m_currentMap->CreateEntityOfType(m_selectedEntityType, Vec3::ZERO, EulerAngles::ZERO, 1.f);
	}
	if (g_input->WasKeyJustPressed('Q'))
	{
		m_selectedEntityType = EntityType((int)m_selectedEntityType - 1);
		if ((int)m_selectedEntityType < 0)
		{
			m_selectedEntityType = EntityType((int)EntityType::NUM - 1);
		}
		m_selectedEntity = m_game->m_currentMap->CreateEntityOfType(m_selectedEntityType, Vec3::ZERO, EulerAngles::ZERO, 1.f);
	}
	if (g_input->WasKeyJustReleased(KEYCODE_LMB))
	{
		SpawnEntities();
	}
	if (g_input->IsKeyDown(KEYCODE_LMB))
	{
		m_entitySpawnEndPosition = m_raycastPosition;
	}
	else
	{
		m_entitySpawnStartPosition = m_raycastPosition;
		m_entitySpawnEndPosition = m_raycastPosition;
	}
}

void Player::HandleKeyboardMouseEditing_Edit()
{
	if (g_input->WasKeyJustPressed(KEYCODE_LMB) && !g_input->IsKeyDown(KEYCODE_LEFT_ALT) && (m_mouseActionState == ActionType::NONE || m_mouseActionState == ActionType::SELECT))
	{
		if (m_hoveredEntity)
		{
			m_mouseActionState = ActionType::TRANSLATE;
			m_selectedEntityType = m_hoveredEntity->m_type;
			m_selectedEntityPosition = m_hoveredEntity->m_position;
			m_selectedEntityOrientation = m_hoveredEntity->m_orientation;
			m_selectedEntityScale = m_hoveredEntity->m_scale;
			m_selectedEntity = m_hoveredEntity;

			Action translateAction;
			translateAction.m_actionType = ActionType::TRANSLATE;
			translateAction.m_actionEntity = m_hoveredEntity;
			translateAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			m_undoActionStack.push(translateAction);
			m_game->m_currentMap->m_isUnsaved = true;
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_LMB) && g_input->IsKeyDown(KEYCODE_LEFT_ALT) && (m_mouseActionState == ActionType::NONE || m_mouseActionState == ActionType::SELECT))
	{
		if (m_hoveredEntity)
		{
			m_mouseActionState = ActionType::CLONE;
			m_selectedEntityType = m_hoveredEntity->m_type;
			m_selectedEntityPosition = m_hoveredEntity->m_position;
			m_selectedEntityOrientation = m_hoveredEntity->m_orientation;
			m_selectedEntityScale = m_hoveredEntity->m_scale;
			m_selectedEntity = m_game->m_currentMap->SpawnNewEntityOfType(m_selectedEntityType, m_selectedEntityPosition, m_selectedEntityOrientation, m_selectedEntityScale);

			Action cloneAction;
			cloneAction.m_actionType = ActionType::CLONE;
			cloneAction.m_actionEntity = m_selectedEntity;
			m_undoActionStack.push(cloneAction);
			m_game->m_currentMap->m_isUnsaved = true;
		}
	}
	if (g_input->WasKeyJustReleased(KEYCODE_LMB) && (m_mouseActionState == ActionType::TRANSLATE || m_mouseActionState == ActionType::CLONE))
	{
		m_selectedEntityType = EntityType::NONE;
		m_selectedEntity = nullptr;
		m_game->m_currentMap->SetSelectedEntity(nullptr);
		m_mouseActionState = ActionType::NONE;
	}
	if (g_input->WasKeyJustPressed(KEYCODE_DELETE) && (m_mouseActionState == ActionType::NONE || m_mouseActionState == ActionType::SELECT))
	{
		if (m_selectedEntity)
		{
			Action deleteAction;
			deleteAction.m_actionType = ActionType::DELETE;
			deleteAction.m_actionEntity = m_selectedEntity;
			deleteAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			deleteAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			deleteAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(deleteAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_game->m_currentMap->RemoveEntityFromMap(m_selectedEntity);
			m_selectedEntity = nullptr;
			m_selectedEntityPosition = Vec3::ZERO;
			m_selectedEntityOrientation = EulerAngles::ZERO;
			m_selectedEntityScale = 1.f;
			m_mouseActionState = ActionType::NONE;
		}
		else if (m_hoveredEntity)
		{
			Action deleteAction;
			deleteAction.m_actionType = ActionType::DELETE;
			deleteAction.m_actionEntity = m_hoveredEntity;
			deleteAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			deleteAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			deleteAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(deleteAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_game->m_currentMap->RemoveEntityFromMap(m_hoveredEntity);
		}
	}
	if (g_input->IsKeyDown(KEYCODE_LMB) && (m_mouseActionState == ActionType::TRANSLATE || m_mouseActionState == ActionType::CLONE))
	{
		if (m_selectedEntity)
		{
			m_entityDistance += g_input->m_cursorState.m_wheelScrollDelta * ENTITY_DISTANCE_ADJUST_PER_MOUSE_WHEEL_DELTA;
			m_entityDistance = GetClamped(m_entityDistance, 0.5f, 10.f);

			Vec3 mouseRaycastDelta = m_raycastPosition - m_raycastPositionLastFrame;
			TranslateEntity(m_selectedEntity, mouseRaycastDelta);
			SnapEntityToGrid(m_selectedEntity, m_raycastPosition);

			ArchiLeapRaycastResult3D groundwardRaycastResult = m_game->m_currentMap->RaycastVsEntities(m_selectedEntity->m_position, Vec3::GROUNDWARD, 100.f, m_selectedEntity);
			if (groundwardRaycastResult.m_didImpact)
			{
				std::vector<Vertex_PCU> dropShadowVerts;
				float shadowOpacityFloat = RangeMapClamped(groundwardRaycastResult.m_impactDistance, 0.f, 10.f, 1.f, 0.f);
				unsigned char shadowOpacity = DenormalizeByte(shadowOpacityFloat);
				if (shadowOpacityFloat == 1.f)
				{
					shadowOpacity = 0;
				}
				Vec3 const dropShadowBL = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowBR = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowTR = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowTL = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
				AddVertsForQuad3D(m_dropShadowVerts, dropShadowBL, dropShadowBR, dropShadowTR, dropShadowTL, Rgba8(0, 0, 0, shadowOpacity));
			}
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_SPACE))
	{
		if (m_mouseActionState == ActionType::NONE && m_hoveredEntity)
		{
			m_mouseActionState = ActionType::SELECT;
			m_selectedEntity = m_hoveredEntity;
			m_game->m_currentMap->SetSelectedEntity(m_hoveredEntity);
		}
		else if (m_selectedEntity && m_mouseActionState == ActionType::SELECT)
		{
			m_selectedEntity = nullptr;
			m_game->m_currentMap->SetSelectedEntity(nullptr);
			m_mouseActionState = ActionType::NONE;
		}
		else if (m_selectedEntity && m_hoveredEntity && m_mouseActionState == ActionType::LINK)
		{
			Activator* activator = nullptr;
			Activatable* activatable = nullptr;
			if (m_selectedEntity->m_type == EntityType::BUTTON || m_selectedEntity->m_type == EntityType::LEVER)
			{
				activator = (Activator*)m_selectedEntity;
			}
			if (m_hoveredEntity->m_type == EntityType::BUTTON || m_hoveredEntity->m_type == EntityType::LEVER)
			{
				activator = (Activator*)m_hoveredEntity;
			}
			if (m_selectedEntity->m_type == EntityType::DOOR || m_selectedEntity->m_type == EntityType::MOVING_PLATFORM)
			{
				activatable = (Activatable*)m_selectedEntity;
			}
			if (m_hoveredEntity->m_type == EntityType::DOOR || m_hoveredEntity->m_type == EntityType::MOVING_PLATFORM)
			{
				activatable = (Activatable*)m_hoveredEntity;
			}

			if (activatable && activator)
			{
				Action linkAction;
				linkAction.m_actionType = ActionType::LINK;
				linkAction.m_activator = activator;
				linkAction.m_prevLinkedActivatable = activator->m_activatableUID;
				linkAction.m_activatable = activatable;
				linkAction.m_prevLinkedActivator = activatable->m_activatorUID;

				m_undoActionStack.push(linkAction);
				m_game->m_currentMap->m_isUnsaved = true;
			}
			
			m_game->m_currentMap->LinkEntities(m_hoveredEntity, m_selectedEntity);
			m_linkingEntity = nullptr;
			m_selectedEntity = nullptr;
			m_mouseActionState = ActionType::NONE;
			m_leftController->m_selectedEntity = nullptr;
			m_leftController->m_actionState = ActionType::NONE;
			m_rightController->m_selectedEntity = nullptr;
			m_rightController->m_actionState = ActionType::NONE;
			m_game->m_currentMap->SetSelectedEntity(nullptr);
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_LEFTARROW))
	{
		if (m_selectedEntity)
		{
			Action rotateAction;
			rotateAction.m_actionType = ActionType::ROTATE;
			rotateAction.m_actionEntity = m_selectedEntity;
			rotateAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			rotateAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			rotateAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(rotateAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_selectedEntity->m_orientation.m_yawDegrees += 15.f;
		}
		else if (m_hoveredEntity)
		{
			Action rotateAction;
			rotateAction.m_actionType = ActionType::ROTATE;
			rotateAction.m_actionEntity = m_hoveredEntity;
			rotateAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			rotateAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			rotateAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(rotateAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_hoveredEntity->m_orientation.m_yawDegrees += 15.f;
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_RIGHTARROW))
	{
		if (m_selectedEntity)
		{
			Action rotateAction;
			rotateAction.m_actionType = ActionType::ROTATE;
			rotateAction.m_actionEntity = m_selectedEntity;
			rotateAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			rotateAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			rotateAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(rotateAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_selectedEntity->m_orientation.m_yawDegrees -= 15.f;
		}
		else if (m_hoveredEntity)
		{
			Action rotateAction;
			rotateAction.m_actionType = ActionType::ROTATE;
			rotateAction.m_actionEntity = m_hoveredEntity;
			rotateAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			rotateAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			rotateAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(rotateAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_hoveredEntity->m_orientation.m_yawDegrees -= 15.f;
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_UPARROW))
	{
		if (m_selectedEntity)
		{
			Action scaleAction;
			scaleAction.m_actionType = ActionType::SCALE;
			scaleAction.m_actionEntity = m_selectedEntity;
			scaleAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			scaleAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			scaleAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(scaleAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_selectedEntity->m_scale += 0.1f;
		}
		else if (m_hoveredEntity)
		{
			Action scaleAction;
			scaleAction.m_actionType = ActionType::SCALE;
			scaleAction.m_actionEntity = m_hoveredEntity;
			scaleAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			scaleAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			scaleAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(scaleAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_hoveredEntity->m_scale += 0.1f;
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_DOWNARROW))
	{
		if (m_selectedEntity)
		{
			Action scaleAction;
			scaleAction.m_actionType = ActionType::SCALE;
			scaleAction.m_actionEntity = m_selectedEntity;
			scaleAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			scaleAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			scaleAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(scaleAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_selectedEntity->m_scale -= 0.1f;
		}
		if (m_hoveredEntity)
		{
			Action scaleAction;
			scaleAction.m_actionType = ActionType::SCALE;
			scaleAction.m_actionEntity = m_hoveredEntity;
			scaleAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			scaleAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			scaleAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(scaleAction);
			m_game->m_currentMap->m_isUnsaved = true;

			m_hoveredEntity->m_scale -= 0.1f;
		}
	}
	if (g_input->WasKeyJustPressed(KEYCODE_END))
	{
		if (m_selectedEntity && m_mouseActionState == ActionType::TRANSLATE)
		{
			constexpr int NUM_RAYCASTS = 9;
			OBB3 selectedEntityBounds = m_selectedEntity->GetBounds();
			Vec3 cornerPoints[8];
			selectedEntityBounds.GetCornerPoints(cornerPoints);
			Vec3 raycastPoints[9] = {
				m_selectedEntity->m_position,
				// Vertexes- taking only alternate to discard skyward vertexes
				cornerPoints[0],
				cornerPoints[2],
				cornerPoints[4],
				cornerPoints[6],
				// Edge centers: Weird calculation to account for the order of corner points
				// I drew a picture to figure this out
				(cornerPoints[0] + cornerPoints[2]) * 0.5f,
				(cornerPoints[0] + cornerPoints[4]) * 0.5f,
				(cornerPoints[4] + cornerPoints[6]) * 0.5f,
				(cornerPoints[6] + cornerPoints[2]) * 0.5f,
			};

			float leastImpactDistance = FLT_MAX;
			float closestImpactZ = 0.f;
			bool foundRaycastImpact = false;

			for (int raycastPointIndex = 0; raycastPointIndex < NUM_RAYCASTS; raycastPointIndex++)
			{
				ArchiLeapRaycastResult3D downwardRaycastResult = m_game->m_currentMap->RaycastVsEntities(raycastPoints[raycastPointIndex], Vec3::GROUNDWARD, 100.f, m_selectedEntity);
				if (downwardRaycastResult.m_didImpact)
				{
					foundRaycastImpact = true;
					if (downwardRaycastResult.m_impactDistance < leastImpactDistance)
					{
						leastImpactDistance = downwardRaycastResult.m_impactDistance;
						closestImpactZ = downwardRaycastResult.m_impactPosition.z;
					}
				}
			}
			
			if (foundRaycastImpact)
			{
				m_selectedEntity->m_position.z = closestImpactZ - m_selectedEntity->m_localBounds.m_mins.z;
			}
			else
			{
				m_selectedEntity->m_position.z = 0.f;
			}

			m_selectedEntity = nullptr;
			m_game->m_currentMap->SetSelectedEntity(nullptr);
			m_mouseActionState = ActionType::NONE;
		}
	}
}

void Player::SpawnEntities()
{
	if (m_selectedEntityType == EntityType::NONE)
	{
		return;
	}

	Action spawnAction;
	spawnAction.m_actionType = ActionType::CREATE;

	Vec3 multiSpawnMins(GetMin(m_entitySpawnStartPosition.x, m_entitySpawnEndPosition.x), GetMin(m_entitySpawnStartPosition.y, m_entitySpawnEndPosition.y), GetMin(m_entitySpawnStartPosition.z, m_entitySpawnEndPosition.z));
	Vec3 multiSpawnMaxs(GetMax(m_entitySpawnStartPosition.x, m_entitySpawnEndPosition.x), GetMax(m_entitySpawnStartPosition.y, m_entitySpawnEndPosition.y), GetMax(m_entitySpawnStartPosition.z, m_entitySpawnEndPosition.z));

	for (int xPos = (int)multiSpawnMins.x; xPos <= (int)multiSpawnMaxs.x; xPos++)
	{
		for (int yPos = (int)multiSpawnMins.y; yPos <= (int)multiSpawnMaxs.y; yPos++)
		{
			for (int zPos = (int)multiSpawnMins.z; zPos <= (int)multiSpawnMaxs.z; zPos++)
			{
				Entity* spawnedEntity = m_game->m_currentMap->SpawnNewEntityOfType(m_selectedEntityType, Vec3((float)xPos, (float)yPos, (float)zPos), EulerAngles::ZERO, 1.f);
				spawnAction.m_createdEntities.push_back(spawnedEntity);
			}
		}
	}

	m_undoActionStack.push(spawnAction);
	m_game->m_currentMap->m_isUnsaved = true;

	m_selectedEntity = nullptr;
	m_selectedEntityType = EntityType::NONE;
	m_entitySpawnStartPosition = m_raycastPosition;
	m_entitySpawnEndPosition = m_raycastPosition;
}

void Player::TranslateEntity(Entity* entity, Vec3 const& translation) const
{
	if (m_axisLockDirection == AxisLockDirection::NONE)
	{
		entity->m_position += translation;
		return;
	}
	if (m_axisLockDirection == AxisLockDirection::X)
	{
		entity->m_position += Vec3::EAST * translation.x;
		return;
	}
	if (m_axisLockDirection == AxisLockDirection::Y)
	{
		entity->m_position += Vec3::NORTH * translation.y;
		return;
	}
	if (m_axisLockDirection == AxisLockDirection::Z)
	{
		entity->m_position += Vec3::SKYWARD * translation.z;
		return;
	}
}

void Player::SnapEntityToGrid(Entity* entity, Vec3& controllerRaycast) const
{
	Vec3 entityPositionInt = Vec3(roundf(entity->m_position.x), roundf(entity->m_position.y), roundf(entity->m_position.z));
	Vec3 entityDistanceFromGrid = entityPositionInt - entity->m_position;
	if (fabsf(entityDistanceFromGrid.x) < 0.1f)
	{
		float& snapDelta = entityDistanceFromGrid.x;
		entity->m_position.x += snapDelta;
		controllerRaycast.x += snapDelta;
	}
	if (fabsf(entityDistanceFromGrid.y) < 0.1f)
	{
		float& snapDelta = entityDistanceFromGrid.y;
		entity->m_position.y += snapDelta;
		controllerRaycast.y += snapDelta;
	}
	if (fabsf(entityDistanceFromGrid.z) < 0.1f)
	{
		float& snapDelta = entityDistanceFromGrid.z;
		entity->m_position.z += snapDelta;
		controllerRaycast.z += snapDelta;
	}
}

void Player::RenderFakeEntitiesForSpawn() const
{
	if (m_game->m_state != GameState::GAME)
	{
		return;
	}
	if (m_state != PlayerState::EDITOR_CREATE)
	{
		return;
	}
	if (m_selectedEntityType == EntityType::NONE)
	{
		return;
	}

	Vec3 multiSpawnMins(GetMin(m_entitySpawnStartPosition.x, m_entitySpawnEndPosition.x), GetMin(m_entitySpawnStartPosition.y, m_entitySpawnEndPosition.y), GetMin(m_entitySpawnStartPosition.z, m_entitySpawnEndPosition.z));
	Vec3 multiSpawnMaxs(GetMax(m_entitySpawnStartPosition.x, m_entitySpawnEndPosition.x), GetMax(m_entitySpawnStartPosition.y, m_entitySpawnEndPosition.y), GetMax(m_entitySpawnStartPosition.z, m_entitySpawnEndPosition.z));

	for (int xPos = (int)multiSpawnMins.x; xPos <= (int)multiSpawnMaxs.x; xPos++)
	{
		for (int yPos = (int)multiSpawnMins.y; yPos <= (int)multiSpawnMaxs.y; yPos++)
		{
			for (int zPos = (int)multiSpawnMins.z; zPos <= (int)multiSpawnMaxs.z; zPos++)
			{
				Vec3 entityPosition((float)xPos, (float)yPos, (float)zPos);

				Mat44 transform = Mat44::CreateTranslation3D(entityPosition + Vec3::SKYWARD * (m_selectedEntityType == EntityType::ENEMY_ORC ? 0.6f : 0.f));
				transform.Append(m_selectedEntityOrientation.GetAsMatrix_iFwd_jLeft_kUp());
				transform.AppendScaleUniform3D(m_game->m_currentMap->GetDefaultEntityScaleForType(m_selectedEntityType));

				g_renderer->SetBlendMode(BlendMode::ALPHA);
				g_renderer->SetDepthMode(DepthMode::DISABLED);
				g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
				g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
				g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
				g_renderer->BindTexture(nullptr);
				g_renderer->BindShader(nullptr);
				g_renderer->SetModelConstants(transform, Rgba8(255, 255, 255, 195));
				g_renderer->DrawIndexBuffer(m_selectedEntity->m_model->GetVertexBuffer(), m_selectedEntity->m_model->GetIndexBuffer(), m_selectedEntity->m_model->GetIndexCount());

				ArchiLeapRaycastResult3D groundwardRaycastResult = m_game->m_currentMap->RaycastVsEntities(entityPosition, Vec3::GROUNDWARD, 100.f, m_selectedEntity);
				if (groundwardRaycastResult.m_didImpact)
				{
					std::vector<Vertex_PCU> dropShadowVerts;
					float shadowOpacityFloat = RangeMapClamped(groundwardRaycastResult.m_impactDistance, 0.f, 10.f, 1.f, 0.f);
					unsigned char shadowOpacity = DenormalizeByte(shadowOpacityFloat);
					if (shadowOpacityFloat == 1.f)
					{
						shadowOpacity = 0;
					}
					Vec3 const dropShadowBL = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
					Vec3 const dropShadowBR = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
					Vec3 const dropShadowTR = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
					Vec3 const dropShadowTL = groundwardRaycastResult.m_impactPosition + Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
					AddVertsForQuad3D(dropShadowVerts, dropShadowBL, dropShadowBR, dropShadowTR, dropShadowTL, Rgba8(0, 0, 0, shadowOpacity));
					g_renderer->SetBlendMode(BlendMode::ALPHA);
					g_renderer->SetDepthMode(DepthMode::DISABLED);
					g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
					g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
					g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
					g_renderer->BindTexture(nullptr);
					g_renderer->BindShader(nullptr);
					g_renderer->SetModelConstants();
					g_renderer->DrawVertexArray(dropShadowVerts);
				}
			}
		}
	}
}

void Player::UpdateVRControllers()
{
	m_leftController->UpdateTransform();
	m_rightController->UpdateTransform();
}

void Player::RenderVRControllers() const
{
	if (!g_openXR || !g_openXR->IsInitialized())
	{
		return;
	}

	m_leftController->Render();
	m_rightController->Render();
}

void Player::RenderLinkingArrows() const
{
	//if (m_state == PlayerState::PLAY || !m_game->m_currentMap)
	//{
	//	return;
	//}

	//std::vector<Vertex_PCU> linkingArrowsVerts;
	//for (int linkingArrowIndex = 0; linkingArrowIndex < (int)m_linkingArrows.size(); linkingArrowIndex++)
	//{
	//	AddVertsForLineSegment3D(linkingArrowsVerts, m_linkingArrows[linkingArrowIndex].first, m_linkingArrows[linkingArrowIndex].second, 0.01f, Rgba8::MAGENTA);
	//}

	//g_renderer->SetBlendMode(BlendMode::ALPHA);
	//g_renderer->SetDepthMode(DepthMode::ENABLED);
	//g_renderer->SetModelConstants();
	//g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	//g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	//g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	//g_renderer->BindTexture(nullptr);
	//g_renderer->BindShader(nullptr);
	//g_renderer->DrawVertexArray(linkingArrowsVerts);
}

Vec3 const Player::GetPlayerPosition() const
{
	return m_position;
}

EulerAngles const Player::GetPlayerOrientation() const
{
	return m_orientation;
}

Mat44 const Player::GetModelMatrix() const
{
	Mat44 result = Mat44::CreateTranslation3D(m_position);
	result.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	return result;
}

std::string Player::GetCurrentStateStr() const
{
	switch (m_state)
	{
		case PlayerState::EDITOR_CREATE:			return "Create";
		case PlayerState::EDITOR_EDIT:				return "Edit";
		case PlayerState::PLAY:						return "Play";
	}

	return "View";
}

void Player::UndoLastAction()
{
	if (m_undoActionStack.empty())
	{
		return;
	}

	Action lastAction = m_undoActionStack.top();
	m_undoActionStack.pop();

	switch (lastAction.m_actionType)
	{
		case ActionType::TRANSLATE:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::TRANSLATE;
			redoAction.m_actionEntity = lastAction.m_actionEntity;
			redoAction.m_actionEntityPreviousPosition = lastAction.m_actionEntity->m_position;
			m_redoActionStack.push(redoAction);

			lastAction.m_actionEntity->m_position = lastAction.m_actionEntityPreviousPosition;
			break;
		}
		case ActionType::ROTATE:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::ROTATE;
			redoAction.m_actionEntity = lastAction.m_actionEntity;
			redoAction.m_actionEntityPreviousOrientation = lastAction.m_actionEntity->m_orientation;
			m_redoActionStack.push(redoAction);

			lastAction.m_actionEntity->m_orientation = lastAction.m_actionEntityPreviousOrientation;
			break;
		}
		case ActionType::SCALE:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::SCALE;
			redoAction.m_actionEntity = lastAction.m_actionEntity;
			redoAction.m_actionEntityPreviousScale = lastAction.m_actionEntity->m_scale;
			m_redoActionStack.push(redoAction);

			lastAction.m_actionEntity->m_scale = lastAction.m_actionEntityPreviousScale;
			break;
		}
		case ActionType::CREATE:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::DELETE;

			if (lastAction.m_createdEntities.empty())
			{
				redoAction.m_createdEntities.push_back(lastAction.m_actionEntity);
				redoAction.m_createdEntityPositions.push_back(lastAction.m_actionEntity->m_position);
				redoAction.m_createdEntityOrientations.push_back(lastAction.m_actionEntity->m_orientation);
				redoAction.m_createdEntityScales.push_back(lastAction.m_actionEntity->m_scale);

				m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
			}
			else
			{
				for (int entityIndex = 0; entityIndex < (int)lastAction.m_createdEntities.size(); entityIndex++)
				{
					Entity*& entity = lastAction.m_createdEntities[entityIndex];

					redoAction.m_createdEntities.push_back(entity);
					redoAction.m_createdEntityPositions.push_back(entity->m_position);
					redoAction.m_createdEntityOrientations.push_back(entity->m_orientation);
					redoAction.m_createdEntityScales.push_back(entity->m_scale);

					m_game->m_currentMap->RemoveEntityFromMap(entity);
				}
			}

			m_redoActionStack.push(redoAction);
			break;
		}
		case ActionType::CLONE:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::DELETE;
			redoAction.m_actionEntity = lastAction.m_actionEntity;
			redoAction.m_actionEntityPreviousPosition = lastAction.m_actionEntity->m_position;
			redoAction.m_actionEntityPreviousOrientation = lastAction.m_actionEntity->m_orientation;
			redoAction.m_actionEntityPreviousScale = lastAction.m_actionEntity->m_scale;
			m_redoActionStack.push(redoAction);

			m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
			break;
		}
		case ActionType::DELETE:
		{
			Entity* createdEntity = m_game->m_currentMap->SpawnNewEntityOfType(lastAction.m_actionEntity->m_type, lastAction.m_actionEntityPreviousPosition, lastAction.m_actionEntityPreviousOrientation, lastAction.m_actionEntityPreviousScale);
			
			Action redoAction;
			redoAction.m_actionType = ActionType::CREATE;
			redoAction.m_actionEntity = createdEntity;
			m_redoActionStack.push(redoAction);
			
			break;
		}
		case ActionType::LINK:
		{
			Action redoAction;
			redoAction.m_actionType = ActionType::LINK;
			redoAction.m_activator = lastAction.m_activator;
			redoAction.m_activatable = lastAction.m_activatable;
			redoAction.m_prevLinkedActivatable = lastAction.m_activator->m_activatableUID;
			redoAction.m_prevLinkedActivator = lastAction.m_activatable->m_activatorUID;
			m_redoActionStack.push(redoAction);

			lastAction.m_activator->m_activatableUID = lastAction.m_prevLinkedActivatable;
			lastAction.m_activatable->m_activatorUID = lastAction.m_prevLinkedActivator;

			break;
		}
	}
}

void Player::RedoLastAction()
{
	if (m_redoActionStack.empty())
	{
		return;
	}

	Action lastAction = m_redoActionStack.top();
	m_redoActionStack.pop();

	switch (lastAction.m_actionType)
	{
		case ActionType::TRANSLATE:
		{
			lastAction.m_actionEntity->m_position = lastAction.m_actionEntityPreviousPosition;
			break;
		}
		case ActionType::ROTATE:
		{
			lastAction.m_actionEntity->m_orientation = lastAction.m_actionEntityPreviousOrientation;
			break;
		}
		case ActionType::SCALE:
		{
			lastAction.m_actionEntity->m_scale = lastAction.m_actionEntityPreviousScale;
			break;
		}
		case ActionType::CREATE:
		{
			if (lastAction.m_createdEntities.empty())
			{
				m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
				delete lastAction.m_actionEntity;
				lastAction.m_actionEntity = nullptr;
			}
			else
			{
				for (int entityIndex = 0; entityIndex < (int)lastAction.m_createdEntities.size(); entityIndex++)
				{
					Entity*& entity = lastAction.m_createdEntities[entityIndex];

					m_game->m_currentMap->RemoveEntityFromMap(entity);
					delete entity;
					entity = nullptr;
				}
			}

			break;
		}
		case ActionType::CLONE:
		{
			m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
			break;
		}
		case ActionType::DELETE:
		{
			if (lastAction.m_createdEntities.empty())
			{
				m_game->m_currentMap->SpawnNewEntityOfType(lastAction.m_actionEntity->m_type, lastAction.m_actionEntityPreviousPosition, lastAction.m_actionEntityPreviousOrientation, lastAction.m_actionEntityPreviousScale);
			}
			else
			{
				for (int entityIndex = 0; entityIndex < (int)lastAction.m_createdEntities.size(); entityIndex++)
				{
					Entity*& entity = lastAction.m_createdEntities[entityIndex];
					Vec3 const& entityPosition = lastAction.m_createdEntityPositions[entityIndex];
					EulerAngles const& entityOrientation = lastAction.m_createdEntityOrientations[entityIndex];
					float entityScale = lastAction.m_createdEntityScales[entityIndex];

					m_game->m_currentMap->SpawnNewEntityOfType(entity->m_type, entityPosition, entityOrientation, entityScale);
				}
			}
			break;
		}
		case ActionType::LINK:
		{
			lastAction.m_activator->m_activatableUID = lastAction.m_prevLinkedActivatable;
			lastAction.m_activatable->m_activatorUID = lastAction.m_prevLinkedActivator;

			break;
		}
	}
}

void Player::ChangeState(PlayerState prevState, PlayerState newState)
{
	if (prevState == PlayerState::PLAY)
	{
		m_game->m_currentMap->ResetAllEntityStates();
	}
	if (newState == PlayerState::PLAY)
	{
		m_game->m_currentMap->SaveAllEntityStates();
		m_game->m_currentMap->SetSelectedEntity(nullptr);

		m_game->m_isMapImageVisible = false;
		m_game->m_toggleMapImageButton->SetImage("Data/Images/Image.png");

		m_mouseActionState = ActionType::NONE;
		m_leftController->m_actionState = ActionType::NONE;
		m_rightController->m_actionState = ActionType::NONE;

		if (m_isStartPlayAtCameraPosition)
		{
			m_pawn->m_position = m_position;
			m_pawn->m_orientation = m_orientation;
			m_pawn->m_velocity = Vec3::ZERO;
		}
		else
		{
			m_pawn->m_position = m_game->m_currentMap->m_playerStart->m_position;
			m_pawn->m_orientation = m_game->m_currentMap->m_playerStart->m_orientation;
			m_pawn->m_velocity = Vec3::ZERO;
		}
		m_pawn->m_health = PlayerPawn::MAX_HEALTH;
	}

	m_state = newState;
}

bool Player::Event_ChangeState(EventArgs& args)
{
	PlayerState prevState = g_app->m_game->m_player->m_state;

	PlayerState newState = (PlayerState)args.GetValue("newState", (int)PlayerState::NONE);
	if (newState == PlayerState::NONE)
	{
		return false;
	}

	g_app->m_game->m_player->ChangeState(prevState, newState);

	return true;
}

bool Player::Event_TogglePlayStartLocation(EventArgs& args)
{
	UNUSED(args);
	g_app->m_game->m_player->m_isStartPlayAtCameraPosition = !g_app->m_game->m_player->m_isStartPlayAtCameraPosition;
	return true;
}

bool Player::Event_LinkEntity(EventArgs& args)
{
	unsigned int entityUID = (unsigned int)args.GetValue("entity", (int)ENTITYUID_INVALID);
	if (entityUID == ENTITYUID_INVALID)
	{
		return false;
	}

	Entity* linkingEntity = g_app->m_game->m_currentMap->GetEntityFromUID(entityUID);
	if (!linkingEntity)
	{
		return false;
	}

	linkingEntity->m_detailsWidget->SetFocus(false)->SetVisible(false);
	if (linkingEntity->m_type == EntityType::LEVER || linkingEntity->m_type == EntityType::BUTTON)
	{
		g_app->m_game->m_currentMap->TogglePulseActivatables();
		//linkingEntity->m_linkButtonWidget->SetText("Select Activatable");
	}
	else if (linkingEntity->m_type == EntityType::DOOR || linkingEntity->m_type == EntityType::MOVING_PLATFORM)
	{
		g_app->m_game->m_currentMap->TogglePulseActivators();
		//linkingEntity->m_linkButtonWidget->SetText("Select Activator");
	}

	g_app->m_game->m_player->m_linkingEntity = linkingEntity;

	g_app->m_game->m_player->m_mouseActionState = ActionType::LINK;
	g_app->m_game->m_player->m_leftController->m_actionState = ActionType::LINK;
	g_app->m_game->m_player->m_rightController->m_actionState = ActionType::LINK;
	return true;
}
