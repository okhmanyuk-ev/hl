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
		const auto& getPath() const { return mPath; }

	private:
		float mZoom = 1.0f;
		glm::vec3 mOrigin = { 0.0f, 0.0f, 0.0f };
		bool mRotated = false;
		std::string mPath;
	};

	class GenericDrawNode : public Scene::Node
	{
	public:
		void draw() override;

	public:
		using DrawCallback = std::function<void()>;

	public:
		void setDrawCallback(DrawCallback value) { mDrawCallback = value; }

	private:
		DrawCallback mDrawCallback = nullptr;
	};

	class GameplayViewNode : public Scene::Node
	{
	public:
		GameplayViewNode(std::shared_ptr<BaseClient> client);

	public:
		void draw() override;

	protected:
		virtual void drawOnBackground(Scene::Node& holder);

	private:
		void drawEntities(Scene::Node& holder);
		void drawPlayers(Scene::Node& holder);
		void drawPlayer(Scene::Node& holder, int index, const glm::vec3& origin, std::optional<glm::vec3> angles,
			const glm::vec3& color, const std::vector<std::pair<std::string, std::string>>& labels);

	private:
		std::shared_ptr<skygfx::Texture> getCurrentMapTexture() const;
		void ensureOverviewInfoLoaded();

	public:
		glm::vec2 worldToScreen(const glm::vec3& value) const;
		glm::vec3 screenToWorld(const glm::vec2& value) const;
		float worldToScreenAngles(const glm::vec3& value) const;
		std::string getNiceModelName(const HL::Protocol::Resource& model) const;
		std::string getShortMapName() const;
		auto getBackgroundNode() const { return mBackground; }

		auto isCenterized() const { return mCenterized; }
		void setCenterized(bool value) { mCenterized = value; }

	private:
		std::shared_ptr<BaseClient> mClient = nullptr; // TODO: weak_ptr ?
		std::optional<OverviewInfo> mOverviewInfo;
		std::shared_ptr<Scene::Node> mBackground;
		bool mCenterized = false;
	};
}
