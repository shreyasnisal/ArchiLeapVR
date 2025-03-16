#pragma once

#include "Game/Activator.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"

class Activatable;
class Map;

class Lever : public Activator
{
public:
	~Lever() = default;
	Lever() = default;
	explicit Lever(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;

	Vec3 const GetHandleWorldPosition() const;

public:
	static constexpr float CONTROLLER_VIBRATION_AMPLITUDE = 0.1f;
	static constexpr float CONTROLLER_VIBRATION_DURATION = 0.1f;

	float m_value = -1.f;
	float m_valueLastFrame = -1.f;

	bool m_shouldCheckForLeftHandGrip = false;
	float m_previousFrameLeftHandGripValue = 0.f;
	bool m_shouldCheckForRightHandGrip = false;
	float m_previousFrameRightHandGripValue = 0.f;

	bool m_isLeftHandGripped = false;
	bool m_isRightHandGripped = false;

	SoundID m_crankSFX = MISSING_SOUND_ID;
};
