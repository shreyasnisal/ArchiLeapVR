#include "Game/Entity.hpp"

#include "Game/Activatable.hpp"
#include "Game/Activator.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Map.hpp"
#include "Game/MovingPlatform.hpp"
#include "Game/Player.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/UI/UISystem.hpp"


Entity::~Entity()
{
	m_map->m_game->m_gameWidget->RemoveChild(m_detailsWidget);
	delete m_detailsWidget;
	m_detailsWidget = nullptr;
}

Entity::Entity(Map* map, EntityUID uid, Vec3 const& position, EulerAngles const& orientation, float scale, EntityType type)
	: m_map(map)
	, m_uid(uid)
	, m_position(position)
	, m_editorPosition(position)
	, m_orientation(orientation)
	, m_editorOrientation(orientation)
	, m_scale(scale)
	, m_editorScale(scale)
	, m_type(type)
	, m_pulseTimer(&m_map->m_game->m_clock, 1.f)
{
	InitializeUI();
}

void Entity::InitializeUI()
{
	m_detailsWidget = g_ui->CreateWidget(m_map->m_game->m_gameWidget);
	m_detailsWidget->SetPosition(Vec2(0.525f, 0.1f))
		->SetDimensions(Vec2(0.45f, 0.65f))
		->SetPivot(Vec2(0.f, 0.f))
		->SetBackgroundColor(Rgba8(255, 255, 255, 225))
		->SetHoverBackgroundColor(Rgba8(255, 255, 255, 225))
		->SetBorderRadius(0.5f)
		->SetBorderWidth(0.2f)
		->SetBorderColor(PRIMARY_COLOR)
		->SetHoverBorderColor(PRIMARY_COLOR)
		->SetRaycastTarget(false);

	UIWidget* entityTypeWidget = g_ui->CreateWidget(m_detailsWidget);
	entityTypeWidget->SetText(m_map->GetEntityNameFromType(m_type))
		->SetPosition(Vec2(0.5f, 0.95f))
		->SetDimensions(Vec2(0.8f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetFontSize(8.f)
		->SetRaycastTarget(false);

	UIWidget* entityUIDWidget = g_ui->CreateWidget(m_detailsWidget);
	entityUIDWidget->SetText(Stringf("%#010x", m_uid))
		->SetPosition(Vec2(0.5f, 0.9f))
		->SetDimensions(Vec2(0.8f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	UIWidget* positionWidget = g_ui->CreateWidget(m_detailsWidget);
	positionWidget->SetText("Position")
		->SetPosition(Vec2(0.05f, 0.8f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	m_positionValuesWidget = g_ui->CreateWidget(m_detailsWidget);
	m_positionValuesWidget->SetText("")
		->SetPosition(Vec2(0.4f, 0.8f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	UIWidget* orientationWidget = g_ui->CreateWidget(m_detailsWidget);
	orientationWidget->SetText("Rotation")
		->SetPosition(Vec2(0.05f, 0.7f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	m_orientationValuesWidget = g_ui->CreateWidget(m_detailsWidget);
	m_orientationValuesWidget->SetText("")
		->SetPosition(Vec2(0.4f, 0.7f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	UIWidget* scaleWidget = g_ui->CreateWidget(m_detailsWidget);
	scaleWidget->SetText("Scale")
		->SetPosition(Vec2(0.05f, 0.6f))
		->SetDimensions(Vec2(0.3f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(SECONDARY_COLOR)
		->SetHoverColor(SECONDARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	m_scaleValueWidget = g_ui->CreateWidget(m_detailsWidget);
	m_scaleValueWidget->SetText("")
		->SetPosition(Vec2(0.4f, 0.6f))
		->SetDimensions(Vec2(0.5f, 0.05f))
		->SetPivot(Vec2(0.f, 0.5f))
		->SetAlignment(Vec2(0.f, 0.5f))
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR)
		->SetFontSize(4.f)
		->SetRaycastTarget(false);

	UIWidget* resetTransformButton = g_ui->CreateWidget(m_detailsWidget);
	resetTransformButton->SetText("Reset Transform")
		->SetPosition(Vec2(0.5f, 0.5f))
		->SetDimensions(Vec2(0.8f, 0.05f))
		->SetPivot(Vec2(0.5f, 0.5f))
		->SetAlignment(Vec2(0.5f, 0.5f))
		->SetBackgroundColor(SECONDARY_COLOR)
		->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
		->SetColor(PRIMARY_COLOR)
		->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetFontSize(4.f)
		->SetBorderColor(PRIMARY_COLOR)
		->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT)
		->SetBorderRadius(0.2f)
		->SetBorderWidth(0.1f)
		->SetClickEventName(Stringf("ResetTransform entity=%d", m_uid.m_uid))
		->SetRaycastTarget(true);

	if (m_type == EntityType::BUTTON || m_type == EntityType::LEVER || m_type == EntityType::DOOR || m_type == EntityType::MOVING_PLATFORM)
	{
		std::string linkText = "Linked Activator";
		if (m_type == EntityType::BUTTON || m_type == EntityType::LEVER)
		{
			linkText = "Linked Activatable";
		}

		UIWidget* linkedEntityWidget = g_ui->CreateWidget(m_detailsWidget);
		linkedEntityWidget->SetText(linkText)
			->SetPosition(Vec2(0.5f, 0.4f))
			->SetDimensions(Vec2(0.8f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(SECONDARY_COLOR)
			->SetHoverColor(SECONDARY_COLOR)
			->SetFontSize(4.f)
			->SetRaycastTarget(false);

		m_linkedEntityValueWidget = g_ui->CreateWidget(m_detailsWidget);
		m_linkedEntityValueWidget->SetText("Moving Platform (0x23489F0B)")
			->SetPosition(Vec2(0.5f, 0.35f))
			->SetDimensions(Vec2(0.8f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR)
			->SetFontSize(4.f)
			->SetRaycastTarget(false);

		m_linkButtonWidget = g_ui->CreateWidget(m_detailsWidget);
		m_linkButtonWidget->SetText("Link")
			->SetPosition(Vec2(0.5f, 0.3f))
			->SetDimensions(Vec2(0.8f, 0.05f))
			->SetPivot(Vec2(0.5f, 0.5f))
			->SetAlignment(Vec2(0.5f, 0.5f))
			->SetBackgroundColor(SECONDARY_COLOR)
			->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
			->SetColor(PRIMARY_COLOR)
			->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetFontSize(4.f)
			->SetBorderColor(PRIMARY_COLOR)
			->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT)
			->SetBorderRadius(0.2f)
			->SetBorderWidth(0.1f)
			->SetClickEventName(Stringf("LinkEntity entity=%d", m_uid.m_uid));

		if (m_type == EntityType::MOVING_PLATFORM)
		{
			UIWidget* movementDirectionTextWidget = g_ui->CreateWidget(m_detailsWidget);
			movementDirectionTextWidget->SetText("Movement Direction")
				->SetPosition(Vec2(0.5f, 0.2f))
				->SetDimensions(Vec2(0.8f, 0.05f))
				->SetPivot(Vec2(0.5f, 0.5f))
				->SetAlignment(Vec2(0.5f, 0.5f))
				->SetColor(SECONDARY_COLOR)
				->SetHoverColor(SECONDARY_COLOR)
				->SetFontSize(4.f)
				->SetRaycastTarget(false);

			m_movementDirButtonX = g_ui->CreateWidget(m_detailsWidget);
			m_movementDirButtonX->SetText("X")
				->SetPosition(Vec2(0.2f, 0.15f))
				->SetDimensions(Vec2(0.2f, 0.05f))
				->SetPivot(Vec2(0.5f, 0.5f))
				->SetAlignment(Vec2(0.5f, 0.5f))
				->SetBackgroundColor(SECONDARY_COLOR)
				->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
				->SetColor(PRIMARY_COLOR)
				->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetFontSize(4.f)
				->SetBorderColor(PRIMARY_COLOR)
				->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetBorderRadius(0.2f)
				->SetBorderWidth(0.1f)
				->SetClickEventName(Stringf("ChangeMovementDirection entity=%d direction=%d", m_uid.m_uid, MovementDirection::FORWARD_BACK));

			m_movementDirButtonY = g_ui->CreateWidget(m_detailsWidget);
			m_movementDirButtonY->SetText("Y")
				->SetPosition(Vec2(0.5f, 0.15f))
				->SetDimensions(Vec2(0.2f, 0.05f))
				->SetPivot(Vec2(0.5f, 0.5f))
				->SetAlignment(Vec2(0.5f, 0.5f))
				->SetBackgroundColor(SECONDARY_COLOR)
				->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
				->SetColor(PRIMARY_COLOR)
				->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetFontSize(4.f)
				->SetBorderColor(PRIMARY_COLOR)
				->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetBorderRadius(0.2f)
				->SetBorderWidth(0.1f)
				->SetClickEventName(Stringf("ChangeMovementDirection entity=%d direction=%d", m_uid.m_uid, MovementDirection::LEFT_RIGHT));

			m_movementDirButtonZ = g_ui->CreateWidget(m_detailsWidget);
			m_movementDirButtonZ->SetText("Z")
				->SetPosition(Vec2(0.8f, 0.15f))
				->SetDimensions(Vec2(0.2f, 0.05f))
				->SetPivot(Vec2(0.5f, 0.5f))
				->SetAlignment(Vec2(0.5f, 0.5f))
				->SetBackgroundColor(SECONDARY_COLOR)
				->SetHoverBackgroundColor(SECONDARY_COLOR_VARIANT_LIGHT)
				->SetColor(PRIMARY_COLOR)
				->SetHoverColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetFontSize(4.f)
				->SetBorderColor(PRIMARY_COLOR)
				->SetHoverBorderColor(PRIMARY_COLOR_VARIANT_LIGHT)
				->SetBorderRadius(0.2f)
				->SetBorderWidth(0.1f)
				->SetClickEventName(Stringf("ChangeMovementDirection entity=%d direction=%d", m_uid.m_uid, MovementDirection::UP_DOWN));
		}
	}

	m_detailsWidget->SetVisible(false)->SetFocus(false);
}

void Entity::Update()
{
	m_positionValuesWidget->SetText(Stringf("%.2f, %.2f, %.2f", m_position.x, m_position.y, m_position.z));
	m_orientationValuesWidget->SetText(Stringf("%.2f", m_orientation.m_yawDegrees));
	m_scaleValueWidget->SetText(Stringf("%.2f", m_scale));

	if (m_map->m_game->m_player->m_linkingEntity != this && (m_type == EntityType::BUTTON || m_type == EntityType::LEVER))
	{
		Activator* activator = (Activator*)this;
		Entity* linkedEntity = m_map->GetEntityFromUID(activator->m_activatableUID);
		if (linkedEntity)
		{
			m_linkedEntityValueWidget->SetText(Stringf("%s (%#010x)", m_map->GetEntityNameFromType(linkedEntity->m_type).c_str(), linkedEntity->m_uid));
			m_linkButtonWidget->SetText("Change");
		}
		else
		{
			m_linkedEntityValueWidget->SetText(Stringf("None"));
			m_linkButtonWidget->SetText("Link");
		}
	}

	if (m_map->m_game->m_player->m_linkingEntity != this && (m_type == EntityType::DOOR || m_type == EntityType::MOVING_PLATFORM))
	{
		Activatable* activator = (Activatable*)this;
		Entity* linkedEntity = m_map->GetEntityFromUID(activator->m_activatorUID);
		if (linkedEntity)
		{
			m_linkedEntityValueWidget->SetText(Stringf("%s (%#010x)", m_map->GetEntityNameFromType(linkedEntity->m_type).c_str(), linkedEntity->m_uid));
			m_linkButtonWidget->SetText("Change");
		}
		else
		{
			m_linkedEntityValueWidget->SetText(Stringf("None"));
			m_linkButtonWidget->SetText("Link");
		}
	}
}

void Entity::AppendToBuffer(BufferWriter& writer)
{
	SaveEditorState();

	writer.AppendByte((uint8_t)m_type);
	writer.AppendUint32(m_uid.m_uid);
	writer.AppendVec3(m_editorPosition);
	writer.AppendEulerAngles(m_editorOrientation);
	writer.AppendFloat(m_editorScale);
}

void Entity::SaveEditorState()
{
	m_editorPosition = m_position;
	m_editorOrientation = m_orientation;
	m_editorScale = m_scale;
}

void Entity::ResetState()
{
	m_position = m_editorPosition;
	m_orientation = m_editorOrientation;
	m_scale = m_editorScale;
	m_color = Rgba8::WHITE;
	m_isMouseHovered = false;
	m_isRightHovered = false;
	m_isLeftHovered = false;
	m_isSelected = false;
}

Vec3 const Entity::GetForwardNormal() const
{
	return m_orientation.GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D();
}

OBB3 const Entity::GetBounds() const
{
	Vec3 fwd, left, up;
	m_orientation.GetAsVectors_iFwd_jLeft_kUp(fwd, left, up);
	Vec3 halfDimensions = m_localBounds.GetDimensions() * m_scale * 0.5f;
	OBB3 bounds(m_position + Vec3::SKYWARD * m_localBounds.GetDimensions().z * m_scale * 0.5f, halfDimensions, fwd, left);

	return bounds;
}

ArchiLeapRaycastResult3D Entity::Raycast(Vec3 const& rayStartPos, Vec3 const& fwdNormal, float maxDistance)
{
	RaycastResult3D raycastResult = RaycastVsOBB3(rayStartPos, fwdNormal, maxDistance, GetBounds());
	return ArchiLeapRaycastResult3D(raycastResult, this);
}

Rgba8 Entity::GetColor() const
{
	Player* const& player = m_map->m_game->m_player;
	if (player->m_state == PlayerState::PLAY)
	{
		return Rgba8::WHITE;
	}

	if (m_isSelected)
	{
		return Rgba8(255, 255, 0, 127);
	}

	if (m_isMouseHovered || m_isLeftHovered || m_isRightHovered)
	{
		return Rgba8(0, 255, 255, 127);
	}

	if (!m_pulseTimer.IsStopped())
	{
		return Interpolate(Rgba8::WHITE, SECONDARY_COLOR, 0.5f + 0.5f * sinf(2.f * m_pulseTimer.GetElapsedTime()));
	}
	
	return Rgba8::WHITE;
}

void Entity::SetMouseHovered(bool hovered)
{
	m_isMouseHovered = hovered;
}

void Entity::SetRightHovered(bool hovered)
{
	m_isRightHovered = hovered;
}

void Entity::SetLeftHovered(bool hovered)
{
	m_isLeftHovered = hovered;
}

void Entity::SetSelected(bool selected)
{
	m_isSelected = selected;
	m_detailsWidget->SetVisible(selected);
	m_detailsWidget->SetFocus(selected);
}

bool Entity::IsActivatable() const
{
	return m_type == EntityType::DOOR || m_type == EntityType::MOVING_PLATFORM;
}

bool Entity::IsInteractable() const
{
	return m_type == EntityType::BUTTON || m_type == EntityType::LEVER;
}
