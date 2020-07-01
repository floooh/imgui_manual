#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include "ImGuiExt.h"
#include "TextEditor.h"
#include "Sources.h"
#include "MarkdownHelper.h"
#include "HyperlinkHelper.h"
#include <fplus/fplus.hpp>
#include <functional>

using VoidFunction = std::function<void(void)>;

static std::string gImGuiRepoUrl = "https://github.com/pthom/imgui/blob/DemoCode/";

TextEditor *gEditorImGuiDemo = nullptr;

// This is a callback that will be called by imgui_demo.cpp
void implImGuiDemoCallbackDemoCallback(int line_number)
{
    int cursorLineOnPage = 3;
    gEditorImGuiDemo->SetCursorPosition({line_number, 0}, cursorLineOnPage);
}

std::vector<TextEditor *> gAllEditors;

void menuEditorTheme()
{
    if (ImGui::BeginMenu("Editor"))
    {
        if (ImGui::MenuItem("Dark palette"))
            for (auto editor: gAllEditors)
                editor->SetPalette(TextEditor::GetDarkPalette());
        if (ImGui::MenuItem("Light palette"))
            for (auto editor: gAllEditors)
                editor->SetPalette(TextEditor::GetLightPalette());
        if (ImGui::MenuItem("Retro blue palette"))
            for (auto editor: gAllEditors)
                editor->SetPalette(TextEditor::GetRetroBluePalette());
        ImGui::EndMenu();
    }
}


class WindowWithEditor
{
public:
    WindowWithEditor()
    {
        mEditor.SetPalette(TextEditor::GetLightPalette());
        mEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
        mEditor.SetReadOnly(true);
        gAllEditors.push_back(&mEditor);
    }
    void setEditorAnnotatedSource(const Sources::AnnotatedSource &annotatedSource)
    {
        mEditor.SetText(annotatedSource.source.sourceCode);
        std::unordered_set<int> lineNumbers;
        for (auto line : annotatedSource.linesWithTags)
            lineNumbers.insert(line.lineNumber);
        mEditor.SetBreakpoints(lineNumbers);
    }
    void RenderEditor(const std::string& filename, VoidFunction additionalGui = {})
    {
        guiIconBar(additionalGui);
        guiStatusLine(filename);
        mEditor.Render(filename.c_str());
    }

    TextEditor * _GetTextEditorPtr() { return &mEditor; }


private:
    void guiStatusLine(const std::string& filename)
    {
        auto & editor = mEditor;
        auto cpos = editor.GetCursorPosition();
        ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
                    editor.IsOverwrite() ? "Ovr" : "Ins",
                    editor.CanUndo() ? "*" : " ",
                    editor.GetLanguageDefinition().mName.c_str(), filename.c_str());
    }

    void guiFind()
    {
        ImGui::SameLine();
        // Draw filter
        bool filterChanged = false;
        {
            ImGui::SetNextItemWidth(100.f);
            filterChanged = mFilter.Draw("Search code"); ImGui::SameLine();
            ImGui::SameLine();
            ImGui::TextDisabled("?");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Filter using -exc,inc. For example search for '-widgets,DemoCode'");
            ImGui::SameLine();
        }
        // If changed, check number of matches
        if (filterChanged)
        {
            const auto & lines = mEditor.GetTextLines();
            mNbFindMatches = (int)fplus::count_if([this](auto s){ return mFilter.PassFilter(s.c_str());}, lines);
        }

        // Draw number of matches
        {
            if (mNbFindMatches > 0)
            {
                const auto & lines = mEditor.GetTextLines();
                bool thisLineMatch = mFilter.PassFilter(mEditor.GetCurrentLineText().c_str());
                if (!thisLineMatch)
                    ImGui::Text("---/%3i", mNbFindMatches);
                else
                {
                    std::vector<size_t> allMatchingLinesNumbers =
                        fplus::find_all_idxs_by(
                            [this](const std::string& s) {
                                return mFilter.PassFilter(s.c_str());
                            },
                            lines);
                    auto matchNumber = fplus::find_first_idx(
                        (size_t)mEditor.GetCursorPosition().mLine,
                        allMatchingLinesNumbers);
                    if (matchNumber.is_just())
                        ImGui::Text("%3i/%3i", (int)matchNumber.unsafe_get_just() + 1, mNbFindMatches);
                    else
                        ImGui::Text("Houston, we have a bug");
                    ImGui::SameLine();
                }
                ImGui::SameLine();
            }
            ImGui::SameLine();
        }

        // Perform search down or up
        {
            bool searchDown = ImGui::SmallButton(ICON_FA_ARROW_DOWN); ImGui::SameLine();
            bool searchUp = ImGui::SmallButton(ICON_FA_ARROW_UP); ImGui::SameLine();
            std::vector<std::string> linesToSearch;
            int currentLine = mEditor.GetCursorPosition().mLine ;
            if (searchUp)
            {
                const auto & lines = mEditor.GetTextLines();
                linesToSearch = fplus::get_segment(0, currentLine, lines);
                auto line_idx = fplus::find_last_idx_by(
                    [this](const std::string &line) {
                      return mFilter.PassFilter(line.c_str());
                    },
                    linesToSearch);
                if (line_idx.is_just())
                    mEditor.SetCursorPosition({(int)line_idx.unsafe_get_just(), 0});
            }
            if (searchDown)
            {
                const auto &lines = mEditor.GetTextLines();
                linesToSearch = fplus::get_segment(currentLine + 1, lines.size(), lines);
                auto line_idx = fplus::find_first_idx_by(
                    [this](const std::string &line) {
                        return mFilter.PassFilter(line.c_str());
                    },
                    linesToSearch);
                if (line_idx.is_just())
                    mEditor.SetCursorPosition({(int)line_idx.unsafe_get_just() + currentLine + 1, 0});
            }
        }

        ImGui::SameLine();
    }
    void guiIconBar(VoidFunction additionalGui)
    {
        auto & editor = mEditor;
        static bool canWrite = ! editor.IsReadOnly();
        if (ImGui::Checkbox(ICON_FA_EDIT, &canWrite))
            editor.SetReadOnly(!canWrite);
        ImGui::SameLine();
        if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_UNDO, editor.CanUndo() && canWrite, true))
            editor.Undo();
        if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_REDO, editor.CanRedo() && canWrite, true))
            editor.Redo();
        if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_COPY, editor.HasSelection(), true))
            editor.Copy();
        if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_CUT, editor.HasSelection() && canWrite, true))
            editor.Cut();
        if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_PASTE, (ImGui::GetClipboardText() != nullptr)  && canWrite, true))
            editor.Paste();
        // missing icon from font awesome
        // if (ImGuiExt::SmallButton_WithEnabledFlag(ICON_FA_SELECT_ALL, ImGui::GetClipboardText() != nullptr, true))
        //      editor.PASTE();

        guiFind();

        if (additionalGui)
            additionalGui();

        ImGui::NewLine();
    }

protected:
    TextEditor mEditor;
    ImGuiTextFilter mFilter;
    int mNbFindMatches = 0;
};

class LibrariesCodeBrowser: public WindowWithEditor
{
public:
    LibrariesCodeBrowser(
            const std::vector<Sources::Library>& librarySources,
            std::string currentSourcePath
            ) :
              WindowWithEditor()
            , mLibrarySources(librarySources)
            , mCurrentSource(Sources::ReadSource(currentSourcePath))
    {
        mEditor.SetText(mCurrentSource.sourceCode);
    }

    void gui()
    {
        if (guiSelectLibrarySource())
            mEditor.SetText(mCurrentSource.sourceCode);

        if (fplus::is_suffix_of(std::string(".md"), mCurrentSource.sourcePath))
            MarkdownHelper::Markdown(mCurrentSource.sourceCode);
        else
            RenderEditor(mCurrentSource.sourcePath.c_str());
    }
private:
    bool guiSelectLibrarySource()
    {
        bool changed = false;
        for (const auto & librarySource: mLibrarySources)
        {
            ImGui::Text("%s", librarySource.name.c_str());
            ImGui::SameLine(ImGui::GetWindowSize().x - 350.f );
            ImGuiExt::Hyperlink(librarySource.url);
            MarkdownHelper::Markdown(librarySource.shortDoc);
            for (const auto & source: librarySource.sourcePaths)
            {
                std::string currentSourcePath = librarySource.path + "/" + source;
                bool isSelected = (currentSourcePath == mCurrentSource.sourcePath);
                std::string buttonLabel = source + "##" + librarySource.path;
                if (isSelected)
                    ImGui::TextDisabled("%s", source.c_str());
                else if (ImGui::Button(buttonLabel.c_str()))
                {
                    mCurrentSource = Sources::ReadSource(currentSourcePath);
                    changed = true;
                }
                ImGuiExt::SameLine_IfPossible(80.f);
            }
            ImGui::NewLine();
            ImGui::Separator();
        }
        return changed;
    }

private:
    std::vector<Sources::Library> mLibrarySources;
    Sources::Source mCurrentSource;
};


class ImGuiDemoBrowser: public WindowWithEditor
{
public:
    ImGuiDemoBrowser() :
            WindowWithEditor()
          , mAnnotatedSource(Sources::ReadImGuiDemoCode("imgui/imgui_demo.cpp"))
    {
        setEditorAnnotatedSource(mAnnotatedSource);
    }

    void gui()
    {
        guiHelp();
        guiDemoCodeTags();
        guiSave();
        RenderEditor("imgui_demo.cpp", [this] { this->guiGithubButton(); });
    }

private:
    void guiHelp()
    {
        static bool showHelp = true;
        if (showHelp)
        {
            std::string help =
                "This is the code of imgui_demo.cpp. It is the best way to learn about Dear ImGui! \n"
                "On the left, you can see a demo that showcases all the widgets and features of ImGui: "
                "Click on the \"Code\" buttons to see their code and learn about them. \n"
                "Alternatively, you can also search for some features (try searching for \"widgets\", \"layout\", \"drag\", etc)";
            ImGui::TextWrapped("%s", help.c_str());
            //ImGui::SameLine();
            if (ImGui::Button(ICON_FA_THUMBS_UP " Got it"))
                showHelp = false;
        }
    }
    void guiSave()
    {
#ifdef IMGUI_MANUAL_CAN_WRITE_IMGUI_DEMO_CPP
        if (ImGui::Button("Save"))
        {
            std::string fileSrc = IMGUI_MANUAL_REPO_DIR "/external/imgui/imgui_demo.cpp";
            fplus::write_text_file(fileSrc, mEditor.GetText())();
        }
#endif
    }
    void guiGithubButton()
    {
        if (ImGui::SmallButton("View on github at this line"))
        {
            std::string url = gImGuiRepoUrl + "imgui_demo.cpp#L"
                              + std::to_string(mEditor.GetCursorPosition().mLine);
            HyperlinkHelper::OpenUrl(url);
        }
    }


    void guiDemoCodeTags()
    {
        bool showTooltip = false;
        ImGui::Text("Search demos"); ImGui::SameLine();
        if (ImGui::IsItemHovered())
            showTooltip = true;
        ImGui::TextDisabled("?"); ImGui::SameLine();
        if (ImGui::IsItemHovered())
            showTooltip = true;
        if (showTooltip)
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Filter usage:\n"
                                   "  \"\"         display all\n"
                                   "  \"xxx\"      display items containing \"xxx\"\n"
                                   "  \"xxx,yyy\"  display items containing \"xxx\" or \"yyy\"\n"
                                   "  \"-xxx\"     hide items containing \"xxx\"");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }


        static ImGuiTextFilter filter;
        filter.Draw();
        if (strlen(filter.InputBuf) >= 3)
        {
            for (const auto & lineWithNote : mAnnotatedSource.linesWithTags)
            {
                if (filter.PassFilter(lineWithNote.tag.c_str()))
                {
                    if (ImGui::Button(lineWithNote.tag.c_str()))
                        mEditor.SetCursorPosition({lineWithNote.lineNumber, 0}, 3);
                    ImGuiExt::SameLine_IfPossible();
                }
            }
            ImGui::NewLine();
        }
    }

    Sources::AnnotatedSource mAnnotatedSource;
};


// Show ImGui Readme.md
class ImGuiReadmeBrowser
{
public:
    ImGuiReadmeBrowser() : mSource(Sources::ReadSource("imgui/README.md")) {}
    void gui() {
        MarkdownHelper::Markdown(mSource.sourceCode);
    }
private:
    Sources::Source mSource;
};

// Show doc in imgui.cpp
class ImGuiCppDocBrowser: public WindowWithEditor
{
public:
    ImGuiCppDocBrowser()
        : WindowWithEditor()
        , mAnnotatedSource(Sources::ReadImGuiCppDoc("imgui/imgui.cpp"))
    {
        setEditorAnnotatedSource(mAnnotatedSource);
    }
    void gui()
    {
        ImGui::Text("The doc for Dear ImGui is simply stored inside imgui.cpp");
        guiTags();
        RenderEditor("imgui.cpp", [this] { this->guiGithubButton(); });
    }
private:
    void guiTags()
    {
        for (auto lineWithTag : mAnnotatedSource.linesWithTags)
        {
            // tags are of type H1 or H2, and begin with "H1 " or "H2 " (3 characters)
            std::string title = fplus::drop(3, lineWithTag.tag);
            bool isHeader1 = (fplus::take(3, lineWithTag.tag) == "H1 ");
            if (isHeader1)
            {
                if (ImGuiExt::ClickableText(title.c_str()))
                    mEditor.SetCursorPosition({lineWithTag.lineNumber, 0}, 3);
            }
        }
    }
    void guiGithubButton()
    {
        if (ImGui::SmallButton("View on github at this line"))
        {
            std::string url = gImGuiRepoUrl + "imgui.cpp#L"
                              + std::to_string(mEditor.GetCursorPosition().mLine);
            HyperlinkHelper::OpenUrl(url);
        }
    }

    Sources::AnnotatedSource mAnnotatedSource;
};


class ImGuiCodeBrowser: public WindowWithEditor
{
public:
    ImGuiCodeBrowser()
        : WindowWithEditor()
        , mLibrariesCodeBrowser(Sources::imguiLibrary(), "imgui/imgui.h")
    {

    }
    void gui()
    {
        guiHelp();
        mLibrariesCodeBrowser.gui();
    }
private:
    void guiHelp()
    {
        static bool showHelp = true;
        if (showHelp)
        {
            // Readme
            std::string help = R"(
This is the core of ImGui code.

Usage (extract from [ImGui Readme](https://github.com/ocornut/imgui#usage))

The core of Dear ImGui is self-contained within a few platform-agnostic files which you can easily compile in your application/engine. They are all the files in the root folder of the repository (imgui.cpp, imgui.h, imgui_demo.cpp, imgui_draw.cpp etc.).

No specific build process is required. You can add the .cpp files to your existing project.

You will need a backend to integrate Dear ImGui in your app. The backend passes mouse/keyboard/gamepad inputs and variety of settings to Dear ImGui, and is in charge of rendering the resulting vertices.

Backends for a variety of graphics api and rendering platforms are provided in the [examples/](https://github.com/ocornut/imgui/tree/master/examples) folder, along with example applications. See the [Integration](https://github.com/ocornut/imgui#integration) section of this document for details. You may also create your own backend. Anywhere where you can render textured triangles, you can render Dear ImGui.
)";
            MarkdownHelper::Markdown(help.c_str());
            if (ImGui::Button(ICON_FA_THUMBS_UP " Got it"))
                showHelp = false;
        }
    }

    LibrariesCodeBrowser mLibrariesCodeBrowser;
};


class Acknowledgments: public WindowWithEditor
{
public:
    Acknowledgments()
        : WindowWithEditor()
        , mLibrariesCodeBrowser(Sources::otherLibraries(), "")
    {

    }
    void gui()
    {
        guiHelp();
        mLibrariesCodeBrowser.gui();
    }
private:
    void guiHelp()
    {
        static bool showHelp = true;
        if (showHelp)
        {
            std::string help = R"(
This interactive manual was developed using [Hello ImGui](https://github.com/pthom/hello_imgui), which provided the emscripten port, as well as the assets and source code embedding.
See also a [related demo for Implot](https://traineq.org/implot_demo/src/implot_demo.html), which also provides code navigation.

This manual uses some great libraries, which are shown below.
)";
            MarkdownHelper::Markdown(help.c_str());
            if (ImGui::Button(ICON_FA_THUMBS_UP " Got it"))
                showHelp = false;
        }
    }

    LibrariesCodeBrowser mLibrariesCodeBrowser;
};



int main(int, char **)
{
    ImGuiDemoBrowser imGuiDemoBrowser;
    ImGuiCppDocBrowser imGuiCppDocBrowser;
    ImGuiCodeBrowser imGuiCodeBrowser;
    Acknowledgments otherLibraries;

    gEditorImGuiDemo = imGuiDemoBrowser._GetTextEditorPtr();

    HelloImGui::RunnerParams runnerParams;

    // App window params
    runnerParams.appWindowParams.windowTitle = "ImGui Manual";
    runnerParams.appWindowParams.windowSize = { 1200, 800};

    // ImGui window params
    runnerParams.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenDockSpace;
    runnerParams.imGuiWindowParams.showMenuBar = true;
    runnerParams.imGuiWindowParams.showStatusBar = true;

    // Split the screen in two parts
    runnerParams.dockingParams.dockingSplits = {
        { "MainDockSpace", "CodeSpace", ImGuiDir_Right, 0.65f },
    };

    // Dockable windows definitions
    HelloImGui::DockableWindow dock_imguiDemoWindow;
    {
        dock_imguiDemoWindow.label = "Dear ImGui Demo";
        dock_imguiDemoWindow.dockSpaceName = "MainDockSpace";
        dock_imguiDemoWindow.GuiFonction = [&dock_imguiDemoWindow] {
            if (dock_imguiDemoWindow.isVisible)
                ImGui::ShowDemoWindow(nullptr);
        };
        dock_imguiDemoWindow.callBeginEnd = false;
    };

    HelloImGui::DockableWindow dock_imguiDemoCode;
    {
        dock_imguiDemoCode.label = "ImGui - Demo Code";
        dock_imguiDemoCode.dockSpaceName = "CodeSpace";
        dock_imguiDemoCode.GuiFonction = [&imGuiDemoBrowser]{ imGuiDemoBrowser.gui(); };
    };

    HelloImGui::DockableWindow dock_imguiReadme;
    {
        dock_imguiReadme.label = "ImGui - Readme";
        dock_imguiReadme.dockSpaceName = "CodeSpace";
        dock_imguiReadme.GuiFonction = []{
            static ImGuiReadmeBrowser w;
            w.gui();
        };
    };

    HelloImGui::DockableWindow dock_imGuiCppDocBrowser;
    {
        dock_imGuiCppDocBrowser.label = "ImGui - Doc";
        dock_imGuiCppDocBrowser.dockSpaceName = "CodeSpace";
        dock_imGuiCppDocBrowser.GuiFonction = [&imGuiCppDocBrowser]{ imGuiCppDocBrowser.gui(); };
    };

    HelloImGui::DockableWindow dock_imguiCodeBrowser;
    {
        dock_imguiCodeBrowser.label = "ImGui - Code";
        dock_imguiCodeBrowser.dockSpaceName = "CodeSpace";
        dock_imguiCodeBrowser.GuiFonction = [&imGuiCodeBrowser]{ imGuiCodeBrowser.gui(); };
    };

    HelloImGui::DockableWindow dock_acknowledgments;
    {
        dock_acknowledgments.label = "Acknowledgments";
        dock_acknowledgments.dockSpaceName = "CodeSpace";
        dock_acknowledgments.GuiFonction = [&otherLibraries]{ otherLibraries.gui(); };
    };

    // Menu
    runnerParams.callbacks.ShowMenus = []() {
        menuEditorTheme();
    };

    // Fonts
    runnerParams.callbacks.LoadAdditionalFonts = MarkdownHelper::LoadFonts;

    // Set app dockable windows
    runnerParams.dockingParams.dockableWindows = {
        dock_imguiDemoCode,
        dock_imguiDemoWindow,
        dock_imguiReadme,
        dock_imGuiCppDocBrowser,
        dock_imguiCodeBrowser,
        dock_acknowledgments,
    };

    gImGuiDemoCallback = implImGuiDemoCallbackDemoCallback;

    HelloImGui::Run(runnerParams);
    return 0;
}
