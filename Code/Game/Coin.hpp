#pragma once

#include "Game/Entity.hpp"


class Coin : public Entity
{
public:
	~Coin() = default;
	explicit Coin(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;

public:
	bool m_isCollected = false;
};
