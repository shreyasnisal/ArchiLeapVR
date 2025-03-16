#include "Game/Enemy_Orc.hpp"

#include "Game/HandController.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"


Enemy_Orc::Enemy_Orc(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Entity(map, uid, position, orientation, scale, EntityType::ENEMY_ORC)
	, m_walkAnimationTimer(&map->m_game->m_clock, 0.5f)
	, m_lastKnownPlayerLocation(position)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Enemies/character-orc", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3(0.f, 0.f, -0.3f)));
	m_walkAnimationTimer.Start();
	m_localBounds = AABB3(Vec3(-0.2f, -0.2f, 0.f), Vec3(0.2f, 0.2f, 1.f));
	m_scale = MODEL_SCALE;

	m_playerSensedSFX = g_audio->CreateOrGetSound("Data/SFX/Orc_See.wav", true);
	m_dieSFX = g_audio->CreateOrGetSound("Data/SFX/Orc_Die.wav", true);
	m_attackSFX = g_audio->CreateOrGetSound("Data/SFX/Orc_Attack.wav", true);
}

void Enemy_Orc::Update()
{
	Entity::Update();

	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();

	if (m_map->m_game->m_player->m_state != PlayerState::PLAY)
	{
		return;
	}
	if (m_isDead)
	{
		return;
	}

	PlayerPawn* const& playerPawn = m_map->m_game->m_player->m_pawn;
	if (GetDistanceXYSquared3D(m_position, playerPawn->m_position) < 25.f)
	{
		m_lastKnownPlayerLocation = playerPawn->m_position;
		if (!m_wasPlayerInRangeLastFrame)
		{
			g_audio->StartSound(m_playerSensedSFX);
			m_wasPlayerInRangeLastFrame = true;
		}
	}
	else
	{
		m_wasPlayerInRangeLastFrame = false;
	}

	if (GetDistanceXYSquared3D(m_position, m_lastKnownPlayerLocation) > 0.01f)
	{
		Vec3 const directionToPlayer = (m_lastKnownPlayerLocation - m_position).GetXY().GetNormalized().ToVec3();
		TurnToYaw(directionToPlayer.GetAngleAboutZDegrees());
		MoveInDirection(GetForwardNormal());
	}
	else
	{
		m_acceleration = Vec3::ZERO;
		m_velocity = Vec3::ZERO;
	}

	if (m_isHeldInLeftHand || m_isHeldInRightHand)
	{
		m_walkAnimationTimer.m_duration = 0.1f;
		m_isGrounded = false;

		if (m_isHeldInLeftHand)
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(GRAB_CONTROLLER_VIBRATION_AMPLITUDE, m_map->m_game->m_clock.GetDeltaSeconds());
		}
		else if (m_isHeldInRightHand)
		{
			g_openXR->GetRightController().ApplyHapticFeedback(GRAB_CONTROLLER_VIBRATION_AMPLITUDE, m_map->m_game->m_clock.GetDeltaSeconds());
		}
	}
	else
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

		float speed = m_velocity.GetLength();
		if (speed == 0.f)
		{
			m_walkAnimationTimer.m_duration = 0.f;
		}
		else
		{
			m_walkAnimationTimer.m_duration = 1.f / m_velocity.GetLength() * 0.5f;
		}

		m_velocity += m_acceleration * deltaSeconds;
		m_position += m_velocity * deltaSeconds;
		m_acceleration = Vec3::ZERO;
	}

	if (m_walkAnimationTimer.m_duration != 0.f && m_walkAnimationTimer.HasDurationElapsed())
	{
		m_walkAnimationTimer.Restart();
		m_animationLeg = m_animationLeg == AnimationLeg::LEFT ? AnimationLeg::RIGHT : AnimationLeg::LEFT;
	}

	if (m_position.z < -10.f)
	{
		m_isDead = true;
	}
}

void Enemy_Orc::Render() const
{
	if (m_isDead)
	{
		return;
	}

	float animationFraction = m_walkAnimationTimer.GetElapsedFraction();
	if (m_animationLeg == AnimationLeg::RIGHT)
	{
		animationFraction = 1.f - animationFraction;
	}
	if (m_map->m_game->m_player->m_state != PlayerState::PLAY)
	{
		animationFraction = 0.f;
	}

	Mat44 transform = Mat44::CreateTranslation3D(m_position + Vec3::SKYWARD * 0.6f);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->BindTexture(nullptr);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("body"), m_model->GetVertexCount("body"));

	Mat44 headTransform = transform;
	headTransform.AppendYRotation(RangeMap(animationFraction, 0.f, 1.f, -5.f, 5.f));
	g_renderer->SetModelConstants(headTransform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("head"), m_model->GetVertexCount("head"));

	Mat44 leftArmTransform = transform;
	leftArmTransform.AppendXRotation(-20.f);
	leftArmTransform.AppendYRotation(RangeMap(animationFraction, 0.f, 1.f, -15.f, 15.f));
	g_renderer->SetModelConstants(leftArmTransform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("arm-left"), m_model->GetVertexCount("arm-left"));

	Mat44 rightArmTransform = transform;
	rightArmTransform.AppendXRotation(20.f);
	rightArmTransform.AppendYRotation(RangeMap(animationFraction, 0.f, 1.f, 15.f, -15.f));
	g_renderer->SetModelConstants(rightArmTransform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("arm-right"), m_model->GetVertexCount("arm-right"));

	Mat44 leftLegTransform = transform;
	leftLegTransform.AppendYRotation(RangeMap(animationFraction, 0.f, 1.f, 15.f, -15.f));
	g_renderer->SetModelConstants(leftLegTransform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("leg-left"), m_model->GetVertexCount("leg-left"));

	Mat44 rightLegTransform = transform;
	rightLegTransform.AppendYRotation(RangeMap(animationFraction, 0.f, 1.f, -15.f, 15.f));
	g_renderer->SetModelConstants(rightLegTransform, GetColor());
	g_renderer->DrawVertexBuffer(m_model->GetVertexBuffer("leg-right"), m_model->GetVertexCount("leg-right"));
}

void Enemy_Orc::HandlePlayerInteraction()
{
	if (m_isDead)
	{
		return;
	}

	Player* const& player = m_map->m_game->m_player;
	PlayerPawn* const& playerPawn = player->m_pawn;

	if (m_isHeldInLeftHand)
	{
		if (player->m_leftController->GetController().WasGripJustReleased())
		{
			AddImpulse(player->m_leftController->GetLinearVelocity());
			m_isHeldInLeftHand = false;
			return;
		}

		m_position = player->m_leftController->m_worldPosition + Vec3::GROUNDWARD * HEIGHT * 0.5f;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		return;
	}
	if (m_isHeldInRightHand)
	{
		if (player->m_rightController->GetController().WasGripJustReleased())
		{
			AddImpulse(player->m_rightController->GetLinearVelocity());
			m_isHeldInRightHand = false;
			return;
		}

		m_position = player->m_rightController->m_worldPosition + Vec3::GROUNDWARD * HEIGHT * 0.5f;
		m_velocity = Vec3::ZERO;
		m_acceleration = Vec3::ZERO;
		return;
	}

	if (DoZCylindersOverlap(playerPawn->m_position, playerPawn->m_position + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT, PlayerPawn::PLAYER_RADIUS, m_position, m_position + Vec3::SKYWARD * Enemy_Orc::HEIGHT, Enemy_Orc::RADIUS))
	{
		playerPawn->m_health -= 1;
		playerPawn->AddImpulse((playerPawn->m_position - m_position).GetXY().GetNormalized().ToVec3() * ATTACK_IMPULSE);
		g_audio->StartSoundAt(m_attackSFX, m_position);
	}

	if (DoSphereAndCylinderOverlap(player->m_leftController->m_worldPosition, Player::CONTROLLER_RADIUS, m_position, m_position + Vec3::SKYWARD * HEIGHT, RADIUS))
	{
		if (!m_isHeldInRightHand && player->m_leftController->GetController().WasGripJustPressed())
		{
			m_isHeldInLeftHand = true;
		}

		if (player->m_leftController->m_velocity.GetLengthSquared() > 16.f && player->m_leftController->GetController().GetGrip() && player->m_leftController->GetController().GetTrigger())
		{
			g_openXR->GetLeftController().ApplyHapticFeedback(PUNCH_CONTROLLER_VIBRATION_AMPLITUDE, PUNCH_CONTROLLER_VIBRATION_DURATION);
			g_audio->StartSoundAt(m_dieSFX, m_position);
			for (int particleIndex = 0; particleIndex < NUM_PARTICLES_ON_DAMAGE; particleIndex++)
			{
				Vec3 particleRandomVelocity = g_rng->RollRandomVec3InRadius(Vec3::ZERO, 1.f);
				m_map->SpawnParticle(player->m_leftController->m_worldPosition, player->m_leftController->GetLinearVelocity() + particleRandomVelocity, EulerAngles::ZERO, 0.025f, Rgba8::RED, 0.25f);
			}
			m_isDead = true;
		}
	}
	if (DoSphereAndCylinderOverlap(player->m_rightController->m_worldPosition, Player::CONTROLLER_RADIUS, m_position, m_position + Vec3::SKYWARD * HEIGHT, RADIUS))
	{
		if (!m_isHeldInLeftHand && player->m_rightController->GetController().WasGripJustPressed())
		{
			m_isHeldInRightHand = true;
		}

		if (player->m_rightController->m_velocity.GetLengthSquared() > 16.f && player->m_rightController->GetController().GetGrip() && player->m_rightController->GetController().GetTrigger())
		{
			g_openXR->GetRightController().ApplyHapticFeedback(PUNCH_CONTROLLER_VIBRATION_AMPLITUDE, PUNCH_CONTROLLER_VIBRATION_DURATION);
			g_audio->StartSoundAt(m_dieSFX, m_position);
			for (int particleIndex = 0; particleIndex < NUM_PARTICLES_ON_DAMAGE; particleIndex++)
			{
				Vec3 particleRandomVelocity = g_rng->RollRandomVec3InRadius(Vec3::ZERO, 1.f);
				m_map->SpawnParticle(player->m_rightController->m_worldPosition, player->m_rightController->GetLinearVelocity() + particleRandomVelocity, EulerAngles::ZERO, 0.025f, Rgba8::RED, 0.25f);
			}
			m_isDead = true;
		}
	}
}

void Enemy_Orc::SaveEditorState()
{
	Entity::SaveEditorState();
	m_lastKnownPlayerLocation = m_position;
}

void Enemy_Orc::ResetState()
{
	Entity::ResetState();
	m_velocity = Vec3::ZERO;
	m_acceleration = Vec3::ZERO;
	m_walkAnimationTimer.m_duration = 0.5f;
	m_animationLeg = AnimationLeg::LEFT;
	m_isDead = false;
	m_lastKnownPlayerLocation = m_position;
}

void Enemy_Orc::AddForce(Vec3 const& force)
{
	m_acceleration += force / MASS;
}

void Enemy_Orc::AddImpulse(Vec3 const& impulse)
{
	m_velocity += impulse;
}

void Enemy_Orc::MoveInDirection(Vec3 const& direction)
{
	float movementSpeed = WALK_SPEED;
	AddForce(direction * movementSpeed * MASS);
}

void Enemy_Orc::TurnToYaw(float goalYaw)
{
	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();
	m_orientation.m_yawDegrees = GetTurnedTowardDegrees(m_orientation.m_yawDegrees, goalYaw, TURN_RATE * deltaSeconds);
}
