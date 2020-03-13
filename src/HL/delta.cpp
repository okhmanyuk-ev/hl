#include "delta.h"

#include <Common/buffer_helpers.h>
#include <cassert>
#include <stdexcept>

using namespace HL;

const Delta::Table Delta::MetaDescription =
{
	{ "fieldType", DT_INTEGER, 32, 1.0f, 1.0f },
	{ "fieldName", DT_STRING, },
	{ "fieldOffset", DT_INTEGER, 16, 1.0f, 1.0f },
	{ "fieldSize", DT_INTEGER, 8, 1.0f, 1.0f },
	{ "significant_bits", DT_INTEGER, 8, 1.0f, 1.0f },
	{ "premultiply", DT_FLOAT, 32, 4000.0f, 1.0f },
	{ "postmultiply", DT_FLOAT, 32, 4000.0f, 1.0f }
};

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

		resultField.name = field.name;

		switch (type)
		{
		case DT_BYTE:
		case DT_SHORT:
		case DT_INTEGER:
		{
			if (sign)
				resultField.value = static_cast<int64_t>(Common::BufferHelpers::ReadSBits(msg, field.bits));
			else
				resultField.value = static_cast<int64_t>(msg.readBits(field.bits));

			assert(field.scale == 1.0f);
			assert(field.pscale == 1.0f);
		}
		
		break;

		case DT_TIMEWINDOW_8:
			resultField.value = static_cast<float>(Common::BufferHelpers::ReadSBits(msg, 8));
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

			resultField.value = value;
			break;
		}
		case DT_ANGLE:
			resultField.value = Common::BufferHelpers::ReadBitAngle(msg, field.bits);
			break;

		case DT_STRING:
			resultField.value = Common::BufferHelpers::ReadString(msg);
			break;
		
		default:
			throw std::runtime_error(("unknown delta field: " + std::to_string(type)).c_str());
			break;
		}

		result.push_back(resultField);
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
#define S_INT(X, Y) if (f.name == #X) field.Y = std::get<int64_t>(f.value)
#define S_FLOAT(X, Y) if (f.name == #X) field.Y = std::get<float>(f.value)
#define S_STR(X, Y) if (f.name == #X) field.Y = std::get<std::string>(f.value)

#define C_INT(X, Y) else S_INT(X, Y)
#define C_FLOAT(X, Y) else S_FLOAT(X, Y)
#define C_STR(X, Y) else S_STR(X, Y)

	auto result = read(msg, MetaDescription);

	for (auto& f : result)
	{
		S_INT(fieldType, type);
		C_STR(fieldName, name);
		//C_INT(fieldOffset, offset);
		//C_INT(fieldSize, size);
		C_INT(significant_bits, bits);
		C_FLOAT(premultiply, scale);
		C_FLOAT(postmultiply, pscale);
	}

#undef S_INT
#undef S_FLOAT
#undef S_STR

#undef C_INT
#undef C_FLOAT
#undef C_STR
}

void Delta::readClientData(Common::BitBuffer& msg, Protocol::ClientData& clientData)
{
#define S_INT2(X, Y) if (f.name == #X) clientData.Y = std::get<int64_t>(f.value);
#define S_FLOAT2(X, Y) if (f.name == #X) clientData.Y = std::get<float>(f.value);
#define S_STR2(X, Y) if (f.name == #X) clientData.Y = std::get<std::string>(f.value);

#define C_INT2(X, Y) else S_INT2(X, Y)
#define C_FLOAT2(X, Y) else S_FLOAT2(X, Y)
#define C_STR2(X, Y) else S_STR2(X, Y)

#define S_INT(X) S_INT2(X, X)
#define S_FLOAT(X) S_FLOAT2(X, X) 
#define S_STR(X) S_STR2(X, X)

#define C_INT(X) C_INT2(X, X)
#define C_FLOAT(X) C_FLOAT2(X, X)
#define C_STR(X) S_STR2(X, X)

	auto result = read(msg, mTables.at("clientdata_t"));

	for (auto& f : result)
	{
		S_FLOAT(origin[0])
		C_FLOAT(origin[1])
		C_FLOAT(origin[2])
	
		C_FLOAT(velocity[0])
		C_FLOAT(velocity[1])
		C_FLOAT(velocity[2])

		C_INT(viewmodel)
	
		C_FLOAT(punchangle[0])
		C_FLOAT(punchangle[1])
		C_FLOAT(punchangle[2])
	
		C_INT(flags)
		C_INT(waterlevel)
		C_INT(watertype)

		C_FLOAT(view_ofs[0])
		C_FLOAT(view_ofs[1])
		C_FLOAT(view_ofs[2])

		C_FLOAT(health)

		C_INT(bInDuck)
		C_INT(weapons)

		C_INT(flTimeStepSound)
		C_INT(flDuckTime)
		C_INT(flSwimTime)
		C_INT(waterjumptime)

		C_FLOAT(maxspeed)
		C_FLOAT(fov)

		C_INT(weaponanim)
	
		C_INT(m_iId)
		C_INT(ammo_shells)
		C_INT(ammo_nails)
		C_INT(ammo_cells)
		C_INT(ammo_rockets)
		C_FLOAT(m_flNextAttack)

		C_INT(tfstate)
	
		C_INT(pushmsec)

		C_INT(deadflag)
	
		C_STR(physinfo)

		C_INT(iuser1)
		C_INT(iuser2)
		C_INT(iuser3)
		C_INT(iuser4)

		C_FLOAT(fuser1)
		C_FLOAT(fuser2)
		C_FLOAT(fuser3)
		C_FLOAT(fuser4)

		C_FLOAT(vuser1[0])
		C_FLOAT(vuser1[1])
		C_FLOAT(vuser1[2])

		C_FLOAT(vuser2[0])
		C_FLOAT(vuser2[1])
		C_FLOAT(vuser2[2])

		C_FLOAT(vuser3[0])
		C_FLOAT(vuser3[1])
		C_FLOAT(vuser3[2])

		C_FLOAT(vuser4[0])
		C_FLOAT(vuser4[1])
		C_FLOAT(vuser4[2])
	};

#undef S_INT2
#undef S_FLOAT2
#undef S_STR2

#undef C_INT2
#undef C_FLOAT2
#undef C_STR2

#undef S_INT
#undef S_FLOAT
#undef S_STR

#undef C_INT
#undef C_FLOAT
#undef C_STR
}

void Delta::readWeaponData(Common::BitBuffer& msg, Protocol::WeaponData& weaponData)
{
#define S_INT2(X, Y) if (f.name == #X) weaponData.Y = std::get<int64_t>(f.value);
#define S_FLOAT2(X, Y) if (f.name == #X)  weaponData.Y = std::get<float>(f.value);
#define S_STR2(X, Y) if (f.name == #X)  weaponData.Y = std::get<std::string>(f.value);

#define C_INT2(X, Y) else S_INT2(X, Y)
#define C_FLOAT2(X, Y) else S_FLOAT2(X, Y)
#define C_STR2(X, Y) else S_STR2(X, Y)

#define S_INT(X) S_INT2(X, X)
#define S_FLOAT(X) S_FLOAT2(X, X) 
#define S_STR(X) S_STR2(X, X)

#define C_INT(X) C_INT2(X, X)
#define C_FLOAT(X) C_FLOAT2(X, X)
#define C_STR(X) S_STR2(X, X)

	auto result = read(msg, mTables.at("weapon_data_t"));

	for (auto& f : result)
	{
		S_INT(m_iId)
		C_INT(m_iClip)
		
		C_FLOAT(m_flNextPrimaryAttack)
		C_FLOAT(m_flNextSecondaryAttack)
		C_FLOAT(m_flTimeWeaponIdle)
		
		C_INT(m_fInReload)
		C_INT(m_fInSpecialReload)
		C_FLOAT(m_flNextReload)
		C_FLOAT(m_flPumpTime)
		C_FLOAT(m_fReloadTime)

		C_FLOAT(m_fAimedDamage)
		C_FLOAT(m_fNextAimBonus)
		C_INT(m_fInZoom)
		C_INT(m_iWeaponState)

		C_INT(iuser1)
		C_INT(iuser2)
		C_INT(iuser3)
		C_INT(iuser4)

		C_FLOAT(fuser1)
		C_FLOAT(fuser2)
		C_FLOAT(fuser3)
		C_FLOAT(fuser4)
	};

#undef S_INT2
#undef S_FLOAT2
#undef S_STR2

#undef C_INT2
#undef C_FLOAT2
#undef C_STR2

#undef S_INT
#undef S_FLOAT
#undef S_STR

#undef C_INT
#undef C_FLOAT
#undef C_STR
}

void Delta::readEvent(Common::BitBuffer& msg, Protocol::EventArgs& evt)
{
#define S_INT2(X, Y) if (f.name == #X) evt.Y = std::get<int64_t>(f.value);
#define S_FLOAT2(X, Y) if (f.name == #X) evt.Y = std::get<float>(f.value);
#define S_STR2(X, Y) if (f.name == #X) evt.Y = std::get<std::string>(f.value);

#define C_INT2(X, Y) else S_INT2(X, Y)
#define C_FLOAT2(X, Y) else S_FLOAT2(X, Y)
#define C_STR2(X, Y) else S_STR2(X, Y)

#define S_INT(X) S_INT2(X, X)
#define S_FLOAT(X) S_FLOAT2(X, X) 
#define S_STR(X) S_STR2(X, X)

#define C_INT(X) C_INT2(X, X)
#define C_FLOAT(X) C_FLOAT2(X, X)
#define C_STR(X) S_STR2(X, X)

	auto result = read(msg, mTables.at("event_t"));

	for (auto& f : result)
	{
		S_INT(entindex)
		C_FLOAT(origin[0])
		C_FLOAT(origin[1])
		C_FLOAT(origin[2])
		C_FLOAT(angles[0])
		C_FLOAT(angles[1])
		C_FLOAT(angles[2])
		C_INT(ducking)
		C_FLOAT(fparam1)
		C_FLOAT(fparam2)
		C_INT(iparam1)
		C_INT(iparam2)
		C_INT(bparam1)
		C_INT(bparam2)
	}

#undef S_INT2
#undef S_FLOAT2
#undef S_STR2

#undef C_INT2
#undef C_FLOAT2
#undef C_STR2

#undef S_INT
#undef S_FLOAT
#undef S_STR

#undef C_INT
#undef C_FLOAT
#undef C_STR
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
#define S_INT2(X, Y) if (f.name == #X) entity.Y = std::get<int64_t>(f.value);
#define S_FLOAT2(X, Y) if (f.name == #X) entity.Y = std::get<float>(f.value); 
#define S_STR2(X, Y) if (f.name == #X) entity.Y = std::get<std::string>(f.value);

#define C_INT2(X, Y) else S_INT2(X, Y)
#define C_FLOAT2(X, Y) else S_FLOAT2(X, Y)
#define C_STR2(X, Y) else S_STR2(X, Y)

#define S_INT(X) S_INT2(X, X)
#define S_FLOAT(X) S_FLOAT2(X, X) 
#define S_STR(X) S_STR2(X, X)

#define C_INT(X) C_INT2(X, X)
#define C_FLOAT(X) C_FLOAT2(X, X)
#define C_STR(X) S_STR2(X, X)

	auto result = read(msg, mTables.at(table));

	for (auto& f : result)
	{
		S_FLOAT(origin[0])
		C_FLOAT(origin[1])
		C_FLOAT(origin[2])

		C_FLOAT(angles[0])
		C_FLOAT(angles[1])
		C_FLOAT(angles[2])

		C_INT(modelindex)
		C_INT(sequence)
		C_FLOAT(frame)
		C_INT(colormap)
		C_INT(skin)
		C_INT(solid)
		C_INT(effects)
		C_FLOAT(scale)

		C_INT(eflags)

		C_INT(rendermode)
		C_INT(renderamt)
		C_INT2(rendercolor.r, rendercolor[0])
		C_INT2(rendercolor.g, rendercolor[1])
		C_INT2(rendercolor.b, rendercolor[2])
		C_INT(renderfx)

		C_INT(movetype)
		C_FLOAT(animtime)
		C_FLOAT(framerate)
		C_INT(body)

		C_INT(controller[0])
		C_INT(controller[1])
		C_INT(controller[2])
		C_INT(controller[3])

		C_INT(blending[0])
		C_INT(blending[1])

		C_FLOAT(velocity[0])
		C_FLOAT(velocity[1])
		C_FLOAT(velocity[2])

		C_FLOAT(mins[0])
		C_FLOAT(mins[1])
		C_FLOAT(mins[2])

		C_FLOAT(maxs[0])
		C_FLOAT(maxs[1])
		C_FLOAT(maxs[2])

		C_INT(aiment)

		C_INT(owner)

		C_FLOAT(friction)
		C_FLOAT(gravity)

		C_INT(team)
		C_INT(playerclass)
		C_INT(health)
		C_INT(spectator)
		C_INT(weaponmodel)
		C_INT(gaitsequence)

		C_FLOAT(basevelocity[0])
		C_FLOAT(basevelocity[1])
		C_FLOAT(basevelocity[2])

		C_INT(usehull)
		C_INT(oldbuttons)
		C_INT(onground)
		C_INT(iStepLeft)
		C_FLOAT(flFallVelocity)

		// TODO: where is fov ?

		C_INT(weaponanim)

		C_FLOAT(startpos[0])
		C_FLOAT(startpos[1])
		C_FLOAT(startpos[2])

		C_FLOAT(endpos[0])
		C_FLOAT(endpos[1])
		C_FLOAT(endpos[2])

		C_FLOAT(impacttime)
		C_FLOAT(starttime)

		C_INT(iuser1)
		C_INT(iuser2)
		C_INT(iuser3)
		C_INT(iuser4)

		C_FLOAT(fuser1)
		C_FLOAT(fuser2)
		C_FLOAT(fuser3)
		C_FLOAT(fuser4)

		C_FLOAT(vuser1[0])
		C_FLOAT(vuser1[1])
		C_FLOAT(vuser1[2])

		C_FLOAT(vuser2[0])
		C_FLOAT(vuser2[1])
		C_FLOAT(vuser2[2])

		C_FLOAT(vuser3[0])
		C_FLOAT(vuser3[1])
		C_FLOAT(vuser3[2])

		C_FLOAT(vuser4[0])
		C_FLOAT(vuser4[1])
		C_FLOAT(vuser4[2])
	}

#undef S_INT2
#undef S_FLOAT2
#undef S_STR2

#undef C_INT2
#undef C_FLOAT2
#undef C_STR2

#undef S_INT
#undef S_FLOAT
#undef S_STR

#undef C_INT
#undef C_FLOAT
#undef C_STR
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