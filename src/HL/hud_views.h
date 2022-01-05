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
		void onFrame() override;

	private:
		BaseClient& mBaseClient;

	private:
		int mWantShowEntities = 0;
	};
}