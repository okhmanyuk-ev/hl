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

// generic draw node

void GenericDrawNode::draw()
{
	Scene::Node::draw();

	GRAPHICS->pushModelMatrix(getTransform());

	if (mDrawCallback)
		mDrawCallback();

	GRAPHICS->pop();
}

// gameplay view node

GameplayViewNode::GameplayViewNode(std::shared_ptr<BaseClient> client) :
	mClient(client)
{
	mClient->setDisconnectCallback([this](auto) {
		mOverviewInfo.reset();
	});

	mClient->setBeamPointsCallback([this](const glm::vec3& start, const glm::vec3& end, uint8_t lifetime, const glm::vec4& color) {
		auto start_scr = worldToScreen(start);
		auto end_scr = worldToScreen(end);
		auto node = std::make_shared<GenericDrawNode>();
		node->setStretch(1.0f);
		node->setDrawCallback([node, start_scr, end_scr, color] {
			GRAPHICS->draw(Renderer::Topology::LineList, {
				{ { start_scr, 0.0f }, color },
				{ { end_scr, 0.0f }, color }
			});
		});
		node->runAction(Actions::Collection::Delayed((float)lifetime / 10.0f,
			Actions::Collection::Kill(node)
		));
		attach(node);
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

	ensureOverviewInfoLoaded();

	auto texture = getCurrentMapTexture();

	if (texture == nullptr)
		return;

	auto background = IMSCENE->attachTemporaryNode<Scene::Sprite>(*this);
	background->setStretch(1.0f);
	background->setPivot(0.5f);
	background->setAnchor(0.5f);
	background->setTexture(texture);
	if (IMSCENE->nodeWasInitialized())
	{
		background->setScale(0.0f);
		background->runAction(Actions::Collection::ChangeScale(background, { 1.0f, 1.0f }, 0.5f, Easing::BackOut));
	}
	IMSCENE->setupPreKillAction(background, Actions::Collection::ChangeScale(background, { 0.0f, 0.0f }, 0.5f, Easing::BackIn));

	drawEntities(*background);
	drawPlayers(*background);
}

void GameplayViewNode::drawEntities(Scene::Node& holder)
{
	const auto dTime = FRAME->getTimeDelta();
	const auto& entities = mClient->getEntities();

	for (auto [index, entity] : entities)
	{
		if (mClient->isPlayerIndex(index))
			continue;

		auto model = findModel(entity->modelindex);

		if (!model.has_value())
			continue;

		auto origin_scr = worldToScreen(entity->origin);

		if (model.value().name.substr(0, 1) == "*")
		{
			auto position = (entity->maxs + entity->mins) / 2.0f;
			origin_scr = worldToScreen(position + entity->origin);
		}

		auto rotation = worldToScreenAngles(entity->angles);

		auto node = IMSCENE->attachTemporaryNode(holder, fmt::format("player_{}", index));
		node->setSize(4.0f);
		node->setPivot(0.5f);
		node->setPosition(IMSCENE->nodeWasInitialized() ? origin_scr : Common::Helpers::SmoothValueAssign(node->getPosition(), origin_scr, dTime));

		auto body = IMSCENE->attachTemporaryNode<Scene::Rectangle>(*node);
		body->setRotation(rotation);
		body->setStretch(1.0f);
		body->setPivot(0.5f);
		body->setAnchor(0.5f);
		body->setColor(Graphics::Color::Yellow);

		auto label = IMSCENE->attachTemporaryNode<Scene::Label>(*node);
		label->setPivot(0.5f);
		label->setAnchor({ 0.5f, 0.0f });
		label->setY(-10.0f);
		label->setFontSize(8.0f);
		label->setText(getNiceModelName(model.value()));
	}
}

void GameplayViewNode::drawPlayers(Scene::Node& holder)
{
	const auto& serverinfo = mClient->getServerInfo().value();
	const auto& gamemod = mClient->getGameMod();
	const auto& entities = mClient->getEntities();
	const auto& userinfos = mClient->getPlayerUserInfos();
	const auto& clientdata = mClient->getClientData();
	const auto counter_strike = std::dynamic_pointer_cast<CounterStrike>(gamemod);

	for (int index = 1; index < serverinfo.max_players; index++)
	{
		if (!gamemod->isPlayerAlive(index))
			continue;

		HL::Protocol::Entity* entity = nullptr;

		if (entities.contains(index))
			entity = entities.at(index);

		bool is_me = index == serverinfo.index + 1;

		std::optional<glm::vec3> origin;

		if (is_me)
		{
			origin = clientdata.origin;
		}
		else if (entity != nullptr)
		{
			origin = entity->origin;
		}
		else if (counter_strike)
		{
			auto radar = counter_strike->getPlayerRadarCoord(index);
			if (radar.has_value())
			{
				origin = radar.value();
			}
		}

		if (!origin.has_value())
			continue;

		std::optional<glm::vec3> angles;
		std::vector<std::pair<std::string, std::string>> labels;

		if (entity != nullptr)
		{
			auto weapon_model = findModel(entity->weaponmodel);
			if (weapon_model.has_value())
			{
				labels.push_back({ "weapon", getNiceModelName(weapon_model.value()) });
			}
			angles = entity->angles;
		}

		if (userinfos.count(index - 1) > 0)
			labels.push_back({ "name", HL::Utils::GetInfoValue(userinfos.at(index - 1), "name") });

		auto color = gamemod->getPlayerColor(index);

		drawPlayer(holder, index, origin.value(), angles, color, labels);
	}
}

void GameplayViewNode::drawPlayer(Scene::Node& holder, int index, const glm::vec3& origin, std::optional<glm::vec3> angles,
	const glm::vec3& color, const std::vector<std::pair<std::string, std::string>>& labels)
{
	auto pos = worldToScreen(origin);
	const auto dTime = FRAME->getTimeDelta();

	const float SpawnDuration = 0.5f;
	const float KillDuration = 0.5f;

	auto player = IMSCENE->attachTemporaryNode(holder, fmt::format("player_{}", index));
	player->setSize(8.0f);
	player->setPivot(0.5f);
	player->setPosition(IMSCENE->nodeWasInitialized() ? pos : Common::Helpers::SmoothValueAssign(player->getPosition(), pos, dTime));
	IMSCENE->setupPreKillAction(player, Actions::Collection::Wait(KillDuration)); // wait until childs die

	auto body = IMSCENE->attachTemporaryNode<Scene::Circle>(*player);
	if (angles.has_value())
	{
		auto rotation = worldToScreenAngles(angles.value());
		body->setRotation(IMSCENE->nodeWasInitialized() ? rotation : Common::Helpers::SmoothRotationAssign(body->getRotation(), rotation, dTime));
	}
	body->setStretch(1.0f);
	body->setPivot(0.5f);
	body->setAnchor(0.5f);
	body->setColor(color);
	if (IMSCENE->nodeWasInitialized())
	{
		body->setScale(0.0f);
		body->runAction(Actions::Collection::ChangeScale(body, { 1.0f, 1.0f }, SpawnDuration, Easing::BackOut));
	}
	IMSCENE->setupPreKillAction(body, Actions::Collection::ChangeScale(body, { 0.0f, 0.0f }, KillDuration, Easing::BackIn));

	if (angles.has_value())
	{
		auto arrow = IMSCENE->attachTemporaryNode<Scene::Rectangle>(*body);
		arrow->setSize({ 1.0f, 4.0f });
		arrow->setPivot({ 0.5f, 1.0f });
		arrow->setAnchor({ 0.5f, 0.0f });
		arrow->setColor(color);
		IMSCENE->setupPreKillAction(player, Actions::Collection::Wait(KillDuration)); // wait until parent die
	}

	float y = -2.0f;

	for (const auto& [key, text] : labels)
	{
		y -= 10.0f;
		auto label = IMSCENE->attachTemporaryNode<Scene::Label>(*player, key);
		label->setPivot(0.5f);
		label->setAnchor({ 0.5f, 0.0f });
		label->setY(IMSCENE->nodeWasInitialized() ? y : Common::Helpers::SmoothValueAssign(label->getY(), y, dTime));
		label->setFontSize(10.0f);
		label->setText(text);
		IMSCENE->setupPreKillAction(label, Actions::Collection::ChangeScale(label, { 0.0f, 0.0f }, KillDuration, Easing::BackIn));
		if (IMSCENE->nodeWasInitialized())
		{
			label->setScale(0.0f);
			label->runAction(Actions::Collection::ChangeScale(label, { 1.0f, 1.0f }, SpawnDuration, Easing::BackOut));
		}
	}
}

glm::vec2 GameplayViewNode::worldToScreen(const glm::vec3& value) const
{
	auto origin = mOverviewInfo->getOrigin();

	auto result = glm::vec2{ value.y, value.x };
	result -= glm::vec2{ origin.y, origin.x };
	if (mOverviewInfo->isRotated())
	{
		auto _x = result.x;
		result.x = -result.y;
		result.y = _x;
	}
	result /= 8192.0f;
	result *= mOverviewInfo->getZoom();
	result *= getAbsoluteSize();
	result.y *= 1024.0f / 768.0f;
	result *= -1.0f;
	result += getAbsoluteSize() * 0.5f;

	return result;
}

glm::vec3 GameplayViewNode::screenToWorld(const glm::vec2& value) const
{
	auto origin = mOverviewInfo->getOrigin();

	auto result = value;
	result -= getAbsoluteSize() * 0.5f;
	result *= -1.0f;
	result /= getAbsoluteSize();
	result /= mOverviewInfo->getZoom();
	result *= 8192.0f;
	result.y /= 1024.0f / 768.0f;
	if (mOverviewInfo->isRotated())
	{
		auto _x = result.x;
		result.x = result.y;
		result.y = -_x;
	}
	result += glm::vec2{ origin.y, origin.x };

	return { result.y, result.x, 0.0f };
}

float GameplayViewNode::worldToScreenAngles(const glm::vec3& value) const
{
	auto result = glm::radians(-value.y);

	if (mOverviewInfo->isRotated())
		result += glm::radians(90.0f);

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

std::string GameplayViewNode::getNiceModelName(const HL::Protocol::Resource& model) const
{
	if (model.name.substr(0, 1) == "*")
		return model.name;

	auto path = std::filesystem::path(model.name);
	return path.filename().replace_extension().string();
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

void GameplayViewNode::ensureOverviewInfoLoaded()
{
	if (mOverviewInfo.has_value())
		return;

	auto info = mClient->getServerInfo().value();
	std::filesystem::path map_path = info.map;
	auto map_name = map_path.filename().replace_extension().string();
	auto txt_path = fmt::format("overviews/{}.txt", map_name);
	mOverviewInfo = OverviewInfo();
	if (Platform::Asset::Exists(txt_path))
	{
		mOverviewInfo->load(Platform::Asset(txt_path));
	}
}
