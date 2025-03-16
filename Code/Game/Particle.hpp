#pragma once

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"

class Map;


class Particle
{
public:
	~Particle() = default;
	Particle(Map* map, Vec3 const& position, Vec3 const& velocity, EulerAngles const& orientation, float size, Rgba8 const& color, float lifetime, Model* model);
	void Update();
	void Render();

public:
	Map* m_map = nullptr;
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;
	Vec3 m_velocity = Vec3::ZERO;
	float m_size = 1.f;
	Rgba8 m_color = Rgba8::WHITE;
	float m_lifetime = 0.f;
	float m_age = 0.f;
	Model* m_model = nullptr;
	bool m_isDestroyed = false;
};

