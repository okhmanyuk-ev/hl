#include "delta.h"

#include <Common/buffer_helpers.h>
#include <cassert>
#include <stdexcept>

using namespace HL;

namespace
{
	template <class T, class U>
	bool ReadResultValue(const Delta::ReadResult& result, const std::string& name, U& value) {
		if (result.count(name) == 0)
			return false;

		value = std::get<T>(result.at(name));
		return true;
	};

	const Delta::Table MetaDescription = {
		{ "fieldType", DT_INTEGER, 32, 1.0f, 1.0f },
		{ "fieldName", DT_STRING, },
		{ "fieldOffset", DT_INTEGER, 16, 1.0f, 1.0f },
		{ "fieldSize", DT_INTEGER, 8, 1.0f, 1.0f },
		{ "significant_bits", DT_INTEGER, 8, 1.0f, 1.0f },
		{ "premultiply", DT_FLOAT, 32, 4000.0f, 1.0f },
		{ "postmultiply", DT_FLOAT, 32, 4000.0f, 1.0f }
	};
}

void Delta::clear()
{
	mTables.clear();
}

void Delta::add(Common::BitBuffer& msg, const std::string& name, uint32_t fieldCount)
{
	Table table;

	for (uint32_t i = 0; i < fieldCount; i++)
	{
		Field field;
	
		field.bits = 1;
		field.scale = 1.0f;
		field.pscale = 1.0f;
		
		read(msg, field);
		
		table.push_back(field);
	}

	mTables.insert({ name, table });
}

Delta::ReadResult Delta::read(Common::BitBuffer& msg, const Table& table)
{
	uint64_t marks = 0;
	uint32_t count = msg.readBits(3);

	msg.read(&marks, count);
		
	ReadResult result;

	int i = -1;

	for (auto& field : table)
	{
		i++;

		if (!(marks & (1ULL << i)))
			continue;

		bool sign = field.type & DT_SIGNED;
		int type = field.type & ~DT_SIGNED;

		ReadResultField resultField;

		switch (type)
		{
		case DT_BYTE:
		case DT_SHORT:
		case DT_INTEGER:
		{
			if (sign)
				resultField = static_cast<int64_t>(Common::BufferHelpers::ReadSBits(msg, field.bits));
			else
				resultField = static_cast<int64_t>(msg.readBits(field.bits));

			assert(field.scale == 1.0f);
			assert(field.pscale == 1.0f);
		}
		
		break;

		case DT_TIMEWINDOW_8:
			resultField = static_cast<float>(Common::BufferHelpers::ReadSBits(msg, 8));
			break;

		case DT_TIMEWINDOW_BIG:
		case DT_FLOAT:
		{
			float value = 0.0f;

			if (sign)
				value = (float)Common::BufferHelpers::ReadSBits(msg, field.bits);
			else
				value = (float)msg.readBits(field.bits);

			value /= field.scale;
			value *= field.pscale;

			resultField = value;
			break;
		}
		case DT_ANGLE:
			resultField = Common::BufferHelpers::ReadBitAngle(msg, field.bits);
			break;

		case DT_STRING:
			resultField = Common::BufferHelpers::ReadString(msg);
			break;
		
		default:
			throw std::runtime_error(("unknown delta field: " + std::to_string(type)).c_str());
			break;
		}

		result.insert({ field.name, resultField });
	}

	return result;
}

void Delta::write(Common::BitBuffer& msg, const Table& table, const WriteFields& writeFields)
{
	uint64_t marks = 0;
	
	int lastMark = -1;

	for (auto& field : writeFields)
	{
		marks |= 1ULL << field.index;
		lastMark = field.index;
	}

	uint32_t count = 0; 

	if (lastMark != -1)
		count = (lastMark >> 3) + 1;

	msg.writeBits(count, 3);
	msg.write(&marks, count);
	
	for (auto& field : writeFields)
	{
		auto normal = std::next(table.begin(), field.index);
		
		bool sign = normal->type & DT_SIGNED;
		int type = normal->type & ~DT_SIGNED;

		switch (type)
		{
		case DT_BYTE:
		case DT_SHORT:
		case DT_INTEGER:
		{
			assert(normal->scale == 1.0f);
			assert(normal->pscale == 1.0f);

			if (sign)
				Common::BufferHelpers::WriteSBits(msg, field.valueInt, normal->bits);
			else
				msg.writeBits(field.valueInt, normal->bits);
		}
		break;

		case DT_TIMEWINDOW_8:
			Common::BufferHelpers::WriteSBits(msg, (int)field.valueFloat, 8); // TODO: time fix
			break;

		case DT_TIMEWINDOW_BIG:
		case DT_FLOAT:
		{
			float value = field.valueFloat;

			if (normal->scale <= 0.9999 || normal->scale >= 1.0001)
				value /= normal->scale;

			if (normal->pscale <= 0.9999 || normal->pscale >= 1.0001)
				value *= normal->pscale;

			if (sign)
				Common::BufferHelpers::WriteSBits(msg, (int32_t)value, normal->bits);
			else
				msg.writeBits((uint32_t)value, normal->bits);
		}
		break;

		case DT_ANGLE:
			Common::BufferHelpers::WriteBitAngle(msg, field.valueFloat, normal->bits);
			break;

		case DT_STRING:
			Common::BufferHelpers::WriteString(msg, field.valueStr);
			break;

		default:
			throw std::runtime_error(("unknown delta field: " + std::to_string(type)).c_str());
			break;
		}
	}
}

void Delta::read(Common::BitBuffer& msg, Field& field)
{
	auto result = read(msg, MetaDescription);

#define READ_INT(X, Y) ReadResultValue<int64_t>(result, #X, field.Y)
#define READ_FLOAT(X, Y) ReadResultValue<float>(result, #X, field.Y)
#define READ_STR(X, Y) ReadResultValue<std::string>(result, #X, field.Y)

	READ_INT(fieldType, type);
	READ_STR(fieldName, name);
	//READ_INT(fieldOffset, offset);
	//READ_INT(fieldSize, size);
	READ_INT(significant_bits, bits);
	READ_FLOAT(premultiply, scale);
	READ_FLOAT(postmultiply, pscale);
	
#undef READ_INT
#undef READ_FLOAT
#undef READ_STR
}

void Delta::readClientData(Common::BitBuffer& msg, Protocol::ClientData& clientData)
{
	auto result = read(msg, mTables.at("clientdata_t"));

#define READ_INT2(X, Y) ReadResultValue<int64_t>(result, #X, clientData.Y)
#define READ_FLOAT2(X, Y) ReadResultValue<float>(result, #X, clientData.Y)
#define READ_STR2(X, Y) ReadResultValue<std::string>(result, #X, clientData.Y)

#define READ_INT(X) READ_INT2(X, X)
#define READ_FLOAT(X) READ_FLOAT2(X, X)
#define READ_STR(X) READ_STR2(X, X)

	READ_FLOAT(origin[0]);
	READ_FLOAT(origin[1]);
	READ_FLOAT(origin[2]);
	
	READ_FLOAT(velocity[0]);
	READ_FLOAT(velocity[1]);
	READ_FLOAT(velocity[2]);

	READ_INT(viewmodel);
	
	READ_FLOAT(punchangle[0]);
	READ_FLOAT(punchangle[1]);
	READ_FLOAT(punchangle[2]);
	
	READ_INT(flags);
	READ_INT(waterlevel);
	READ_INT(watertype);

	READ_FLOAT(view_ofs[0]);
	READ_FLOAT(view_ofs[1]);
	READ_FLOAT(view_ofs[2]);

	READ_FLOAT(health);

	READ_INT(bInDuck);
	READ_INT(weapons);

	READ_INT(flTimeStepSound);
	READ_INT(flDuckTime);
	READ_INT(flSwimTime);
	READ_INT(waterjumptime);

	READ_FLOAT(maxspeed);
	READ_FLOAT(fov);

	READ_INT(weaponanim);
	
	READ_INT(m_iId);
	READ_INT(ammo_shells);
	READ_INT(ammo_nails);
	READ_INT(ammo_cells);
	READ_INT(ammo_rockets);
	READ_FLOAT(m_flNextAttack);

	READ_INT(tfstate);
	
	READ_INT(pushmsec);

	READ_INT(deadflag);
	
	READ_STR(physinfo);

	READ_INT(iuser1);
	READ_INT(iuser2);
	READ_INT(iuser3);
	READ_INT(iuser4);

	READ_FLOAT(fuser1);
	READ_FLOAT(fuser2);
	READ_FLOAT(fuser3);
	READ_FLOAT(fuser4);

	READ_FLOAT(vuser1[0]);
	READ_FLOAT(vuser1[1]);
	READ_FLOAT(vuser1[2]);

	READ_FLOAT(vuser2[0]);
	READ_FLOAT(vuser2[1]);
	READ_FLOAT(vuser2[2]);

	READ_FLOAT(vuser3[0]);
	READ_FLOAT(vuser3[1]);
	READ_FLOAT(vuser3[2]);

	READ_FLOAT(vuser4[0]);
	READ_FLOAT(vuser4[1]);
	READ_FLOAT(vuser4[2]);

#undef READ_INT2
#undef READ_FLOAT2
#undef READ_STR2

#undef READ_INT
#undef READ_FLOAT
#undef READ_STR
}

void Delta::readWeaponData(Common::BitBuffer& msg, Protocol::WeaponData& weaponData)
{
	auto result = read(msg, mTables.at("weapon_data_t"));

#define READ_INT2(X, Y) ReadResultValue<int64_t>(result, #X, weaponData.Y)
#define READ_FLOAT2(X, Y) ReadResultValue<float>(result, #X, weaponData.Y)
#define READ_STR2(X, Y) ReadResultValue<std::string>(result, #X, weaponData.Y)

#define READ_INT(X) READ_INT2(X, X)
#define READ_FLOAT(X) READ_FLOAT2(X, X)
#define READ_STR(X) READ_STR2(X, X)

	READ_INT(m_iId);
	READ_INT(m_iClip);
		
	READ_FLOAT(m_flNextPrimaryAttack);
	READ_FLOAT(m_flNextSecondaryAttack);
	READ_FLOAT(m_flTimeWeaponIdle);
		
	READ_INT(m_fInReload);
	READ_INT(m_fInSpecialReload);
	READ_FLOAT(m_flNextReload);
	READ_FLOAT(m_flPumpTime);
	READ_FLOAT(m_fReloadTime);

	READ_FLOAT(m_fAimedDamage);
	READ_FLOAT(m_fNextAimBonus);
	READ_INT(m_fInZoom);
	READ_INT(m_iWeaponState);

	READ_INT(iuser1);
	READ_INT(iuser2);
	READ_INT(iuser3);
	READ_INT(iuser4);

	READ_FLOAT(fuser1);
	READ_FLOAT(fuser2);
	READ_FLOAT(fuser3);
	READ_FLOAT(fuser4);

#undef READ_INT2
#undef READ_FLOAT2
#undef READ_STR2

#undef READ_INT
#undef READ_FLOAT
#undef READ_STR
}

void Delta::readEvent(Common::BitBuffer& msg, Protocol::EventArgs& evt)
{
	auto result = read(msg, mTables.at("event_t"));

#define READ_INT2(X, Y) ReadResultValue<int64_t>(result, #X, evt.Y)
#define READ_FLOAT2(X, Y) ReadResultValue<float>(result, #X, evt.Y)
#define READ_STR2(X, Y) ReadResultValue<std::string>(result, #X, evt.Y)

#define READ_INT(X) READ_INT2(X, X)
#define READ_FLOAT(X) READ_FLOAT2(X, X)
#define READ_STR(X) READ_STR2(X, X)

	READ_INT(entindex);
	READ_FLOAT(origin[0]);
	READ_FLOAT(origin[1]);
	READ_FLOAT(origin[2]);
	READ_FLOAT(angles[0]);
	READ_FLOAT(angles[1]);
	READ_FLOAT(angles[2]);
	READ_INT(ducking);
	READ_FLOAT(fparam1);
	READ_FLOAT(fparam2);
	READ_INT(iparam1);
	READ_INT(iparam2);
	READ_INT(bparam1);
	READ_INT(bparam2);

#undef READ_INT2
#undef READ_FLOAT2
#undef READ_STR2

#undef READ_INT
#undef READ_FLOAT
#undef READ_STR
}

void Delta::readEntityNormal(Common::BitBuffer& msg, Protocol::Entity& entity)
{
	read(msg, entity, "entity_state_t");
}

void Delta::readEntityPlayer(Common::BitBuffer& msg, Protocol::Entity& entity)
{
	read(msg, entity, "entity_state_player_t");
}

void Delta::readEntityCustom(Common::BitBuffer& msg, Protocol::Entity& entity)
{
	read(msg, entity, "custom_entity_state_t");
}

void Delta::read(Common::BitBuffer& msg, Protocol::Entity& entity, const std::string& table)
{
	auto result = read(msg, mTables.at(table));

#define READ_INT2(X, Y) ReadResultValue<int64_t>(result, #X, entity.Y)
#define READ_FLOAT2(X, Y) ReadResultValue<float>(result, #X, entity.Y)
#define READ_STR2(X, Y) ReadResultValue<std::string>(result, #X, entity.Y)

#define READ_INT(X) READ_INT2(X, X)
#define READ_FLOAT(X) READ_FLOAT2(X, X)
#define READ_STR(X) READ_STR2(X, X)

	READ_FLOAT(origin[0]);
	READ_FLOAT(origin[1]);
	READ_FLOAT(origin[2]);

	READ_FLOAT(angles[0]);
	READ_FLOAT(angles[1]);
	READ_FLOAT(angles[2]);

	READ_INT(modelindex);
	READ_INT(sequence);
	READ_FLOAT(frame);
	READ_INT(colormap);
	READ_INT(skin);
	READ_INT(solid);
	READ_INT(effects);
	READ_FLOAT(scale);

	READ_INT(eflags);

	READ_INT(rendermode);
	READ_INT(renderamt);
	READ_INT2(rendercolor.r, rendercolor[0]);
	READ_INT2(rendercolor.g, rendercolor[1]);
	READ_INT2(rendercolor.b, rendercolor[2]);
	READ_INT(renderfx);

	READ_INT(movetype);
	READ_FLOAT(animtime);
	READ_FLOAT(framerate);
	READ_INT(body);

	READ_INT(controller[0]);
	READ_INT(controller[1]);
	READ_INT(controller[2]);
	READ_INT(controller[3]);

	READ_INT(blending[0]);
	READ_INT(blending[1]);

	READ_FLOAT(velocity[0]);
	READ_FLOAT(velocity[1]);
	READ_FLOAT(velocity[2]);

	READ_FLOAT(mins[0]);
	READ_FLOAT(mins[1]);
	READ_FLOAT(mins[2]);

	READ_FLOAT(maxs[0]);
	READ_FLOAT(maxs[1]);
	READ_FLOAT(maxs[2]);

	READ_INT(aiment);

	READ_INT(owner);

	READ_FLOAT(friction);
	READ_FLOAT(gravity);

	READ_INT(team);
	READ_INT(playerclass);
	READ_INT(health);
	READ_INT(spectator);
	READ_INT(weaponmodel);
	READ_INT(gaitsequence);

	READ_FLOAT(basevelocity[0]);
	READ_FLOAT(basevelocity[1]);
	READ_FLOAT(basevelocity[2]);

	READ_INT(usehull);
	READ_INT(oldbuttons);
	READ_INT(onground);
	READ_INT(iStepLeft);
	READ_FLOAT(flFallVelocity);

	// TODO: where is fov ?

	READ_INT(weaponanim);

	READ_FLOAT(startpos[0]);
	READ_FLOAT(startpos[1]);
	READ_FLOAT(startpos[2]);

	READ_FLOAT(endpos[0]);
	READ_FLOAT(endpos[1]);
	READ_FLOAT(endpos[2]);

	READ_FLOAT(impacttime);
	READ_FLOAT(starttime);

	READ_INT(iuser1);
	READ_INT(iuser2);
	READ_INT(iuser3);
	READ_INT(iuser4);

	READ_FLOAT(fuser1);
	READ_FLOAT(fuser2);
	READ_FLOAT(fuser3);
	READ_FLOAT(fuser4);

	READ_FLOAT(vuser1[0]);
	READ_FLOAT(vuser1[1]);
	READ_FLOAT(vuser1[2]);

	READ_FLOAT(vuser2[0]);
	READ_FLOAT(vuser2[1]);
	READ_FLOAT(vuser2[2]);

	READ_FLOAT(vuser3[0]);
	READ_FLOAT(vuser3[1]);
	READ_FLOAT(vuser3[2]);

	READ_FLOAT(vuser4[0]);
	READ_FLOAT(vuser4[1]);
	READ_FLOAT(vuser4[2]);

#undef READ_INT2
#undef READ_FLOAT2
#undef READ_STR2

#undef READ_INT
#undef READ_FLOAT
#undef READ_STR
};

void Delta::writeUserCmd(Common::BitBuffer& msg, const Protocol::UserCmd& newCmd, const Protocol::UserCmd& oldCmd)
{

#define S_CHECK(X, Y) if (f.name == #X && newCmd.Y != oldCmd.Y)
#define S_PREPARE WriteField writeField; writeField.index = i;
#define S_PUSH writeFields.push_back(writeField);
#define S_TOTAL(X, Y, F, A) S_CHECK(X, Y) { assert(A); S_PREPARE writeField.F = newCmd.Y; S_PUSH }

#define S_INT2(X, Y) S_TOTAL(X, Y, valueInt, type == DT_BYTE || type == DT_SHORT || type == DT_INTEGER)
#define S_FLOAT2(X, Y) S_TOTAL(X, Y, valueFloat, type == DT_TIMEWINDOW_8 || type == DT_TIMEWINDOW_BIG || type == DT_FLOAT || type == DT_ANGLE)
#define S_STR2(X, Y) S_TOTAL(X, Y, valueString, type == DT_STRING)

#define S_INT(X) S_INT2(X, X)
#define S_FLOAT(X) S_FLOAT2(X, X)
#define S_STR(X) S_STR2(X, X)

#define C_INT2(X, Y) else S_INT2(X, Y)
#define C_FLOAT2(X, Y) else S_FLOAT2(X, Y)
#define C_STR2(X, Y) else S_STR2(X, Y)

#define C_INT(X) C_INT2(X, X)
#define C_FLOAT(X) C_FLOAT2(X, X)
#define C_STR(X) C_STR2(X, X)

	auto table = mTables.at("usercmd_t");

	WriteFields writeFields;

	int i = -1;

	for (auto& f : table)
	{
		int type = f.type & ~DT_SIGNED;
		i++;
		S_INT(lerp_msec)
		C_INT(msec)
		C_FLOAT(viewangles[0])
		C_FLOAT(viewangles[1])
		C_FLOAT(viewangles[2])
		C_FLOAT(forwardmove)
		C_FLOAT(sidemove)
		C_FLOAT(upmove)
		C_INT(lightlevel)
		C_INT(buttons)
		C_INT(impulse)
		C_INT(weaponselect)
		C_INT(impact_index)
		C_FLOAT(impact_position[0])
		C_FLOAT(impact_position[1])
		C_FLOAT(impact_position[2])
	}

	write(msg, table, writeFields);
}