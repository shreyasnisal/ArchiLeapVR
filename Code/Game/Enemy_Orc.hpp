#pragma once

#include "Game/Entity.hpp"


enum class AnimationLeg
{
	NONE = -1,
	LEFT,
	RIGHT,
	NUM,
};

class Enemy_Orc : public Entity
{
public:
	~Enemy_Orc() = default;
	Enemy_Orc(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void SaveEditorState() override;
	virtual void ResetState() override;

	void AddForce(Vec3 const& force);
	void AddImpulse(Vec3 const& impulse);
	void MoveInDirection(Vec3 const& direction);
	void TurnToYaw(float goalYaw);

public:
	static constexpr float AIR_DRAG = 0.1f;
	static constexpr float FRICTION = 0.6f;
	static constexpr float MASS = 50.f;
	static constexpr float TURN_RATE = 90.f;
	static constexpr float WALK_SPEED = 10.f;
	static constexpr float HEIGHT = 1.5f;
	static constexpr float RADIUS = 0.3f;
	static constexpr float ATTACK_IMPULSE = 1.5f;
	static constexpr int NUM_PARTICLES_ON_DAMAGE = 10;
	static constexpr float PUNCH_CONTROLLER_VIBRATION_AMPLITUDE = 0.25f;
	static constexpr float PUNCH_CONTROLLER_VIBRATION_DURATION = 0.1f;
	static constexpr float GRAB_CONTROLLER_VIBRATION_AMPLITUDE = 0.05f;

	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
	Stopwatch m_walkAnimationTimer;
	AnimationLeg m_animationLeg = AnimationLeg::LEFT;
	bool m_isDead = false;
	bool m_isGrounded = false;
	Vec3 m_lastKnownPlayerLocation = Vec3::ZERO;
	bool m_wasPlayerInRangeLastFrame = false;
	bool m_isHeldInLeftHand = false;
	bool m_isHeldInRightHand = false;

	SoundID m_playerSensedSFX = MISSING_SOUND_ID;
	SoundID m_attackSFX = MISSING_SOUND_ID;
	SoundID m_dieSFX = MISSING_SOUND_ID;
};
