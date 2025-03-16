#pragma once

#include "Game/Entity.hpp"


class Crate : public Entity
{
public:
	~Crate() = default;
	explicit Crate(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;

	void AddForce(Vec3 const& force);
	void AddImpulse(Vec3 const& impulse);

public:
	static constexpr float AIR_DRAG = 0.1f;
	static constexpr float FRICTION = 0.6f;
	static constexpr float MASS = 20.f;

	bool m_isHeldInLeftHand = false;
	bool m_isHeldInRightHand = false;
	bool m_isGrounded = false;

	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
};
