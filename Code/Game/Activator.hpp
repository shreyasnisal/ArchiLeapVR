#pragma once

#include "Game/Entity.hpp"

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"


class Map;
class Activatable;


class Activator : public Entity
{
public:
	~Activator() = default;
	Activator() = default;
	explicit Activator(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale, EntityType type) : Entity(map, uid, position, orientation, scale, type) {};

	virtual void Update() = 0;
	virtual void Render() const = 0;
	virtual void SetActivatable(EntityUID activatableUID) { m_activatableUID = activatableUID; }

	virtual void AppendToBuffer(BufferWriter& writer) override;

public:
	EntityUID m_activatableUID = EntityUID::INVALID; // Serialized
};

