#include "Game/Particle.hpp"

#include "Game/Game.hpp"
#include "Game/Map.hpp"

#include "Engine/Math/MathUtils.hpp"


Particle::Particle(Map* map, Vec3 const& position, Vec3 const& velocity, EulerAngles const& orientation, float size, Rgba8 const& color, float lifetime, Model* model)
	: m_map(map)
	, m_position(position)
	, m_velocity(velocity)
	, m_orientation(orientation)
	, m_size(size)
	, m_color(color)
	, m_lifetime(lifetime)
	, m_model(model)
{
}

void Particle::Update()
{
	if (m_isDestroyed)
	{
		return;
	}

	float deltaSeconds = m_map->m_game->m_clock.GetDeltaSeconds();

	m_position += m_velocity * deltaSeconds;
	m_age += deltaSeconds;
	float opacity = GetFractionWithinRange(m_age, m_lifetime, 0.f);
	m_color.a = DenormalizeByte(opacity);

	if (m_age >= m_lifetime)
	{
		m_isDestroyed = true;
	}
}

void Particle::Render()
{
	if (m_isDestroyed)
	{
		return;
	}

	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.AppendScaleUniform3D(m_size);
	g_renderer->SetModelConstants(transform, m_color);
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer(), m_model->GetIndexBuffer(), m_model->GetIndexCount());
}
