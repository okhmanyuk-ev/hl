#pragma once

#include <shared/all.h>
#include <HL/base_client.h>

namespace HL
{
	class OverviewInfo
	{
	public:
		void load(const Platform::Asset& txt_file);

	public:
		const auto& getOrigin() const { return mOrigin; }
		auto getZoom() const { return mZoom; }
		auto isRotated() const { return mRotated; }

	private:
		float mZoom = 1.0f;
		glm::vec3 mOrigin = { 0.0f, 0.0f, 0.0f };
		bool mRotated = false;
	};

	class GameplayViewNode : public Scene::Node
	{
	public:
		GameplayViewNode(std::shared_ptr<BaseClient> client);

	public:
		void draw() override;

	private:
		glm::vec2 worldToScreen(const glm::vec3& value) const;
		std::optional<HL::Protocol::Resource> findModel(int model_index) const;
		std::shared_ptr<Renderer::Texture> getCurrentMapTexture() const;

	private:
		std::shared_ptr<BaseClient> mClient = nullptr; // TODO: weak_ptr ?
		std::shared_ptr<OverviewInfo> mOverviewInfo;
	};
}