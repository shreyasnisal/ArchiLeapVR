#include "Game/EntityUID.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"


EntityUID::EntityUID(unsigned int uid)
	: m_uid(uid)
{
}

EntityUID::EntityUID(unsigned int index, unsigned int salt)
	: m_uid(index << 16 | salt)
{
}

unsigned int EntityUID::GetIndex() const
{
	return (m_uid & 0xFFFF0000) >> 16;
}

bool EntityUID::operator==(EntityUID compareUID)
{
	return m_uid == compareUID.m_uid;
}

bool EntityUID::operator!=(EntityUID compareUID)
{
	return m_uid != compareUID.m_uid;
}
