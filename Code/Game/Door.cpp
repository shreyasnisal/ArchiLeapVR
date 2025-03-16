#include "Game/Door.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Math/MathUtils.hpp"


Door::Door(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Activatable(map, uid, position, orientation, scale, EntityType::DOOR)
{
	m_closedModel = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Activatables/doorClosed", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_openModel = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Activatables/doorOpen", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_model = m_closedModel;
	m_localBounds = AABB3(Vec3(-0.1f, -0.35f, 0.f), Vec3(0.1f, 0.35f, 1.f));
	m_scale = MODEL_SCALE;
}

void Door::Update()
{
	Entity::Update();
}

void Door::Render() const
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

void Door::HandlePlayerInteraction()
{
	if (!m_isOpen)
	{
		Mat44 transform = Mat44::CreateTranslation3D(m_position);
		transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
		transform.AppendScaleUniform3D(MODEL_SCALE);

		Vec3& playerPosition = m_map->m_game->m_player->m_pawn->m_position;
		Vec3 playerCylinderTop = playerPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT;
		PushZCylinderOutOfFixedAABB3(playerPosition, playerCylinderTop, PlayerPawn::PLAYER_RADIUS, AABB3(transform.TransformPosition3D(m_localBounds.m_mins), transform.TransformPosition3D(m_localBounds.m_maxs)));
	}
}

void Door::ResetState()
{
	Entity::ResetState();
	m_isOpen = false;
	m_model = m_closedModel;
}

void Door::Activate()
{
	m_isOpen = true;
	m_model = m_openModel;
}

void Door::Deactivate()
{
	m_isOpen = false;
	m_model = m_closedModel;
}
