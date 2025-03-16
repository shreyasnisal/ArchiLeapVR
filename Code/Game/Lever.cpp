#include "Game/Lever.hpp"

#include "Game/Activatable.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/HandController.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"
#include "Engine/VirtualReality/VRController.hpp"


Lever::Lever(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Activator(map, uid, position, orientation, scale, EntityType::LEVER)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Activators/lever", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_scale = MODEL_SCALE;
	m_localBounds = AABB3(Vec3(-0.1f, -0.3f, 0.f), Vec3(0.1f, 0.3f, 1.f));
	m_crankSFX = g_audio->CreateOrGetSound("Data/SFX/Lever.wav", true);
}

//---------------------------------------------------------------------------------------

void Lever::Update()
{
	Entity::Update();

	if (m_value <= -0.9f && m_valueLastFrame > -0.9f)
	{
		m_value = -1.f;
		g_audio->StartSoundAt(m_crankSFX, m_position);

		if (m_isLeftHandGripped)
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
		else if (m_isRightHandGripped)
		{
			g_openXR->GetRightController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
	}
	else if (m_value > -0.9f && m_valueLastFrame <= -0.9f)
	{
		g_audio->StartSoundAt(m_crankSFX, m_position);
		if (m_isLeftHandGripped)
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
		else if (m_isRightHandGripped)
		{
			g_openXR->GetRightController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
	}

	if (m_value >= 0.9f && m_valueLastFrame < 0.9f)
	{
		g_audio->StartSoundAt(m_crankSFX, m_position);
		if (m_isLeftHandGripped)
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
		else if (m_isRightHandGripped)
		{
			g_openXR->GetRightController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}

		m_value = 1.f;
		Entity* activatableEntity = m_map->GetEntityFromUID(m_activatableUID);
		if (activatableEntity)
		{
			Activatable* activatable = dynamic_cast<Activatable*>(activatableEntity);
			if (activatable)
			{
				activatable->Activate();
			}
		}
	}
	else if (m_value < 0.9f && m_valueLastFrame >= 0.9f)
	{
		g_audio->StartSoundAt(m_crankSFX, m_position);
		if (m_isLeftHandGripped)
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}
		else if (m_isRightHandGripped)
		{
			g_openXR->GetRightController().ApplyHapticFeedback(CONTROLLER_VIBRATION_AMPLITUDE, CONTROLLER_VIBRATION_DURATION);
		}

		Entity* activatableEntity = m_map->GetEntityFromUID(m_activatableUID);
		if (activatableEntity)
		{
			Activatable* activatable = dynamic_cast<Activatable*>(activatableEntity);
			if (activatable)
			{
				activatable->Deactivate();
			}
		}
	}

	m_valueLastFrame = m_value;
}

//---------------------------------------------------------------------------------------

void Lever::Render() const
{
	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	Mat44 handleTransform = Mat44::CreateTranslation3D(m_position);
	handleTransform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	handleTransform.AppendScaleUniform3D(m_scale);
	handleTransform.AppendXRotation(RangeMapClamped(m_value, -1.f, 1.f, -45.f, 45.f));

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);

	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer("lever"), m_model->GetIndexBuffer("lever"), m_model->GetIndexCount("lever"));

	g_renderer->SetModelConstants(handleTransform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer("handle"), m_model->GetIndexBuffer("handle"), m_model->GetIndexCount("handle"));
}

//---------------------------------------------------------------------------------------

void Lever::HandlePlayerInteraction()
{
	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();

	Player* player = m_map->m_game->m_player;
	Vec3& playerLeftControllerPosition = player->m_leftController->m_worldPosition;
	Vec3& playerRightControllerPosition = player->m_rightController->m_worldPosition;

	if (player->m_state == PlayerState::PLAY)
	{
		if (GetDistanceSquared3D(playerLeftControllerPosition, GetHandleWorldPosition()) < 0.1f)
		{
			m_shouldCheckForLeftHandGrip = true;
		}
		else
		{
			m_shouldCheckForLeftHandGrip = false;
		}
		if (GetDistanceSquared3D(playerRightControllerPosition, GetHandleWorldPosition()) < 0.1f)
		{
			m_shouldCheckForRightHandGrip = true;
		}
		else
		{
			m_shouldCheckForRightHandGrip = false;
		}

		if (m_shouldCheckForLeftHandGrip && m_previousFrameLeftHandGripValue == 0.f)
		{
			VRController leftController = g_openXR->GetLeftController();
			float leftHandGrip = leftController.GetGrip();
			if (leftHandGrip > 0.f)
			{
				m_isLeftHandGripped = true;
				m_shouldCheckForLeftHandGrip = false;
				Vec3 leftControllerFwd, leftControllerLeft, leftControllerUp;
				leftController.GetOrientation_iFwd_jLeft_kUp().GetAsVectors_iFwd_jLeft_kUp(leftControllerFwd, leftControllerLeft, leftControllerUp);
				playerLeftControllerPosition = GetHandleWorldPosition() - leftControllerFwd * 0.075f + leftControllerLeft * 0.125f;
				player->m_leftController->m_orientation = m_orientation;
			}
		}

		if (m_shouldCheckForRightHandGrip && m_previousFrameRightHandGripValue == 0.f)
		{
			VRController rightController = g_openXR->GetRightController();
			float rightHandGrip = rightController.GetGrip();
			if (rightHandGrip > 0.f)
			{
				m_isRightHandGripped = true;
				m_shouldCheckForRightHandGrip = false;
				Vec3 rightControllerFwd, rightControllerLeft, rightControllerUp;
				rightController.GetOrientation_iFwd_jLeft_kUp().GetAsVectors_iFwd_jLeft_kUp(rightControllerFwd, rightControllerLeft, rightControllerUp);
				playerRightControllerPosition = GetHandleWorldPosition() - rightControllerFwd * 0.075f - rightControllerLeft * 0.125f;
				player->m_rightController->m_orientation = m_orientation;
			}
		}

		if (m_isLeftHandGripped)
		{
			VRController leftController = g_openXR->GetLeftController();
			float leftHandGrip = leftController.GetGrip();
			if (leftHandGrip == 0.f)
			{
				m_isLeftHandGripped = false;
			}

			Vec3 leverMovementAxis = Vec3::SOUTH;
			Mat44 transform = m_orientation.GetAsMatrix_iFwd_jLeft_kUp();
			leverMovementAxis = transform.TransformVectorQuantity3D(leverMovementAxis);

			Vec3 leftHandDeltaPos = playerLeftControllerPosition - player->m_leftController->m_worldPositionLastFrame;
			float leftHandDeltaPosAlongLeverMovementAxis = GetProjectedLength3D(leftHandDeltaPos, leverMovementAxis);
			m_value += leftHandDeltaPosAlongLeverMovementAxis;

			m_value = GetClamped(m_value, -1.f, 1.f);
			Vec3 leftControllerFwd, leftControllerLeft, leftControllerUp;
			leftController.GetOrientation_iFwd_jLeft_kUp().GetAsVectors_iFwd_jLeft_kUp(leftControllerFwd, leftControllerLeft, leftControllerUp);
			playerLeftControllerPosition = GetHandleWorldPosition() - leftControllerFwd * 0.075f + leftControllerLeft * 0.125f;
			player->m_leftController->m_orientation = m_orientation;

			// Don't allow player to go too far
			if (GetDistanceSquared3D(m_position, player->m_pawn->m_position) > 1.f)
			{
				Vec3 directionPlayerToLever = (m_position - player->m_pawn->m_position).GetNormalized();
				player->m_pawn->m_position = m_position - directionPlayerToLever;
			}
		}
		if (m_isRightHandGripped)
		{
			VRController rightController = g_openXR->GetRightController();
			float rightHandGrip = rightController.GetGrip();
			if (rightHandGrip == 0.f)
			{
				m_isRightHandGripped = false;
			}

			Vec3 leverMovementAxis = Vec3::SOUTH;
			Mat44 transform = m_orientation.GetAsMatrix_iFwd_jLeft_kUp();
			leverMovementAxis = transform.TransformVectorQuantity3D(leverMovementAxis);

			Vec3 rightHandDeltaPos = playerRightControllerPosition - player->m_rightController->m_worldPositionLastFrame;
			float rightHandDeltaPosAlongLeverMovementAxis = GetProjectedLength3D(rightHandDeltaPos, leverMovementAxis);
			m_value += rightHandDeltaPosAlongLeverMovementAxis;
			Vec3 rightControllerFwd, rightControllerLeft, rightControllerUp;
			rightController.GetOrientation_iFwd_jLeft_kUp().GetAsVectors_iFwd_jLeft_kUp(rightControllerFwd, rightControllerLeft, rightControllerUp);
			playerRightControllerPosition = GetHandleWorldPosition() - rightControllerFwd * 0.075f - rightControllerLeft * 0.125f;
			player->m_rightController->m_orientation = m_orientation;

			// Don't allow player to go too far
			if (GetDistanceSquared3D(m_position, player->m_pawn->m_position) > 1.f)
			{
				Vec3 directionPlayerToLever = (m_position - player->m_pawn->m_position).GetNormalized();
				player->m_pawn->m_position = m_position - directionPlayerToLever;
			}
		}
	}

	if (GetDistanceSquared3D(m_position, m_map->m_game->m_player->m_pawn->m_position) < 1.f)
	{
		if (g_input->IsKeyDown('E'))
		{
			m_value += 1.f * deltaSeconds;
		}
		if (g_input->IsKeyDown('Q'))
		{
			m_value -= 1.f * deltaSeconds;
		}
	}

	m_previousFrameLeftHandGripValue = g_openXR->GetLeftController().GetGrip();
	m_previousFrameRightHandGripValue = g_openXR->GetRightController().GetGrip();
}

void Lever::ResetState()
{
	Entity::ResetState();
	m_value = -1.f;
	m_valueLastFrame = -1.f;
	m_shouldCheckForLeftHandGrip = false;
	m_previousFrameLeftHandGripValue = 0.f;
	m_shouldCheckForRightHandGrip = false;
	m_previousFrameRightHandGripValue = 0.f;

	m_isLeftHandGripped = false;
	m_isRightHandGripped = false;
}

//---------------------------------------------------------------------------------------

Vec3 const Lever::GetHandleWorldPosition() const
{
	Mat44 handleTransform = Mat44::CreateTranslation3D(m_position);
	handleTransform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	handleTransform.AppendScaleUniform3D(m_scale);
	handleTransform.AppendXRotation(RangeMapClamped(m_value, -1.f, 1.f, -45.f, 45.f));

	return handleTransform.TransformPosition3D(Vec3::SKYWARD * 0.6f);
}
