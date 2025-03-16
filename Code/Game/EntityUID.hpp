#pragma once


constexpr unsigned int ENTITYUID_INVALID = 0xFFFFFFFF;

class EntityUID
{
public:
	~EntityUID() = default;
	EntityUID() = default;
	EntityUID(unsigned int uid);
	EntityUID(unsigned int index, unsigned int salt);

	unsigned int GetIndex() const;
	bool operator==(EntityUID compareUID);
	bool operator!=(EntityUID compareUID);

public:
	unsigned int m_uid = ENTITYUID_INVALID;

public:
	static const EntityUID INVALID;
};

inline const EntityUID EntityUID::INVALID{0xFFFF, 0xFFFF};
