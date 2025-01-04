#pragma once

#include <HL/bspfile.h>
#include <sky/sky.h>

namespace HL
{
	class BspMapEntity : public Scene::Entity3D
	{
	public:
		using TexId = int;

	public:
		BspMapEntity(const BSPFile& bspfile, std::unordered_map<TexId, std::shared_ptr<skygfx::Texture>> textures = {});
		void provideModels(std::vector<skygfx::utils::Model>& models) override;

	private:
		skygfx::utils::Mesh mMesh;
		std::unordered_map<TexId, std::shared_ptr<skygfx::Texture>> mTextures;
		std::shared_ptr<skygfx::Texture> mDefaultTexture;
		std::vector<skygfx::utils::Model> mModels;
	};
}
