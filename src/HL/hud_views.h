#pragma once

#include <Core/engine.h>

#include <Console/system.h>
#include <Common/frame_system.h>
#include <Platform/system.h>
#include "base_client.h"

namespace HL
{
	class HudViews : public Common::FrameSystem::Frameable
	{
	public:
		HudViews(BaseClient& baseClient);
		
	private:
		void frame() override;

	private:
		BaseClient& mBaseClient;

	private:
		int mWantShowNet = 0;
		int mWantShowEntities = 0;
	};
}