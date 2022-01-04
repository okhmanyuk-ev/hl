#include "gameplay_view_node.h"
#include <HL/utils.h>
#include <filesystem>

using namespace HL;

void OverviewInfo::load(const Platform::Asset& txt_file)
{
	auto str = std::string((char*)txt_file.getMemory(), txt_file.getSize());

	auto getDataInBraces = [&](std::string name) {
		auto pos = str.find(name);
		auto data = str.substr(pos + name.length());
		auto open_pos = data.find("{");
		auto close_pos = data.find("}");
		auto result = data.substr(open_pos + 1, close_pos - open_pos - 2);
		return result;
	};

	auto global = getDataInBraces("global");
	auto global_tokens_v = Console::System::MakeTokensFromString(global);
	std::list<std::string> global_tokens;
	std::copy(global_tokens_v.begin(), global_tokens_v.end(), std::back_inserter(global_tokens));

	auto readNextToken = [](auto& tokens) {
		auto result = tokens.front();
		tokens.pop_front();
		return result;
	};

	while (!global_tokens.empty())
	{
		auto token = readNextToken(global_tokens);

		if (token == "ZOOM")
		{
			auto value = readNextToken(global_tokens);
			mZoom = std::stof(value);
		}
		else if (token == "ORIGIN")
		{
			auto x = readNextToken(global_tokens);
			auto y = readNextToken(global_tokens);
			auto z = readNextToken(global_tokens);
			mOrigin.x = std::stof(x);
			mOrigin.y = std::stof(y);
			mOrigin.z = std::stof(z);
		}
		else if (token == "ROTATED")
		{
			auto value = readNextToken(global_tokens);
			mRotated = value != "0";
		}
	}

	auto layer = getDataInBraces("layer");
	auto layer_tokens_v = Console::System::MakeTokensFromString(layer);
	std::list<std::string> layer_tokens;
	std::copy(layer_tokens_v.begin(), layer_tokens_v.end(), std::back_inserter(layer_tokens));

	while (!layer_tokens.empty())
	{
		auto token = readNextToken(layer_tokens);

		if (token == "IMAGE")
		{
			mPath = readNextToken(layer_tokens);
		}
		else if (token == "HEIGHT")
		{
			auto value = readNextToken(layer_tokens);
		}
	}
}

GameplayViewNode::GameplayViewNode(std::shared_ptr<BaseClient> client) :
	mClient(client)
{
	mClient->setGameEngineInitializedCallback([this] {
		auto info = mClient->getServerInfo().value();
		std::filesystem::path map_path = info.map;
		auto map_name = map_path.filename().replace_extension().string();
		auto txt_path = fmt::format("overviews/{}.txt", map_name);
		mOverviewInfo = OverviewInfo();
		if (Platform::Asset::Exists(txt_path))
		{
			mOverviewInfo->load(Platform::Asset::Asset(txt_path));
		}
	});

	mClient->setDisconnectCallback([this](auto) {
		mOverviewInfo.reset();
	});

	mClient->setBeamPointsCallback([this](const glm::vec3& start, const glm::vec3& end, uint8_t lifetime, const glm::vec4& color) {
		auto start_scr = worldToScreen(start);
		auto end_scr = worldToScreen(end);
		
		auto rect = std::make_shared<Scene::Rectangle>();
		rect->setColor(color);
		rect->setPosition(start_scr);
		rect->setWidth(1.0f);
		rect->setHeight(glm::distance(start_scr, end_scr));
		rect->setHorizontalPivot(0.5f);

		auto forward = end - start;
		auto rotation = glm::atan(forward.y, -forward.x);

		rect->setRotation(rotation);
		rect->runAction(Actions::Collection::Delayed((float)lifetime / 10.0f,
			Actions::Collection::Kill(rect)
		));
		attach(rect);
	});

	mClient->setBloodSpriteCallback([this](const glm::vec3& origin) {
		auto label = std::make_shared<Scene::Label>();
		label->setText("BLOOD");
		label->setFontSize(9.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Red);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});

	mClient->setSparksCallback([this](const glm::vec3& origin) {
		auto label = std::make_shared<Scene::Label>();
		label->setText("SPARKS");
		label->setFontSize(9.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Brown);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});

	mClient->setGlowSpriteCallback([this](const glm::vec3& origin, int model_index) {
		auto model = findModel(model_index);

		auto label = std::make_shared<Scene::Label>();
		label->setText(model.value().name);
		label->setFontSize(10.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Red);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});

	mClient->setSpriteCallback([this](const glm::vec3& origin, int model_index) {
		auto model = findModel(model_index);

		auto label = std::make_shared<Scene::Label>();
		label->setText(model.value().name);
		label->setFontSize(10.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Purple);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});

	mClient->setSmokeCallback([this](const glm::vec3& origin, int model_index) {
		auto model = findModel(model_index);

		auto label = std::make_shared<Scene::Label>();
		label->setText(model.value().name);
		label->setFontSize(10.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Black);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});

	mClient->setExplosionCallback([this](const glm::vec3& origin, int model_index) {
		auto model = findModel(model_index);

		auto label = std::make_shared<Scene::Label>();
		label->setText(model.value().name);
		label->setFontSize(10.0f);
		label->setPivot(0.5f);
		label->setPosition(worldToScreen(origin));
		label->setScale(0.0f);
		label->setOutlineThickness(1.0f);
		label->getOutlineColor()->setColor(Graphics::Color::Red);
		label->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Wait(1.0f),
			Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, 0.5f, Easing::CubicInOut),
			Actions::Collection::Kill(label)
		));
		attach(label);
	});
}

void GameplayViewNode::draw()
{
	Node::draw();

	if (!mClient->getServerInfo().has_value())
		return;

	auto texture = getCurrentMapTexture();

	if (texture == nullptr)
		return;

	auto bg = IMSCENE->attachTemporaryNode<Scene::Sprite>(*this);
	bg->setStretch(1.0f);
	bg->setPivot(0.5f);
	bg->setAnchor(0.5f);
	bg->setTexture(texture);		
	if (IMSCENE->nodeWasInitialized())
	{
		bg->setScale(0.0f);
		bg->runAction(
			Actions::Collection::ChangeScale(bg, { 1.0f, 1.0f }, 0.5f, Easing::BackOut)
		);
	}
	IMSCENE->setupPreKillAction(bg, Actions::Collection::ChangeScale(bg, { 0.0f, 0.0f }, 0.5f, Easing::BackIn));

	const auto& userinfos = mClient->getPlayerUserInfos();
	
	auto dTime = FRAME->getTimeDelta();

	for (const auto& [index, entity] : mClient->getEntities())
	{
		bool is_player = mClient->isPlayerIndex(index);

		auto pos = worldToScreen(entity->origin);

		auto rotation = glm::radians(-entity->angles[1]);

		if (mOverviewInfo->isRotated())
			rotation += glm::radians(90.0f);

		const float SlowFriction = 0.01f;

		auto model = findModel(entity->modelindex);
		
		if (model->name.substr(0, 1) == "*")
			continue;

		auto holder = IMSCENE->attachTemporaryNode(*bg, fmt::format("entity_{}", index));
		holder->setSize(8.0f);
		holder->setPivot(0.5f);
		holder->setPosition(IMSCENE->nodeWasInitialized() ? pos : Common::Helpers::SmoothValueAssign(holder->getPosition(), pos, dTime));
		if (IMSCENE->nodeWasInitialized())
		{
			holder->setScale(0.0f);
			holder->runAction(
				Actions::Collection::ChangeScale(holder, { 1.0f, 1.0f }, 0.5f, Easing::BackOut)
			);
		}
		IMSCENE->setupPreKillAction(holder, Actions::Collection::ChangeScale(holder, { 0.0f, 0.0f }, 0.5f, Easing::BackIn));

		auto team_color = mClient->getGameMod()->getPlayerColor(index);

		if (is_player)
		{
			auto body = IMSCENE->attachTemporaryNode<Scene::Circle>(*holder);
			body->setRotation(IMSCENE->nodeWasInitialized() ? rotation : Common::Helpers::SmoothRotationAssign(body->getRotation(), rotation, dTime));
			body->setStretch(1.0f);
			body->setPivot(0.5f);
			body->setAnchor(0.5f);
			body->setColor(Common::Helpers::SmoothValueAssign(body->getColor(), team_color, dTime, SlowFriction));
			IMSCENE->dontKill(body);

			auto arrow = IMSCENE->attachTemporaryNode<Scene::Rectangle>(*body);
			arrow->setSize({ 1.0f, 4.0f });
			arrow->setPivot({ 0.5f, 1.0f });
			arrow->setAnchor({ 0.5f, 0.0f });
			arrow->setColor(Common::Helpers::SmoothValueAssign(arrow->getColor(), team_color, dTime, SlowFriction));
			IMSCENE->dontKill(arrow);

			if (userinfos.count(index - 1) > 0)
			{
				auto name = IMSCENE->attachTemporaryNode<Scene::Label>(*holder);
				name->setPivot(0.5f);
				name->setAnchor({ 0.5f, 0.0f });
				name->setPosition({ 0.0f, -18.0f });
				name->setFontSize(10.0f);
				auto name_str = HL::Utils::GetInfoValue(userinfos.at(index - 1), "name");
				name->setText(name_str);
				IMSCENE->dontKill(name);
			}

			auto weapon_model = findModel(entity->weaponmodel);

			if (weapon_model.has_value())
			{
				auto weapon = IMSCENE->attachTemporaryNode<Scene::Label>(*holder);
				weapon->setPivot(0.5f);
				weapon->setAnchor({ 0.5f, 0.0f });
				weapon->setPosition({ 0.0f, -8.0f });
				weapon->setFontSize(9.0f);
				weapon->setText(weapon_model->name);
				IMSCENE->dontKill(weapon);
			}
		}
		else
		{
			auto body = IMSCENE->attachTemporaryNode<Scene::Rectangle>(*holder);
			body->setRotation(IMSCENE->nodeWasInitialized() ? rotation : Common::Helpers::SmoothRotationAssign(body->getRotation(), rotation, dTime));
			body->setStretch(0.75f);
			body->setPivot(0.5f);
			body->setAnchor(0.5f);
			body->setColor(Common::Helpers::SmoothValueAssign(body->getColor(), Graphics::Color::Yellow, dTime, SlowFriction));
			IMSCENE->dontKill(body);

			auto name = IMSCENE->attachTemporaryNode<Scene::Label>(*holder);
			name->setPivot(0.5f);
			name->setAnchor({ 0.5f, 0.0f });
			name->setPosition({ 0.0f, -12.0f });
			name->setFontSize(8.0f);
			name->setText(model->name);
			IMSCENE->dontKill(name);
		}
	}
}

glm::vec2 GameplayViewNode::worldToScreen(const glm::vec3& value) const
{
	auto o = (value - mOverviewInfo->getOrigin()) / 8192.0f * mOverviewInfo->getZoom();
	auto size = getAbsoluteSize();

	if (mOverviewInfo->isRotated())
	{
		auto a = o.x;
		o.x = o.y;
		o.y = -a;
	}

	auto result = size / 2.0f;
	result.x -= o.y * size.x;
	result.y -= o.x * size.y * (1024.0f / 768.0f);

	return result;
}

std::optional<HL::Protocol::Resource> GameplayViewNode::findModel(int model_index) const
{
	const auto& resources = mClient->getResources();

	auto result = std::find_if(resources.cbegin(), resources.cend(), [model_index](const auto& res) {
		return res.index == model_index && res.type == HL::Protocol::Resource::Type::Model;
	});

	if (result == resources.cend())
		return std::nullopt;
	else
		return *result;
}

std::shared_ptr<Renderer::Texture> GameplayViewNode::getCurrentMapTexture() const
{
	const auto& info = mClient->getServerInfo().value();
	auto path = std::filesystem::path(info.map);
	auto map = path.filename().replace_extension().string();
	auto texture_name = "overview_" + map;
	auto result = TEXTURE(texture_name);

	if (result.getTexture() == nullptr)
	{
		auto img_path = mOverviewInfo->getPath();

		if (Platform::Asset::Exists(img_path))
		{
			auto image = std::make_shared<Graphics::Image>(Platform::Asset(img_path));

			for (int x = 0; x < image->getWidth(); x++)
			{
				for (int y = 0; y < image->getHeight(); y++)
				{
					auto pixel = image->getPixel(x, y);

					auto& r = pixel[0];
					auto& g = pixel[1];
					auto& b = pixel[2];
					auto& a = pixel[3];

					if (r == 0 && g == 255 && b == 0)
					{
						g = 0;
						a = 0;
					}
				}
			}

			PRECACHE_TEXTURE_ALIAS(image, texture_name);
			result = TEXTURE(texture_name);
		}
	}

	return result;
}