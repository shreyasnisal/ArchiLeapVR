#include "Game/PlayerStart.hpp"

#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"


PlayerStart::~PlayerStart()
{
}

PlayerStart::PlayerStart(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation)
	: Entity(map, uid, position, orientation, 1.f, EntityType::NONE)
{
	std::vector<Vertex_PCU> vertexes;
	AddVertsForGradientLineSegment3D(vertexes, Vec3(0.f, 0.f, 0.01f), Vec3(0.f, 0.f, 1.f), 0.5f, Rgba8::LIME, Rgba8(0, 255, 0, 0), AABB2::ZERO_TO_ONE, 32);
	m_vertexBuffer = g_renderer->CreateVertexBuffer(vertexes.size() * sizeof(Vertex_PCU));
	g_renderer->CopyCPUToGPU(vertexes.data(), vertexes.size() * sizeof(Vertex_PCU), m_vertexBuffer);

	std::vector<Vertex_PCU> basisVertexes;
	AddVertsForAABB3(basisVertexes, AABB3(m_position + Vec3::SKYWARD * 0.5f - Vec3(0.001f, 0.001f, 0.001f), m_position + Vec3::SKYWARD * 0.5f + Vec3(0.001f, 0.001f, 0.001f)), Rgba8::WHITE);
	AddVertsForArrow3D(basisVertexes, m_position + Vec3::SKYWARD * 0.5f, m_position + Vec3::SKYWARD * 0.5f + m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D() * 0.5f, 0.01f, Rgba8::RED);
	AddVertsForArrow3D(basisVertexes, m_position + Vec3::SKYWARD * 0.5f, m_position + Vec3::SKYWARD * 0.5f + m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetJBasis3D() * 0.5f, 0.01f, Rgba8::GREEN);
	AddVertsForArrow3D(basisVertexes, m_position + Vec3::SKYWARD * 0.5f, m_position + Vec3::SKYWARD * 0.5f + m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetKBasis3D() * 0.5f, 0.01f, Rgba8::BLUE);
	m_basisVBO = g_renderer->CreateVertexBuffer(basisVertexes.size() * sizeof(Vertex_PCU));
	g_renderer->CopyCPUToGPU(basisVertexes.data(), basisVertexes.size() * sizeof(Vertex_PCU), m_basisVBO);

	m_localBounds = AABB3(-0.5f, -0.5f, 0.f, 0.5f, 0.5f, 1.f);
}

void PlayerStart::Update()
{
	Entity::Update();
}

void PlayerStart::Render() const
{
	if (m_map->m_game->m_player->m_state == PlayerState::PLAY)
	{
		return;
	}

	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	g_renderer->BindShader(nullptr);
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawVertexBuffer(m_vertexBuffer, (int)(m_vertexBuffer->m_size / sizeof(Vertex_PCU)));

	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->DrawVertexBuffer(m_basisVBO, (int)(m_basisVBO->m_size / sizeof(Vertex_PCU)));
}

void PlayerStart::HandlePlayerInteraction()
{
}
