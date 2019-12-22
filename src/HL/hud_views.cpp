#include "hud_views.h"

#include <Common/console_commands.h>

#include <imgui.h>
#include <Shared/imgui_user.h>

#include <algorithm>

namespace HL
{
	HudViews::HudViews(BaseClient& baseClient) : 
		mBaseClient(baseClient)
	{
		CONSOLE->registerCVar("hud_show_net", "show net graph on screen", { "int" },
			CVAR_GETTER_INT(mWantShowNet),
			CVAR_SETTER(mWantShowNet = CON_ARG_INT(0)));
		
		CONSOLE->registerCVar("hud_show_ents", "show entities information on screen", { "int" },
			CVAR_GETTER_INT(mWantShowEntities),
			CVAR_SETTER(mWantShowEntities = CON_ARG_INT(0)));
	}

	void HudViews::frame()
	{
		if (mWantShowNet > 0)
		{
			ImGui::Begin("NetGraph", nullptr, ImGui::User::ImGuiWindowFlags_Overlay);
			ImGui::SetWindowPos(ImGui::User::BottomRightCorner());
			ImGui::Text("Seq: %d", mBaseClient.getChannel().getIncomingSequence());
			ImGui::Text("Ack: %d", mBaseClient.getChannel().getOutgoingSequence());
			ImGui::Text("Ping: %d", mBaseClient.getChannel().getLatency());
			ImGui::End();
		}

		if (mWantShowEntities)
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(-1, PLATFORM->getLogicalHeight() - 20.0f));
			
			
			
			/*ImGui::Begin("Entities", nullptr, ImGui::User::ImGuiWindowFlags_ControlPanel);
			ImGui::SetWindowPos(ImGui::User::TopRightCorner(mPlatformSystem.getWidth(), mPlatformSystem.getHeight()));

			const auto& resources = mBaseClient.getResources();

			for (const auto& [index, entity] : mBaseClient.getEntities())
			{
				auto model = std::find_if(resources.cbegin(), resources.cend(), [&entity](const HL::Protocol::Resource& res) {
					return res.index == entity->modelindex && res.type == HL::Protocol::Resource::Type::Model;
				});

				if (model == resources.end())
					continue;

				ImGui::Text("%d. %s %.0f %.0f %.0f", index, model->name.c_str(),
					entity->origin[0], entity->origin[1], entity->origin[2]);

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Model: %s", model->name.c_str());
					ImGui::Text("Origin: %.0f %.0f %.0f", entity->origin[0], entity->origin[1], entity->origin[2]);
					ImGui::Text("Angles: %.0f %.0f %.0f", entity->angles[0], entity->angles[1], entity->angles[2]);
					ImGui::EndTooltip();
				}
			}

			ImGui::End();*/










			ImGui::Begin("Entities", (bool*)1,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings);

			ImGui::SetWindowPos(ImGui::User::TopRightCorner());

			struct Column
			{
				const char* header;
				float size = 0;
			};

			std::vector<Column> columns
			{
				{ "" },
				{ "Model" },
				{ "Position" },
				{ "Angles" },
				{ "MoveType" }
			};

			ImGui::Columns(columns.size(), (const char*)0, false);

			for (auto &column : columns)
			{
				ImGui::Text(column.header);
				ImGui::NextColumn();
				column.size = ImGui::CalcTextSize(column.header).x;
			}

			ImGui::Separator();


			const auto& resources = mBaseClient.getResources();

			for (const auto [index, entity] : mBaseClient.getEntities())
			{
				auto model = std::find_if(resources.cbegin(), resources.cend(), [e = entity](const auto& res) {
					return res.index == e->modelindex && res.type == HL::Protocol::Resource::Type::Model;
				});

				if (model == resources.end()) // TODO: assert here
					continue;

				if (model->name.empty())
					continue;

				if (model->name[0] == '*')
					continue;
				
				const std::vector<std::string> content
				{
					{ std::to_string(index) + "." },
					{ model->name },
					{ std::to_string((int)entity->origin[0]) + " " +
						std::to_string((int)entity->origin[1]) + " " +
						std::to_string((int)entity->origin[2]) },
					{ std::to_string((int)entity->angles[0]) + " " +
						std::to_string((int)entity->angles[1]) + " " +
						std::to_string((int)entity->angles[2]) },
					{ std::to_string(entity->movetype) }
				};

				for (size_t i = 0; i < content.size(); i++)
				{
					auto text = std::next(content.begin(), i);

					ImGui::Text((*text).c_str());
					ImGui::NextColumn();

					auto column = std::next(columns.begin(), i);
					auto size = ImGui::CalcTextSize((*text).c_str()).x;

					if (column->size < size)
						column->size = size;
				}
			}

			float offset = 0.0f;

			for (size_t i = 0; i < columns.size(); i++)
			{
				ImGui::SetColumnOffset(i, offset);
				offset += std::next(columns.begin(), i)->size + 25;
			}

			ImGui::SetWindowSize(ImVec2(offset + 15, -1));

			ImGui::End();
			











		}
	}
}