#include "EditorPopup.hpp"

#include "../backend/LevelBackend.hpp"
#include "HelpPopup.hpp"

#include "gdcode/Encoder.hpp"

#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace gdcode::ui {

namespace {

constexpr float kPopupW = 560.f;
constexpr float kPopupH = 300.f;
constexpr float kEditorW = 356.f;
constexpr float kEditorH = 202.f;
constexpr float kProblemsW = 172.f;
constexpr float kProblemsH = kEditorH;
constexpr float kMargin = 12.f;
constexpr float kTopOffset = 34.f;

ccColor3B colorFor(Severity s) {
    switch (s) {
        case Severity::Error: return {255, 90, 90};
        case Severity::Warning: return {255, 200, 80};
        case Severity::Info: return {150, 200, 255};
    }
    return {255, 255, 255};
}

char const* prefixFor(Severity s) {
    switch (s) {
        case Severity::Error: return "error";
        case Severity::Warning: return "warning";
        case Severity::Info: return "info";
    }
    return "";
}

} // namespace

EditorPopup* EditorPopup::create(storage::Project project) {
    auto* ret = new EditorPopup();
    if (ret->init(std::move(project))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool EditorPopup::init(storage::Project project) {
    m_project = std::move(project);
    if (!Popup::init(kPopupW, kPopupH, "GJ_square02.png")) return false;
    this->setID("editor-popup"_spr);
    this->setTitle(fmt::format("GDCode - {}", m_project.meta.name), "goldFont.fnt", 0.65f, 18.f);

    // --- code editor (left) -------------------------------------------------
    m_editor = CodeEditor::create({kEditorW, kEditorH});
    m_mainLayer->addChildAtPosition(m_editor, Anchor::TopLeft,
                                    {kMargin + kEditorW / 2, -(kTopOffset + kEditorH / 2)});
    m_editor->setText(m_project.source);
    m_editor->setOnChange([this] {
        m_dirty = true;
        updateStatus();
        scheduleLiveCheck();
        scheduleAutosave();
    });

    // --- problems panel (right) --------------------------------------------
    auto* problemsBg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    problemsBg->setColor({0, 0, 0});
    problemsBg->setOpacity(120);
    problemsBg->setContentSize({kProblemsW, kProblemsH});
    m_mainLayer->addChildAtPosition(problemsBg, Anchor::TopRight,
                                    {-(kMargin + kProblemsW / 2), -(kTopOffset + kProblemsH / 2)});

    m_problemsTitle = CCLabelBMFont::create("Problems", "bigFont.fnt");
    m_problemsTitle->setScale(0.4f);
    m_problemsTitle->setAnchorPoint({0.f, 0.5f});
    problemsBg->addChildAtPosition(m_problemsTitle, Anchor::TopLeft, {6.f, -10.f});

    m_problems = ScrollLayer::create({kProblemsW - 8.f, kProblemsH - 24.f});
    m_problems->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(2.f));
    m_problems->setTouchEnabled(true);
    problemsBg->addChildAtPosition(m_problems, Anchor::BottomLeft, {4.f, 4.f});

    // --- status line --------------------------------------------------------
    m_status = CCLabelBMFont::create("", "chatFont.fnt");
    m_status->setScale(0.55f);
    m_status->setAnchorPoint({0.f, 0.5f});
    m_mainLayer->addChildAtPosition(m_status, Anchor::TopLeft, {kMargin, -(kTopOffset - 10.f)});

    m_position = CCLabelBMFont::create("", "chatFont.fnt");
    m_position->setScale(0.55f);
    m_position->setAnchorPoint({1.f, 0.5f});
    m_position->setOpacity(160);
    m_mainLayer->addChildAtPosition(m_position, Anchor::TopLeft,
                                    {kMargin + kEditorW, -(kTopOffset - 10.f)});

    // --- buttons ------------------------------------------------------------
    auto* row = CCMenu::create();
    row->setContentSize({kPopupW - 2 * kMargin, 34.f});
    row->setLayout(RowLayout::create()->setGap(6.f)->setAxisAlignment(AxisAlignment::Center));
    m_mainLayer->addChildAtPosition(row, Anchor::Bottom, {0.f, 26.f});

    row->addChild(makeButton("Check", "GJ_button_04.png", 62.f, [this] { onCheck(nullptr); }));
    row->addChild(makeButton("Generate", "GJ_button_01.png", 86.f, [this] { onGenerate(false); }));
    row->addChild(makeButton("New Level", "GJ_button_02.png", 88.f, [this] { onGenerate(true); }));
    row->addChild(makeButton("Copy", "GJ_button_04.png", 58.f, [this] { onCopyAll(nullptr); }));
    row->addChild(makeButton("Paste", "GJ_button_04.png", 58.f, [this] { onPaste(nullptr); }));
    row->addChild(makeButton("Undo", "GJ_button_04.png", 58.f, [this] { onUndo(nullptr); }));
    row->addChild(makeButton("?", "GJ_button_05.png", 30.f, [this] { onHelp(nullptr); }));
    row->updateLayout();

    updateStatus();
    runLiveCheck(0.f);
#ifdef GEODE_IS_DESKTOP
    // On desktop start typing right away; on mobile wait for a tap so the
    // soft keyboard does not cover the window unasked.
    m_editor->focus();
#endif
    return true;
}

CCMenuItemSpriteExtra* EditorPopup::makeButton(char const* text, char const* bg, float width,
                                               std::function<void()> cb) {
    auto* spr = ButtonSprite::create(text, static_cast<int>(width), 0, 0.55f, true, "bigFont.fnt",
                                     bg, 26.f);
    auto* btn = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) { cb(); });
    return btn;
}

// ---------------------------------------------------------------------------
// keyboard / close
// ---------------------------------------------------------------------------

void EditorPopup::keyDown(enumKeyCodes key, double timestamp) {
    // While typing, Enter/Space must not "click" popup buttons and the editor
    // owns navigation keys; Escape still closes (and autosaves).
    if (m_editor && m_editor->isFocused() && key != KEY_Escape) return;
    Popup::keyDown(key, timestamp);
}

void EditorPopup::onClose(CCObject* sender) {
    if (m_editor) m_editor->blur();
    this->unschedule(schedule_selector(EditorPopup::runAutosave));
    this->unschedule(schedule_selector(EditorPopup::runLiveCheck));
    if (m_dirty) saveProject(false);
    if (m_onClosed) m_onClosed();
    Popup::onClose(sender);
}

// ---------------------------------------------------------------------------
// compile / diagnostics
// ---------------------------------------------------------------------------

CompileResult EditorPopup::compileCurrent() {
    CompileOptions opts;
    opts.defaultLevelName = m_project.meta.name;
    auto maxObjects = Mod::get()->getSettingValue<int64_t>("max-objects");
    if (maxObjects > 0) opts.limits.maxObjects = static_cast<std::size_t>(maxObjects);
    return compile(m_editor->getText(), opts);
}

void EditorPopup::scheduleLiveCheck() {
    this->unschedule(schedule_selector(EditorPopup::runLiveCheck));
    this->scheduleOnce(schedule_selector(EditorPopup::runLiveCheck), 0.45f);
}

void EditorPopup::runLiveCheck(float) {
    auto result = compileCurrent();
    m_lastErrorCount = result.diagnostics.errorCount();
    m_lastObjectCount = result.stats.objectCount;
    rebuildProblems(result);
    updateStatus();
}

void EditorPopup::onCheck(CCObject*) {
    auto result = compileCurrent();
    m_lastErrorCount = result.diagnostics.errorCount();
    m_lastObjectCount = result.stats.objectCount;
    rebuildProblems(result);
    updateStatus();
    if (result.ok()) {
        Notification::create(fmt::format("OK: {} objects, no errors", result.stats.objectCount),
                             NotificationIcon::Success)
            ->show();
    } else {
        Notification::create(fmt::format("{} error(s) - see Problems", m_lastErrorCount),
                             NotificationIcon::Error)
            ->show();
        // Jump to the first error.
        for (auto const& d : result.diagnostics.items()) {
            if (d.isError() && d.span.valid()) {
                m_editor->gotoPosition(d.span.begin.line, d.span.begin.column);
                m_editor->setHighlightLine(d.span.begin.line);
                break;
            }
        }
    }
}

void EditorPopup::rebuildProblems(CompileResult const& result) {
    auto* content = m_problems->m_contentLayer;
    content->removeAllChildrenWithCleanup(true);

    std::size_t errors = result.diagnostics.errorCount();
    m_problemsTitle->setString(errors ? fmt::format("Problems ({})", errors).c_str() : "Problems");
    m_problemsTitle->setColor(errors ? ccColor3B{255, 110, 110} : ccColor3B{255, 255, 255});

    float const rowWidth = m_problems->getContentWidth();

    // One row = fixed-width node holding a menu with a single clickable text
    // area; clicking jumps the editor to the diagnostic's position.
    auto addRow = [&](std::string const& text, ccColor3B color, std::function<void()> onClick) {
        auto* area = SimpleTextArea::create(text, "chatFont.fnt", 0.45f, rowWidth - 8.f);
        area->setWrappingMode(WrappingMode::WORD_WRAP);
        area->setColor(to4B(color));
        float rowHeight = std::max(12.f, area->getHeight() + 4.f);

        auto* row = CCNode::create();
        row->setContentSize({rowWidth, rowHeight});
        row->setAnchorPoint({0.5f, 0.5f});

        auto* bg = CCLayerColor::create({255, 255, 255, 14}, rowWidth, rowHeight);
        row->addChild(bg);

        auto* holder = CCNode::create();
        holder->setContentSize({rowWidth, rowHeight});
        holder->setAnchorPoint({0.5f, 0.5f});
        area->setAnchorPoint({0.f, 0.5f});
        holder->addChildAtPosition(area, Anchor::Left, {4.f, 0.f});

        auto* menu = CCMenu::create();
        menu->setContentSize({rowWidth, rowHeight});
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({0.5f, 0.5f});
        auto* item = CCMenuItemExt::createSpriteExtra(holder, [onClick = std::move(onClick)](auto) {
            if (onClick) onClick();
        });
        item->m_scaleMultiplier = 1.f;
        menu->addChildAtPosition(item, Anchor::Center);
        row->addChildAtPosition(menu, Anchor::Center);
        content->addChild(row);
    };

    if (result.diagnostics.items().empty()) {
        std::string text = result.stats.objectCount
                               ? fmt::format("No problems. {} objects ready.", result.stats.objectCount)
                               : "No problems. The level is empty.";
        addRow(text, {140, 255, 140}, nullptr);
    }

    for (auto const& d : result.diagnostics.items()) {
        std::string head = d.span.valid()
                               ? fmt::format("L{} {}: ", d.span.begin.line, prefixFor(d.severity))
                               : fmt::format("{}: ", prefixFor(d.severity));
        std::string text = head + d.message;
        if (!d.suggestion.empty()) text += " Did you mean " + d.suggestion + "?";
        for (auto const& n : d.notes) text += " " + n;

        std::size_t line = d.span.begin.line;
        std::size_t column = d.span.begin.column;
        bool positioned = d.span.valid();
        addRow(text, colorFor(d.severity), [this, line, column, positioned] {
            if (!positioned) return;
            m_editor->gotoPosition(line, column);
            m_editor->setHighlightLine(line);
            m_editor->focus();
        });
    }
    content->updateLayout();
    m_problems->scrollToTop();
    handleTouchPriority(this);
}

void EditorPopup::updateStatus() {
    auto [line, col] = m_editor->cursorLineColumn();
    m_position->setString(fmt::format("Ln {}, Col {}  |  {} lines", line, col, m_editor->lineCount()).c_str());
    if (m_lastErrorCount > 0) {
        setStatus(fmt::format("{} error(s)", m_lastErrorCount), {255, 110, 110});
    } else {
        setStatus(fmt::format("{} objects{}", m_lastObjectCount, m_dirty ? "  (unsaved)" : ""),
                  {200, 255, 200});
    }
}

void EditorPopup::setStatus(std::string const& text, ccColor3B color) {
    m_status->setString(text.c_str());
    m_status->setColor(color);
}

// ---------------------------------------------------------------------------
// persistence
// ---------------------------------------------------------------------------

void EditorPopup::scheduleAutosave() {
    this->unschedule(schedule_selector(EditorPopup::runAutosave));
    this->scheduleOnce(schedule_selector(EditorPopup::runAutosave), 1.5f);
}

void EditorPopup::runAutosave(float) {
    if (m_dirty) saveProject(false);
}

bool EditorPopup::saveProject(bool announce) {
    m_project.source = m_editor->getText();
    auto res = storage::ProjectStore::get().save(m_project);
    if (!res) {
        log::error("GDCode: failed to save project {}: {}", m_project.meta.id, res.unwrapErr());
        Notification::create("Could not save project: " + res.unwrapErr(), NotificationIcon::Error)->show();
        return false;
    }
    m_dirty = false;
    updateStatus();
    if (announce) Notification::create("Project saved", NotificationIcon::Success)->show();
    return true;
}

// ---------------------------------------------------------------------------
// clipboard / undo / help
// ---------------------------------------------------------------------------

void EditorPopup::onCopyAll(CCObject*) {
    if (utils::clipboard::write(m_editor->getText())) {
        Notification::create("Code copied to clipboard", NotificationIcon::Success)->show();
    } else {
        Notification::create("Clipboard is not available", NotificationIcon::Error)->show();
    }
}

void EditorPopup::onPaste(CCObject*) {
    std::string clip = utils::clipboard::read();
    if (clip.empty()) {
        Notification::create("Clipboard is empty", NotificationIcon::Warning)->show();
        return;
    }
    if (m_editor->getText().empty()) {
        m_editor->insertAtCursor(clip);
        return;
    }
    createQuickPopup(
        "Paste",
        fmt::format("Clipboard holds <cy>{}</c> characters.\nInsert at the cursor, or replace the whole script?",
                    clip.size()),
        "Insert", "Replace",
        [this, clip](FLAlertLayer*, bool replace) {
            if (replace) {
                m_editor->setText(clip);
                m_dirty = true;
                scheduleLiveCheck();
                scheduleAutosave();
                updateStatus();
            } else {
                m_editor->insertAtCursor(clip);
            }
        });
}

void EditorPopup::onUndo(CCObject*) {
    if (!m_editor->undo()) Notification::create("Nothing to undo", NotificationIcon::Info)->show();
}

void EditorPopup::onHelp(CCObject*) {
    HelpPopup::create()->show();
}

// ---------------------------------------------------------------------------
// generate
// ---------------------------------------------------------------------------

void EditorPopup::onGenerate(bool forceNewLevel) {
    if (m_generating) return;
    auto result = compileCurrent();
    m_lastErrorCount = result.diagnostics.errorCount();
    m_lastObjectCount = result.stats.objectCount;
    rebuildProblems(result);
    updateStatus();

    if (!result.ok()) {
        Notification::create(fmt::format("Fix {} error(s) before generating", m_lastErrorCount),
                             NotificationIcon::Error)
            ->show();
        for (auto const& d : result.diagnostics.items()) {
            if (d.isError() && d.span.valid()) {
                m_editor->gotoPosition(d.span.begin.line, d.span.begin.column);
                m_editor->setHighlightLine(d.span.begin.line);
                break;
            }
        }
        return;
    }
    if (result.ir.objects.empty()) {
        Notification::create("The script places no objects - nothing to generate",
                             NotificationIcon::Warning)
            ->show();
        return;
    }

    // Always persist the source that produced a level.
    if (!saveProject(false)) return;

    auto ir = std::make_shared<LevelIR>(std::move(result.ir));
    std::string const& name = ir->settings.name;

    if (forceNewLevel) {
        writeAndOpen(*ir, nullptr);
        return;
    }

    auto lookup = backend::findLinkedLevel(m_project.meta.linkedLevel);
    if (lookup.ambiguous) {
        Notification::create(fmt::format("Several levels are named '{}' - creating a new one", name),
                             NotificationIcon::Info, 3.f)
            ->show();
        writeAndOpen(*ir, nullptr);
        return;
    }
    if (!lookup.level) {
        writeAndOpen(*ir, nullptr);
        return;
    }

    std::string existingName = lookup.level->m_levelName;
    Ref<GJGameLevel> target = lookup.level;
    if (lookup.modifiedSinceGeneration) {
        createQuickPopup(
            "Level was edited",
            fmt::format("<cy>{}</c> was changed in the editor since GDCode generated it.\n\n"
                        "<cr>Overwrite</c> replaces it with this generation (manual edits are lost). "
                        "Use <cg>New Level</c> instead to keep it.",
                        existingName),
            "Cancel", "Overwrite",
            [this, ir, target](FLAlertLayer*, bool overwrite) {
                if (overwrite) writeAndOpen(*ir, target);
            });
        return;
    }
    if (Mod::get()->getSettingValue<bool>("confirm-overwrite")) {
        createQuickPopup(
            "Update level",
            fmt::format("Regenerate <cy>{}</c> in place?\n({} objects)", existingName, ir->objects.size()),
            "Cancel", "Update",
            [this, ir, target](FLAlertLayer*, bool update) {
                if (update) writeAndOpen(*ir, target);
            });
        return;
    }
    writeAndOpen(*ir, target);
}

void EditorPopup::writeAndOpen(LevelIR const& ir, GJGameLevel* target) {
    m_generating = true;
    auto written = backend::writeLevel(ir, target);
    m_generating = false;
    if (!written.error.empty()) {
        log::error("GDCode: level write failed: {}", written.error);
        FLAlertLayer::create("Generation failed",
                             "The level could not be written:\n<cr>" + written.error + "</c>\n"
                             "No level was changed.",
                             "OK")
            ->show();
        return;
    }

    m_project.meta.linkedLevel = written.link;
    m_project.meta.lastObjectCount = ir.objects.size();
    m_project.source = m_editor->getText();
    if (auto res = storage::ProjectStore::get().save(m_project); !res) {
        log::warn("GDCode: could not store level link: {}", res.unwrapErr());
    }
    m_dirty = false;
    updateStatus();

    auto how = backend::afterGenerateSetting();
    if (how == backend::AfterGenerate::Stay) {
        Notification::create(fmt::format("{} '{}' ({} objects)",
                                         written.createdNew ? "Created" : "Updated",
                                         ir.settings.name, ir.objects.size()),
                             NotificationIcon::Success, 3.f)
            ->show();
        return;
    }
    if (m_editor) m_editor->blur();
    if (m_onClosed) m_onClosed();
    backend::openLevel(written.level, how);
}

} // namespace gdcode::ui
