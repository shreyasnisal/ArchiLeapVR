#pragma once

#include "Game/Entity.hpp"


class PlayerStart : public Entity
{
public:
	~PlayerStart();
	explicit PlayerStart(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation);

	virtual void Update() override;
	virtual void Render() const override;
	virtual void HandlePlayerInteraction() override;

private:
	VertexBuffer* m_vertexBuffer = nullptr;
	VertexBuffer* m_basisVBO = nullptr;
};
