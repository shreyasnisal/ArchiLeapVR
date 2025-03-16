#pragma once

#include "Game/EntityUID.hpp"

#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/UI/UIWidget.hpp"

#include <string>


#if defined (DELETE)
	#undef DELETE
#endif


class AudioSystem;
class Renderer;
class Window;
class BitmapFont;
class OpenXR;
class ModelLoader;
class RandomNumberGenerator;
class VertexBuffer;
class UISystem;

class App;
class Entity;

extern App*							g_app;
extern Renderer*					g_renderer;
extern Window*						g_window;
extern BitmapFont*					g_squirrelFont;
extern OpenXR*						g_openXR;
extern ModelLoader*					g_modelLoader;
extern RandomNumberGenerator*		g_rng;
extern UISystem*					g_ui;
extern AudioSystem*					g_audio;

constexpr float SCREEN_SIZE_Y		= 8000.f;
constexpr float NEAR_PLANE_DISTANCE = 0.01f;
constexpr float FAR_PLANE_DISTANCE = 1000.f;

constexpr float GRAVITY = 9.8f;

constexpr float MODEL_SCALE = 2.f;

const Rgba8 PRIMARY_COLOR(15, 25, 50, 255);
const Rgba8 PRIMARY_COLOR_VARIANT_LIGHT(30, 50, 90, 255);
const Rgba8 PRIMARY_COLOR_VARIANT_DARK(10, 15, 35, 255);

const Rgba8 SECONDARY_COLOR(140, 50, 230, 255);
const Rgba8 SECONDARY_COLOR_VARIANT_LIGHT(170, 90, 250, 255);
const Rgba8 SECONDARY_COLOR_VARIANT_DARK(110, 40, 200, 255);

const Rgba8 TERTIARY_COLOR(50, 220, 230, 255);
const Rgba8 TERTIARY_COLOR_VARIANT_LIGHT(90, 250, 255, 255);
const Rgba8 TERTIARY_COLOR_VARIANT_DARK(30, 180, 190, 255);

const Vec3 SUN_DIRECTION = Vec3(1.f, 2.f, -2.f).GetNormalized();
constexpr float SUN_INTENSITY = 0.9f;

constexpr int g_archiLeapShaderConstantsSlot = 4;
struct ArchiLeapShaderConstants
{
	Vec4 m_skyColor;
	float m_fogStartDistance = 5.f;
	float m_fogEndDistance = 10.f;
	float m_fogMaxAlpha = 0.9f;
	float padding0;
};

struct ArchiLeapRaycastResult3D : RaycastResult3D
{
public:
	~ArchiLeapRaycastResult3D() = default;
	ArchiLeapRaycastResult3D() = default;
	ArchiLeapRaycastResult3D(RaycastResult3D const& raycastResult, Entity* impactEntity) : RaycastResult3D(raycastResult), m_impactEntity(impactEntity) {}

public:
	Vec2 m_screenImpactCoords = Vec2::ZERO;
	UIWidget* m_impactWidget = nullptr;
	Entity* m_impactEntity = nullptr;
};

enum class EntityType
{
	NONE,

	TILE_GRASS,
	TILE_DIRT,
	LEVER,
	DOOR,
	BUTTON,
	MOVING_PLATFORM,
	COIN,
	CRATE,
	ENEMY_ORC,
	FLAG,

	NUM
};

enum class PlayerState
{
	NONE = -1,
	EDITOR_CREATE,
	EDITOR_EDIT,
	PLAY,
	NUM,
};

enum class ActionType
{
	NONE = -1,
	CREATE,
	TRANSLATE,
	ROTATE,
	SCALE,
	CLONE,
	SELECT,
	LINK,
	DELETE,
	NUM
};

enum class MapMode
{
	NONE = -1,
	PLAY,
	EDIT,
	NUM
};

class Activator;
class Activatable;

struct Action
{
	ActionType m_actionType = ActionType::NONE;
	std::vector<Entity*> m_createdEntities;
	std::vector<Vec3> m_createdEntityPositions;
	std::vector<EulerAngles> m_createdEntityOrientations;
	std::vector<float> m_createdEntityScales;
	Entity* m_actionEntity = nullptr;
	Vec3 m_actionEntityPreviousPosition = Vec3::ZERO;
	EulerAngles m_actionEntityPreviousOrientation = EulerAngles::ZERO;
	float m_actionEntityPreviousScale = 1.f;
	Activator* m_activator = nullptr;
	EntityUID m_prevLinkedActivatable = EntityUID::INVALID;
	Activatable* m_activatable = nullptr;
	EntityUID m_prevLinkedActivator = EntityUID::INVALID;
};

enum class AxisLockDirection
{
	NONE,
	X,
	Y,
	Z,
	NUM
};

std::string GetAxisLockDirectionStr(AxisLockDirection axisLockDirection);

constexpr char const* SAVEFILE_4CC_CODE = "GHAL";
constexpr uint8_t SAVEFILE_VERSION = 2;
