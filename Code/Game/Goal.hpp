#pragma once

#include "Game/Entity.hpp"

#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/Models/ModelLoader.hpp"


class Map;


class Goal : public Entity
{
public:
	~Goal() = default;
	Goal() = default;
	explicit Goal(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;

public:
};
