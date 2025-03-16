#include "Game/Activatable.hpp"


void Activatable::AppendToBuffer(BufferWriter& writer)
{
	Entity::AppendToBuffer(writer);
	writer.AppendUint32(m_activatorUID.m_uid);
}
