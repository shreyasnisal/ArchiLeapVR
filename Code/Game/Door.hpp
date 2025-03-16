#pragma once

#include "Game/Activatable.hpp"

#include "Engine/Math/AABB3.hpp"


class Door : public Activatable
{
public:
	~Door() = default;
	Door() = default;
	explicit Door(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;
	virtual void ResetState() override;

	virtual void Activate() override;
	virtual void Deactivate() override;

public:
	bool m_isOpen = false;
	Model* m_closedModel = nullptr;
	Model* m_openModel = nullptr;
};
