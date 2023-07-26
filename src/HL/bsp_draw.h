#pragma once

#include <graphics/all.h>
#include <HL/bspfile.h>
#include <skygfx/utils.h>

namespace HL
{
	class BspDraw
	{
	public:
		using TexId = int;

	public:
		BspDraw(const BSPFile& bspfile, std::unordered_map<TexId, std::shared_ptr<skygfx::Texture>> textures = {},
			const glm::mat4& model_matrix = glm::mat4(1.0f));

		void draw(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos,
			float yaw, float pitch, float fov = 80.0f, const glm::vec3& world_up = { 0.0f, 0.0f, 1.0f });

		const auto& getLights() const { return mLights; }
		void setLights(const std::vector<skygfx::utils::Light> value) { mLights = value; }

	private:
		glm::mat4 mModelMatrix;
		skygfx::utils::Mesh mMesh;
		std::unordered_map<TexId, std::vector<skygfx::utils::DrawVerticesCommand>> mDrawcalls;
		std::unordered_map<TexId, std::shared_ptr<skygfx::Texture>> mTextures;
		std::vector<skygfx::utils::Light> mLights;
		std::shared_ptr<skygfx::Texture> mDefaultTexture;
		std::vector<skygfx::utils::Model> mModels;
	};
}
