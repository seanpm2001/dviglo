// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) the Dviglo project
// License: MIT

#pragma once

#include "../core/object.h"
#include "../io/log.h"

namespace dviglo
{

class Button;
class BorderImage;
class DropDownList;
class Engine;
class Font;
class LineEdit;
class ListView;
class Text;
class UiElement;
class XmlFile;

/// %Console window with log history and command line prompt.
class DV_API Console : public Object
{
    DV_OBJECT(Console);

    /// Только Ui может создать и уничтожить объект
    friend class UI;

private:
    /// Инициализируется в конструкторе
    inline static Console* instance_ = nullptr;

public:
    static Console* instance() { return instance_; }

private:
    /// Construct.
    explicit Console();
    /// Destruct.
    ~Console() override;

public:
    SlotLogMessage log_message;

    // Запрещаем копирование
    Console(const Console&) = delete;
    Console& operator =(const Console&) = delete;

    /// Set UI elements' style from an XML file.
    void SetDefaultStyle(XmlFile* style);
    /// Show or hide.
    void SetVisible(bool enable);
    /// Toggle visibility.
    void Toggle();

    /// Automatically set console to visible when receiving an error log message.
    void SetAutoVisibleOnError(bool enable) { autoVisibleOnError_ = enable; }

    /// Set the command interpreter.
    void SetCommandInterpreter(const String& interpreter) { commandInterpreter_ = interpreter; }

    /// Set number of buffered rows.
    void SetNumBufferedRows(i32 rows);
    /// Set number of displayed rows.
    void SetNumRows(i32 rows);
    /// Set command history maximum size, 0 disables history.
    void SetNumHistoryRows(i32 rows);
    /// Set whether to automatically focus the line edit when showing. Default true on desktops and false on mobile devices, as on mobiles it would pop up the screen keyboard.
    void SetFocusOnShow(bool enable);
    /// Add auto complete option.
    void AddAutoComplete(const String& option);
    /// Remove auto complete option.
    void RemoveAutoComplete(const String& option);
    /// Update elements to layout properly. Call this after manually adjusting the sub-elements.
    void UpdateElements();

    /// Return the UI style file.
    XmlFile* GetDefaultStyle() const;

    /// Return the background element.
    BorderImage* GetBackground() const { return background_; }

    /// Return the line edit element.
    LineEdit* GetLineEdit() const { return lineEdit_; }

    /// Return the close butoon element.
    Button* GetCloseButton() const { return closeButton_; }

    /// Return whether is visible.
    bool IsVisible() const;

    /// Return true when console is set to automatically visible when receiving an error log message.
    bool IsAutoVisibleOnError() const { return autoVisibleOnError_; }

    /// Return the last used command interpreter.
    const String& GetCommandInterpreter() const { return commandInterpreter_; }

    /// Return number of buffered rows.
    i32 GetNumBufferedRows() const;

    /// Return number of displayed rows.
    i32 GetNumRows() const { return displayedRows_; }

    /// Copy selected rows to system clipboard.
    void CopySelectedRows() const;

    /// Return history maximum size.
    i32 GetNumHistoryRows() const { return historyRows_; }

    /// Return current history position.
    i32 GetHistoryPosition() const { return historyPosition_; }

    /// Return history row at index.
    const String& GetHistoryRow(i32 index) const;

    /// Return whether automatically focuses the line edit when showing.
    bool GetFocusOnShow() const { return focusOnShow_; }

private:
    /// Populate the command line interpreters that could handle the console command.
    bool PopulateInterpreter();
    /// Handle interpreter being selected on the drop down list.
    void HandleInterpreterSelected(StringHash eventType, VariantMap& eventData);
    /// Handle text change in the line edit.
    void HandleTextChanged(StringHash eventType, VariantMap& eventData);
    /// Handle enter pressed on the line edit.
    void HandleTextFinished(StringHash eventType, VariantMap& eventData);
    /// Handle unhandled key on the line edit for scrolling the history.
    void HandleLineEditKey(StringHash eventType, VariantMap& eventData);
    /// Handle close button being pressed.
    void HandleCloseButtonPressed(StringHash eventType, VariantMap& eventData);
    /// Handle UI root resize.
    void HandleRootElementResized(StringHash eventType, VariantMap& eventData);
    /// Handle a log message.
    void handle_log_message(const String& message, i32 level);
    /// Handle the application post-update.
    void HandlePostUpdate(StringHash eventType, VariantMap& eventData);

    /// Auto visible on error flag.
    bool autoVisibleOnError_;
    /// Background.
    SharedPtr<BorderImage> background_;
    /// Container for text rows.
    ListView* rowContainer_;
    /// Container for the command line.
    UiElement* commandLine_;
    /// Interpreter drop down list.
    DropDownList* interpreters_;
    /// Line edit.
    LineEdit* lineEdit_;
    /// Close button.
    SharedPtr<Button> closeButton_;
    /// Last used command interpreter.
    String commandInterpreter_;

    /// Command history.
    Vector<String> history_;
    /// Pending log message rows.
    Vector<Pair<int, String>> pendingRows_;
    /// Current row being edited.
    String currentRow_;
    /// Maximum displayed rows.
    i32 displayedRows_{};
    /// Command history maximum rows.
    i32 historyRows_;
    /// Command history current position.
    i32 historyPosition_;

    /**
    Command auto complete options.

    down arrow key
    Unless currently going through history options, will loop through next auto complete options.

    up arrow key
    Unless currently going through history options, will go through previous auto complete options.
    When no previous options are left will start going through history options.
    */
    Vector<String> autoComplete_;
    /// Command auto complete current position.
    i32 autoCompletePosition_;
    /// Store the original line which is being auto-completed.
    String autoCompleteLine_;

    /// Flag when printing messages to prevent endless loop.
    bool printing_;
    /// Flag for automatically focusing the line edit on showing the console.
    bool focusOnShow_;
    /// Internal flag whether currently in an autocomplete or history change.
    bool historyOrAutoCompleteChange_;
};

#define DV_CONSOLE (dviglo::Console::instance())

}
