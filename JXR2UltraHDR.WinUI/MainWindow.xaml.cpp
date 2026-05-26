#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
// ConverterAPI is provided by JXR2UltraHDR-lib submodule (ThirdParty/JXR2UltraHDR-lib)
// Core libs are linked via vcxproj AdditionalLibraryDirectories + AdditionalDependencies
#include "ConverterAPI.h"

#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>

#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Storage.Search.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <sstream>
#include <iomanip>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Microsoft::UI::Xaml::Controls::Primitives;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::JXR2UltraHDR::implementation
{
    // ── Helper: format file size ──
    static std::wstring FormatFileSize(uint64_t bytes)
    {
        if (bytes < 1024) return std::to_wstring(bytes) + L" B";
        if (bytes < 1024 * 1024) return std::to_wstring(bytes / 1024) + L" KB";
        return std::to_wstring(bytes / (1024 * 1024)) + L" MB";
    }

    // ── Helper: format resolution ──
    static std::wstring FormatResolution(int w, int h)
    {
        if (w <= 0 || h <= 0) return L"";
        return std::to_wstring(w) + L"x" + std::to_wstring(h);
    }

    // ── Status symbols (✓ done, ✗ failed) ──
    static std::wstring StatusSymbol(const std::wstring& status, bool)
    {
        if (status == L"Done")
            return L"\u2713";
        if (status == L"Converting..." || status == L"Cancelled" ||
            status.find(L"Error") != std::wstring::npos)
            return L"\u2717";
        return L"";
    }

    MainWindow::MainWindow()
    {
        // Set window to ~25% of desktop before it's shown, avoiding resize flash
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int effW = static_cast<int>(screenW * 0.25);
        int effH = static_cast<int>(screenH * 0.25);
        if (effW < 640) effW = 640;
        if (effH < 500) effH = 500;

        auto windowNative = this->try_as<::IWindowNative>();
        if (windowNative)
        {
            HWND hwnd = nullptr;
            windowNative->get_WindowHandle(&hwnd);
            auto appWindow = this->AppWindow();
            UINT dpi = GetDpiForWindow(hwnd);
            double scale = dpi / 96.0;
            Windows::Graphics::SizeInt32 rawSize;
            rawSize.Width = static_cast<int>(effW * scale);
            rawSize.Height = static_cast<int>(effH * scale);
            appWindow.Resize(rawSize);
        }
    }

    void MainWindow::OnWindowLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        // Save header for toggle in thumbnail mode
        m_listHeader = FileListView().Header();
    }

    // ══════════════════════════════════════════════════════
    // Drag & Drop
    // ══════════════════════════════════════════════════════

    void MainWindow::OnDragOver(IInspectable const&, DragEventArgs const& e)
    {
        if (m_converting) return;
        e.AcceptedOperation(DataPackageOperation::Copy);
    }

    void MainWindow::OnDrop(IInspectable const&, DragEventArgs const& e)
    {
        if (m_converting) return;
        auto dataView = e.DataView();
        if (!dataView.Contains(StandardDataFormats::StorageItems())) return;
        try
        {
            auto items = dataView.GetStorageItemsAsync().get();
            for (uint32_t i = 0; i < items.Size(); i++)
            {
                auto item = items.GetAt(i);
                if (item.IsOfType(StorageItemTypes::File))
                {
                    hstring path = item.Path();
                    std::wstring filePath(path.c_str());
                    std::wstring ext = filePath.substr(filePath.find_last_of(L'.'));
                    for (auto& c : ext) c = towlower(c);
                    if (ext == L".jxr" || ext == L".wdp")
                        AddFileEntry(filePath);
                }
            }
            RebuildListView();
            UpdateUIState();
            // Enrich metadata in background
            EnrichEntriesAsync();
        }
        catch (...) {}
    }

    // ══════════════════════════════════════════════════════
    // Buttons
    // ══════════════════════════════════════════════════════

    void MainWindow::OnBrowseOutput(IInspectable const&, RoutedEventArgs const&)
    {
        auto picker = Windows::Storage::Pickers::FolderPicker();
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::Desktop);
        picker.FileTypeFilter().Append(L"*");
        auto interop = picker.as<::IInitializeWithWindow>();
        interop->Initialize(GetWindowHandle());
        picker.PickSingleFolderAsync().Completed([this](IAsyncOperation<StorageFolder> const& op, AsyncStatus)
        {
            try {
                auto folder = op.GetResults();
                if (folder) {
                    m_outputDir = std::wstring(folder.Path().c_str());
                    DispatcherQueue().TryEnqueue([this, path = folder.Path()]() {
                        OutputDirBox().Text(path);
                    });
                }
            } catch (...) {}
        });
    }

    void MainWindow::OnAddFolder(IInspectable const&, RoutedEventArgs const&)
    {
        auto picker = Windows::Storage::Pickers::FolderPicker();
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::Desktop);
        picker.FileTypeFilter().Append(L"*");
        auto interop = picker.as<::IInitializeWithWindow>();
        interop->Initialize(GetWindowHandle());
        picker.PickSingleFolderAsync().Completed([this](IAsyncOperation<StorageFolder> const& op, AsyncStatus)
        {
            try {
                auto folder = op.GetResults();
                if (folder) {
                    ScanAndAddFiles(std::wstring(folder.Path().c_str()));
                }
            } catch (...) {}
        });
    }

    void MainWindow::OnConvert(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_converting || m_entries.empty()) return;
        m_cancelled = false;
        m_converting = true;
        if (!OutputDirBox().Text().empty())
            m_outputDir = std::wstring(OutputDirBox().Text().c_str());
        else
            m_outputDir.clear();
        UpdateUIState();
        StatusText().Text(L"Starting...");
        OverallProgress().Value(0.0);
        OverallProgress().IsIndeterminate(false);
        m_worker = std::thread(&MainWindow::ConvertFilesAsync, this);
        m_worker.detach();
    }

    void MainWindow::OnCancel(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_converting) { m_cancelled = true; Converter_Cancel(); }
    }

    void MainWindow::OnClear(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_converting) return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_entries.clear();
        }
        RebuildListView();
        UpdateUIState();
        StatusText().Text(L"");
        OverallProgress().Value(0.0);
    }

    // ══════════════════════════════════════════════════════
    // Conversion worker thread
    // ══════════════════════════════════════════════════════

    void MainWindow::ConvertFilesAsync()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            DispatcherQueue().TryEnqueue([this]() { StatusText().Text(L"Failed to initialize COM"); });
            m_converting = false; return;
        }
        int total = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            total = (int)m_entries.size();
        }
        DispatcherQueue().TryEnqueue([this, total]() {
            std::wostringstream ss; ss << L"Converting " << total << L" file(s)...";
            StatusText().Text(winrt::hstring(ss.str().c_str()));
        });
        int successCount = 0, failCount = 0;
        for (int i = 0; i < total; i++) {
            if (m_cancelled) break;
            FileEntry entry;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (i < (int)m_entries.size()) {
                    entry = m_entries[i];
                    m_entries[i].status = L"Converting...";
                } else break;
            }
            // Update status to Converting in-place (no full rebuild)
            DispatcherQueue().TryEnqueue([this, i]() {
                UpdateStatusAt(i, L"Converting...", false);
            });
            int result = ConvertSingleFile(entry);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (i < (int)m_entries.size()) {
                    if (m_cancelled) {
                        m_entries[i].status = L"Cancelled";
                        m_entries[i].isComplete = true; m_entries[i].isSuccess = false;
                    } else if (result == 0) {
                        m_entries[i].status = L"Done";
                        m_entries[i].isComplete = true; m_entries[i].isSuccess = true;
                        successCount++;
                    } else {
                        const wchar_t* err = Converter_GetLastError();
                        m_entries[i].status = (err && *err) ? err : (L"Error #" + std::to_wstring(result));
                        m_entries[i].isComplete = true; m_entries[i].isSuccess = false;
                        failCount++;
                    }
                }
            }
            // Update status in-place (no full rebuild)
            double pct = (double)(i + 1) / total * 100.0;
            bool done = (result == 0 || m_cancelled);
            std::wstring newStatus;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (i < (int)m_entries.size())
                    newStatus = m_entries[i].status;
            }
            DispatcherQueue().TryEnqueue([this, i, pct, newStatus, done]() {
                OverallProgress().Value(pct);
                UpdateStatusAt(i, newStatus, !done || newStatus == L"Done");
            });
        }
        m_converting = false; m_cancelled = false;
        CoUninitialize();
        DispatcherQueue().TryEnqueue([this, successCount, failCount]() {
            std::wostringstream ss;
            if (failCount > 0) ss << L"Done: " << successCount << L" ok, " << failCount << L" failed.";
            else ss << L"All " << successCount << L" file(s) converted successfully.";
            StatusText().Text(winrt::hstring(ss.str().c_str()));
            OverallProgress().Value(100.0);
            RebuildListView();
            UpdateUIState();
        });
    }

    int MainWindow::ConvertSingleFile(const FileEntry& entry)
    {
        auto outputPath = GetOutputPath(entry.filePath);
        if (outputPath.empty()) return -1;
        auto callback = [](float, void* userData) -> int { return static_cast<MainWindow*>(userData)->m_cancelled ? 1 : 0; };
        return Converter_ConvertFile(entry.filePath.c_str(), outputPath.c_str(), 95, 90, callback, this);
    }

    // ══════════════════════════════════════════════════════
    // Helpers
    // ══════════════════════════════════════════════════════

    std::wstring MainWindow::GetOutputPath(const std::wstring& inputPath)
    {
        std::wstring outPath;
        if (!m_outputDir.empty()) {
            auto pos = inputPath.find_last_of(L'\\');
            auto fileName = (pos != std::wstring::npos) ? inputPath.substr(pos + 1) : inputPath;
            outPath = m_outputDir + L"\\" + fileName;
        } else { outPath = inputPath; }
        auto dotPos = outPath.find_last_of(L'.');
        if (dotPos != std::wstring::npos) outPath = outPath.substr(0, dotPos) + L".jpg";
        else outPath += L".jpg";
        return outPath;
    }

    void MainWindow::AddFileEntry(const std::wstring& filePath)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& e : m_entries)
            if (_wcsicmp(e.filePath.c_str(), filePath.c_str()) == 0) return;
        FileEntry entry;
        entry.filePath = filePath;
        auto pos = filePath.find_last_of(L'\\');
        entry.fileName = (pos != std::wstring::npos) ? filePath.substr(pos + 1) : filePath;
        entry.status = L"Queued";
        entry.fileSize = 0;
        entry.imageWidth = 0; entry.imageHeight = 0;
        m_entries.push_back(entry);
    }

    // ── Enrich entry with file metadata & image info ──
    void MainWindow::EnrichFileEntry(FileEntry& entry)
    {
        // File size
        try {
            auto file = StorageFile::GetFileFromPathAsync(winrt::hstring(entry.filePath)).get();
            auto props = file.GetBasicPropertiesAsync().get();
            entry.fileSize = props.Size();
        } catch (...) { entry.fileSize = 0; }

        // Image info via C API (header decode only)
        ConverterImageInfo info = {};
        int ret = Converter_GetImageInfo(entry.filePath.c_str(), &info);
        if (ret == 0) {
            entry.imageWidth = info.width;
            entry.imageHeight = info.height;
            if (info.pixelFormat) entry.pixelFormat = info.pixelFormat;
            if (info.colorSpace) entry.colorSpace = info.colorSpace;
            entry.isHDR = (info.isHDR != 0);
        }
    }

    // ── Background enrichment for all pending entries ──
    void MainWindow::EnrichEntriesAsync()
    {
        std::thread([this]() {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& entry : m_entries) {
                    if (entry.fileSize == 0) {
                        EnrichFileEntry(entry);
                    }
                }
            }
            CoUninitialize();
            DispatcherQueue().TryEnqueue([this]() {
                RebuildListView();
            });
        }).detach();
    }

    // ══════════════════════════════════════════════════════
    // Scan folder (background, progressive, with progress)
    // ══════════════════════════════════════════════════════
    void MainWindow::ScanAndAddFiles(const std::wstring& folderPath)
    {
        std::thread([this, folderPath]() {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            try {
                auto folder = StorageFolder::GetFolderFromPathAsync(winrt::hstring(folderPath)).get();
                auto query = folder.CreateFileQueryWithOptions(Search::QueryOptions(
                    Search::CommonFileQuery::DefaultQuery,
                    { L".jxr", L".wdp" }));
                auto files = query.GetFilesAsync().get();
                int total = (int)files.Size();

                DispatcherQueue().TryEnqueue([this, total]() {
                    OverallProgress().Value(0.0);
                    OverallProgress().IsIndeterminate(false);
                    std::wostringstream ss;
                    ss << L"Scanning folder: found " << total << L" file(s)...";
                    StatusText().Text(winrt::hstring(ss.str().c_str()));
                });

                int added = 0;
                int lastReported = -1;

                for (int i = 0; i < total; i++) {
                    auto file = files.GetAt(i);
                    std::wstring fpath(file.Path().c_str());

                    std::wstring ext = fpath.substr(fpath.find_last_of(L'.'));
                    for (auto& c : ext) c = towlower(c);
                    if (ext == L".jxr" || ext == L".wdp") {
                        AddFileEntry(fpath);
                        added++;
                    }

                    int pct = (i + 1) * 100 / total;
                    int reportStep = (total > 50) ? 5 : 1;
                    if (pct / reportStep != lastReported / reportStep) {
                        lastReported = pct;
                        DispatcherQueue().TryEnqueue([this, added, i, total, pct]() {
                            std::wostringstream ss;
                            ss << L"Found " << total << L" .jxr files, added " << added << L"...";
                            StatusText().Text(winrt::hstring(ss.str().c_str()));
                            OverallProgress().Value((double)pct);
                        });
                    }
                }

                DispatcherQueue().TryEnqueue([this, added, total]() {
                    std::wostringstream ss;
                    if (added == 0)
                        ss << L"No .jxr files found in folder.";
                    else
                        ss << L"Added " << added << L" of " << total << L" file(s).";
                    StatusText().Text(winrt::hstring(ss.str().c_str()));
                    OverallProgress().Value(100.0);
                    RebuildListView();
                    UpdateUIState();
                });

                // Enrich metadata (already on background thread)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (auto& entry : m_entries) {
                        if (entry.fileSize == 0) {
                            EnrichFileEntry(entry);
                        }
                    }
                }

                DispatcherQueue().TryEnqueue([this]() {
                    RebuildListView();
                });
            }
            catch (...) {
                DispatcherQueue().TryEnqueue([this]() {
                    StatusText().Text(L"Error scanning folder.");
                    OverallProgress().Value(0.0);
                });
            }

            if (SUCCEEDED(hr)) CoUninitialize();
        }).detach();
    }

    void MainWindow::RebuildListView()
    {
        FileListView().Items().Clear();
        if (m_viewMode == ViewMode::List) RebuildListItems();
        else RebuildThumbnailItems();
    }

    // ── Multi-column list view ──
    void MainWindow::RebuildListItems()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (size_t i = 0; i < m_entries.size(); ++i)
        {
            const auto& entry = m_entries[i];
            auto item = ListViewItem();

            auto grid = Grid();
            for (int c = 0; c < 7; c++)
                grid.ColumnDefinitions().Append(ColumnDefinition());
            // Column widths: *, 70, 90, 160, 110, 80, 40
            grid.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            grid.ColumnDefinitions().GetAt(1).Width({ 70, GridUnitType::Pixel });
            grid.ColumnDefinitions().GetAt(2).Width({ 90, GridUnitType::Pixel });
            grid.ColumnDefinitions().GetAt(3).Width({ 160, GridUnitType::Pixel });
            grid.ColumnDefinitions().GetAt(4).Width({ 110, GridUnitType::Pixel });
            grid.ColumnDefinitions().GetAt(5).Width({ 80, GridUnitType::Pixel });
            grid.ColumnDefinitions().GetAt(6).Width({ 40, GridUnitType::Pixel });

            // Name
            auto nameBlock = TextBlock();
            nameBlock.Text(winrt::hstring(entry.fileName));
            nameBlock.TextTrimming(TextTrimming::CharacterEllipsis);
            nameBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(nameBlock, 0);
            grid.Children().Append(nameBlock);

            // Size
            auto sizeBlock = TextBlock();
            sizeBlock.Text(winrt::hstring(FormatFileSize(entry.fileSize)));
            sizeBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(sizeBlock, 1);
            grid.Children().Append(sizeBlock);

            // Resolution
            auto resBlock = TextBlock();
            resBlock.Text(winrt::hstring(FormatResolution(entry.imageWidth, entry.imageHeight)));
            resBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(resBlock, 2);
            grid.Children().Append(resBlock);

            // Format (pixel format = HDR info with bit depth)
            auto fmtBlock = TextBlock();
            if (entry.isHDR && !entry.pixelFormat.empty()) {
                fmtBlock.Text(winrt::hstring(entry.pixelFormat));
            } else if (entry.isHDR) {
                fmtBlock.Text(L"HDR");
            } else if (!entry.pixelFormat.empty()) {
                fmtBlock.Text(winrt::hstring(entry.pixelFormat));
            } else {
                fmtBlock.Text(L"");
            }
            fmtBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(fmtBlock, 3);
            grid.Children().Append(fmtBlock);

            // Color Space
            auto colorBlock = TextBlock();
            colorBlock.Text(winrt::hstring(entry.colorSpace));
            colorBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(colorBlock, 4);
            grid.Children().Append(colorBlock);

            // Status
            auto statusBlock = TextBlock();
            {
                std::wstring sym = StatusSymbol(entry.status, entry.isSuccess);
                if (!sym.empty())
                    statusBlock.Text(winrt::hstring(sym + L" " + entry.status));
                else
                    statusBlock.Text(winrt::hstring(entry.status));
            }
            statusBlock.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(statusBlock, 5);
            grid.Children().Append(statusBlock);

            // Remove button
            auto removeBtn = Button();
            removeBtn.Content(winrt::box_value(L"\u2715"));
            removeBtn.Width(32); removeBtn.Height(32);
            removeBtn.Padding(Microsoft::UI::Xaml::Thickness());
            removeBtn.HorizontalAlignment(HorizontalAlignment::Right);
            removeBtn.IsEnabled(!m_converting.load());
            Grid::SetColumn(removeBtn, 6);

            size_t idx = i;
            removeBtn.Click([this, idx](IInspectable const&, RoutedEventArgs const&) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (idx < m_entries.size())
                        m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(idx));
                }
                DispatcherQueue().TryEnqueue([this]() { RebuildListView(); UpdateUIState(); });
            });

            grid.Children().Append(removeBtn);
            item.Content(grid);
            item.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            FileListView().Items().Append(item);
        }
    }

    // ══════════════════════════════════════════════════════
    // Thumbnail view — Phase 1: placeholder items (UI thread)
    // ══════════════════════════════════════════════════════
    void MainWindow::RebuildThumbnailItems()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < m_entries.size(); ++i) {
            const auto& entry = m_entries[i];
            auto item = ListViewItem();
            auto outer = StackPanel();
            outer.Orientation(Orientation::Vertical);
            outer.Width(130);
            outer.HorizontalAlignment(HorizontalAlignment::Center);
            outer.Margin({ 4, 8, 4, 8 });

            // Placeholder image — no IO, Source stays null
            auto img = Image();
            img.Width(120);
            img.Height(80);
            img.HorizontalAlignment(HorizontalAlignment::Center);
            img.Stretch(Microsoft::UI::Xaml::Media::Stretch::Uniform);
            outer.Children().Append(img);

            auto nameBlock = TextBlock();
            nameBlock.Text(winrt::hstring(entry.fileName));
            nameBlock.TextTrimming(TextTrimming::CharacterEllipsis);
            nameBlock.TextWrapping(TextWrapping::Wrap);
            nameBlock.MaxLines(2);
            nameBlock.TextAlignment(TextAlignment::Center);
            nameBlock.HorizontalAlignment(HorizontalAlignment::Center);
            nameBlock.Width(130);
            outer.Children().Append(nameBlock);

            // Status — shows symbol only (no text) for cleaner look
            auto statusBlock = TextBlock();
            {
                std::wstring sym = StatusSymbol(entry.status, entry.isSuccess);
                if (!sym.empty())
                    statusBlock.Text(winrt::hstring(sym));
                else if (!entry.status.empty() && entry.status != L"Queued")
                    statusBlock.Text(winrt::hstring(entry.status));
            }
            statusBlock.FontSize(14);
            statusBlock.HorizontalAlignment(HorizontalAlignment::Center);
            outer.Children().Append(statusBlock);

            item.Content(outer);
            item.HorizontalContentAlignment(HorizontalAlignment::Center);

            // Right-click / context menu — Remove from queue
            auto flyout = MenuFlyout();
            auto removeItem = MenuFlyoutItem();
            removeItem.Text(L"Remove from queue");
            size_t idx = i;
            removeItem.Click([this, idx](IInspectable const&, RoutedEventArgs const&) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (idx < m_entries.size())
                        m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(idx));
                }
                DispatcherQueue().TryEnqueue([this]() { RebuildListView(); UpdateUIState(); });
            });
            flyout.Items().Append(removeItem);
            item.ContextFlyout(flyout);

            FileListView().Items().Append(item);
        }

        // Phase 2: background thumbnail loading
        std::thread(&MainWindow::LoadThumbnailsAsync, this).detach();
    }

    // ══════════════════════════════════════════════════════
    // Thumbnail view — Phase 2: background async loading
    // ══════════════════════════════════════════════════════
    void MainWindow::LoadThumbnailsAsync()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        std::vector<std::pair<std::wstring, size_t>> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (size_t i = 0; i < m_entries.size(); ++i)
                snapshot.push_back({ m_entries[i].filePath, i });
        }

        for (const auto& [path, idx] : snapshot) {
            if (m_viewMode != ViewMode::Thumbnail)
                break;

            try {
                auto file = StorageFile::GetFileFromPathAsync(winrt::hstring(path)).get();
                auto thumb = file.GetThumbnailAsync(Windows::Storage::FileProperties::ThumbnailMode::SingleItem, 256u).get();
                if (thumb && thumb.Size() > 0) {
                    bool valid = false;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        if (idx < m_entries.size() && m_entries[idx].filePath == path)
                            valid = true;
                    }
                    if (valid) {
                        DispatcherQueue().TryEnqueue([this, idx, stream = std::move(thumb)]() {
                            try {
                                if (idx < FileListView().Items().Size()) {
                                    auto item = FileListView().Items().GetAt(idx).as<ListViewItem>();
                                    auto outer = item.Content().as<StackPanel>();
                                    auto img = outer.Children().GetAt(0).as<Image>();
                                    auto bmp = Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                                    bmp.SetSource(stream);
                                    img.Source(bmp);
                                }
                            } catch (...) {}
                        });
                    }
                }
            }
            catch (...) { }
        }

        if (SUCCEEDED(hr)) CoUninitialize();
    }

    // ══════════════════════════════════════════════════════
    // In-place status update (no full rebuild)
    // ══════════════════════════════════════════════════════
    void MainWindow::UpdateStatusAt(size_t idx, const std::wstring& status, bool isSuccess)
    {
        if (idx >= FileListView().Items().Size())
            return;

        std::wstring sym = StatusSymbol(status, isSuccess);
        std::wstring displayText;

        if (m_viewMode == ViewMode::List) {
            if (!sym.empty())
                displayText = sym + L" " + status;
            else
                displayText = status;
        } else {
            // Thumbnail: symbol only, or full text for errors
            if (!sym.empty())
                displayText = sym;
            else if (status.find(L"Error") != std::wstring::npos ||
                     status.find(L"#") != std::wstring::npos)
                displayText = status;
            else
                displayText = L"";
        }

        try {
            auto item = FileListView().Items().GetAt(idx).as<ListViewItem>();
            if (m_viewMode == ViewMode::List) {
                // List: Content → Grid → Children[5] = status TextBlock
                auto grid = item.Content().as<Panel>();
                auto statusText = grid.Children().GetAt(5).as<TextBlock>();
                statusText.Text(winrt::hstring(displayText));
            } else {
                // Thumbnail: Content → StackPanel → Children[2] = status TextBlock
                auto outer = item.Content().as<Panel>();
                auto statusText = outer.Children().GetAt(2).as<TextBlock>();
                statusText.Text(winrt::hstring(displayText));
            }
        } catch (...) {}
    }

    // ══════════════════════════════════════════════════════

    void MainWindow::OnSwitchToList(IInspectable const&, RoutedEventArgs const&)
    {
        m_viewMode = ViewMode::List;
        auto templ = FileListView().Resources().Lookup(winrt::box_value(L"ListPanel")).as<ItemsPanelTemplate>();
        FileListView().ItemsPanel(templ);
        FileListView().Header(m_listHeader);  // restore header
        RebuildListView();
    }

    void MainWindow::OnSwitchToThumbnail(IInspectable const&, RoutedEventArgs const&)
    {
        m_viewMode = ViewMode::Thumbnail;
        auto templ = FileListView().Resources().Lookup(winrt::box_value(L"GridPanel")).as<ItemsPanelTemplate>();
        FileListView().ItemsPanel(templ);
        FileListView().Header(nullptr);  // hide header
        RebuildListView();
    }

    HWND MainWindow::GetWindowHandle()
    {
        auto windowNative = this->try_as<::IWindowNative>();
        if (windowNative) { HWND hwnd = nullptr; windowNative->get_WindowHandle(&hwnd); return hwnd; }
        return nullptr;
    }

    void MainWindow::UpdateUIState()
    {
        ConvertBtn().IsEnabled(!m_converting && !m_entries.empty());
        CancelBtn().IsEnabled(m_converting);
        BrowseOutputBtn().IsEnabled(!m_converting);
        AddFolderBtn().IsEnabled(!m_converting);
    }
}
