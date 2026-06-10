#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <string>
#include <shobjidl.h> // Required for Windows File Dialog
#include <locale>
#include <codecvt>

namespace fs = std::filesystem;

struct FolderInfo {
    std::string name;
    fs::path full_path;
    uint64_t size = 0;
};

// Open a native Windows folder browser dialog
std::string browse_for_folder() {
    std::string result_path = "";

    // Initialize the COM library
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFileOpen;

        // Create the FileOpenDialog object
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (SUCCEEDED(hr)) {
            // Set options to pick folders instead of files
            DWORD dwOptions;
            if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
                pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }

            // Show the Dialog box
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    // Convert Windows Wide String (wchar_t*) to standard std::string
                    if (SUCCEEDED(hr)) {
                        std::wstring wpath(pszFilePath);
                        result_path = std::string(wpath.begin(), wpath.end());
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return result_path;
}

// Helper function to recursively calculate a folder's size
uint64_t get_directory_size(const fs::path& dir_path) {
    uint64_t size = 0;
    try {
        if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied)) {
                if (fs::is_regular_file(entry.status())) {
                    size += fs::file_size(entry);
                }
            }
        }
    }
    catch (const std::exception&) {
        // Silently catch restricted files to keep console clean
    }
    return size;
}

// Helper function to format bytes into human-readable strings (GB or MB)
std::string format_size(uint64_t size_in_bytes) {
    double size = static_cast<double>(size_in_bytes);
    std::string unit = " Bytes";

    if (size >= 1024ULL * 1024 * 1024) {
        size /= (1024.0 * 1024 * 1024);
        unit = " GB";
    }
    else if (size >= 1024ULL * 1024) {
        size /= (1024.0 * 1024);
        unit = " MB";
    }
    else if (size >= 1024ULL) {
        size /= 1024.0;
        unit = " KB";
    }

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%.2f", size);
    return std::string(buffer) + unit;
}

int main() {
    // 1. Browse for the folder natively using Windows API
    std::string target_path = browse_for_folder();

    if (target_path.empty() || !fs::exists(target_path) || !fs::is_directory(target_path)) {
        std::cerr << "No valid directory selected. Exiting.\n";
        return 1;
    }

    std::cout << "Selected Directory: " << target_path << "\n";

    // Main application interactive loop
    while (true) {
        std::vector<FolderInfo> folders;
        std::cout << "\nScanning directories..." << std::endl;

        for (const auto& entry : fs::directory_iterator(target_path)) {
            if (fs::is_directory(entry.status())) {
                FolderInfo info;
                info.name = entry.path().filename().string();
                info.full_path = entry.path();
                info.size = get_directory_size(entry.path());
                folders.push_back(info);
            }
        }

        if (folders.empty()) {
            std::cout << "No subfolders found in this directory!\n";
            break;
        }

        // Sort folders from biggest to smallest
        std::sort(folders.begin(), folders.end(), [](const FolderInfo& a, const FolderInfo& b) {
            return a.size > b.size;
            });

        // 2. Display Interactive Menu
        std::cout << "\n--- Folders Sorted by Size (Largest First) ---\n";
        for (size_t i = 0; i < folders.size(); ++i) {
            std::cout << i + 1 << ") [" << format_size(folders[i].size) << "] " << folders[i].name << "\n";
        }
        std::cout << "----------------------------------------------\n";
        std::cout << "Enter the number of a folder to DELETE it, or '0' to exit: ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            std::cout << "Invalid input. Try again.\n";
            continue;
        }

        if (choice == 0) {
            std::cout << "Exiting application. Clean drive, happy gaming!\n";
            break;
        }

        if (choice < 1 || choice > static_cast<int>(folders.size())) {
            std::cout << "Invalid choice. Please pick a number from the list.\n";
            continue;
        }

        // 3. Delete Logic
        const auto& selected_folder = folders[choice - 1];
        std::cout << "\nWARNING: Are you sure you want to completely delete:\n";
        std::cout << selected_folder.full_path.string() << " (" << format_size(selected_folder.size) << ")?\n";
        std::cout << "Type 'y' to confirm: ";

        std::string confirmation;
        std::cin >> confirmation;

        if (confirmation == "y" || confirmation == "Y") {
            try {
                std::cout << "Deleting..." << std::endl;
                fs::remove_all(selected_folder.full_path);
                std::cout << "Successfully deleted " << selected_folder.name << "!\n";
            }
            catch (const fs::filesystem_error& e) {
                std::cerr << "Error deleting folder: " << e.what() << "\n";
            }
        }
        else {
            std::cout << "Deletion canceled.\n";
        }
    }

    return 0;
}