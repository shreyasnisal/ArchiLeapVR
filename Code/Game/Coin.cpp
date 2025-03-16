#include "Game/Coin.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


Coin::Coin(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Entity(map, uid, position, orientation, scale, EntityType::COIN)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Entities/coinGold", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_localBounds = AABB3(Vec3(-0.1f, -0.1f, 0.f), Vec3(0.1f, 0.1f, 1.f));
	m_orientation.m_yawDegrees = g_rng->RollRandomFloatInRange(0.f, 360.f);
}

void Coin::Update()
{
	Entity::Update();

	if (m_isCollected)
	{
		return;
	}

	constexpr float ROTATION_SPEED = 25.f;

	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();
	m_orientation.m_yawDegrees += ROTATION_SPEED * deltaSeconds;
}

void Coin::Render() const
{
	if (m_isCollected)
	{
		return;
	}

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

void Coin::HandlePlayerInteraction()
{
	if (m_isCollected)
	{
		return;
	}

	Vec3 const& playerPawnPosition = m_map->m_game->m_player->m_pawn->m_position;
	Vec3 playerPawnTop(playerPawnPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT);
	if (!m_map->m_game->m_player->m_pawn->m_hasWon && DoZCylinderAndZOBB3Overlap(playerPawnPosition, playerPawnTop, PlayerPawn::PLAYER_RADIUS, GetBounds()))
	{
		m_map->m_coinsCollected++;
		m_isCollected = true;
	}
}

void Coin::ResetState()
{
	Entity::ResetState();
	m_isCollected = false;
}
