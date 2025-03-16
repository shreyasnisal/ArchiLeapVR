#pragma once

#include "Game/Activator.hpp"

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"


class Activatable;
class Map;


class Button : public Activator
{
public:
	~Button() = default;
	Button() = default;
	explicit Button(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;

	virtual void SetActivatable(EntityUID activatableUID) override;

public:
	bool m_isPressed = false;
	mutable bool m_wasPressedLastFrame = false;
};
