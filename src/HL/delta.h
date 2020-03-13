#pragma once

#include <string>
#include <stdint.h>
#include <list>
#include <vector>
#include <variant>
#include <optional>
#include <unordered_map>

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
	private:
		struct Field
		{
			std::string name;
			int type;
		//	int size;
			int bits;
			float scale;
			float pscale;
		//	int offset;
		};

		using Table = std::list<Field>;

		struct ReadResultField
		{
			std::string name;
			std::variant<int64_t, float, std::string> value;
		};

		using ReadResult = std::list<ReadResultField>;

		struct WriteField
		{
			int index;
			int valueInt;
			float valueFloat;
			std::string valueStr;
		};

		typedef std::list<WriteField> WriteFields;

	public:
		void clear();

		void add(Common::BitBuffer& msg, const std::string& name, uint32_t fieldCount);

		void readClientData(Common::BitBuffer& msg, Protocol::ClientData& clientData);
		void readWeaponData(Common::BitBuffer& msg, Protocol::WeaponData& weaponData);
		void readEvent(Common::BitBuffer& msg, Protocol::EventArgs& evt);
		void readEntityNormal(Common::BitBuffer& msg, Protocol::Entity& entity);
		void readEntityPlayer(Common::BitBuffer& msg, Protocol::Entity& entity);
		void readEntityCustom(Common::BitBuffer& msg, Protocol::Entity& entity);
		
		void writeUserCmd(Common::BitBuffer& msg, const Protocol::UserCmd& newCmd, const Protocol::UserCmd& oldCmd);

	private:
		void read(Common::BitBuffer& msg, Protocol::Entity& entity, const std::string& table);
		void read(Common::BitBuffer& msg, Field& field);

	private:
		std::optional<ReadResult> read(Common::BitBuffer& msg, const std::string& name);
		ReadResult read(Common::BitBuffer& msg, const Table& table);

		void write(Common::BitBuffer& msg, const Table& table, const WriteFields& writeFields);

	private:
		std::unordered_map<std::string, Table> mTables;

	private:
		static const Table MetaDescription;

	};
}