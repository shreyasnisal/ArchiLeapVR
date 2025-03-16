#include "Game/Crate.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/HandController.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


Crate::Crate(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Entity(map, uid, position, orientation, scale, EntityType::CRATE)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Entities/crate", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_localBounds = AABB3(Vec3(-0.25f, -0.25f, 0.f), Vec3(0.25f, 0.25f, 0.5f));
}

void Crate::Update()
{
	Entity::Update();

	if (m_map->m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();

	if (!m_isHeldInLeftHand && !m_isHeldInRightHand)
	{
		// Add force due to gravity
		AddForce(Vec3::GROUNDWARD * GRAVITY * MASS);

		// Add drag force
		AddForce(-m_velocity * AIR_DRAG);

		// Add friction force
		if (m_isGrounded)
		{
			float frictionMagnitude = FRICTION * GRAVITY * MASS;
			AddForce(-m_velocity.GetXY().ToVec3() * frictionMagnitude);
		}

		m_velocity += m_acceleration * deltaSeconds;
		m_position += m_velocity * deltaSeconds;
		m_acceleration = Vec3::ZERO;
	}
}

void Crate::Render() const
{
	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(this->m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer(), m_model->GetIndexBuffer(), m_model->GetIndexCount());
}

void Crate::HandlePlayerInteraction()
{
	Player* const& player = m_map->m_game->m_player;
	PlayerPawn* const& playerPawn = player->m_pawn;

	if (!m_isHeldInLeftHand && !m_isHeldInRightHand)
	{
		if (DoSphereAndOBB3Overlap(player->m_leftController->m_worldPosition, Player::CONTROLLER_RADIUS, GetBounds()) && player->m_leftController->GetController().WasGripJustPressed())
		{
			m_isHeldInLeftHand = true;
			m_isGrounded = false;
		}
		if (DoSphereAndOBB3Overlap(player->m_rightController->m_worldPosition, Player::CONTROLLER_RADIUS, GetBounds()) && player->m_rightController->GetController().WasGripJustPressed())
		{
			m_isHeldInRightHand = true;
			m_isGrounded = false;
		}
	}
	else if (m_isHeldInLeftHand)
	{
		if (player->m_leftController->GetController().WasGripJustReleased())
		{
			m_isHeldInLeftHand = false;
			AddImpulse(3.f * player->m_leftController->GetLinearVelocity());
		}
	}
	else if (m_isHeldInRightHand)
	{
		if (player->m_rightController->GetController().WasGripJustReleased())
		{
			m_isHeldInRightHand = false;
			AddImpulse(3.f * player->m_rightController->GetLinearVelocity());
		}
	}

	if (m_isHeldInLeftHand)
	{
		m_position = player->m_leftController->m_worldPosition;
		m_orientation.m_yawDegrees = player->m_leftController->m_orientation.m_yawDegrees;
	}
	else if (m_isHeldInRightHand)
	{
		m_position = player->m_rightController->m_worldPosition;
		m_orientation.m_yawDegrees = player->m_rightController->m_orientation.m_yawDegrees;
	}
	else
	{
		OBB3 bounds = GetBounds();
		PushZOBB3OutOfFixedZCylinder(bounds, playerPawn->m_position, playerPawn->m_position + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT, PlayerPawn::PLAYER_RADIUS);
		m_position = bounds.m_center + Vec3::GROUNDWARD * m_localBounds.GetDimensions().z * m_scale * 0.5f;
	}
}

void Crate::ResetState()
{
	Entity::ResetState();
	m_isHeldInLeftHand = false;
	m_isHeldInRightHand = false;
	m_isGrounded = false;
	m_velocity = Vec3::ZERO;
	m_acceleration = Vec3::ZERO;
}

void Crate::AddForce(Vec3 const& force)
{
	m_acceleration += force / MASS;
}

void Crate::AddImpulse(Vec3 const& impulse)
{
	m_velocity += impulse;
}
