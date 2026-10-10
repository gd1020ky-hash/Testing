#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

// --- Data ---
struct FrameInput {
    double timestamp;
    bool isPlayer1;
    int frameWindow;
};

// --- Globals ---
static std::vector<FrameInput> s_inputHistory;
static std::vector<int> s_frameWindows;
static double s_levelStartTime = 0.0;

// Settings
static bool s_hudVisible = true;
static bool s_showHistory = true;
static bool s_showPrecision = true;
static bool s_playSound = true;

// HUD elements
static cocos2d::CCLabelBMFont* s_currentFWLabel = nullptr;
static cocos2d::CCLabelBMFont* s_historyLabel = nullptr;
static cocos2d::CCLabelBMFont* s_precisionLabel = nullptr;

ccColor3B getColorForFW(int fw) {
    if (fw == 0) return ccGREEN;
    if (std::abs(fw) <= 2) return ccYELLOW;
    return ccRED;
}

// ============================================================
// POPUP MENU
// ============================================================
class FrameAnalysisPopup : public geode::Popup<> {
protected:
    bool setup() override {
        this->setTitle("Frame Analysis");

        auto winSize = m_mainLayer->getContentSize();

        // Helper to create a labeled toggle
        auto makeToggle = [&](const char* label, bool* setting, CCPoint pos) {
            auto lbl = CCLabelBMFont::create(label, "bigFont.fnt");
            lbl->setScale(0.4f);
            lbl->setAnchorPoint({ 0.f, 0.5f });
            lbl->setPosition({ pos.x - 60.f, pos.y });
            m_mainLayer->addChild(lbl);

            auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            auto on  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto tog = CCMenuItemToggler::create(off, on, this,
                menu_selector(FrameAnalysisPopup::onToggle));
            tog->setPosition(pos);
            tog->setUserObject(CCString::create(label));
            tog->toggle(*setting);
            m_buttonMenu->addChild(tog);
        };

        makeToggle("Show HUD",        &s_hudVisible,     { winSize.width / 2, winSize.height - 50 });
        makeToggle("Input History",   &s_showHistory,    { winSize.width / 2, winSize.height - 80 });
        makeToggle("Precision Score", &s_showPrecision,  { winSize.width / 2, winSize.height - 110 });
        makeToggle("Sound Feedback",  &s_playSound,      { winSize.width / 2, winSize.height - 140 });

        return true;
    }

    void onToggle(CCObject* sender) {
        auto tog = static_cast<CCMenuItemToggler*>(sender);
        auto name = static_cast<CCString*>(tog->getUserObject())->getCString();
        bool state = !tog->isToggled();

        std::string n = name;
        if (n == "Show HUD")        s_hudVisible = state;
        if (n == "Input History")   s_showHistory = state;
        if (n == "Precision Score") s_showPrecision = state;
        if (n == "Sound Feedback")  s_playSound = state;
    }

public:
    static FrameAnalysisPopup* create() {
        auto ret = new FrameAnalysisPopup();
        if (ret && ret->initAnchored(240.f, 200.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ============================================================
// MENU LAYER: Add the button
// ============================================================
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto winSize = CCDirector::get()->getWinSize();

        // Stylish button using GD's own button sprite
        auto spr = ButtonSprite::create("FA", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyMenuLayer::onOpenMenu));
        btn->setPosition({ winSize.width - 35.f, 35.f }); // bottom-right, out of the way

        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setZOrder(100);
        this->addChild(menu);

        return true;
    }

    void onOpenMenu(CCObject*) {
        FrameAnalysisPopup::create()->show();
    }
};

// ============================================================
// PLAYER HOOK
// ============================================================
class $modify(MyPlayerObject, PlayerObject) {
    void pushButton(PlayerButton button) {
        double now = CCDirector::get()->getActualDeltaTime();
        double timeSinceStart = now - s_levelStartTime;

        double tickPosition = timeSinceStart * 240.0;
        int exactTick = static_cast<int>(tickPosition);
        double fractionalOffset = tickPosition - exactTick;
        int frameWindow = static_cast<int>(std::round(fractionalOffset * 240.0));

        FrameInput input { timeSinceStart, (this == PlayLayer::get()->m_player1), frameWindow };
        s_inputHistory.push_back(input);
        s_frameWindows.push_back(frameWindow);

        if (s_playSound && frameWindow == 0) {
            FMODAudioEngine::sharedEngine()->playEffect("achievement_01.ogg");
        }

        PlayerObject::pushButton(button);
    }
};

// ============================================================
// PLAYLAYER HOOK
// ============================================================
class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        s_inputHistory.clear();
        s_frameWindows.clear();
        s_levelStartTime = CCDirector::get()->getActualDeltaTime();

        auto winSize = CCDirector::get()->getWinSize();

        s_currentFWLabel = CCLabelBMFont::create("FW: 0", "bigFont.fnt");
        s_currentFWLabel->setPosition({ 80, winSize.height - 40 });
        s_currentFWLabel->setScale(0.6f);
        s_currentFWLabel->setZOrder(9999);
        this->addChild(s_currentFWLabel);

        s_historyLabel = CCLabelBMFont::create("Recent: --", "chatFont.fnt");
        s_historyLabel->setPosition({ 80, winSize.height - 70 });
        s_historyLabel->setScale(0.5f);
        s_historyLabel->setZOrder(9999);
        this->addChild(s_historyLabel);

        s_precisionLabel = CCLabelBMFont::create("Precision: --", "chatFont.fnt");
        s_precisionLabel->setPosition({ 80, winSize.height - 95 });
        s_precisionLabel->setScale(0.5f);
        s_precisionLabel->setZOrder(9999);
        this->addChild(s_precisionLabel);

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (!s_currentFWLabel) return;

        s_currentFWLabel->setVisible(s_hudVisible);
        s_historyLabel->setVisible(s_hudVisible && s_showHistory);
        s_precisionLabel->setVisible(s_hudVisible && s_showPrecision);

        if (!s_hudVisible) return;

        if (!s_frameWindows.empty()) {
            int lastFW = s_frameWindows.back();
            s_currentFWLabel->setString(fmt::format("FW: {}", lastFW).c_str());
            s_currentFWLabel->setColor(getColorForFW(lastFW));
        }

        if (s_showHistory) {
            std::string hist = "Recent: ";
            int start = std::max(0, (int)s_frameWindows.size() - 5);
            for (int i = start; i < (int)s_frameWindows.size(); i++)
                hist += fmt::format("{} ", s_frameWindows[i]);
            if (s_frameWindows.empty()) hist += "--";
            s_historyLabel->setString(hist.c_str());
        }

        if (s_showPrecision && !s_frameWindows.empty()) {
            double sum = 0.0;
            for (int fw : s_frameWindows) sum += std::abs(fw);
            double score = sum / s_frameWindows.size();
            s_precisionLabel->setString(fmt::format("Precision: {:.2f}", score).c_str());
        }
    }
};
