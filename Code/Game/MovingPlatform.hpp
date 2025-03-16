#pragma once

#include "Game/Activatable.hpp"


enum class MovementDirection
{
	NONE = -1,
	FORWARD_BACK,
	LEFT_RIGHT,
	UP_DOWN
};

class MovingPlatform : public Activatable
{
public:
	~MovingPlatform() = default;
	MovingPlatform() = default;
	explicit MovingPlatform(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;
	virtual void AppendToBuffer(BufferWriter& writer) override;

	virtual void Activate() override;
	virtual void Deactivate() override;

public:
	float m_movementTime = 0.f;
	bool m_isMoving = false;
	bool m_isObstructed = false;
	MovementDirection m_movementDirection = MovementDirection::UP_DOWN; // Serialized
	float m_movementFrequency = 1.f;
	float m_movementAmplitude = 1.f;

private:
	bool m_isPlayerStandingOn = false;
};
