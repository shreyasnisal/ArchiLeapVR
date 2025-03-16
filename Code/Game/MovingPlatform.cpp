#include "Game/MovingPlatform.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"


MovingPlatform::MovingPlatform(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Activatable(map, uid, position, orientation, scale, EntityType::MOVING_PLATFORM)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Activatables/blockMoving", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_scale = MODEL_SCALE;
	m_localBounds = AABB3(Vec3(-0.425f, -0.425f, 0.f), Vec3(0.425f, 0.425f, 0.25f));
}

void MovingPlatform::Update()
{
	Entity::Update();

	if (m_movementDirection == MovementDirection::FORWARD_BACK)
	{
		m_movementDirButtonX->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(PRIMARY_COLOR)
			->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonY->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonZ->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
	}
	else if (m_movementDirection == MovementDirection::LEFT_RIGHT)
	{
		m_movementDirButtonX->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonY->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(PRIMARY_COLOR)
			->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonZ->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
	}
	else if (m_movementDirection == MovementDirection::UP_DOWN)
	{
		m_movementDirButtonX->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonY->SetBackgroundColor(PRIMARY_COLOR)
			->SetHoverBackgroundColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(SECONDARY_COLOR)
			->SetHoverBorderColor(SECONDARY_COLOR_VARIANT_LIGHT);
		m_movementDirButtonZ->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBorderColor(PRIMARY_COLOR)
			->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT);
	}

	if (!m_isMoving)
	{
		return;
	}
	if (m_isObstructed)
	{
		return;
	}

	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();
	m_movementTime += deltaSeconds;

	Player* player = m_map->m_game->m_player;
	PlayerPawn* playerPawn = player->m_pawn;

	Vec3 fwd, left, up;
	m_orientation.GetAsVectors_iFwd_jLeft_kUp(fwd, left, up);

	Vec3 movementDir = Vec3::ZERO;

	switch (m_movementDirection)
	{
		case MovementDirection::FORWARD_BACK:		movementDir = fwd;			break;
		case MovementDirection::LEFT_RIGHT:			movementDir = left;			break;
		case MovementDirection::UP_DOWN:			movementDir = up;			break;
	}

	m_position += m_movementAmplitude * sinf(m_movementTime * m_movementFrequency) * movementDir * deltaSeconds;

	if (m_isPlayerStandingOn)
	{
		playerPawn->m_position += m_movementAmplitude * sinf(m_movementTime * m_movementFrequency) * movementDir * deltaSeconds;
	}

	m_isPlayerStandingOn = false;
}

void MovingPlatform::Render() const
{
	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);

	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer(), m_model->GetIndexBuffer(), m_model->GetIndexCount());
}

void MovingPlatform::HandlePlayerInteraction()
{
	Player* player = m_map->m_game->m_player;
	PlayerPawn* playerPawn = player->m_pawn;

	Vec3& playerPawnPosition = playerPawn->m_position;
	Vec3 pawnCylinderTop(playerPawnPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT);
	
	constexpr int NUM_RAYCASTS = 9;
	OBB3 selectedEntityBounds = GetBounds();
	Vec3 cornerPoints[8];
	selectedEntityBounds.GetCornerPoints(cornerPoints);
	Vec3 raycastPoints[9] = {
		m_position,
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

	bool isObstructedThisFrame = false;
	for (int raycastPointIndex = 0; raycastPointIndex < NUM_RAYCASTS; raycastPointIndex++)
	{
		RaycastResult3D raycastVsPlayer = RaycastVsCylinder3D(raycastPoints[raycastPointIndex], Vec3::GROUNDWARD, m_movementAmplitude, playerPawnPosition, pawnCylinderTop, PlayerPawn::PLAYER_RADIUS);
		if (raycastVsPlayer.m_didImpact)
		{
			isObstructedThisFrame = true;
			m_isObstructed = true;
		}
	}

	if (!isObstructedThisFrame)
	{
		m_isObstructed = false;
	}
	
	Vec3 playerPawnPositionBeforePush(playerPawnPosition);
	if (PushZCylinderOutOfFixedZOBB3(playerPawnPosition, pawnCylinderTop, PlayerPawn::PLAYER_RADIUS, GetBounds()))
	{
		if (playerPawnPosition.z > playerPawnPositionBeforePush.z)
		{
			playerPawn->m_velocity.z = 0.f;
			playerPawn->m_isGrounded = true;
			m_isPlayerStandingOn = true;
		}
	}
}

void MovingPlatform::ResetState()
{
	Entity::ResetState();
	m_movementTime = 0.f;
	m_isMoving = false;
	m_movementFrequency = 1.f;
	m_movementAmplitude = 1.f;
	m_isPlayerStandingOn = false;
}

void MovingPlatform::AppendToBuffer(BufferWriter& writer)
{
	Activatable::AppendToBuffer(writer);
	writer.AppendByte((uint8_t)m_movementDirection);
}

void MovingPlatform::Activate()
{
	m_isMoving = true;
}	

void MovingPlatform::Deactivate()
{
	m_isMoving = false;
}
