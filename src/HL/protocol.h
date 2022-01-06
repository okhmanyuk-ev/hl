#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace HL::Protocol
{
	const int Version = 48;
	const uint16_t DefaultServerPort = 27015;

	namespace Client
	{
		enum class ConnectionlessPacket
		{
			Ping = 'i',
			Info = 'T',
			Players = 'U',
			Rules = 'V',
			Challenge = 'W'
		};

		enum class Message
		{
			Bad = 0,
			Nop = 1,
			Move = 2,
			StringCmd = 3,
			Delta = 4,
			ResourceList = 5,
			TMove = 6,
			FileConsistency = 7,
			VoiceData = 8,
			HLTV = 9,
			CVarValue = 10,
			CVarValue2 = 11
		};
	}

	namespace Server
	{
		enum class ConnectionlessPacket
		{
			Print = 'l',
			Challenge = 'A',
			Accepted = 'B',
			Password = '8',
			Reject = '9',
			Info_Old = 'm',
			Info_New = 'I',
			Players = 'D',
			Rules = 'E',
		};

		enum class Message
		{
			Bad = 0,
			Nop = 1,
			Disconnect = 2,
			Event = 3,
			Version = 4,
			SetView = 5,
			Sound = 6,
			Time = 7,
			Print = 8,
			StuffText = 9,
			SetAngle = 10,
			ServerInfo = 11,
			LightStyle = 12,
			UpdateUserinfo = 13,
			DeltaDescription = 14,
			ClientData = 15,
			StopSound = 16,
			Pings = 17,
			Particle = 18,
			Damage = 19,
			SpawnStatic = 20,
			EventReliable = 21,
			SpawnBaseline = 22,
			TempEntity = 23,
			SetPause = 24,
			SignonNum = 25,
			CenterPrint = 26,
			KilledMonster = 27,
			FoundSecret = 28,
			SpawnStaticSound = 29,
			Intermission = 30,
			Finale = 31,
			CDTrack = 32,
			Restore = 33,
			CutScene = 34,
			WeaponAnim = 35,
			DecalName = 36,
			RoomType = 37,
			AddAngle = 38,
			NewUserMsg = 39,
			PacketEntities = 40,
			DeltaPacketEntities = 41,
			Choke = 42,
			ResourceList = 43,
			NewMoveVars = 44,
			ResourceRequest = 45,
			Customization = 46,
			CrosshairAngle = 47,
			SoundFade = 48,
			FileTxferFailed = 49,
			HLTV = 50,
			Director = 51,
			VoiceInit = 52,
			VoiceData = 53,
			SendExtraInfo = 54,
			TimeScale = 55,
			ResourceLocation = 56,
			SendCVarValue = 57,
			SendCVarValue2 = 58
		};
	}

	// flags
	const int RES_FATALIFMISSING = 1 << 0;
	const int RES_WASMISSING = 1 << 1;
	const int RES_CUSTOM = 1 << 2;
	const int RES_REQUESTED = 1 << 3;
	const int RES_PRECACHED = 1 << 4;
	const int RES_ALWAYS = 1 << 5;
	const int RES_PADDING = 1 << 6;
	const int RES_CHECKFILE = 1 << 7;
	const int RES_RESERVED = 1 << 8; // custom by me

	struct Resource
	{
		enum class Type : int
		{
			Sound = 0,
			Skin = 1,
			Model = 2,
			Decal = 3,
			Generic = 4,
			EventScript = 5,
			World = 6
		};
		
		Type type;
		std::string name;
		int index;
		int size;
		int flags;
		uint8_t hash[16];
		uint8_t reserved[32];
	};

	struct GameMessage
	{
		std::string name;
		uint8_t size;
	};

	enum class TempEntity
	{
		BeamPoints = 0,        // Beam effect between two points
		BeamEntPoint = 1,        // Beam effect between point and entity
		GunShot = 2,        // Particle effect plus ricochet sound
		Explosion = 3,        // Additive sprite, 2 dynamic lights, flickering particles, explosion sound, move vertically 8 pps
		//TE_EXPLFLAG_NONE = 0;        // All flags clear makes default Half-Life explosion
		//TE_EXPLFLAG_NOADDITIVE = 1 shl 0;       // Sprite will be drawn opaque (ensure that the sprite you send is a non-additive sprite)
		//TE_EXPLFLAG_NODLIGHTS = 1 shl 1;        // Do not render dynamic lights
		//TE_EXPLFLAG_NOSOUND = 1 shl 2;        // Do not play client explosion sound
		//TE_EXPLFLAG_NOPARTICLES = 1 shl 3;        // Do not draw particles
		TarExplosion = 4,        // Quake1 "tarbaby" explosion with sound
		Smoke = 5,       // Alphablend sprite, move vertically 30 pps
		Tracer = 6,        // Tracer effect from point to point
		Lightning = 7,        // TE_BEAMPOINTS with simplified parameters
		BeamEnts = 8,
		Sparks = 9,        // 8 random tracers with gravity, ricochet sprite
		LavaSplash = 10,       // Quake1 lava splash
		Teleport = 11,       // Quake1 teleport splash
		Explosion2 = 12,       // Quake1 colormaped (base palette) particle explosion with sound
		BspDecal = 13,       // Decal from the .BSP file
		Implosion = 14,       // Tracers moving toward a point
		SpriteTrail = 15,       // Line of moving glow sprites with gravity, fadeout, and collisions
		Sprite = 17,       // Additive sprite, plays 1 cycle
		BeamSprite = 18,       // A beam with a sprite at the end
		BeamTorus = 19,       // Screen aligned beam ring, expands to max radius over lifetime
		BeamDisk = 20,       // Disk that expands to max radius over lifetime
		BeamCylinder = 21,       // Cylinder that expands to max radius over lifetime
		BeamFollow = 22,       // Create a line of decaying beam segments until entity stops moving
		GlowSprite = 23,
		BeamRing = 24,       // Connect a beam ring to two entities
		StreakSplash = 25,       // Oriented shower of tracers
		DLight = 27,       // Dynamic light, effect world, minor entity effect
		ELight = 28,       // Point entity light, no world effect
		TextMessage = 29,
		Line = 30,
		Box = 31,
		KillBeam = 99,
		LargeFunnel = 100,
		BloodStream = 101,      // Particle spray
		ShowLine = 102,      // Line of particles every 5 units, dies in 30 seconds
		Blood = 103,      // Particle spray
		Decal = 104,      // Decal applied to a brush entity (not the world)
		Fizz = 105,      // Create alpha sprites inside of entity, float upwards
		Model = 106,      // Create a moving model that bounces and makes a sound when it hits
		ExplodeModel = 107,      // Spherical shower of models, picks from set
		BreakModel = 108,      // Box of models or sprites
		GunshotDecal = 109,      // Decal and ricochet sound
		SpriteSpray = 110,      // Spray of alpha sprites
		ArmorRicochet = 111,      // Quick spark sprite, client ricochet sound.
		PlayerDecal = 112,
		Bubbles = 113,      // Create alpha sprites inside of box, float upwards
		BubbleTrail = 114,      // Create alpha sprites along a line, float upwards
		BloodSprite = 115,      // Spray of opaque sprite1's that fall, single sprite2 for 1..2 secs (this is a high-priority tent)
		WorldDecal = 116,      // Decal applied to the world brush
		WorldDecalHigh = 117,      // Decal (with texture index > 256) applied to world brush
		DecalHigh = 118,      // Same as TE_DECAL, but the texture index was greater than 256
		Projectile = 119,      // Makes a projectile (like a nail) (this is a high-priority tent)
		Spray = 120,      // Throws a shower of sprites or models
		PlayerSprites = 121,      // Sprites emit from a player's bounding box (ONLY use for players!)
		ParticleBurst = 122,      // Very similar to lavasplash
		FireField = 123,      // Makes a field of fire
		//TEFIRE_FLAG_ALLFLOAT = 1 shl 0;        // All sprites will drift upwards as they animate
		//TEFIRE_FLAG_SOMEFLOAT = 1 shl 1;        // Some of the sprites will drift upwards. (50% chance)
		//TEFIRE_FLAG_LOOP = 1 shl 2;        // If set, sprite plays at 15 fps, otherwise plays at whatever rate stretches the animation over the sprite's duration.
		//TEFIRE_FLAG_ALPHA = 1 shl 3;        // If set, sprite is rendered alpha blended at 50% else, opaque
		//TEFIRE_FLAG_PLANAR = 1 shl 4;       // If set, all fire sprites have same initial Z instead of randomly filling a cube.
		PlayerAttachment = 124,      // Attaches a TENT to a player (this is a high-priority tent)
		KillPlayerAttachments = 125,      // Will expire all TENTS attached to a player.
		MultiGunshot = 126,      // Much more compact shotgun message
		UserTracer = 127      // Larger message than the standard tracer, but allows some customization.
	};

	enum class EntityType
	{
		Normal = 1 << 0,
		Beam = 1 << 1,
		Uninitialized = 1 << 30
	};

	struct Entity
	{
		// Fields which are filled in by routines outside of delta compression
		int entityType;
		// Index into cl_entities array for this entity.
		int number = -1;
		float msg_time;

		// Message number last time the player/entity state was updated.
		int messagenum;

		// Fields which can be transitted and reconstructed over the network stream
		glm::vec3 origin;
		float angles[3];

		int modelindex;
		int sequence;
		float frame;
		int colormap;
		short skin;
		short solid;
		int effects;
		float scale;

		uint8_t eflags;

		// Render information
		int rendermode;
		int renderamt;
		glm::u8vec3 rendercolor;
		int renderfx;

		int movetype;
		float animtime;
		float framerate;
		int body;
		uint8_t controller[4];
		uint8_t blending[4];
		float velocity[3];

		// Send bbox down to client for use during prediction.
		float mins[3];
		float maxs[3];

		int aiment;
		// If owned by a player, the index of that player ( for projectiles ).
		int owner;

		// Friction, for prediction.
		float friction;
		// Gravity multiplier
		float gravity;

		// PLAYER SPECIFIC
		int team;
		int playerclass;
		int health;
		bool spectator;
		int weaponmodel;
		int gaitsequence;
		// If standing on conveyor, e.g.
		float basevelocity[3];
		// Use the crouched hull, or the regular player hull.
		int usehull;
		// Latched buttons last time state updated.
		int oldbuttons;
		// -1 = in air, else pmove entity number
		int onground;
		int iStepLeft;
		// How fast we are falling
		float flFallVelocity;

		float fov;
		int weaponanim;

		// Parametric movement overrides
		float startpos[3];
		float endpos[3];
		float impacttime;
		float starttime;

		// For mods
		int iuser1;
		int iuser2;
		int iuser3;
		int iuser4;
		float fuser1;
		float fuser2;
		float fuser3;
		float fuser4;
		float vuser1[3];
		float vuser2[3];
		float vuser3[3];
		float vuser4[3];
	};

	struct ClientData
	{
		glm::vec3 origin;
		float			velocity[3];

		int					viewmodel;
		float			punchangle[3];
		int					flags;
		int					waterlevel;
		int					watertype;
		float			view_ofs[3];
		float				health;

		int					bInDuck;

		int					weapons; // remove?

		int					flTimeStepSound;
		int					flDuckTime;
		int					flSwimTime;
		int					waterjumptime;

		float				maxspeed;

		float				fov;
		int					weaponanim;

		int					m_iId;
		int					ammo_shells;
		int					ammo_nails;
		int					ammo_cells;
		int					ammo_rockets;
		float				m_flNextAttack;

		int					tfstate;

		int					pushmsec;

		int					deadflag;

		std::string			physinfo;

		// For mods
		int					iuser1;
		int					iuser2;
		int					iuser3;
		int					iuser4;
		float				fuser1;
		float				fuser2;
		float				fuser3;
		float				fuser4;
		float			vuser1[3];
		float			vuser2[3];
		float			vuser3[3];
		float			vuser4[3];
	};

	struct WeaponData
	{
		int			m_iId;
		int			m_iClip;

		float		m_flNextPrimaryAttack;
		float		m_flNextSecondaryAttack;
		float		m_flTimeWeaponIdle;

		int			m_fInReload;
		int			m_fInSpecialReload;
		float		m_flNextReload;
		float		m_flPumpTime;
		float		m_fReloadTime;

		float		m_fAimedDamage;
		float		m_fNextAimBonus;
		int			m_fInZoom;
		int			m_iWeaponState;

		int			iuser1;
		int			iuser2;
		int			iuser3;
		int			iuser4;
		float		fuser1;
		float		fuser2;
		float		fuser3;
		float		fuser4;
	};



	static const int SND_VOLUME = 1 << 0;
	static const int SND_ATTN = 1 << 1;
	static const int SND_LONG_INDEX = 1 << 2;
	static const int SND_PITCH = 1 << 3;
	static const int SND_SENTENCE = 1 << 4;

	static const int SND_STOP = 1 << 5;
	static const int SND_CHANGE_VOL = 1 << 6;
	static const int SND_CHANGE_PITCH = 1 << 7;
	static const int SND_SPAWNING = 1 << 8; // 9 bits

	static const int CHAN_AUTO = 0;
	static const int CHAN_WEAPON = 1;
	static const int CHAN_VOICE = 2;
	static const int CHAN_ITEM = 3;
	static const int CHAN_BODY = 4;
	static const int CHAN_STREAM = 5;
	static const int CHAN_STATIC = 6;
	static const int CHAN_NETWORKVOICE_BASE = 7;
	static const int CHAN_NETWORKVOICE_END = 500;

	static const float ATTN_NONE = 0;
	static const float ATTN_NORM = 0.8f;
	static const float ATTN_IDLE = 2;
	static const float ATTN_STATIC = 1.25f;

	static const int PITCH_NORM = 100;
	static const int PITCH_LOW = 95;
	static const int PITCH_HIGH = 120;

	static const int VOL_NORM = 255;


	struct Sound
	{
		int16_t index;
		int16_t entity;
		int16_t channel;
		int16_t volume;
		int16_t pitch;
		float attenuation;
		int16_t flags;
		float origin[3];
	};

	struct EventArgs
	{
		int		flags = 0;

		// Transmitted
		int		entindex = 0;

		glm::vec3	origin = { 0.0f, 0.0f, 0.0f };
		glm::vec3	angles = { 0.0f, 0.0f, 0.0f };
		glm::vec3	velocity = { 0.0f, 0.0f, 0.0f };

		int		ducking = 0;

		float	fparam1 = 0;
		float	fparam2 = 0;

		int		iparam1 = 0;
		int		iparam2 = 0;

		int		bparam1 = 0;
		int		bparam2 = 0;
	};

	struct Event // event_info_t
	{
		unsigned short index = 0;			  // 0 implies not in use

		short packet_index = 0;      // Use data from state info for entity in delta_packet .  -1 implies separate info based on event
								 // parameter signature
		short entity_index = 0;      // The edict this event is associated with

		float fire_time = 0.0f;        // if non-zero, the time when the event should be fired ( fixed up on the client )

		EventArgs args;

		// CLIENT ONLY	
		int	  flags = 0;			// Reliable or not, etc.
	};

	struct UserCmd
	{
		short	lerp_msec;		// Interpolation time on client
		uint8_t	msec;			// Duration in ms of command
		glm::vec3 viewangles;	// Command view angles.

								// intended velocities

		float	forwardmove;    // Forward velocity.
		float	sidemove;       // Sideways velocity.
		float	upmove;         // Upward velocity.

		uint8_t	lightlevel;     // Light level at spot where we are standing.
		unsigned short  buttons;  // Attack buttons
		uint8_t    impulse;          // Impulse command issued.
		uint8_t	weaponselect;	// Current weapon id

								// Experimental player impact stuff.

		int		impact_index;
		float	impact_position[3];
	};

	struct ServerInfo
	{
		int32_t protocol = 0;
		int32_t spawn_count = 0;
		int32_t map_crc = 0;
		std::string crc;
		int32_t max_players = 0;
		int32_t index = 0;
		bool deathmatch = false;
		std::string hostname;
		std::string map;
		std::string game_dir;
		std::string map_list;
		bool vac2 = false;
	};

	struct MoveVars
	{
		float gravity = 0.0f;
		float stop_speed = 0.0f;
		float max_speed = 0.0f;
		float spectator_max_speed = 0.0f;
		float accelerate = 0.0f;
		float air_accelerate = 0.0f;
		float water_accelerate = 0.0f;
		float friction = 0.0f;
		float edge_friction = 0.0f;
		float water_friction = 0.0f;
		float ent_gravity = 0.0f;
		float bounce = 0.0f;
		float step_size = 0.0f;
		float max_velocity = 0.0f;
		float z_max = 0.0f;
		float wave_height = 0.0f;
		uint8_t foot_steps = 0;
		float roll_angle = 0.0f;
		float roll_speed = 0.0f;
		glm::vec3 sky_color = { 0.0f, 0.0f, 0.0f };
		glm::vec3 sky_vec = { 0.0f, 0.0f, 0.0f };
		std::string sky_name;
	};
}

#define BIT(n) (1<<(n))

#define IN_ATTACK	(1<<0)
#define IN_JUMP		(1<<1)
#define IN_DUCK		(1<<2)
#define IN_FORWARD	(1<<3)
#define IN_BACK		(1<<4)
#define IN_USE		(1<<5)
#define IN_CANCEL	(1<<6)
#define IN_LEFT		(1<<7)
#define IN_RIGHT	(1<<8)
#define IN_MOVELEFT	(1<<9)
#define IN_MOVERIGHT	(1<<10)
#define IN_ATTACK2	(1<<11)
#define IN_RUN		(1<<12)
#define IN_RELOAD	(1<<13)
#define IN_ALT1		(1<<14)
#define IN_SCORE	(1<<15)	// Used by client.dll for when scoreboard is held down

// edict->flags
#define FL_FLY                      BIT(0)  // Changes the SV_Movestep() behavior to not need to be on ground
#define FL_SWIM                     BIT(1)  // Changes the SV_Movestep() behavior to not need to be on ground (but stay in water)
#define FL_CONVEYOR                 BIT(2)
#define FL_CLIENT                   BIT(3)
#define FL_INWATER                  BIT(4)
#define FL_MONSTER                  BIT(5)
#define FL_GODMODE                  BIT(6)
#define FL_NOTARGET                 BIT(7)
#define FL_SKIPLOCALHOST            BIT(8)  // Don't send entity to local host, it's predicting this entity itself
#define FL_ONGROUND                 BIT(9)  // At rest / on the ground
#define FL_PARTIALGROUND            BIT(10) // not all corners are valid
#define FL_WATERJUMP                BIT(11) // player jumping out of water
#define FL_FROZEN                   BIT(12) // Player is frozen for 3rd person camera
#define FL_FAKECLIENT               BIT(13) // JAC: fake client, simulated server side; don't send network messages to them
#define FL_DUCKING                  BIT(14) // Player flag -- Player is fully crouched
#define FL_FLOAT                    BIT(15) // Apply floating force to this entity when in water
#define FL_GRAPHED                  BIT(16) // worldgraph has this ent listed as something that blocks a connection

// UNDONE: Do we need these?
#define FL_IMMUNE_WATER             BIT(17)
#define FL_IMMUNE_SLIME             BIT(18)
#define FL_IMMUNE_LAVA              BIT(19)

#define FL_PROXY                    BIT(20) // This is a spectator proxy
#define FL_ALWAYSTHINK              BIT(21) // Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY             BIT(22) // Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP              BIT(23) // Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN                  BIT(24) // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH               BIT(25) // Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR                BIT(26) // This client is a spectator, don't run touch functions, etc.
#define FL_CUSTOMENTITY             BIT(29) // This is a custom entity
#define FL_KILLME                   BIT(30) // This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT                  BIT(31) // Entity is dormant, no updates to client