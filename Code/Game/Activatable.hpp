#pragma once

#include "Game/Entity.hpp"

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"


class Map;


class Activatable : public Entity
{
public:
	virtual ~Activatable() = default;
	Activatable(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale, EntityType type) : Entity(map, uid, position, orientation, scale, type) {}
	
	virtual void Activate() = 0;
	virtual void Deactivate() = 0;

	virtual void Update() = 0;
	virtual void Render() const = 0;

	virtual void AppendToBuffer(BufferWriter& writer) override;

public:
	EntityUID m_activatorUID = EntityUID::INVALID; // Serialized
};
