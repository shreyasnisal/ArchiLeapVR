#pragma once

#include "Game/Entity.hpp"
#include "Game/TileDefinition.hpp"

#include "Engine/Core/Models/Model.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"


class Map;


class Tile : public Entity
{
public:
	~Tile() = default;
	Tile() = default;
	Tile(Map* map, EntityUID uid, TileDefinition const& definition, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;

public:
	TileDefinition m_definition;
};

