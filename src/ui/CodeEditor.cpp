#include "CodeEditor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace geode::prelude;

namespace gdcode::ui {

namespace {

double nowSeconds() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------

CodeEditor* CodeEditor::create(CCSize const& size) {
    auto* ret = new CodeEditor();
    if (ret->init(size)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

CodeEditor::~CodeEditor() {
    CC_SAFE_RELEASE(m_measureLabel);
}

bool CodeEditor::init(CCSize const& size) {
    if (!CCLayer::init()) return false;

    this->ignoreAnchorPointForPosition(false);
    this->setAnchorPoint({0.5f, 0.5f});
    this->setContentSize(size);
    this->setID("code-editor"_spr);

    m_measureLabel = CCLabelBMFont::create("", kFont);
    m_measureLabel->retain();

    // Font metrics from a probe string.
    m_measureLabel->setString("Hg{}", true);
    m_lineHeight = std::max(8.f, m_measureLabel->getContentSize().height * m_fontScale + 1.f);
    m_visibleLines = static_cast<std::size_t>(
        std::max(1.f, std::floor((size.height - 2 * m_padding) / m_lineHeight)));

    m_bg = CCLayerColor::create({0, 0, 0, 140}, size.width, size.height);
    this->addChild(m_bg, -3);

    m_gutterBg = CCLayerColor::create({0, 0, 0, 90}, m_gutterWidth, size.height);
    this->addChild(m_gutterBg, -2);

    m_focusBorder = CCLayerColor::create({255, 255, 255, 0}, size.width, size.height);
    this->addChild(m_focusBorder, -3);

    m_highlight = CCLayerColor::create({255, 60, 60, 70}, size.width - m_gutterWidth, m_lineHeight);
    m_highlight->setVisible(false);
    this->addChild(m_highlight, -1);

    m_gutterContainer = CCNode::create();
    this->addChild(m_gutterContainer, 0);

    m_lineContainer = CCNode::create();
    m_lineContainer->setPosition({m_gutterWidth + m_padding, 0.f});
    this->addChild(m_lineContainer, 0);

    // The caret is a child of the editor itself (not of the line container,
    // which is rebuilt on every refresh).
    m_caret = CCLayerColor::create({255, 255, 255, 230}, 1.5f, m_lineHeight - 2.f);
    m_caret->setVisible(false);
    this->addChild(m_caret, 5);

    m_placeholder = CCLabelBMFont::create("Tap here and type your level code...", kFont);
    m_placeholder->setScale(m_fontScale);
    m_placeholder->setOpacity(110);
    m_placeholder->setAnchorPoint({0.f, 0.5f});
    m_placeholder->setPosition({m_gutterWidth + m_padding, size.height - m_padding - m_lineHeight / 2});
    this->addChild(m_placeholder, 1);

    this->setTouchMode(kCCTouchesOneByOne);
    this->setTouchEnabled(true);
    this->setKeyboardEnabled(true);
    this->setMouseEnabled(true);
    this->scheduleUpdate();

    this->rebuildLineIndex();
    this->refresh();
    return true;
}

// ---------------------------------------------------------------------------
// text model
// ---------------------------------------------------------------------------

void CodeEditor::rebuildLineIndex() {
    m_lineStarts.clear();
    m_lineStarts.push_back(0);
    for (std::size_t i = 0; i < m_text.size(); ++i) {
        if (m_text[i] == '\n') m_lineStarts.push_back(i + 1);
    }
}

std::size_t CodeEditor::lineOfOffset(std::size_t offset) const {
    auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    return static_cast<std::size_t>(it - m_lineStarts.begin()) - 1;
}

std::size_t CodeEditor::lineStart(std::size_t line) const {
    if (line >= m_lineStarts.size()) return m_text.size();
    return m_lineStarts[line];
}

std::size_t CodeEditor::lineEnd(std::size_t line) const {
    if (line + 1 < m_lineStarts.size()) return m_lineStarts[line + 1] - 1;
    return m_text.size();
}

std::string CodeEditor::lineText(std::size_t line) const {
    std::size_t start = lineStart(line);
    std::size_t end = lineEnd(line);
    return m_text.substr(start, end - start);
}

std::pair<std::size_t, std::size_t> CodeEditor::cursorLineColumn() const {
    std::size_t line = lineOfOffset(m_cursor);
    return {line + 1, m_cursor - lineStart(line) + 1};
}

void CodeEditor::snapshotForUndo(bool groupable) {
    double now = nowSeconds();
    bool merge = groupable && m_lastEditGroupable && (now - m_lastEditTime) < 1.0 && !m_undo.empty();
    if (!merge) {
        m_undo.push_back({m_text, m_cursor});
        if (m_undo.size() > kMaxUndo) m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
    m_lastEditTime = now;
    m_lastEditGroupable = groupable;
}

void CodeEditor::replaceRange(std::size_t begin, std::size_t end, std::string const& with) {
    begin = std::min(begin, m_text.size());
    end = std::clamp(end, begin, m_text.size());
    m_text.replace(begin, end - begin, with);
    m_cursor = begin + with.size();
    rebuildLineIndex();
}

void CodeEditor::applyEdit(std::size_t begin, std::size_t end, std::string const& with,
                           bool groupable) {
    if (m_text.size() - (end - begin) + with.size() > kMaxEditorChars) {
        Notification::create("Editor limit reached (200k characters)", NotificationIcon::Warning)->show();
        return;
    }
    snapshotForUndo(groupable);
    replaceRange(begin, end, with);
    m_preferredColumn = m_cursor - lineStart(lineOfOffset(m_cursor));
    textChanged();
}

void CodeEditor::textChanged() {
    m_highlightLine.reset();
    ensureCursorVisible();
    refresh();
    m_blink = 0.f;
    if (m_onChange) m_onChange();
}

bool CodeEditor::setText(std::string const& text) {
    auto normalised = prepareEditorText(text);
    if (!normalised) {
        Notification::create("Source exceeds 200k characters - nothing changed",
                             NotificationIcon::Warning)->show();
        return false;
    }
    m_text = std::move(*normalised);
    m_cursor = 0;
    m_preferredColumn = 0;
    m_firstVisibleLine = 0;
    m_scrollX = 0.f;
    m_undo.clear();
    m_redo.clear();
    m_highlightLine.reset();
    rebuildLineIndex();
    refresh();
    return true;
}

bool CodeEditor::replaceText(std::string const& text) {
    auto normalised = prepareEditorText(text);
    if (!normalised) {
        Notification::create("Source exceeds 200k characters - nothing changed",
                             NotificationIcon::Warning)->show();
        return false;
    }
    applyEdit(0, m_text.size(), *normalised, false);
    return true;
}

void CodeEditor::insertAtCursor(std::string const& text) {
    std::string clean;
    clean.reserve(text.size());
    bool dropped = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') continue;
            clean += '\n';
        } else if (c == '\t') {
            clean += "    ";
        } else if (c == '\n' || (c >= 0x20 && c < 0x7F)) {
            clean += static_cast<char>(c);
        } else {
            dropped = true;
        }
    }
    if (dropped) {
        Notification::create("Skipped characters the game font cannot show", NotificationIcon::Info)->show();
    }
    if (clean.empty()) return;
    applyEdit(m_cursor, m_cursor, clean, false);
}

bool CodeEditor::undo() {
    if (m_undo.empty()) return false;
    m_redo.push_back({m_text, m_cursor});
    auto snap = m_undo.back();
    m_undo.pop_back();
    m_text = std::move(snap.text);
    m_cursor = std::min(snap.cursor, m_text.size());
    m_lastEditGroupable = false;
    rebuildLineIndex();
    textChanged();
    return true;
}

bool CodeEditor::redo() {
    if (m_redo.empty()) return false;
    m_undo.push_back({m_text, m_cursor});
    auto snap = m_redo.back();
    m_redo.pop_back();
    m_text = std::move(snap.text);
    m_cursor = std::min(snap.cursor, m_text.size());
    m_lastEditGroupable = false;
    rebuildLineIndex();
    textChanged();
    return true;
}

// ---------------------------------------------------------------------------
// caret & viewport
// ---------------------------------------------------------------------------

void CodeEditor::setCursor(std::size_t offset, bool rememberColumn) {
    m_cursor = std::min(offset, m_text.size());
    if (rememberColumn) m_preferredColumn = m_cursor - lineStart(lineOfOffset(m_cursor));
    m_blink = 0.f;
    ensureCursorVisible();
    refresh();
}

void CodeEditor::moveVertical(long delta) {
    long line = static_cast<long>(lineOfOffset(m_cursor)) + delta;
    line = std::clamp<long>(line, 0, static_cast<long>(m_lineStarts.size()) - 1);
    std::size_t start = lineStart(static_cast<std::size_t>(line));
    std::size_t end = lineEnd(static_cast<std::size_t>(line));
    std::size_t col = std::min(m_preferredColumn, end - start);
    setCursor(start + col, false);
}

void CodeEditor::gotoPosition(std::size_t line, std::size_t column) {
    if (line == 0) line = 1;
    std::size_t l = std::min(line - 1, m_lineStarts.size() - 1);
    std::size_t start = lineStart(l);
    std::size_t end = lineEnd(l);
    std::size_t col = column == 0 ? 0 : std::min(column - 1, end - start);
    setCursor(start + col, true);
    // Put the target line roughly in the middle of the view.
    if (m_lineStarts.size() > m_visibleLines) {
        long first = static_cast<long>(l) - static_cast<long>(m_visibleLines / 2);
        long maxFirst = static_cast<long>(m_lineStarts.size()) - static_cast<long>(m_visibleLines);
        m_firstVisibleLine = static_cast<std::size_t>(std::clamp<long>(first, 0, std::max<long>(0, maxFirst)));
    }
    refresh();
}

void CodeEditor::setHighlightLine(std::optional<std::size_t> line) {
    m_highlightLine = line;
    refresh();
}

void CodeEditor::ensureCursorVisible() {
    std::size_t line = lineOfOffset(m_cursor);
    if (line < m_firstVisibleLine) {
        m_firstVisibleLine = line;
    } else if (line >= m_firstVisibleLine + m_visibleLines) {
        m_firstVisibleLine = line - m_visibleLines + 1;
    }
    float viewWidth = this->getContentSize().width - m_gutterWidth - 2 * m_padding;
    float caretX = measure(std::string_view(m_text).substr(lineStart(line), m_cursor - lineStart(line)));
    if (caretX - m_scrollX > viewWidth - 8.f) {
        m_scrollX = caretX - (viewWidth - 8.f);
    } else if (caretX - m_scrollX < 0.f) {
        m_scrollX = std::max(0.f, caretX - viewWidth * 0.3f);
    }
    if (caretX <= viewWidth - 8.f) m_scrollX = 0.f;
}

void CodeEditor::scrollLines(long delta) {
    long maxFirst = static_cast<long>(m_lineStarts.size()) - static_cast<long>(m_visibleLines);
    long first = std::clamp<long>(static_cast<long>(m_firstVisibleLine) + delta, 0,
                                  std::max<long>(0, maxFirst));
    if (static_cast<std::size_t>(first) != m_firstVisibleLine) {
        m_firstVisibleLine = static_cast<std::size_t>(first);
        refresh();
    }
}

float CodeEditor::measure(std::string_view text) const {
    if (text.empty()) return 0.f;
    // Cap pathological line lengths; the caret only needs to be roughly right
    // beyond a few hundred characters.
    std::string s(text.substr(0, 400));
    m_measureLabel->setString(s.c_str(), true);
    return m_measureLabel->getContentSize().width * m_fontScale;
}

std::size_t CodeEditor::columnFromX(std::size_t line, float x) const {
    std::string text = lineText(line);
    if (x <= 0.f || text.empty()) return 0;
    float prev = 0.f;
    std::size_t limit = std::min<std::size_t>(text.size(), 400);
    for (std::size_t i = 1; i <= limit; ++i) {
        float w = measure(std::string_view(text).substr(0, i));
        if (w >= x) {
            return (x - prev) < (w - x) ? i - 1 : i;
        }
        prev = w;
    }
    return text.size();
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------

CCLabelBMFont* CodeEditor::makeLabel(std::string const& text, bool gutter) const {
    auto* label = CCLabelBMFont::create(text.c_str(), kFont);
    label->setScale(m_fontScale);
    if (gutter) {
        label->setAnchorPoint({1.f, 0.5f});
        label->setOpacity(120);
    } else {
        label->setAnchorPoint({0.f, 0.5f});
    }
    return label;
}

void CodeEditor::refresh() {
    m_lineContainer->removeAllChildrenWithCleanup(true);
    m_gutterContainer->removeAllChildrenWithCleanup(true);

    auto size = this->getContentSize();
    float top = size.height - m_padding;
    std::size_t last = std::min(m_lineStarts.size(), m_firstVisibleLine + m_visibleLines);

    m_placeholder->setVisible(m_text.empty() && !m_focused);
    m_highlight->setVisible(false);

    for (std::size_t line = m_firstVisibleLine; line < last; ++line) {
        float y = top - (static_cast<float>(line - m_firstVisibleLine) + 0.5f) * m_lineHeight;

        auto* number = makeLabel(std::to_string(line + 1), true);
        number->setPosition({m_gutterWidth - 3.f, y});
        m_gutterContainer->addChild(number);

        std::string text = lineText(line);
        if (!text.empty()) {
            auto* label = makeLabel(text, false);
            label->setPosition({-m_scrollX, y});
            m_lineContainer->addChild(label);
        }

        if (m_highlightLine && *m_highlightLine == line + 1) {
            m_highlight->setPosition({m_gutterWidth, y - m_lineHeight / 2});
            m_highlight->setVisible(true);
        }
    }
    layoutCaret();
}

void CodeEditor::layoutCaret() {
    std::size_t line = lineOfOffset(m_cursor);
    bool visible = m_focused && line >= m_firstVisibleLine && line < m_firstVisibleLine + m_visibleLines;
    m_caret->setVisible(visible);
    if (!visible) return;
    float top = this->getContentSize().height - m_padding;
    float y = top - (static_cast<float>(line - m_firstVisibleLine) + 1.f) * m_lineHeight + 1.f;
    float x = measure(std::string_view(m_text).substr(lineStart(line), m_cursor - lineStart(line))) - m_scrollX;
    m_caret->setPosition({m_gutterWidth + m_padding + x, y});
}

void CodeEditor::visit() {
    if (!this->isVisible()) return;
    glEnable(GL_SCISSOR_TEST);
    auto const bottomLeft = this->convertToWorldSpace(ccp(0, 0));
    auto const topRight = this->convertToWorldSpace(this->getContentSize());
    CCEGLView::get()->setScissorInPoints(bottomLeft.x, bottomLeft.y, topRight.x - bottomLeft.x,
                                         topRight.y - bottomLeft.y);
    CCLayer::visit();
    glDisable(GL_SCISSOR_TEST);
}

void CodeEditor::update(float dt) {
    if (!m_focused) return;
    m_blink += dt;
    if (m_blink >= 1.f) m_blink -= 1.f;
    std::size_t line = lineOfOffset(m_cursor);
    bool inView = line >= m_firstVisibleLine && line < m_firstVisibleLine + m_visibleLines;
    m_caret->setVisible(inView && m_blink < 0.55f);
}

void CodeEditor::onExit() {
    blur();
    CCLayer::onExit();
}

// ---------------------------------------------------------------------------
// focus / IME
// ---------------------------------------------------------------------------

void CodeEditor::focus() {
    if (m_focused) return;
    this->attachWithIME();
}

void CodeEditor::blur() {
    if (!m_focused) return;
    this->detachWithIME();
}

bool CodeEditor::attachWithIME() {
    bool ok = CCIMEDelegate::attachWithIME();
    if (ok) {
        // Opens the soft keyboard on mobile; no-op on desktop.
        if (auto* view = CCDirector::sharedDirector()->getOpenGLView()) view->setIMEKeyboardState(true);
    }
    return ok;
}

bool CodeEditor::detachWithIME() {
    bool ok = CCIMEDelegate::detachWithIME();
    if (ok) {
        if (auto* view = CCDirector::sharedDirector()->getOpenGLView()) view->setIMEKeyboardState(false);
    }
    return ok;
}

void CodeEditor::didAttachWithIME() {
    m_focused = true;
    m_blink = 0.f;
    m_lastTabKeyTime = -1.0;
    m_lastEnterKeyTime = -1.0;
    m_focusBorder->setOpacity(25);
    refresh();
}

void CodeEditor::didDetachWithIME() {
    m_focused = false;
    m_focusBorder->setOpacity(0);
    refresh();
}

void CodeEditor::insertNewline() {
    std::string indent;
    std::size_t line = lineOfOffset(m_cursor);
    std::string current = lineText(line);
    while (indent.size() < current.size() && current[indent.size()] == ' ') indent += ' ';
    std::size_t beforeCursor = std::min(m_cursor - lineStart(line), current.size());
    std::string_view head(current.c_str(), beforeCursor);
    while (!head.empty() && head.back() == ' ') head.remove_suffix(1);
    if (!head.empty() && head.back() == '{') indent += "    ";
    // Do not carry indentation onto the new line when splitting inside the
    // leading whitespace itself.
    if (beforeCursor < indent.size()) indent.clear();
    applyEdit(m_cursor, m_cursor, "\n" + indent, false);
}

void CodeEditor::insertTab() {
    applyEdit(m_cursor, m_cursor, "    ", false);
}

void CodeEditor::insertText(char const* text, int len, enumKeyCodes) {
    if (!m_focused || !text || len <= 0) return;
    std::string s(text, static_cast<std::size_t>(len));
    double now = nowSeconds();

    // A Ctrl+V handled in keyDown() may be echoed by the platform as an IME
    // insert of the same clipboard text; swallow that single echo.
    if (!m_pendingPasteSkip.empty()) {
        bool same = (s == m_pendingPasteSkip);
        m_pendingPasteSkip.clear();
        if (same) return;
    }

    if (s == "\n" || s == "\r" || s == "\r\n") {
        // Enter already handled by keyDown() for this very press (desktop)?
        if (m_lastEnterKeyTime >= 0.0 && now - m_lastEnterKeyTime < 0.05) return;
        insertNewline();
        return;
    }
    if (s == "\t") {
        if (m_lastTabKeyTime >= 0.0 && now - m_lastTabKeyTime < 0.05) return;
        insertTab();
        return;
    }
    insertAtCursor(s);
}

void CodeEditor::deleteBackward() {
    if (!m_focused || m_cursor == 0) return;
    std::size_t line = lineOfOffset(m_cursor);
    std::size_t start = lineStart(line);
    // Smart un-indent: only spaces before the caret -> remove up to 4.
    std::size_t col = m_cursor - start;
    bool onlySpaces = col > 0;
    for (std::size_t i = start; i < m_cursor; ++i) {
        if (m_text[i] != ' ') {
            onlySpaces = false;
            break;
        }
    }
    std::size_t remove = 1;
    if (onlySpaces) {
        remove = col % 4 == 0 ? 4 : col % 4;
        remove = std::min(remove, col);
    }
    applyEdit(m_cursor - remove, m_cursor, "", true);
}

void CodeEditor::deleteForward() {
    if (!m_focused || m_cursor >= m_text.size()) return;
    applyEdit(m_cursor, m_cursor + 1, "", true);
}

// ---------------------------------------------------------------------------
// keyboard (navigation + shortcuts)
// ---------------------------------------------------------------------------

void CodeEditor::keyDown(enumKeyCodes key, double) {
    if (!m_focused) return;
    auto* kb = CCKeyboardDispatcher::get();
    bool ctrl = kb->getControlKeyPressed() || kb->getCommandKeyPressed();
    double now = nowSeconds();

    switch (key) {
        case KEY_Left:
        case KEY_ArrowLeft:
            if (m_cursor > 0) setCursor(m_cursor - 1);
            return;
        case KEY_Right:
        case KEY_ArrowRight:
            if (m_cursor < m_text.size()) setCursor(m_cursor + 1);
            return;
        case KEY_Up:
        case KEY_ArrowUp:
            moveVertical(-1);
            return;
        case KEY_Down:
        case KEY_ArrowDown:
            moveVertical(1);
            return;
        case KEY_Home:
            setCursor(ctrl ? 0 : lineStart(lineOfOffset(m_cursor)));
            return;
        case KEY_End:
            setCursor(ctrl ? m_text.size() : lineEnd(lineOfOffset(m_cursor)));
            return;
        case KEY_PageUp:
            moveVertical(-static_cast<long>(m_visibleLines));
            return;
        case KEY_PageDown:
            moveVertical(static_cast<long>(m_visibleLines));
            return;
        case KEY_Enter:
            // Handled here (a key press is always reported) and remembered so
            // insertText() can skip the matching IME "\n" on platforms that
            // send both.
            m_lastEnterKeyTime = now;
            insertNewline();
            return;
        case KEY_Tab:
            m_lastTabKeyTime = now;
            insertTab();
            return;
        case KEY_V:
            if (ctrl) {
                std::string clip = utils::clipboard::read();
                if (!clip.empty()) {
                    m_pendingPasteSkip = clip;
                    insertAtCursor(clip);
                }
            }
            return;
        case KEY_Z:
            if (ctrl) {
                if (kb->getShiftKeyPressed()) redo();
                else undo();
            }
            return;
        case KEY_Y:
            if (ctrl) redo();
            return;
        default:
            return;
    }
}

void CodeEditor::scrollWheel(float y, float) {
    if (m_lineStarts.size() <= m_visibleLines) return;
    long delta = y > 0 ? 3 : -3;
    scrollLines(delta);
}

// ---------------------------------------------------------------------------
// touch
// ---------------------------------------------------------------------------

bool CodeEditor::ccTouchBegan(CCTouch* touch, CCEvent*) {
    if (!this->isVisible()) return false;
    auto pt = this->convertTouchToNodeSpace(touch);
    CCRect bounds{0.f, 0.f, this->getContentSize().width, this->getContentSize().height};
    if (!bounds.containsPoint(pt)) {
        blur();
        return false;
    }
    m_touchStart = pt;
    m_touchStartLine = m_firstVisibleLine;
    m_dragScrolling = false;
    return true;
}

void CodeEditor::ccTouchMoved(CCTouch* touch, CCEvent*) {
    auto pt = this->convertTouchToNodeSpace(touch);
    float dy = pt.y - m_touchStart.y;
    if (!m_dragScrolling && std::fabs(dy) > 6.f) m_dragScrolling = true;
    if (m_dragScrolling) {
        long lines = static_cast<long>(dy / m_lineHeight);
        long maxFirst = static_cast<long>(m_lineStarts.size()) - static_cast<long>(m_visibleLines);
        long first = std::clamp<long>(static_cast<long>(m_touchStartLine) + lines, 0, std::max<long>(0, maxFirst));
        if (static_cast<std::size_t>(first) != m_firstVisibleLine) {
            m_firstVisibleLine = static_cast<std::size_t>(first);
            refresh();
        }
    }
}

void CodeEditor::ccTouchEnded(CCTouch* touch, CCEvent*) {
    if (m_dragScrolling) return;
    auto pt = this->convertTouchToNodeSpace(touch);
    float top = this->getContentSize().height - m_padding;
    long row = static_cast<long>(std::floor((top - pt.y) / m_lineHeight));
    row = std::clamp<long>(row, 0, static_cast<long>(m_visibleLines) - 1);
    std::size_t line = std::min(m_firstVisibleLine + static_cast<std::size_t>(row), m_lineStarts.size() - 1);
    float x = pt.x - m_gutterWidth - m_padding + m_scrollX;
    std::size_t col = columnFromX(line, x);
    focus();
    setCursor(lineStart(line) + col);
}

void CodeEditor::ccTouchCancelled(CCTouch*, CCEvent*) {
    m_dragScrolling = false;
}

} // namespace gdcode::ui
