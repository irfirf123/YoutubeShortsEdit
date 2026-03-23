#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <random>
#include <filesystem>

using namespace geode::prelude;

// --------------------------------------------------
// Utility: random integer
int getRandInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

// --------------------------------------------------
// Flags shared across classes
namespace {
    bool pausedByMod = false;
    bool gonnaPause = false;
    bool canPlayEffect = false;
}

// --------------------------------------------------
// Button position struct
struct ButtonPositionData {
    float xPos;
    float yPos;
    float xAnchor;
    float yAnchor;
};

// --------------------------------------------------
// Keyboard listener
$on_mod(Loaded) {
    log::info("Registering keyboard listener...");
    KeyboardInputEvent::listen([](KeyboardInputData& ev) -> bool {
        if (ev.key == KEY_V && ev.action == KeyboardInputData::Action::Press) {
            auto pl = PlayLayer::get();
            if (!pl || !pl->m_player1) return true;

            auto player = static_cast<ShortsEditPO*>(pl->m_player1);
            gonnaPause = true;
            player->scheduleOnce(schedule_selector(ShortsEditPO::triggerEffect), Mod::get()->getSettingValue<double>("action-delay"));
        }
        return true;
    });
}

// --------------------------------------------------
// Modify PlayerObject
class $modify(ShortsEditPO, PlayerObject) {
public:
    struct Fields {
        // можно хранить данные через Fields, если нужно
    };

    void triggerEffect(float dt) {
        if (!canPlayEffect || pausedByMod) return;

        auto playLayer = PlayLayer::get();
        if (!playLayer) return;

        auto plFields = static_cast<ShortsEditPL*>(playLayer)->m_fields.self();
        if (!plFields) return;

        auto sprite = dynamic_cast<CCSprite*>(playLayer->getChildByID("effect-sprite"_spr));
        auto vign = playLayer->getChildByID("effect-vignette"_spr);
        if (!sprite || !vign || !plFields->grayscreen) return;

        // Update textures
        auto rendered = static_cast<ShortsEditPL*>(playLayer)->renderPL();
        if (rendered) {
            plFields->grayscreen->setTexture(rendered);
            plFields->grayscreen->setTextureRect(CCRect(0, 0, rendered->getContentSize().width, rendered->getContentSize().height));
        }

        vign->setVisible(true);
        sprite->setVisible(true);
        plFields->grayscreen->setVisible(true);

        // Select random image
        std::string chosenImg = static_cast<ShortsEditPL*>(playLayer)->getRandomImage();
        if (chosenImg.find("editImg_") != std::string::npos) {
            auto frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(chosenImg.c_str());
            if (frame) sprite->setDisplayFrame(frame);
        } else {
            auto tex = CCTextureCache::sharedTextureCache()->addImage(chosenImg.c_str(), false);
            if (tex) {
                sprite->setTexture(tex);
                sprite->setTextureRect(CCRect(0, 0, tex->getContentSize().width, tex->getContentSize().height));
            }
        }

        gonnaPause = false;
        pausedByMod = true;
        playLayer->m_uiLayer->m_pauseBtn->activate();
    }

    void updateReleaseValid(float dt) {
        canPlayEffect = true;
        this->unschedule(schedule_selector(ShortsEditPO::updateReleaseValid));
    }
};

// --------------------------------------------------
// Modify PlayLayer
class $modify(ShortsEditPL, PlayLayer) {
public:
    struct Fields {
        Ref<CCRenderTexture> plRenderer = nullptr;
        CCSpriteGrayscale* grayscreen = nullptr;

        std::vector<std::string> builtinImages;
        std::vector<std::string> customImages;
        std::vector<std::string> builtinSounds;
        std::vector<std::string> customSounds;
    };

    void loadCustomAssets() {
        auto fields = m_fields.self();

        fields->builtinImages.clear();
        fields->customImages.clear();
        fields->builtinSounds.clear();
        fields->customSounds.clear();

        for (int i = 1; i <= 14; i++)
            fields->builtinImages.push_back(fmt::format("editImg_{}.png"_spr, i));

        for (int i = 1; i <= 26; i++)
            fields->builtinSounds.push_back(fmt::format("phonk_{}.ogg"_spr, i));

        auto imagesDir = Mod::get()->getConfigDir(true) / "images";
        if (Mod::get()->getSettingValue<bool>("include-custom-images") && std::filesystem::exists(imagesDir)) {
            for (auto& entry : std::filesystem::directory_iterator(imagesDir))
                if (entry.is_regular_file() && entry.path().extension() == ".png")
                    fields->customImages.push_back(entry.path().string());
        }

        auto soundsDir = Mod::get()->getConfigDir(true) / "phonk";
        if (Mod::get()->getSettingValue<bool>("include-custom-sounds") && std::filesystem::exists(soundsDir)) {
            for (auto& entry : std::filesystem::directory_iterator(soundsDir)) {
                auto ext = entry.path().extension().string();
                if (entry.is_regular_file() && (ext == ".ogg" || ext == ".mp3" || ext == ".wav"))
                    fields->customSounds.push_back(entry.path().string());
            }
        }
    }

    std::string getRandomImage() {
        auto fields = m_fields.self();
        std::vector<std::string> allImages;
        if (Mod::get()->getSettingValue<bool>("include-builtin-images"))
            allImages.insert(allImages.end(), fields->builtinImages.begin(), fields->builtinImages.end());
        if (Mod::get()->getSettingValue<bool>("include-custom-images"))
            allImages.insert(allImages.end(), fields->customImages.begin(), fields->customImages.end());
        if (allImages.empty()) return fields->builtinImages[0];
        return allImages[getRandInt(0, allImages.size() - 1)];
    }

    CCTexture2D* renderPL() {
        auto fields = m_fields.self();
        if (!fields->plRenderer) return nullptr;

        auto view = CCEGLView::get();
        auto director = CCDirector::sharedDirector();
        auto winSize = director->getWinSize();
        auto ogRes = view->getDesignResolutionSize();
        float ogX = view->m_fScaleX, ogY = view->m_fScaleY;

        CCSize size = {roundf(320.f * (winSize.width / winSize.height)), 320.f};
        CCSize newScale = {winSize.width / size.width, winSize.height / size.height};
        float scale = director->getContentScaleFactor() / utils::getDisplayFactor();

        director->m_obWinSizeInPoints = size;
        view->setDesignResolutionSize(size.width, size.height, ResolutionPolicy::kResolutionExactFit);
        view->m_fScaleX = scale * newScale.width;
        view->m_fScaleY = scale * newScale.height;

        fields->plRenderer->beginWithClear(0,0,0,0);
        this->visit();
        fields->plRenderer->end();

        director->m_obWinSizeInPoints = ogRes;
        view->setDesignResolutionSize(ogRes.width, ogRes.height, ResolutionPolicy::kResolutionExactFit);
        view->m_fScaleX = ogX; view->m_fScaleY = ogY;

        return fields->plRenderer->getSprite()->getTexture();
    }
};

// --------------------------------------------------
// Modify PauseLayer
class $modify(ShortsEditPauseLayer, PauseLayer) {
    void restoreAfterEffect(float dt) {
        pausedByMod = false;
        FMODAudioEngine::sharedEngine()->stopAllEffects();
        PauseLayer::onResume(nullptr);

        auto pl = PlayLayer::get();
        if (!pl) return;
        auto fields = static_cast<ShortsEditPL*>(pl)->m_fields.self();
        if (!fields) return;

        auto sprite = pl->getChildByID("effect-sprite"_spr);
        auto vign = pl->getChildByID("effect-vignette"_spr);
        if (sprite) sprite->setVisible(false);
        if (vign) vign->setVisible(false);
        if (fields->grayscreen) fields->grayscreen->setVisible(false);
    }

    void customSetup() {
        PauseLayer::customSetup();
        if (!pausedByMod) return;

        this->setPositionY(3000.f);
        auto pl = PlayLayer::get();
        if (!pl) return;

        std::string sound = static_cast<ShortsEditPL*>(pl)->getRandomSound();
        FMODAudioEngine::sharedEngine()->playEffect(sound.c_str());

        this->scheduleOnce(schedule_selector(ShortsEditPauseLayer::restoreAfterEffect), Mod::get()->getSettingValue<double>("effect-duration"));
    }
};
