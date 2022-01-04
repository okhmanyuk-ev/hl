#pragma once

#include <string>
#include <stdint.h>
#include <list>
#include <vector>
#include <variant>
#include <optional>
#include <unordered_map>
#include <map>

#include <common/bitbuffer.h>
#include "protocol.h"

static const int DT_BYTE = 1 << 0;
static const int DT_SHORT = 1 << 1;
static const int DT_FLOAT = 1 << 2;
static const int DT_INTEGER = 1 << 3;
static const int DT_ANGLE = 1 << 4;
static const int DT_TIMEWINDOW_8 = 1 << 5;
static const int DT_TIMEWINDOW_BIG = 1 << 6;
static const int DT_STRING = 1 << 7;
static const int DT_SIGNED = 1 << 31;

static const std::string S_DELTA_EVENT = "event_t";
static const std::string S_DELTA_WEAPON_DATA = "weapon_data_t";
static const std::string S_DELTA_ENTITY_STATE = "entity_state_t";
static const std::string S_DELTA_ENTITY_STATE_PLAYER = "entity_state_player_t";
static const std::string S_DELTA_CUSTOM_ENTITY_STATE = "custom_entity_state_t";
static const std::string S_DELTA_USERCMD = "usercmd_t";
static const std::string S_DELTA_CLIENTDATA = "clientdata_t";
static const std::string S_DELTA_METADELTA = "g_MetaDelta";

namespace HL
{
	class Delta
	{
	public:
		struct Field
		{
			std::string name;
			int type;
			int bits;
			float scale;
			float pscale;
			int size; // unused
			int offset; // unused
		};

		using Table = std::vector<Field>;

		using VariantField = std::variant<int64_t, float, std::string>;
		using ReadFields = std::unordered_map<std::string, VariantField>;
		using WriteFields = std::map<int, VariantField>;

	public:
		void clear();

		void add(BitBuffer& msg, const std::string& name, uint32_t fieldCount);

		void readClientData(BitBuffer& msg, Protocol::ClientData& clientData);
		void readWeaponData(BitBuffer& msg, Protocol::WeaponData& weaponData);
		void readEvent(BitBuffer& msg, Protocol::EventArgs& evt);
		void readEntityNormal(BitBuffer& msg, Protocol::Entity& entity);
		void readEntityPlayer(BitBuffer& msg, Protocol::Entity& entity);
		void readEntityCustom(BitBuffer& msg, Protocol::Entity& entity);
		
		void writeUserCmd(BitBuffer& msg, const Protocol::UserCmd& newCmd, const Protocol::UserCmd& oldCmd);

	private:
		void read(BitBuffer& msg, Protocol::Entity& entity, const std::string& table);
		void read(BitBuffer& msg, Field& field);

	private:
		ReadFields read(BitBuffer& msg, const Table& table);
		void write(BitBuffer& msg, const Table& table, const WriteFields& fields);

	private:
		std::unordered_map<std::string, Table> mTables;
	};
}