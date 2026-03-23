#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <random>
#include <filesystem>

using namespace geode::prelude;

// -------------------------
// Флаги
// -------------------------
bool pausedByMod = false;
bool gonnaPause = false;
bool canPlayEffect = false;

// -------------------------
// Рандом
// -------------------------
int getRandInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

// -------------------------
// Клавиатура
// -------------------------
$on_mod(Loaded) {
    log::info("Registering keyboard listener...");

    KeyboardInputEvent::listen([](KeyboardInputData& ev) -> bool {
        if (ev.key == KEY_V && ev.action == KeyboardInputData::Action::Press) {
            auto pl = PlayLayer::get();
            if (!pl) return true;

            auto player = pl->m_player1;
            if (!player) return true;

            gonnaPause = true;
            static_cast<ShortsEditPO*>(player)->scheduleOnce(
                schedule_selector(ShortsEditPO::thoseWhoKnow),
                Mod::get()->getSettingValue<double>("action-delay")
            );
        }
        return true; // propagate
    });
}

// -------------------------
// Модификация PlayLayer
// -------------------------
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

    void updateReleaseValidPL(float dt) {
        canPlayEffect = true;
        this->unschedule(schedule_selector(ShortsEditPL::updateReleaseValidPL));
    }

    void loadCustomAssets() {
        auto fields = m_fields.self();

        fields->builtinImages.clear();
        fields->customImages.clear();
        fields->builtinSounds.clear();
        fields->customSounds.clear();

        for (int i = 1; i <= 14; i++) {
            fields->builtinImages.push_back(fmt::format("editImg_{}.png"_spr, i));
        }
        for (int i = 1; i <= 26; i++) {
            fields->builtinSounds.push_back(fmt::format("phonk_{}.ogg"_spr, i));
        }

        if (Mod::get()->getSettingValue<bool>("include-custom-images")) {
            auto imagesDir = Mod::get()->getConfigDir(true) / "images";
            if (std::filesystem::exists(imagesDir) && std::filesystem::is_directory(imagesDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(imagesDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".png") {
                        fields->customImages.push_back(entry.path().string());
                    }
                }
            }
        }

        if (Mod::get()->getSettingValue<bool>("include-custom-sounds")) {
            auto soundsDir = Mod::get()->getConfigDir(true) / "phonk";
            if (std::filesystem::exists(soundsDir) && std::filesystem::is_directory(soundsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(soundsDir)) {
                    auto ext = entry.path().extension().string();
                    if (entry.is_regular_file() && (ext == ".ogg" || ext == ".mp3" || ext == ".wav")) {
                        fields->customSounds.push_back(entry.path().string());
                    }
                }
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

    std::string getRandomSound() {
        auto fields = m_fields.self();
        std::vector<std::string> allSounds;
        if (Mod::get()->getSettingValue<bool>("include-builtin-sounds"))
            allSounds.insert(allSounds.end(), fields->builtinSounds.begin(), fields->builtinSounds.end());
        if (Mod::get()->getSettingValue<bool>("include-custom-sounds"))
            allSounds.insert(allSounds.end(), fields->customSounds.begin(), fields->customSounds.end());
        if (allSounds.empty()) return fields->builtinSounds[0];
        return allSounds[getRandInt(0, allSounds.size() - 1)];
    }

    CCTexture2D* renderPL() {
        auto fields = m_fields.self();
        if (!fields->plRenderer) return nullptr;

        if (fields->grayscreen) fields->grayscreen->setVisible(false);

        auto view = CCEGLView::get();
        auto director = CCDirector::sharedDirector();
        auto winSize = director->getWinSize();
        auto ogRes = view->getDesignResolutionSize();
        float ogX = view->m_fScaleX;
        float ogY = view->m_fScaleY;

        CCSize size{roundf(320.f * (winSize.width / winSize.height)), 320.f};
        CCSize newScale{winSize.width / size.width, winSize.height / size.height};
        float scale = director->getContentScaleFactor() / utils::getDisplayFactor();

        director->m_obWinSizeInPoints = size;
        view->setDesignResolutionSize(size.width, size.height, ResolutionPolicy::kResolutionExactFit);
        view->m_fScaleX = scale * newScale.width;
        view->m_fScaleY = scale * newScale.height;

        fields->plRenderer->beginWithClear(0, 0, 0, 0);
        this->visit();
        fields->plRenderer->end();

        director->m_obWinSizeInPoints = ogRes;
        view->setDesignResolutionSize(ogRes.width, ogRes.height, ResolutionPolicy::kResolutionExactFit);
        view->m_fScaleX = ogX;
        view->m_fScaleY = ogY;

        return fields->plRenderer->getSprite()->getTexture();
    }
};

// -------------------------
// Модификация PlayerObject
// -------------------------
class $modify(ShortsEditPO, PlayerObject) {
public:
    void thoseWhoKnow(float dt) {
        if (!canPlayEffect || pausedByMod) return;

        auto playLayer = PlayLayer::get();
        if (!playLayer) return;

        auto plFields = static_cast<ShortsEditPL*>(playLayer)->m_fields.self();
        if (!plFields) return;

        auto sprite = static_cast<CCSprite*>(playLayer->getChildByID("no-description-needed"_spr));
        auto vign = playLayer->getChildByID("edit-vignette"_spr);

        if (!sprite || !vign || !plFields->grayscreen) return;

        auto rendered = static_cast<ShortsEditPL*>(playLayer)->renderPL();
        if (rendered) {
            plFields->grayscreen->setTexture(rendered);
            plFields->grayscreen->setTextureRect(CCRect(0, 0, rendered->getContentSize().width, rendered->getContentSize().height));
        }

        vign->setVisible(true);
        sprite->setVisible(true);
        plFields->grayscreen->setVisible(true);

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

// -------------------------
// Модификация GJBaseGameLayer (кнопки)
// -------------------------
class $modify(ShortsEditGJBGL, GJBaseGameLayer) {
public:
    struct ButtonPositionData {
        float xPos, yPos;
        float xAnchor, yAnchor;
    };

    ButtonPositionData getButtonPosData(std::string chosenSetting) {
        ButtonPositionData data;
        float safeZone = 7.f;

        if (chosenSetting == "Top Left") {
            data.xPos = safeZone;
            data.yPos = CCDirector::sharedDirector()->getWinSize().height - safeZone;
            data.xAnchor = 0.f; data.yAnchor = 1.f;
        } else if (chosenSetting == "Bottom Left") {
            data.xPos = safeZone; data.yPos = safeZone;
            data.xAnchor = 0.f; data.yAnchor = 0.f;
        } else if (chosenSetting == "Bottom Right") {
            data.xPos = CCDirector::sharedDirector()->getWinSize().width - safeZone;
            data.yPos = safeZone;
            data.xAnchor = 1.f; data.yAnchor = 0.f;
        } else { // default
            data.xPos = CCDirector::sharedDirector()->getWinSize().width - safeZone;
            data.yPos = safeZone;
            data.xAnchor = 1.f; data.yAnchor = 0.f;
        }
        return data;
    }

    void activateManually() {
        auto playLayer = PlayLayer::get();
        if (!playLayer) return;
        if (!Mod::get()->getSettingValue<bool>("trigger-manually")) return;
        if (pausedByMod || gonnaPause || !canPlayEffect) return;
        if (!m_isPlatformer && playLayer->getCurrentPercentInt() == 100) return;

        auto player = this->m_player1;
        if (!player) return;
        gonnaPause = true;
        static_cast<ShortsEditPO*>(player)->scheduleOnce(
            schedule_selector(ShortsEditPO::thoseWhoKnow),
            Mod::get()->getSettingValue<double>("action-delay")
        );
    }

    void literallyTheSameThingButForTheButton(CCObject* sender) {
        activateManually();
    }

    bool init() {
        if (!GJBaseGameLayer::init()) return false;
        if (!Mod::get()->getSettingValue<bool>("trigger-manually")) return true;

        #ifndef GEODE_IS_IOS
        this->addEventListener<InvokeBindFilter>([=, this](InvokeBindEvent* event) {
            if (event->isDown()) activateManually();
            return ListenerResult::Propagate;
        }, "activate-phonk-edit"_spr);
        #endif

        if (Mod::get()->getSettingValue<bool>("enable-button")) {
            auto btnPosData = getButtonPosData(Mod::get()->getSettingValue<std::string>("button-pos"));
            auto btnSpr = CCSprite::createWithSpriteFrameName("manualBtn.png"_spr);
            btnSpr->setOpacity(75);

            auto btn = CCMenuItemSpriteExtra::create(
                btnSpr,
                this,
                menu_selector(ShortsEditGJBGL::literallyTheSameThingButForTheButton)
            );

            auto menu = CCMenu::create();
            menu->setContentSize(btnSpr->getContentSize());
            menu->setPosition({btnPosData.xPos, btnPosData.yPos});
            menu->setAnchorPoint({btnPosData.xAnchor, btnPosData.yAnchor});
            menu->ignoreAnchorPointForPosition(false);
            menu->setID("manual-phonk-btn"_spr);
            menu->addChild(btn);

            btn->setPosition({menu->getContentSize().width / 2.f, menu->getContentSize().height / 2.f});
            m_uiLayer->addChild(menu);
        }
        return true;
    }
};

// -------------------------
// Модификация PauseLayer
// -------------------------
class $modify(ShortsEditPauseLayer, PauseLayer) {
public:
    void okayDude(float dt) {
        pausedByMod = false;
        FMODAudioEngine::sharedEngine()->stopAllEffects();
        PauseLayer::onResume(nullptr);

        auto pl = PlayLayer::get();
        if (!pl) return;
        auto plFields = static_cast<ShortsEditPL*>(pl)->m_fields.self();
        if (!plFields) return;

        auto sprite = pl->getChildByID("no-description-needed"_spr);
        auto vign = pl->getChildByID("edit-vignette"_spr);

        if (sprite) sprite->setVisible(false);
        if (vign) vign->setVisible(false);
        if (plFields->grayscreen) plFields->grayscreen->setVisible(false);
    }

    void customSetup() {
        PauseLayer::customSetup();

        auto pl = PlayLayer::get();
        if (!pl) return;
        auto plFields = static_cast<ShortsEditPL*>(pl)->m_fields.self();
        if (!plFields) return;

        if (gonnaPause) gonnaPause = false;

        if (pausedByMod) {
            this->setPositionY(3000.f);
            auto phonkSound = static_cast<ShortsEditPL*>(pl)->getRandomSound();

            auto fmod = FMODAudioEngine::sharedEngine();
            fmod->resumeAllEffects();
            fmod->playEffect(phonkSound.c_str());

            this->scheduleOnce(schedule_selector(ShortsEditPauseLayer::okayDude),
                               Mod::get()->getSettingValue<double>("effect-duration"));
        }
    }

    void onResume(CCObject* sender) {
        if (!pausedByMod) PauseLayer::onResume(sender);

        auto pl = PlayLayer::get();
        if (!pl) return;
        auto player = pl->m_player1;
        if (!player) return;

        canPlayEffect = false;
        static_cast<ShortsEditPO*>(player)->scheduleOnce(
            schedule_selector(ShortsEditPO::updateReleaseValid),
            Mod::get()->getSettingValue<double>("action-cooldown")
        );

        if (gonnaPause) gonnaPause = false;
    }

    void onQuit(CCObject* sender) { if (!pausedByMod) PauseLayer::onQuit(sender); }
    void tryQuit(CCObject* sender) { if (!pausedByMod) PauseLayer::tryQuit(sender); }
    void onRestart(CCObject* sender) { if (!pausedByMod) PauseLayer::onRestart(sender); }
    void onRestartFull(CCObject* sender) { if (!pausedByMod) PauseLayer::onRestartFull(sender); }
    void onPracticeMode(CCObject* sender) { if (!pausedByMod) PauseLayer::onPracticeMode(sender); }
    void onNormalMode(CCObject* sender) { if (!pausedByMod) PauseLayer::onNormalMode(sender); }
};
