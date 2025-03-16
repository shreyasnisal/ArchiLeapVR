#include "Game/Goal.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"


Goal::Goal(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Entity(map, uid, position, orientation, scale, EntityType::FLAG)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Entities/flag", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_localBounds = AABB3(Vec3(-0.05f, -0.05f, 0.f), Vec3(0.05f, 0.05f, 1.f));
	m_scale = MODEL_SCALE;
}

void Goal::Update()
{
	Entity::Update();
}

void Goal::Render() const
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

void Goal::HandlePlayerInteraction()
{
	Vec3 const& playerPawnPosition = m_map->m_game->m_player->m_pawn->m_position;
	Vec3 playerPawnTop(playerPawnPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT);
	if (!m_map->m_game->m_player->m_pawn->m_hasWon && DoZCylinderAndZOBB3Overlap(playerPawnPosition, playerPawnTop, PlayerPawn::PLAYER_RADIUS, GetBounds()))
	{
		m_map->m_game->m_nextState = GameState::LEVEL_COMPLETE;
		m_map->m_game->m_player->m_pawn->m_hasWon = true;
	}
}
