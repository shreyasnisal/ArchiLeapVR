#include "Game/Tile.hpp"

#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/HandController.hpp"
#include "Game/Map.hpp"
#include "Game/Player.hpp"
#include "Game/PlayerPawn.hpp"

#include "Game/GameMathUtils.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"


Tile::Tile(Map* map, EntityUID uid, TileDefinition const& definition, Vec3 const& position, EulerAngles const& orientation, float scale)
	: Entity(map, uid, position, orientation, scale, !strcmp(definition.m_name.c_str(), "Block1x1") ? EntityType::TILE_GRASS : EntityType::TILE_DIRT)
	, m_definition(definition)
{
	m_model = m_definition.m_model;
	m_localBounds = m_definition.m_bounds;
}

void Tile::Update()
{
	Entity::Update();
}

void Tile::Render() const
{
	Mat44 transform = Mat44::CreateTranslation3D(m_position);
	transform.Append(m_orientation.GetAsMatrix_iFwd_jLeft_kUp());
	transform.AppendScaleUniform3D(m_scale);

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->SetModelConstants(transform, GetColor());
	g_renderer->DrawIndexBuffer(m_model->GetVertexBuffer(), m_model->GetIndexBuffer(), m_model->GetIndexCount());
}

void Tile::HandlePlayerInteraction()
{
	Player* player = m_map->m_game->m_player;
	PlayerPawn* playerPawn = player->m_pawn;

	if (player->m_state != PlayerState::PLAY)
	{
		return;
	}

	Vec3& playerPawnPosition = playerPawn->m_position;
	Vec3 playerPawnPositionBeforePush(playerPawnPosition);
	Vec3 pawnCylinderTop(playerPawnPosition + Vec3::SKYWARD * PlayerPawn::PLAYER_HEIGHT);
	if (PushZCylinderOutOfFixedZOBB3(playerPawnPosition, pawnCylinderTop, PlayerPawn::PLAYER_RADIUS, GetBounds()))
	{
		if (playerPawnPosition.z > playerPawnPositionBeforePush.z)
		{
			// Fall Damage
			int fallDamage = RoundDownToInt(RangeMapClamped(-playerPawn->m_velocity.z, GRAVITY, GRAVITY * 4.f, 0.f, 50.f));
			if (fallDamage > 0)
			{
				playerPawn->m_health -= fallDamage;
			}

			playerPawn->m_velocity.z = 0.f;
			playerPawn->m_isGrounded = true;
		}
	}

	if (player->m_state == PlayerState::PLAY)
	{
		HandController* leftController = player->m_leftController;
		Vec3& leftControllerPosition = leftController->m_worldPosition;
		Vec3 leftControllerPositionBeforePush = leftControllerPosition;
		PushSphereOutOfFixedOBB3(leftControllerPosition, Player::CONTROLLER_RADIUS, GetBounds());
		if (leftControllerPositionBeforePush.z < leftControllerPosition.z && playerPawn->m_velocity.z < 0.f && leftController->GetController().GetGrip())
		{
			playerPawn->m_velocity.z = 0.f;
			player->m_pawn->m_isHangingByLeftHand = true;
		}

		HandController* rightController = player->m_rightController;
		Vec3& rightControllerPosition = rightController->m_worldPosition;
		Vec3 rightControllerPositionBeforePush = rightControllerPosition;
		PushSphereOutOfFixedOBB3(rightControllerPosition, Player::CONTROLLER_RADIUS, GetBounds());
		if (rightControllerPositionBeforePush.z < rightControllerPosition.z && playerPawn->m_velocity.z < 0.f && rightController->GetController().GetGrip())
		{
			playerPawn->m_velocity.z = 0.f;
			player->m_pawn->m_isHangingByRightHand = true;
		}
	}
}
