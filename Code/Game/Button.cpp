#include "Game/Button.hpp"

#include "Game/Activatable.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Engine/Core/Models/ModelLoader.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"


Button::Button(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Activator(map, uid, position, orientation, scale, EntityType::BUTTON)
{
	m_model = g_modelLoader->CreateOrGetModelFromObj("Data/Models/Activators/buttonSquare", Mat44(Vec3::NORTH, Vec3::SKYWARD, Vec3::EAST, Vec3::ZERO));
	m_localBounds = AABB3(Vec3(-0.5f, -0.5f, 0.f), Vec3(0.5f, 0.5f, 0.2f));
}

void Button::Update()
{
	Entity::Update();

	if (m_isPressed && !m_wasPressedLastFrame)
	{
		Activatable* activatable = (Activatable*)(m_map->GetEntityFromUID(m_activatableUID));
		if (activatable)
		{
			activatable->Activate();
		}
	}
	else if (!m_isPressed && m_wasPressedLastFrame)
	{
		Activatable* activatable = (Activatable*)(m_map->GetEntityFromUID(m_activatableUID));
		if (activatable)
		{
			activatable->Deactivate();
		}
	}

	m_wasPressedLastFrame = m_isPressed;
}

void Button::Render() const
{
	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(MODEL_SCALE);

	Mat44 knobTransform(transform);
	knobTransform.AppendTranslation3D(Vec3(0.f, 0.f, m_isPressed ? -0.05f : 0.f));

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);

	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer("buttonSquare"), m_model->GetIndexBuffer("buttonSquare"), m_model->GetIndexCount("buttonSquare"));

	g_renderer->SetModelConstants(knobTransform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer("knob"), m_model->GetIndexBuffer("knob"), m_model->GetIndexCount("knob"));
}

void Button::HandlePlayerInteraction()
{
	Vec3 playerPawnPosition = m_map->m_game->m_player->m_pawn->m_position;
	Vec3 playerPawnCylinderTop = playerPawnPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT;
	if (DoZCylinderAndAABB3Overlap(playerPawnPosition, playerPawnCylinderTop, PlayerPawn::PLAYER_RADIUS, AABB3(m_position + m_localBounds.m_mins, m_position + m_localBounds.m_maxs)))
	{
		m_isPressed = true;
	}
	else
	{
		m_isPressed = false;
	}
}

void Button::ResetState()
{
	Entity::ResetState();
	m_isPressed = false;
	m_wasPressedLastFrame = false;
}

void Button::SetActivatable(EntityUID activatableUID)
{
	m_activatableUID = activatableUID;
}
