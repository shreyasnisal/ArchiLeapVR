#pragma once

#include "Game/EntityUID.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/BufferWriter.hpp"
#include "Engine/Core/Models/Model.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec3.hpp"


class Map;


class Entity
{
public:
	virtual ~Entity();
	explicit Entity(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale, EntityType type);

	virtual void InitializeUI();

	virtual void Update();
	virtual void Render() const = 0;
	virtual void HandlePlayerInteraction() = 0;

	virtual void AppendToBuffer(BufferWriter& writer);
	virtual void SaveEditorState();
	virtual void ResetState();

	Vec3 const GetForwardNormal() const;
	OBB3 const GetBounds() const;

	ArchiLeapRaycastResult3D Raycast(Vec3 const& rayStartPos, Vec3 const& fwdNormal, float maxDistance);
	Rgba8 GetColor() const;
	void SetMouseHovered(bool hovered);
	void SetRightHovered(bool hovered);
	void SetLeftHovered(bool hovered);
	void SetSelected(bool selected);

	bool IsActivatable() const;
	bool IsInteractable() const;

public:
	Map* m_map = nullptr;
	EntityUID m_uid = EntityUID::INVALID; // Serialized
	Vec3 m_editorPosition = Vec3::ZERO; // Serialized
	Vec3 m_position = Vec3::ZERO;
	EulerAngles m_editorOrientation = EulerAngles::ZERO; // Serialized
	EulerAngles m_orientation = EulerAngles::ZERO;
	float m_editorScale = 1.f; // Serialized
	float m_scale = 1.f;
	Rgba8 m_color = Rgba8::WHITE;
	bool m_isMouseHovered = false;
	bool m_isRightHovered = false;
	bool m_isLeftHovered = false;
	bool m_isSelected = false;
	Model* m_model = nullptr;
	AABB3 m_localBounds;
	EntityType m_type = EntityType::NONE; // Serialized

	UIWidget* m_detailsWidget = nullptr;
	UIWidget* m_positionValuesWidget = nullptr;
	UIWidget* m_orientationValuesWidget = nullptr;
	UIWidget* m_scaleValueWidget = nullptr;
	UIWidget* m_linkedEntityValueWidget = nullptr;
	UIWidget* m_linkButtonWidget = nullptr;
	UIWidget* m_movementDirButtonX = nullptr;
	UIWidget* m_movementDirButtonY = nullptr;
	UIWidget* m_movementDirButtonZ = nullptr;
	//UIWidget* m_linkedEntityUIDWidget = nullptr;
	
	Stopwatch m_pulseTimer;
};
