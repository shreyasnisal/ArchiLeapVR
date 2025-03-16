#include "Game/PlayerPawn.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Tile.hpp"
#include "Game/Player.hpp"
#include "Game/Goal.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


PlayerPawn::PlayerPawn(Player* player, Vec3 const& position, EulerAngles const& orientation)
	: m_player(player)
	, m_position(position)
	, m_orientation(orientation)
{
}

void PlayerPawn::Update()
{
	if (m_player->m_state != PlayerState::PLAY)
	{
		return;
	}

	float deltaSeconds = m_player->m_game->m_clock.GetDeltaSeconds();

	// Add force due to gravity
	AddForce(Vec3::GROUNDWARD * GRAVITY * MASS);
	
	// Add drag force
	 AddForce(-m_velocity * AIR_DRAG);

	// Add friction force
	float frictionMagnitude = FRICTION * GRAVITY * MASS;
	AddForce(-m_velocity.GetXY().ToVec3() * frictionMagnitude);

	m_velocity += m_acceleration * deltaSeconds;
	m_position += m_velocity * deltaSeconds;
	m_acceleration = Vec3::ZERO;

	if (m_position.z < -10.f || m_health <= 0)
	{
		Respawn();
	}

	m_orientation.m_yawDegrees += m_angularVelocity.m_yawDegrees * deltaSeconds;
	m_orientation.m_pitchDegrees += m_angularVelocity.m_pitchDegrees * deltaSeconds;
	m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
}

void PlayerPawn::FixedUpdate(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void PlayerPawn::Render() const
{
}

void PlayerPawn::RenderScreen() const
{
}

void PlayerPawn::AddForce(Vec3 const& force)
{
	m_acceleration += force / MASS;
}

void PlayerPawn::AddImpulse(Vec3 const& impulse)
{
	m_velocity += impulse;
}

void PlayerPawn::MoveInDirection(Vec3 const& direction)
{
	float movementSpeed = WALK_SPEED;
	if (m_isRunning)
	{
		movementSpeed = RUN_SPEED;
	}

	AddForce(direction * movementSpeed * MASS);
}

void PlayerPawn::Jump()
{
	if (m_isGrounded)
	{
		AddImpulse(Vec3::SKYWARD * JUMP_IMPULSE);
		m_isGrounded = false;
	}
}

void PlayerPawn::Respawn()
{
	m_hasWon = false;
	m_orientation = m_player->m_game->m_currentMap->m_playerStart->m_orientation;
	m_angularVelocity = EulerAngles::ZERO;
	m_acceleration = Vec3::ZERO;
	m_velocity = Vec3::ZERO;
	m_position = m_player->m_game->m_currentMap->m_playerStart->m_position;
	m_health = MAX_HEALTH;
}
