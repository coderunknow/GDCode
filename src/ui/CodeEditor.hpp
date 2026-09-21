#pragma once

// Multi-line code editor node.
//
// Geode/GD ship no editable multi-line text widget (TextInput is single line,
// SimpleTextArea is display-only), so this one is built on the same input
// plumbing GD's own text inputs use:
//   * text arrives through cocos2d's IME dispatcher (CCIMEDelegate), which is
//     fed by the OS keyboard on desktop and by the soft keyboard on mobile;
//   * navigation/shortcut keys arrive through CCKeyboardDelegate;
//   * touches place the caret / scroll.
// Rendering is a window of CCLabelBMFont lines clipped with a scissor rect.

#include <Geode/Geode.hpp>
#include "EditorText.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gdcode::ui {

class CodeEditor : public cocos2d::CCLayer, public cocos2d::CCIMEDelegate {
public:
    static CodeEditor* create(cocos2d::CCSize const& size);

    std::string const& getText() const { return m_text; }
    /// Load a buffer. Oversize input is rejected without changing existing text.
    bool setText(std::string const& text);
    /// Replace as one undoable user edit; reject oversize without changes.
    bool replaceText(std::string const& text);

    /// Insert text at the caret (used by Paste).
    void insertAtCursor(std::string const& text);

    void setOnChange(std::function<void()> cb) { m_onChange = std::move(cb); }

    void focus();
    void blur();
    bool isFocused() const { return m_focused; }

    /// Move the caret to a 1-based line/column and scroll it into view.
    void gotoPosition(std::size_t line, std::size_t column);
    /// Highlight a 1-based line (error marker); nullopt clears it.
    void setHighlightLine(std::optional<std::size_t> line);

    std::size_t lineCount() const { return m_lineStarts.size(); }
    std::pair<std::size_t, std::size_t> cursorLineColumn() const; ///< 1-based

    bool undo();
    bool redo();

    // --- cocos2d::CCLayer ---------------------------------------------------
    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override;
    void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;
    void scrollWheel(float y, float x) override;
    void visit() override;
    void onExit() override;
    void update(float dt) override;

    // --- cocos2d::CCIMEDelegate ---------------------------------------------
    bool attachWithIME() override;
    bool detachWithIME() override;

protected:
    bool canAttachWithIME() override { return true; }
    bool canDetachWithIME() override { return true; }
    void didAttachWithIME() override;
    void didDetachWithIME() override;
    void insertText(char const* text, int len, cocos2d::enumKeyCodes key) override;
    void deleteBackward() override;
    void deleteForward() override;
    char const* getContentText() override { return m_text.c_str(); }

private:
    CodeEditor() = default;
    ~CodeEditor() override;
    bool init(cocos2d::CCSize const& size);

    // text model
    void rebuildLineIndex();
    std::size_t lineOfOffset(std::size_t offset) const;   // 0-based
    std::size_t lineStart(std::size_t line) const;
    std::size_t lineEnd(std::size_t line) const;          // offset of '\n' or text end
    std::string lineText(std::size_t line) const;
    void replaceRange(std::size_t begin, std::size_t end, std::string const& with);
    void applyEdit(std::size_t begin, std::size_t end, std::string const& with,
                   bool groupable);
    void snapshotForUndo(bool groupable);
    void textChanged();
    void insertNewline();
    void insertTab();

    // caret / viewport
    void setCursor(std::size_t offset, bool rememberColumn = true);
    void moveVertical(long lines);
    void ensureCursorVisible();
    void scrollLines(long delta);
    float measure(std::string_view text) const;
    std::size_t columnFromX(std::size_t line, float x) const;

    // rendering
    void refresh();
    void layoutCaret();
    cocos2d::CCLabelBMFont* makeLabel(std::string const& text, bool gutter) const;

    std::function<void()> m_onChange;

    std::string m_text;
    std::size_t m_cursor = 0;
    std::vector<std::size_t> m_lineStarts{0};
    std::size_t m_preferredColumn = 0; ///< column kept while moving up/down

    struct Snapshot {
        std::string text;
        std::size_t cursor;
    };
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    double m_lastEditTime = 0.0;
    bool m_lastEditGroupable = false;

    std::size_t m_firstVisibleLine = 0;
    float m_scrollX = 0.f;
    std::size_t m_visibleLines = 1;
    float m_lineHeight = 12.f;
    float m_fontScale = 0.5f;
    float m_gutterWidth = 26.f;
    float m_padding = 4.f;

    bool m_focused = false;
    float m_blink = 0.f;
    std::optional<std::size_t> m_highlightLine;

    // touch state
    cocos2d::CCPoint m_touchStart;
    std::size_t m_touchStartLine = 0;
    bool m_dragScrolling = false;
    double m_lastTabKeyTime = -1.0;
    double m_lastEnterKeyTime = -1.0;
    std::string m_pendingPasteSkip;

    cocos2d::CCLayerColor* m_bg = nullptr;
    cocos2d::CCLayerColor* m_gutterBg = nullptr;
    cocos2d::CCLayerColor* m_focusBorder = nullptr;
    cocos2d::CCLayerColor* m_highlight = nullptr;
    cocos2d::CCLayerColor* m_caret = nullptr;
    cocos2d::CCNode* m_lineContainer = nullptr;
    cocos2d::CCNode* m_gutterContainer = nullptr;
    cocos2d::CCLabelBMFont* m_measureLabel = nullptr;
    cocos2d::CCLabelBMFont* m_placeholder = nullptr;

    static constexpr std::size_t kMaxUndo = 200;
    static constexpr char const* kFont = "chatFont.fnt";
};

} // namespace gdcode::ui
