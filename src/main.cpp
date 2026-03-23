#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <random>
#include <filesystem>

using namespace geode::prelude;

// Флаги мода
bool pausedByMod = false;
bool gonnaPause = false;
bool canPlayEffect = false;

// Рандом
int getRandInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

// Данные позиции кнопки
struct ButtonPositionData {
    float xPos;
    float yPos;
    float xAnchor;
    float yAnchor;
};

// Подключение к событию клавиатуры
$on_mod(Loaded) {
    log::info("Registering keyboard listener...");

    KeyboardInputEvent::listen([](KeyboardInputData& ev){
        if (ev.key == KEY_V && ev.action == KeyboardInputData::Action::Press) {
            auto pl = PlayLayer::get();
            if (!pl) return true;

            auto player = pl->m_player1;
            if (!player) return true;

            gonnaPause = true;
            static_cast<PlayerObject*>(player)->scheduleOnce(
                schedule_selector([](float dt){ /* эффект */ }),
                Mod::get()->getSettingValue<double>("action-delay")
            );
        }
        return true;
    });
}

// Поля для PlayLayer
struct ShortsEditFields {
    Ref<CCRenderTexture> plRenderer = nullptr;
    CCSpriteGrayscale* grayscreen = nullptr;
    std::vector<std::string> builtinImages;
    std::vector<std::string> customImages;
    std::vector<std::string> builtinSounds;
    std::vector<std::string> customSounds;
};

// Модификация PlayLayer
class $modify(ShortsEditPL, PlayLayer) {
    ShortsEditFields m_fields;

    void loadCustomAssets() {
        m_fields.builtinImages.clear();
        for (int i = 1; i <= 14; i++)
            m_fields.builtinImages.push_back(fmt::format("editImg_{}.png"_spr, i));

        m_fields.builtinSounds.clear();
        for (int i = 1; i <= 26; i++)
            m_fields.builtinSounds.push_back(fmt::format("phonk_{}.ogg"_spr, i));

        if (Mod::get()->getSettingValue<bool>("include-custom-images")) {
            auto dir = Mod::get()->getConfigDir(true) / "images";
            if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
                for (auto& entry : std::filesystem::directory_iterator(dir))
                    if (entry.path().extension() == ".png")
                        m_fields.customImages.push_back(entry.path().string());
        }

        if (Mod::get()->getSettingValue<bool>("include-custom-sounds")) {
            auto dir = Mod::get()->getConfigDir(true) / "phonk";
            if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
                for (auto& entry : std::filesystem::directory_iterator(dir)) {
                    auto ext = entry.path().extension().string();
                    if (ext == ".ogg" || ext == ".mp3" || ext == ".wav")
                        m_fields.customSounds.push_back(entry.path().string());
                }
        }
    }

    std::string getRandomImage() {
        std::vector<std::string> allImages;
        if (Mod::get()->getSettingValue<bool>("include-builtin-images"))
            allImages.insert(allImages.end(), m_fields.builtinImages.begin(), m_fields.builtinImages.end());
        if (Mod::get()->getSettingValue<bool>("include-custom-images"))
            allImages.insert(allImages.end(), m_fields.customImages.begin(), m_fields.customImages.end());
        if (allImages.empty()) return m_fields.builtinImages[0];
        return allImages[getRandInt(0, allImages.size() - 1)];
    }

    std::string getRandomSound() {
        std::vector<std::string> allSounds;
        if (Mod::get()->getSettingValue<bool>("include-builtin-sounds"))
            allSounds.insert(allSounds.end(), m_fields.builtinSounds.begin(), m_fields.builtinSounds.end());
        if (Mod::get()->getSettingValue<bool>("include-custom-sounds"))
            allSounds.insert(allSounds.end(), m_fields.customSounds.begin(), m_fields.customSounds.end());
        if (allSounds.empty()) return m_fields.builtinSounds[0];
        return allSounds[getRandInt(0, allSounds.size() - 1)];
    }

    // Рендер для grayscale
    CCTexture2D* renderPL() {
        if (!m_fields.plRenderer) return nullptr;
        if (m_fields.grayscreen) m_fields.grayscreen->setVisible(false);

        auto view = CCEGLView::get();
        auto director = CCDirector::sharedDirector();
        auto winSize = director->getWinSize();

        m_fields.plRenderer->beginWithClear(0,0,0,0);
        this->visit();
        m_fields.plRenderer->end();

        return m_fields.plRenderer->getSprite()->getTexture();
    }
};

// Модификация PlayerObject
class $modify(ShortsEditPO, PlayerObject) {
    void triggerEffect(float dt) {
        if (!canPlayEffect || pausedByMod) return;
        auto pl = PlayLayer::get();
        if (!pl) return;

        auto fields = static_cast<ShortsEditPL*>(pl)->m_fields;
        auto sprite = pl->getChildByID("no-description-needed"_spr);
        auto vign = pl->getChildByID("edit-vignette"_spr);

        if (fields.grayscreen && fields.plRenderer) {
            auto tex = static_cast<ShortsEditPL*>(pl)->renderPL();
            if (tex) {
                fields.grayscreen->setTexture(tex);
                fields.grayscreen->setTextureRect(CCRect(0,0,tex->getContentSize().width, tex->getContentSize().height));
            }
        }

        if (sprite) sprite->setVisible(true);
        if (vign) vign->setVisible(true);
        if (fields.grayscreen) fields.grayscreen->setVisible(true);

        std::string img = static_cast<ShortsEditPL*>(pl)->getRandomImage();
        if (img.find("editImg_") != std::string::npos) {
            auto frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(img.c_str());
            if (frame && sprite) sprite->setDisplayFrame(frame);
        } else if (sprite) {
            auto tex = CCTextureCache::sharedTextureCache()->addImage(img.c_str(), false);
            if (tex) {
                sprite->setTexture(tex);
                sprite->setTextureRect(CCRect(0,0,tex->getContentSize().width,tex->getContentSize().height));
            }
        }

        gonnaPause = false;
        pausedByMod = true;
        pl->m_uiLayer->m_pauseBtn->activate();
    }
};

// Модификация PauseLayer
class $modify(ShortsEditPauseLayer, PauseLayer) {
    void resumeEffect(float dt) {
        pausedByMod = false;
        FMODAudioEngine::sharedEngine()->stopAllEffects();
        PauseLayer::onResume(nullptr);
    }
};
