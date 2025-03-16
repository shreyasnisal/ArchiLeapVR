#pragma once

#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"


class Player;


class PlayerPawn
{
public:
	~PlayerPawn() = default;
	PlayerPawn() = default;
	explicit PlayerPawn(Player* player, Vec3 const& position, EulerAngles const& orientation);

	void Update();
	void FixedUpdate(float deltaSeconds);
	void Render() const;
	void RenderScreen() const;

	void AddForce(Vec3 const& force);
	void AddImpulse(Vec3 const& impuse);

	void MoveInDirection(Vec3 const& direction);
	void Jump();

	void Respawn();

public:
	static constexpr float WALK_SPEED = 20.f;
	static constexpr float RUN_SPEED = 50.f;
	static constexpr float TURN_RATE = 90.f;
	static constexpr float JUMP_IMPULSE = 5.f;
	static constexpr float MASS = 50.f;
	static constexpr float AIR_DRAG = 0.1f;
	static constexpr float FRICTION = 0.6f;
	static constexpr float PLAYER_HEIGHT = 1.7f;
	static constexpr float PLAYER_RADIUS = 0.4f;
	static constexpr int MAX_HEALTH = 100;

public:
	Player* m_player = nullptr;
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;
	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
	EulerAngles m_angularVelocity = EulerAngles::ZERO;
	bool m_isRunning = false;
	bool m_isGrounded = false;
	bool m_isHangingByLeftHand = false;
	bool m_isHangingByRightHand = false;
	bool m_hasWon = false;
	int m_health = 100;
};
