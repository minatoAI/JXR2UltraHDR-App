#pragma once

#include "MainWindow.g.h"

#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace winrt::JXR2UltraHDR::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnDragOver(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::DragEventArgs const& e);
        void OnDrop(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::DragEventArgs const& e);
        void OnBrowseOutput(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnConvert(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnCancel(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnClear(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnSwitchToList(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnSwitchToThumbnail(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnWindowLoaded(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnAddFolder(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        struct FileEntry
        {
            std::wstring filePath;
            std::wstring fileName;
            std::wstring status = L"Queued";
            bool isComplete = false;
            bool isSuccess = false;
            uint64_t fileSize = 0;
            int32_t imageWidth = 0;
            int32_t imageHeight = 0;
            std::wstring pixelFormat;
            std::wstring colorSpace;
            bool isHDR = false;
        };

        std::vector<FileEntry> m_entries;
        std::wstring m_outputDir;
        std::atomic<bool> m_converting{ false };
        std::atomic<bool> m_cancelled{ false };
        std::thread m_worker;
        std::mutex m_mutex;

        enum class ViewMode { List, Thumbnail };
        ViewMode m_viewMode = ViewMode::List;
        Windows::Foundation::IInspectable m_listHeader{ nullptr };

        HWND GetWindowHandle();
        void UpdateUIState();
        void ConvertFilesAsync();
        int ConvertSingleFile(const FileEntry& entry);
        std::wstring GetOutputPath(const std::wstring& inputPath);
        void AddFileEntry(const std::wstring& filePath);
        void ScanAndAddFiles(const std::wstring& folderPath);
        void EnrichEntriesAsync();
        void RebuildListView();
        void RebuildListItems();
        void RebuildThumbnailItems();
        void EnrichFileEntry(FileEntry& entry);
        void LoadThumbnailsAsync();
        void UpdateStatusAt(size_t idx, const std::wstring& status, bool isSuccess);
    };
}

namespace winrt::JXR2UltraHDR::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
