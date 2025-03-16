#include "Game/Activator.hpp"


void Activator::AppendToBuffer(BufferWriter& writer)
{
	Entity::AppendToBuffer(writer);
	writer.AppendUint32(m_activatableUID.m_uid);
}
