#include "Game/HandController.hpp"

#include "Game/Activatable.hpp"
#include "Game/Activator.hpp"
#include "Game/Entity.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/UI/UISystem.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"


HandController::~HandController()
{
	delete m_sphereVBO;
	m_sphereVBO = nullptr;
}

HandController::HandController(XRHand hand, Player* owner)
	: m_hand(hand)
	, m_player(owner)
	, m_redoDoubleTapTimer(0.2f)
{
	m_diffuseShader = g_renderer->CreateOrGetShader("Data/Shaders/Diffuse", VertexType::VERTEX_PCUTBN);

	if (m_hand == XRHand::LEFT)
	{
		m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/VR_Controller_Left", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	}
	else
	{
		m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/VR_Controller_Right", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	}

	std::vector<Vertex_PCUTBN> jointVertexes;
	AddVertsForSphere3D(jointVertexes, Vec3::ZERO, 1.f, Rgba8::WHITE);
	m_sphereVBO = g_renderer->CreateVertexBuffer(jointVertexes.size() * sizeof(Vertex_PCUTBN), VertexType::VERTEX_PCUTBN);
	g_renderer->CopyCPUToGPU(jointVertexes.data(), jointVertexes.size() * sizeof(Vertex_PCUTBN), m_sphereVBO);
}

void HandController::UpdateTransform()
{
	constexpr float HAND_DISTANCE_SCALING_FACTOR = 1.2f;

	VRController const& controller = GetController();

	m_worldPositionLastFrame = m_worldPosition;
	m_orientationLastFrame = m_orientation;
	m_raycastPositionLastFrame = m_raycastPosition;

	Mat44 const& playerModelMatrix = m_player->GetModelMatrix();

	m_localPosition = controller.GetPosition_iFwd_jLeft_kUp() * HAND_DISTANCE_SCALING_FACTOR;
	m_worldPosition = playerModelMatrix.TransformPosition3D(m_localPosition);
	m_orientation = m_player->m_orientation + controller.GetOrientation_iFwd_jLeft_kUp();
	m_velocity = controller.GetLinearVelocity_iFwd_jLeft_kUp();

	m_raycastDirection = m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D();
	m_raycastPosition = m_worldPosition + m_raycastDirection * (m_entityDistance ? m_entityDistance : CONTROLLER_RAYCAST_DISTANCE);
}

void HandController::HandleInput()
{
	HandleRaycastVsMapAndUI();
	m_dropShadowVerts.clear();

	switch (m_player->m_state)
	{
		case PlayerState::EDITOR_CREATE:
		{
			HandleCreateInput();
			break;
		}
		case PlayerState::EDITOR_EDIT:
		{
			HandleEditInput();
			break;
		}
		case PlayerState::PLAY:
		{
			HandlePlayInput();
			break;
		}
	}

	// Controller undo/redo keybinds
	if ((GetController().WasGripJustPressed() && m_player->m_state == PlayerState::EDITOR_CREATE) ||
		(GetController().WasGripJustPressed() && m_player->m_state == PlayerState::EDITOR_EDIT && !m_hoveredEntity))
	{
		if (m_redoDoubleTapTimer.IsStopped())
		{
			m_redoDoubleTapTimer.Start();
		}
		else
		{
			RedoLastAction();
			m_redoDoubleTapTimer.Stop();
		}
	}
	if (m_redoDoubleTapTimer.HasDurationElapsed())
	{
		m_redoDoubleTapTimer.Stop();
		UndoLastAction();
	}
}

void HandController::Render() const
{
	VRController const& controller = GetController();

	if (!controller.IsActive())
	{
		return;
	}

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

	Rgba8 controllerColor = Rgba8::BROWN;
	Mat44 controllerTransform = Mat44::CreateTranslation3D(m_worldPosition);
	EulerAngles handOrientation = m_orientation;
	handOrientation.m_rollDegrees += m_hand == XRHand::RIGHT ? -90.f : 90.f;
	controllerTransform.Append(handOrientation.GetAsMatrix_iFwd_jLeft_kUp());
	Vec3 const& controllerFwd = controllerTransform.GetIBasis3D();

	if (m_player->m_game->m_state != GameState::GAME || m_player->m_state != PlayerState::PLAY)
	{
		std::vector<Vertex_PCU> rayVerts;
		AddVertsForGradientLineSegment3D(rayVerts, m_worldPosition + controllerFwd * 0.25f, m_worldPosition + controllerFwd * Game::SCREEN_QUAD_DISTANCE, 0.002f, Rgba8(255, 255, 255, 127), Rgba8::TRANSPARENT_WHITE, AABB2::ZERO_TO_ONE, 16);
		g_renderer->BeginRenderEvent("Controller Ray");
		g_renderer->BindShader(nullptr);
		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(nullptr);
		g_renderer->SetModelConstants();
		g_renderer->DrawVertexArray(rayVerts);
		g_renderer->EndRenderEvent("Controller Ray");
	}

	if ((m_player->m_game->m_state != GameState::GAME) || (m_player->m_state != PlayerState::PLAY))
	{
		Mat44 modelTransform = Mat44::CreateTranslation3D(m_worldPosition);
		modelTransform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());

		g_renderer->BeginRenderEvent("Controller Model");
		g_renderer->BindShader(m_diffuseShader);
		g_renderer->SetBlendMode(BlendMode::OPAQUE);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(nullptr);
		g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
		g_renderer->SetModelConstants(modelTransform);
		g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer(), m_model->GetIndexBuffer(), m_model->GetIndexCount());
		g_renderer->EndRenderEvent("Controller Model");
	}
	else
	{
		controllerTransform.AppendScaleUniform3D(CONTROLLER_SCALE);

		g_renderer->BeginRenderEvent("Controller Spheres");
		// Hand Base
		{
			Vec3 handBasePosition(0.f, 0.f, 0.f);

			if (m_hand == XRHand::LEFT)
			{
				handBasePosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 handBaseTransform = controllerTransform;
			handBaseTransform.AppendTranslation3D(handBasePosition);
			g_renderer->SetModelConstants(handBaseTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Palm Left Sphere
		{
			Vec3 palmLeftSpherePosition(1.f, 1.f, -0.5f);

			if (m_hand == XRHand::LEFT)
			{
				palmLeftSpherePosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 palmLeftSphereTransform = controllerTransform;
			palmLeftSphereTransform.AppendTranslation3D(palmLeftSpherePosition);
			palmLeftSphereTransform.AppendScaleUniform3D(0.8f);
			g_renderer->SetModelConstants(palmLeftSphereTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Palm Center Sphere
		{
			Vec3 palmCenterSpherePosition(1.f, 0.f, -1.f);

			if (m_hand == XRHand::LEFT)
			{
				palmCenterSpherePosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 palmCenterSphereTransform = controllerTransform;
			palmCenterSphereTransform.AppendTranslation3D(palmCenterSpherePosition);
			palmCenterSphereTransform.AppendScaleUniform3D(0.8f);
			g_renderer->SetModelConstants(palmCenterSphereTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Palm Right Sphere
		{
			Vec3 palmRightSpherePosition(1.f, -1.f, -0.5f);

			if (m_hand == XRHand::LEFT)
			{
				palmRightSpherePosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 palmRightSphereTransform = controllerTransform;
			palmRightSphereTransform.AppendTranslation3D(palmRightSpherePosition);
			palmRightSphereTransform.AppendScaleUniform3D(0.8f);
			g_renderer->SetModelConstants(palmRightSphereTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Fingers Base Sphere
		{
			Vec3 fingersBaseSphere(2.f, 0.f, 0.f);

			if (m_hand == XRHand::LEFT)
			{
				fingersBaseSphere *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 fingersBaseSphereTransform = controllerTransform;
			fingersBaseSphereTransform.AppendTranslation3D(fingersBaseSphere);
			fingersBaseSphereTransform.AppendScaleUniform3D(0.8f);
			g_renderer->SetModelConstants(fingersBaseSphereTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Thumb
		{
			Vec3 thumbTipPosition(1.3f, -2.4f, 1.5f);

			if (controller.IsJoystickTouched())
			{
				Vec2 joystickPosition = controller.GetJoystick().GetPosition();
				thumbTipPosition = Vec3(1.2f + joystickPosition.y * 0.2f, -1.5f, 1.5f - joystickPosition.x * 0.2f);
			}
			else if (controller.IsSelectButtonTouched())
			{
				thumbTipPosition = Vec3(1.f, -1.3f, 1.7f);
			}
			else if (controller.IsBackButtonTouched())
			{
				thumbTipPosition = Vec3(1.2f, -1.3f, 1.8f);
			}

			if (m_hand == XRHand::LEFT)
			{
				thumbTipPosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 thumbTransform = controllerTransform;
			thumbTransform.AppendTranslation3D(thumbTipPosition);
			thumbTransform.AppendScaleUniform3D(0.4f);
			g_renderer->SetModelConstants(thumbTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Index
		{
			Vec3 indexTipPosition(4.8f, -1.f, 1.6f);

			if (controller.IsTriggerTouched())
			{
				float triggerValue = controller.GetTrigger();
				indexTipPosition = Vec3(4.f - triggerValue, -0.6f + 0.2f * triggerValue, 2.f + 0.5f * triggerValue);
			}

			if (m_hand == XRHand::LEFT)
			{
				indexTipPosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 indexTransform = controllerTransform;
			indexTransform.AppendTranslation3D(indexTipPosition);
			indexTransform.AppendScaleUniform3D(0.39f);
			g_renderer->SetModelConstants(indexTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Middle
		float gripValue = controller.GetGrip();
		{
			Vec3 middleTipPosition(3.6f - gripValue * 0.6f, 0.4f, 2.4f - gripValue * 0.4f);

			if (m_hand == XRHand::LEFT)
			{
				middleTipPosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 middleTransform = controllerTransform;
			middleTransform.AppendTranslation3D(middleTipPosition);
			middleTransform.AppendScaleUniform3D(0.39f);
			g_renderer->SetModelConstants(middleTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Ring
		{
			Vec3 ringTipPosition(3.4f - gripValue * 0.4f, 1.f, 2.f - gripValue * 0.2f);

			if (m_hand == XRHand::LEFT)
			{
				ringTipPosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 ringTransform = controllerTransform;
			ringTransform.AppendTranslation3D(ringTipPosition);
			ringTransform.AppendScaleUniform3D(0.39f);
			g_renderer->SetModelConstants(ringTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}

		// Pinky
		{
			Vec3 pinkyTipPosition(3.2f - gripValue * 0.2f, 1.6f, 1.6f - gripValue * 0.1f);

			if (m_hand == XRHand::LEFT)
			{
				pinkyTipPosition *= Vec3(1.f, -1.f, 1.f);
			}

			g_renderer->BindShader(m_diffuseShader);
			Mat44 pinkyTransform = controllerTransform;
			pinkyTransform.AppendTranslation3D(pinkyTipPosition);
			pinkyTransform.AppendScaleUniform3D(0.38f);
			g_renderer->SetModelConstants(pinkyTransform, controllerColor);
			g_renderer->SetBlendMode(BlendMode::OPAQUE);
			g_renderer->SetDepthMode(DepthMode::ENABLED);
			g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
			g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
			g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
			g_renderer->BindTexture(nullptr);
			g_renderer->SetLightConstants(SUN_DIRECTION, SUN_INTENSITY, 1.f - SUN_INTENSITY);
			g_renderer->DrawVertexBuffer(m_sphereVBO, (int)m_sphereVBO->m_size / sizeof(Vertex_PCUTBN));
		}
		g_renderer->EndRenderEvent("Controller Spheres");
	}

	RenderFakeEntitiesForSpawn();
}

void HandController::HandleCreateInput()
{
	VRController const& controller = GetController();
	float deltaSeconds = m_player->m_game->m_clock.GetDeltaSeconds();

	if (m_selectedEntityType != EntityType::NONE && controller.IsJoystickPressed())
	{
		m_entityDistance += controller.GetJoystick().GetPosition().y * ENTITY_DISTANCE_ADJUST_SPEED * deltaSeconds;
		m_entityDistance = GetClamped(m_entityDistance, 0.5f, 10.f);
	}

	if (controller.WasSelectButtonJustPressed())
	{
		m_selectedEntityType = EntityType(((int)m_selectedEntityType + 1) % (int)EntityType::NUM);
		m_selectedEntity = m_player->m_game->m_currentMap->CreateEntityOfType(m_selectedEntityType, Vec3::ZERO, EulerAngles::ZERO, 1.f);
	}
	if (controller.WasBackButtonJustPressed())
	{
		m_selectedEntityType = EntityType((int)m_selectedEntityType - 1);
		if ((int)m_selectedEntityType < 0)
		{
			m_selectedEntityType = EntityType((int)EntityType::NUM - 1);
		}
		m_selectedEntity = m_player->m_game->m_currentMap->CreateEntityOfType(m_selectedEntityType, Vec3::ZERO, EulerAngles::ZERO, 1.f);
	}
	if (controller.WasTriggerJustReleased())
	{
		SpawnEntities();
	}
	if (controller.GetTrigger())
	{
		m_entitySpawnEndPosition = m_raycastPosition;
	}
	else
	{
		m_entitySpawnStartPosition = m_raycastPosition;
		m_entitySpawnEndPosition = m_raycastPosition;
	}
}

void HandController::HandleEditInput()
{
	VRController const& controller = GetController();
	if (!controller.IsActive())
	{
		return;
	}

	float deltaSeconds = m_player->m_game->m_clock.GetDeltaSeconds();

	//if (controller.WasThumbRestJustTouched())
	//{
	//	m_axisLockDirection = AxisLockDirection(((int)m_axisLockDirection + 1) % (int)AxisLockDirection::NUM);
	//}
	if (controller.WasGripJustPressed())
	{
		HandController* otherController = GetOtherHandController();
		if (otherController->m_actionState == ActionType::TRANSLATE && m_hoveredEntity == otherController->m_selectedEntity)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			m_actionState = ActionType::SCALE;
			m_selectedEntityType = m_hoveredEntity->m_type;
			m_selectedEntityPosition = m_hoveredEntity->m_position;
			m_selectedEntityOrientation = m_hoveredEntity->m_orientation;
			m_selectedEntityScale = m_hoveredEntity->m_scale;
			m_selectedEntity = m_hoveredEntity;

			otherController->m_actionState = ActionType::SCALE;
			
			Action scaleAction;
			scaleAction.m_actionType = ActionType::SCALE;
			scaleAction.m_actionEntity = m_hoveredEntity;
			scaleAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			scaleAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			scaleAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(scaleAction);
			m_player->m_game->m_currentMap->m_isUnsaved = true;

			m_isResponsibleForScaling = true;
		}
		if (m_actionState == ActionType::NONE || m_actionState == ActionType::SELECT)
		{
			if (m_hoveredEntity)
			{
				GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

				m_actionState = ActionType::TRANSLATE;
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
				m_player->m_game->m_currentMap->m_isUnsaved = true;
			}
		}
	}
	if (controller.WasTriggerJustPressed())
	{
		if (m_actionState == ActionType::NONE || m_actionState == ActionType::SELECT)
		{
			if (m_hoveredEntity)
			{
				GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
				GetOtherController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

				m_actionState = ActionType::CLONE;
				m_selectedEntityType = m_hoveredEntity->m_type;
				m_selectedEntityPosition = m_hoveredEntity->m_position;
				m_selectedEntityOrientation = m_hoveredEntity->m_orientation;
				m_selectedEntityScale = m_hoveredEntity->m_scale;
				m_selectedEntity = m_player->m_game->m_currentMap->SpawnNewEntityOfType(m_selectedEntityType, m_selectedEntityPosition, m_selectedEntityOrientation, m_selectedEntityScale);

				Action cloneAction;
				cloneAction.m_actionType = ActionType::CLONE;
				cloneAction.m_actionEntity = m_selectedEntity;
				m_undoActionStack.push(cloneAction);
				m_player->m_game->m_currentMap->m_isUnsaved = true;
			}
		}
		else if (m_actionState == ActionType::TRANSLATE)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			m_actionState = ActionType::ROTATE;
			Action rotateAction;
			rotateAction.m_actionType = ActionType::ROTATE;
			rotateAction.m_actionEntity = m_selectedEntity;
			rotateAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			m_undoActionStack.push(rotateAction);
			m_player->m_game->m_currentMap->m_isUnsaved = true;
		}
	}
	if (controller.WasGripJustReleased())
	{
		if (m_actionState == ActionType::TRANSLATE || m_actionState == ActionType::ROTATE)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			m_selectedEntityType = EntityType::NONE;
			m_selectedEntity = nullptr;
			m_player->m_game->m_currentMap->SetSelectedEntity(nullptr);
			m_actionState = ActionType::NONE;
		}
		else if (m_actionState == ActionType::SCALE)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
			GetOtherController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			HandController* otherController = GetOtherHandController();
			otherController->m_actionState = ActionType::NONE;
			otherController->m_selectedEntityType = EntityType::NONE;
			otherController->m_selectedEntity = nullptr;
			m_selectedEntityType = EntityType::NONE;
			m_selectedEntity = nullptr;
			m_actionState = ActionType::NONE;
			m_player->m_game->m_currentMap->SetSelectedEntity(nullptr);
			m_isResponsibleForScaling = false;
		}
	}
	if (controller.WasTriggerJustReleased())
	{
		if (m_actionState == ActionType::CLONE)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			m_selectedEntityType = EntityType::NONE;
			m_selectedEntity = nullptr;
			m_player->m_game->m_currentMap->SetSelectedEntity(nullptr);
			m_actionState = ActionType::NONE;
		}
		else if (m_actionState == ActionType::ROTATE)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			m_actionState = ActionType::TRANSLATE;
		}
	}
	if (controller.WasBackButtonJustPressed() && (m_actionState == ActionType::NONE || m_actionState == ActionType::SELECT))
	{
		if (m_selectedEntity)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			Action deleteAction;
			deleteAction.m_actionType = ActionType::DELETE;
			deleteAction.m_actionEntity = m_selectedEntity;
			deleteAction.m_actionEntityPreviousPosition = m_selectedEntity->m_position;
			deleteAction.m_actionEntityPreviousOrientation = m_selectedEntity->m_orientation;
			deleteAction.m_actionEntityPreviousScale = m_selectedEntity->m_scale;
			m_undoActionStack.push(deleteAction);
			m_player->m_game->m_currentMap->m_isUnsaved = true;

			m_player->m_game->m_currentMap->RemoveEntityFromMap(m_selectedEntity);
			m_selectedEntity = nullptr;
			m_selectedEntityPosition = Vec3::ZERO;
			m_selectedEntityOrientation = EulerAngles::ZERO;
			m_selectedEntityScale = 1.f;
			m_actionState = ActionType::NONE;
		}
		if (m_hoveredEntity)
		{
			GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

			Action deleteAction;
			deleteAction.m_actionType = ActionType::DELETE;
			deleteAction.m_actionEntity = m_hoveredEntity;
			deleteAction.m_actionEntityPreviousPosition = m_hoveredEntity->m_position;
			deleteAction.m_actionEntityPreviousOrientation = m_hoveredEntity->m_orientation;
			deleteAction.m_actionEntityPreviousScale = m_hoveredEntity->m_scale;
			m_undoActionStack.push(deleteAction);
			m_player->m_game->m_currentMap->m_isUnsaved = true;

			m_player->m_game->m_currentMap->RemoveEntityFromMap(m_hoveredEntity);
		}
	}
	if (controller.GetGrip())
	{
		if (m_selectedEntity)
		{
			if (m_actionState == ActionType::TRANSLATE)
			{
				if (controller.IsJoystickPressed())
				{
					m_entityDistance += controller.GetJoystick().GetPosition().y * ENTITY_DISTANCE_ADJUST_SPEED * deltaSeconds;
					m_entityDistance = GetClamped(m_entityDistance, 0.5f, 10.f);
				}

				Vec3 controllerRaycastDelta = m_raycastPosition - m_raycastPositionLastFrame;
				TranslateEntity(m_selectedEntity, controllerRaycastDelta);
				SnapEntityToGrid(m_selectedEntity, m_raycastPosition);

				ArchiLeapRaycastResult3D groundwardRaycastResult = m_player->m_game->m_currentMap->RaycastVsEntities(m_selectedEntity->m_position, Vec3::GROUNDWARD, 100.f, m_selectedEntity);
				if (groundwardRaycastResult.m_didImpact)
				{
					std::vector<Vertex_PCU> dropShadowVerts;
					float shadowOpacityFloat = RangeMapClamped(groundwardRaycastResult.m_impactDistance, 0.f, 10.f, 0.5f, 0.f);
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

				if (controller.WasBackButtonJustPressed())
				{
					if (m_selectedEntity)
					{
						GetController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);

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
							ArchiLeapRaycastResult3D downwardRaycastResult = m_player->m_game->m_currentMap->RaycastVsEntities(raycastPoints[raycastPointIndex], Vec3::GROUNDWARD, 100.f, m_selectedEntity);
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
						m_player->m_game->m_currentMap->SetSelectedEntity(nullptr);
						m_actionState = ActionType::NONE;
					}
				}
			}
			else if (m_actionState == ActionType::ROTATE)
			{
				m_selectedEntity->m_orientation.m_yawDegrees += m_orientation.m_yawDegrees - m_orientationLastFrame.m_yawDegrees;
			}
			else if (m_actionState == ActionType::SCALE && m_isResponsibleForScaling)
			{
				HandController* otherHandController = GetOtherHandController();
				m_selectedEntity->m_scale += GetDistance3D(m_worldPosition, otherHandController->m_worldPosition) - GetDistance3D(m_worldPositionLastFrame, otherHandController->m_worldPositionLastFrame);
			}
		}
	}
	if (controller.GetTrigger() && m_actionState == ActionType::CLONE)
	{
		if (m_selectedEntity)
		{
			if (controller.IsJoystickPressed())
			{
				m_entityDistance += controller.GetJoystick().GetPosition().y * ENTITY_DISTANCE_ADJUST_SPEED * deltaSeconds;
				m_entityDistance = GetClamped(m_entityDistance, 0.5f, 10.f);
			}

			Vec3 controllerRaycastDelta = m_raycastPosition - m_raycastPositionLastFrame;
			TranslateEntity(m_selectedEntity, controllerRaycastDelta);
			SnapEntityToGrid(m_selectedEntity, m_raycastPosition);

			ArchiLeapRaycastResult3D groundwardRaycastResult = m_player->m_game->m_currentMap->RaycastVsEntities(m_selectedEntity->m_position, Vec3::GROUNDWARD, 100.f, m_selectedEntity);
			if (groundwardRaycastResult.m_didImpact)
			{
				std::vector<Vertex_PCU> dropShadowVerts;
				float shadowOpacityFloat = RangeMapClamped(groundwardRaycastResult.m_impactDistance, 0.f, 10.f, 0.5f, 0.f);
				unsigned char shadowOpacity = DenormalizeByte(shadowOpacityFloat);
				if (shadowOpacityFloat == 1.f)
				{
					shadowOpacity = 0;
				}
				Vec3 const dropShadowBL = Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowBR = Vec3::EAST * m_selectedEntity->m_localBounds.m_mins.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowTR = Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_mins.y * m_selectedEntity->m_scale;
				Vec3 const dropShadowTL = Vec3::EAST * m_selectedEntity->m_localBounds.m_maxs.x * m_selectedEntity->m_scale + Vec3::NORTH * m_selectedEntity->m_localBounds.m_maxs.y * m_selectedEntity->m_scale;
				AddVertsForQuad3D(m_dropShadowVerts, dropShadowBL, dropShadowBR, dropShadowTR, dropShadowTL, Rgba8(0, 0, 0, shadowOpacity));
				TransformVertexArrayXY3D(m_dropShadowVerts, 1.f, m_selectedEntity->m_orientation.m_yawDegrees, groundwardRaycastResult.m_impactPosition.GetXY());
			}
		}
	}
	if (controller.WasSelectButtonJustPressed())
	{
		if (m_selectedEntity && m_actionState == ActionType::SELECT)
		{
			m_actionState = ActionType::NONE;
			m_selectedEntity = nullptr;
			m_player->m_game->m_currentMap->SetSelectedEntity(m_hoveredEntity);
		}
		else if (m_actionState == ActionType::LINK)
		{
			Activator* activator = nullptr;
			Activatable* activatable = nullptr;
			if (m_selectedEntityType == EntityType::BUTTON || m_selectedEntityType == EntityType::LEVER)
			{
				activator = (Activator*)m_selectedEntity;
			}
			if (m_hoveredEntity->m_type == EntityType::BUTTON || m_hoveredEntity->m_type == EntityType::LEVER)
			{
				activator = (Activator*)m_hoveredEntity;
			}
			if (m_selectedEntityType == EntityType::DOOR || m_selectedEntityType == EntityType::MOVING_PLATFORM)
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
				m_player->m_game->m_currentMap->m_isUnsaved = true;
			}

			m_player->m_game->m_currentMap->LinkEntities(m_hoveredEntity, m_selectedEntity);
			m_player->m_linkingEntity = nullptr;
			m_selectedEntity = nullptr;
			m_actionState = ActionType::NONE;
			GetOtherHandController()->m_selectedEntity = nullptr;
			GetOtherHandController()->m_actionState = ActionType::NONE;
			m_player->m_selectedEntity = nullptr;
			m_player->m_mouseActionState = ActionType::NONE;
			m_player->m_game->m_currentMap->SetSelectedEntity(nullptr);	
		}
		if (m_hoveredEntity)
		{
			m_actionState = ActionType::SELECT;
			m_selectedEntity = m_hoveredEntity;
			m_player->m_game->m_currentMap->SetSelectedEntity(m_hoveredEntity);
		}
	}
}

void HandController::HandlePlayInput()
{
	VRController const& controller = GetController();

	if ((m_player->m_pawn->m_isHangingByLeftHand && m_hand == XRHand::LEFT) || (m_player->m_pawn->m_isHangingByRightHand && m_hand == XRHand::RIGHT))
	{
		float handDeltaZ = m_worldPositionLastFrame.z - m_worldPosition.z;
		m_player->m_pawn->AddForce(Vec3::SKYWARD * (GRAVITY + handDeltaZ * 20.f) * PlayerPawn::MASS);
	}

	if (controller.WasGripJustReleased())
	{
		if (m_hand == XRHand::LEFT && m_player->m_pawn->m_isHangingByLeftHand)
		{
			m_player->m_pawn->m_isHangingByLeftHand = false;
		}
		else if (m_hand == XRHand::RIGHT && m_player->m_pawn->m_isHangingByRightHand)
		{
			m_player->m_pawn->m_isHangingByRightHand = false;
		}
	}
}

void HandController::HandleRaycastVsMapAndUI()
{
	HandleRaycastVsScreen();
	HandleButtonClicks();
	HandleRaycastVsMap();
}

void HandController::HandleRaycastVsScreen()
{
	if (m_actionState == ActionType::TRANSLATE || m_actionState == ActionType::ROTATE || m_actionState == ActionType::SCALE)
	{
		return;
	}

	AABB2 screenBounds(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * WINDOW_ASPECT, SCREEN_SIZE_Y));
	ArchiLeapRaycastResult3D raycastVsUIResult = m_player->m_game->RaycastVsScreen(m_worldPosition, m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D(), CONTROLLER_RAYCAST_DISTANCE);
	if (raycastVsUIResult.m_didImpact)
	{
		m_hoveredWidget = raycastVsUIResult.m_impactWidget;
		if (m_hoveredWidget)
		{
			g_ui->SetSelectedInputField(nullptr);
			m_hoveredWidget->m_isVRHovered = true;
		}
	}
	else
	{
		m_hoveredWidget = nullptr;
	}
}

void HandController::HandleButtonClicks()
{
	if (!m_hoveredWidget)
	{
		return;
	}

	VRController const& controller = GetController();
	if (controller.WasTriggerJustPressed())
	{
		m_hoveredWidget->m_isVRClicked = true;
	}
	if (controller.WasTriggerJustReleased())
	{
		m_hoveredWidget->m_isVRClicked = false;

		if (m_hoveredWidget->m_isTextInputField)
		{
			m_hoveredWidget->m_previousText = m_hoveredWidget->m_text;
			m_hoveredWidget->m_text = "";
			m_hoveredWidget->m_uiSystem->SetSelectedInputField(m_hoveredWidget);
			m_hoveredWidget->m_blinkingCaretTimer->Start();
		}
		else
		{
			FireEvent(m_hoveredWidget->m_clickEventName);
		}
	}
}

void HandController::HandleRaycastVsMap()
{
	if (!m_player->m_game->m_currentMap)
	{
		return;
	}
	if (m_hoveredWidget)
	{
		m_hoveredEntity = nullptr;
		m_player->m_game->m_currentMap->SetHoveredEntityForHand(m_hand, m_hoveredEntity);
		return;
	}
	if (m_actionState != ActionType::NONE && m_actionState != ActionType::LINK)
	{
		return;
	}

	ArchiLeapRaycastResult3D raycastVsMapResult = m_player->m_game->m_currentMap->RaycastVsEntities(m_worldPosition, m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D(), CONTROLLER_RAYCAST_DISTANCE);
	if (raycastVsMapResult.m_didImpact)
	{
		if (m_player->m_state == PlayerState::EDITOR_EDIT)
		{
			m_player->m_game->m_currentMap->SetHoveredEntityForHand(m_hand, raycastVsMapResult.m_impactEntity);
			m_hoveredEntity = raycastVsMapResult.m_impactEntity;
			m_entityDistance = raycastVsMapResult.m_impactDistance;
		}
		else if (m_player->m_state == PlayerState::EDITOR_CREATE)
		{
			m_entityDistance = raycastVsMapResult.m_impactDistance;
		}
	}
	else
	{
		m_player->m_game->m_currentMap->SetHoveredEntityForHand(m_hand, nullptr);
		m_hoveredEntity = nullptr;
		m_entityDistance = 0.f;
	}
}

void HandController::SpawnEntities()
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
				Entity* spawnedEntity = m_player->m_game->m_currentMap->SpawnNewEntityOfType(m_selectedEntityType, Vec3((float)xPos, (float)yPos, (float)zPos), EulerAngles::ZERO, 1.f);
				spawnAction.m_createdEntities.push_back(spawnedEntity);
			}
		}
	}

	m_undoActionStack.push(spawnAction);
	m_player->m_game->m_currentMap->m_isUnsaved = true;

	m_selectedEntity = nullptr;
	m_selectedEntityType = EntityType::NONE;
	m_entitySpawnStartPosition = m_raycastPosition;
	m_entitySpawnEndPosition = m_raycastPosition;
}

void HandController::TranslateEntity(Entity* entity, Vec3 const& translation) const
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

void HandController::SnapEntityToGrid(Entity* entity, Vec3& controllerRaycast) const
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

void HandController::RenderFakeEntitiesForSpawn() const
{
	if (m_player->m_game->m_state != GameState::GAME)
	{
		return;
	}
	if (m_player->m_state != PlayerState::EDITOR_CREATE)
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

				Mat44 transform = Mat44::CreateTranslation3D(entityPosition);
				transform.Append(m_selectedEntityOrientation.GetAsMatrix_iFwd_jLeft_kUp());
				transform.AppendScaleUniform3D(m_player->m_game->m_currentMap->GetDefaultEntityScaleForType(m_selectedEntityType));

				g_renderer->SetBlendMode(BlendMode::ALPHA);
				g_renderer->SetDepthMode(DepthMode::DISABLED);
				g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
				g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
				g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
				g_renderer->BindTexture(nullptr);
				g_renderer->BindShader(nullptr);
				g_renderer->SetModelConstants(transform, Rgba8(255, 255, 255, 195));
				g_renderer->DrawIndexBuffer(m_selectedEntity->m_model->GetVertexBuffer(), m_selectedEntity->m_model->GetIndexBuffer(), m_selectedEntity->m_model->GetIndexCount());

				ArchiLeapRaycastResult3D groundwardRaycastResult = m_player->m_game->m_currentMap->RaycastVsEntities(entityPosition, Vec3::GROUNDWARD, 100.f, m_selectedEntity);
				if (groundwardRaycastResult.m_didImpact)
				{
					std::vector<Vertex_PCU> dropShadowVerts;
					float shadowOpacityFloat = RangeMapClamped(groundwardRaycastResult.m_impactDistance, 0.f, 10.f, 0.5f, 0.f);
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

VRController& HandController::GetController()
{
	if (m_hand == XRHand::LEFT)
	{
		return g_openXR->GetLeftController();
	}
	if (m_hand == XRHand::RIGHT)
	{
		return g_openXR->GetRightController();
	}

	ERROR_AND_DIE("Attempted GetController on HandController with invalid hand!");
}

VRController const& HandController::GetController() const
{
	if (m_hand == XRHand::LEFT)
	{
		return g_openXR->GetLeftController();
	}
	if (m_hand == XRHand::RIGHT)
	{
		return g_openXR->GetRightController();
	}

	ERROR_AND_DIE("Attempted GetController on HandController with invalid hand!");
}

VRController& HandController::GetOtherController()
{
	if (m_hand == XRHand::LEFT)
	{
		return g_openXR->GetRightController();
	}
	if (m_hand == XRHand::RIGHT)
	{
		return g_openXR->GetLeftController();
	}

	ERROR_AND_DIE("Attempted GetOtherController on HandController with invalid hand!");
}

VRController const& HandController::GetOtherController() const
{
	if (m_hand == XRHand::LEFT)
	{
		return g_openXR->GetRightController();
	}
	if (m_hand == XRHand::RIGHT)
	{
		return g_openXR->GetLeftController();
	}

	ERROR_AND_DIE("Attempted GetOtherController on HandController with invalid hand!");
}

HandController* HandController::GetOtherHandController() const
{
	if (m_hand == XRHand::LEFT)
	{
		return m_player->m_rightController;
	}
	else if (m_hand == XRHand::RIGHT)
	{
		return m_player->m_leftController;
	}

	return nullptr;
}

Vec3 const HandController::GetLinearVelocity() const
{
	return m_player->GetModelMatrix().TransformVectorQuantity3D(GetController().GetLinearVelocity_iFwd_jLeft_kUp());
}

void HandController::UndoLastAction()
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

				m_player->m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
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

					m_player->m_game->m_currentMap->RemoveEntityFromMap(entity);
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

			m_player->m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
			break;
		}
		case ActionType::DELETE:
		{
			Entity* createdEntity = m_player->m_game->m_currentMap->SpawnNewEntityOfType(lastAction.m_actionEntity->m_type, lastAction.m_actionEntityPreviousPosition, lastAction.m_actionEntityPreviousOrientation, lastAction.m_actionEntityPreviousScale);

			Action redoAction;
			redoAction.m_actionType = ActionType::CREATE;
			redoAction.m_actionEntity = createdEntity;
			m_redoActionStack.push(redoAction);

			break;
		}
	}
}

void HandController::RedoLastAction()
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
				m_player->m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
				lastAction.m_actionEntity = nullptr;
			}
			else
			{
				for (int entityIndex = 0; entityIndex < (int)lastAction.m_createdEntities.size(); entityIndex++)
				{
					Entity*& entity = lastAction.m_createdEntities[entityIndex];

					m_player->m_game->m_currentMap->RemoveEntityFromMap(entity);
					entity = nullptr;
				}
			}

			break;
		}
		case ActionType::CLONE:
		{
			m_player->m_game->m_currentMap->RemoveEntityFromMap(lastAction.m_actionEntity);
			break;
		}
		case ActionType::DELETE:
		{
			if (lastAction.m_createdEntities.empty())
			{
				m_player->m_game->m_currentMap->SpawnNewEntityOfType(lastAction.m_actionEntity->m_type, lastAction.m_actionEntityPreviousPosition, lastAction.m_actionEntityPreviousOrientation, lastAction.m_actionEntityPreviousScale);
			}
			else
			{
				for (int entityIndex = 0; entityIndex < (int)lastAction.m_createdEntities.size(); entityIndex++)
				{
					Entity*& entity = lastAction.m_createdEntities[entityIndex];
					Vec3 const& entityPosition = lastAction.m_createdEntityPositions[entityIndex];
					EulerAngles const& entityOrientation = lastAction.m_createdEntityOrientations[entityIndex];
					float entityScale = lastAction.m_createdEntityScales[entityIndex];

					m_player->m_game->m_currentMap->SpawnNewEntityOfType(entity->m_type, entityPosition, entityOrientation, entityScale);
				}
			}
			break;
		}
	}
}
