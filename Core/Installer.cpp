//---------------------------------------------------------------------------
// Installer implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "Installer.h"
#include <Registry.hpp>
#include <IOUtils.hpp>
#include <Vcl.Forms.hpp>
#include <DateUtils.hpp>
#include <System.Threading.hpp>
#include <fstream>
#include <vector>

namespace DxCore
{

const wchar_t* const TInstaller::DX_ENV_VARIABLE = L"DXVCL";

// Log file - created next to the executable with timestamp name
static std::wofstream g_LogFile;
static String g_LogFileName;

static String GetLogFileName()
{
    if (g_LogFileName.IsEmpty())
    {
        // Get executable directory
        String exePath = ExtractFilePath(Application->ExeName);
        
        // Create filename with timestamp: DD_MM_YYYY_HH_MM.log
        TDateTime now = Now();
        unsigned short year, month, day, hour, min, sec, msec;
        DecodeDate(now, year, month, day);
        DecodeTime(now, hour, min, sec, msec);
        
        g_LogFileName = exePath + Format(L"%02d_%02d_%04d_%02d_%02d.log",
            ARRAYOFCONST((day, month, year, hour, min)));
    }
    return g_LogFileName;
}

static void LogToFile(const String& msg)
{
    if (!g_LogFile.is_open())
    {
        String fileName = GetLogFileName();
        g_LogFile.open(fileName.c_str(), std::ios::out | std::ios::app);
    }
    if (g_LogFile.is_open())
    {
        // Add timestamp to each line
        TDateTime now = Now();
        unsigned short hour, min, sec, msec;
        DecodeTime(now, hour, min, sec, msec);
        
        String timestamp = Format(L"[%02d:%02d:%02d] ", ARRAYOFCONST((hour, min, sec)));
        g_LogFile << timestamp.c_str() << msg.c_str() << std::endl;
        g_LogFile.flush();
    }
}

//---------------------------------------------------------------------------
// TInstaller implementation
//---------------------------------------------------------------------------
TInstaller::TInstaller()
    : FState(TInstallerState::Normal),
      FOnProgress(nullptr),
      FOnProgressState(nullptr)
{
    FIDEDetector = std::make_unique<TIDEDetector>();
    FProfile = std::make_unique<TProfileManager>();
    FCompiler = std::make_unique<TPackageCompiler>();
    
    LogToFile(L"=== DxAutoInstaller Started (BUILD: 2025-12-24 v13 - Win32 Classic) ===");
}

TInstaller::~TInstaller()
{
}

void TInstaller::Initialize()
{
    FIDEDetector->Detect();
    FProfile->LoadFromResource();
    
    // Setup compiler output callback
    FCompiler->SetOnOutput([this](const String& line) {
        this->UpdateProgressState(line);
    });
    
    for (int i = 0; i < FIDEDetector->GetCount(); i++)
    {
        DetectThirdPartyComponents(FIDEDetector->GetIDE(i));
    }
}

void TInstaller::OnCompilerOutput(const String& line)
{
    UpdateProgressState(line);
}

//---------------------------------------------------------------------------
// Helper: Find package file with various suffix patterns
//---------------------------------------------------------------------------
String TInstaller::FindPackageFile(const String& packagesDir, 
                                    const String& pkgBaseName, 
                                    const String& ideSuffix)
{
    if (!DirectoryExists(packagesDir))
        return L"";
    
    // DevExpress package naming conventions:
    // New style (25.1+): dxCore370.dpk (suffix = "370" for RS13)
    // Old style: dxCoreRS37.dpk (RS + major version)
    
    // Try new style first: pkgBaseName + ideSuffix + .dpk
    // For RS12: suffix = "290", for RS13: suffix = "370"
    String exactPath = TPath::Combine(packagesDir, pkgBaseName + ideSuffix + L".dpk");
    if (FileExists(exactPath))
        return exactPath;
    
    // Try with "RS" prefix for older naming convention
    // RS12 = RS29, RS13 = RS37
    String rsNum = ideSuffix.SubString(1, ideSuffix.Length() - 1);  // "290" -> "29", "370" -> "37"
    String rsPath = TPath::Combine(packagesDir, pkgBaseName + L"RS" + rsNum + L".dpk");
    if (FileExists(rsPath))
        return rsPath;
    
    // Try with "D" suffix for Delphi-only packages (older convention)
    // dxCoreD29.dpk
    String dPath = TPath::Combine(packagesDir, pkgBaseName + L"D" + rsNum + L".dpk");
    if (FileExists(dPath))
        return dPath;
    
    // No match found
    return L"";
}

void TInstaller::SetInstallFileDir(const String& value)
{
    DoSetInstallFileDir(value);
}

void TInstaller::DoSetInstallFileDir(const String& value)
{
    FInstallFileDir = value;
    
    for (int i = 0; i < FIDEDetector->GetCount(); i++)
    {
        auto ide = FIDEDetector->GetIDE(i);
        BuildComponentList(ide);
        
        // Set default options based on IDE capabilities
        TInstallOptionSet opts;
        
        // IDE registration - 32-bit always, 64-bit off by default
        opts.insert(TInstallOption::RegisterFor32BitIDE);
        // RegisterFor64BitIDE is NOT set by default
        
        // Target platforms
        opts.insert(TInstallOption::CompileWin32Runtime);
        if (ide->SupportsWin64)
            opts.insert(TInstallOption::CompileWin64Runtime);
        if (ide->SupportsWin64Modern)
            opts.insert(TInstallOption::CompileWin64xRuntime);
        
        // Other options
        opts.insert(TInstallOption::AddBrowsingPath);
        opts.insert(TInstallOption::NativeLookAndFeel);
        
        // Generate C++ files by default for RAD Studio and C++Builder
        if (ide->Personality != TIDEPersonality::Delphi)
            opts.insert(TInstallOption::GenerateCppFiles);
            
        FOptions[ide->BDSVersion] = opts;
    }
}

void TInstaller::DetectThirdPartyComponents(const TIDEInfoPtr& ide)
{
    TThirdPartyComponentSet components;
    
    String keyPath = ide->RegistryKey + L"\\Known Packages";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(keyPath))
    {
        std::unique_ptr<TStringList> values(new TStringList());
        reg->GetValueNames(values.get());
        
        for (int i = 0; i < values->Count; i++)
        {
            String fileName = values->Strings[i].LowerCase();
            
            if (fileName.Pos(L"dclib") > 0)
                components.insert(TThirdPartyComponent::IBX);
            else if (fileName.Pos(L"dcltee") > 0)
                components.insert(TThirdPartyComponent::TeeChart);
            else if (fileName.Pos(L"dclfiredac") > 0 || fileName.Pos(L"anydac_") > 0)
                components.insert(TThirdPartyComponent::FireDAC);
            else if (fileName.Pos(L"dclbde") > 0)
                components.insert(TThirdPartyComponent::BDE);
        }
        
        reg->CloseKey();
    }
    
    FThirdPartyComponents[ide->BDSVersion] = components;
}

void TInstaller::BuildComponentList(const TIDEInfoPtr& ide)
{
    TComponentList& list = FComponents[ide->BDSVersion];
    list.clear();
    
    String ideSuffix = TProfileManager::GetIDEVersionNumberStr(ide);
    
    for (const auto& profile : FProfile->GetComponents())
    {
        auto component = std::make_shared<TComponent>(profile);
        
        // Get packages directory
        String packagesDir = TProfileManager::GetComponentPackagesDir(
            FInstallFileDir, profile->ComponentName);
        
        // Create required packages
        for (int i = 0; i < profile->RequiredPackages->Count; i++)
        {
            String pkgBaseName = profile->RequiredPackages->Strings[i];
            String fullPath = FindPackageFile(packagesDir, pkgBaseName, ideSuffix);
                
            if (!fullPath.IsEmpty() && FileExists(fullPath))
            {
                auto pkg = std::make_shared<TPackage>(fullPath);
                pkg->Required = true;
                component->Packages.push_back(pkg);
            }
        }
        
        // Create optional packages
        for (int i = 0; i < profile->OptionalPackages->Count; i++)
        {
            String pkgBaseName = profile->OptionalPackages->Strings[i];
            String fullPath = FindPackageFile(packagesDir, pkgBaseName, ideSuffix);
                
            if (!fullPath.IsEmpty() && FileExists(fullPath))
            {
                auto pkg = std::make_shared<TPackage>(fullPath);
                pkg->Required = false;
                component->Packages.push_back(pkg);
            }
        }
        
        // Set component state
        String compDir = TProfileManager::GetComponentDir(FInstallFileDir, profile->ComponentName);
        if (!DirectoryExists(compDir))
            component->State = TComponentState::NotFound;
        else if (component->GetExistsPackageCount() == 0)
            component->State = TComponentState::NotSupported;
            
        list.push_back(component);
    }
    
    // Build dependencies between components
    for (auto& comp : list)
    {
        for (auto& pkg : comp->Packages)
        {
            for (auto& otherComp : list)
            {
                if (otherComp.get() == comp.get())
                    continue;
                    
                for (auto& otherPkg : otherComp->Packages)
                {
                    if (pkg->Requires->IndexOf(otherPkg->Name) >= 0)
                    {
                        if (pkg->Required)
                        {
                            auto it = std::find(comp->ParentComponents.begin(), 
                                               comp->ParentComponents.end(),
                                               otherComp.get());
                            if (it == comp->ParentComponents.end())
                            {
                                comp->ParentComponents.push_back(otherComp.get());
                                otherComp->SubComponents.push_back(comp.get());
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Update missing state
    for (auto& comp : list)
    {
        if (comp->State == TComponentState::Install && comp->IsMissingDependents())
            comp->State = TComponentState::Missing;
    }
}

const TComponentList& TInstaller::GetComponents(const TIDEInfoPtr& ide) const
{
    static TComponentList empty;
    auto it = FComponents.find(ide->BDSVersion);
    if (it != FComponents.end())
        return it->second;
    return empty;
}

TInstallOptionSet TInstaller::GetOptions(const TIDEInfoPtr& ide) const
{
    auto it = FOptions.find(ide->BDSVersion);
    if (it != FOptions.end())
        return it->second;
    return TInstallOptionSet();
}

void TInstaller::SetOptions(const TIDEInfoPtr& ide, const TInstallOptionSet& options)
{
    TInstallOptionSet opts = options;
    
    // Validate options against IDE capabilities
    if (!ide->SupportsWin64)
        opts.erase(TInstallOption::CompileWin64Runtime);
    if (ide->Personality == TIDEPersonality::Delphi)
        opts.erase(TInstallOption::GenerateCppFiles);
    
    // 32-bit IDE registration is always enabled
    opts.insert(TInstallOption::RegisterFor32BitIDE);
        
    FOptions[ide->BDSVersion] = opts;
}

TThirdPartyComponentSet TInstaller::GetThirdPartyComponents(const TIDEInfoPtr& ide) const
{
    auto it = FThirdPartyComponents.find(ide->BDSVersion);
    if (it != FThirdPartyComponents.end())
        return it->second;
    return TThirdPartyComponentSet();
}

void TInstaller::SetThirdPartyComponents(const TIDEInfoPtr& ide, const TThirdPartyComponentSet& components)
{
    FThirdPartyComponents[ide->BDSVersion] = components;
}

void TInstaller::SetState(TInstallerState value)
{
    if (FState == value)
        return;
    if (value == TInstallerState::Stopped && FState == TInstallerState::Normal)
        return;
        
    FState = value;
    
    switch (FState)
    {
        case TInstallerState::Normal:
            UpdateProgressState(L"Finished!");
            break;
        case TInstallerState::Stopped:
            UpdateProgressState(L"Stopped.");
            break;
        case TInstallerState::Error:
            UpdateProgressState(L"Error.");
            SetState(TInstallerState::Running);
            break;
        default:
            break;
    }
}

void TInstaller::Stop()
{
    LogToFile(L"Stop() called - setting atomic stop flag");
    FStopped.store(true);
    SetState(TInstallerState::Stopped);
}

void TInstaller::CheckStoppedState()
{
    // Thread-safe check using atomic flag
    if (FStopped.load())
    {
        LogToFile(L"CheckStoppedState: Stop requested, aborting...");
        SetState(TInstallerState::Stopped);
        throw EAbort(L"Operation cancelled by user");
    }
}

void TInstaller::UpdateProgress(const TIDEInfoPtr& ide,
                                 const TComponentProfilePtr& component,
                                 const String& task,
                                 const String& target)
{
    if (FOnProgress)
    {
        // Queue UI update to main thread
        TThread::Queue(nullptr, [this, ide, component, task, target]() {
            if (FOnProgress)
                FOnProgress(ide, component, task, target);
        });
    }
}

void TInstaller::UpdateProgressState(const String& stateText)
{
    if (FOnProgressState)
    {
        // Queue UI update to main thread
        TThread::Queue(nullptr, [this, stateText]() {
            if (FOnProgressState)
                FOnProgressState(stateText);
        });
    }
}

String TInstaller::GetInstallLibraryDir(const String& installFileDir,
                                         const TIDEInfoPtr& ide,
                                         TIDEPlatform platform)
{
    // Safety check for empty installFileDir
    if (installFileDir.IsEmpty())
        return L"";
    
    String result = installFileDir + L"\\Library";
    
    if (ide != nullptr)
    {
        String ideSuffix = TProfileManager::GetIDEVersionNumberStr(ide);
        if (!ideSuffix.IsEmpty())
            result = result + L"\\" + ideSuffix;
        
        // All platforms now have their own subfolder
        switch (platform)
        {
            case TIDEPlatform::Win32:
                result = result + L"\\" + PlatformNames::Win32;
                break;
            case TIDEPlatform::Win64:
                result = result + L"\\" + PlatformNames::Win64;
                break;
            case TIDEPlatform::Win64Modern:
                result = result + L"\\" + PlatformNames::Win64Modern;
                break;
        }
    }
    
    return result;
}

String TInstaller::GetInstallSourcesDir(const String& installFileDir)
{
    if (installFileDir.IsEmpty())
        return L"";
    return installFileDir + L"\\Library\\Sources";
}

void TInstaller::CopySourceFiles(const String& sourceDir, const String& destDir)
{
    // Copy ALL files (legacy behavior for compatibility)
    std::set<String> allExtensions;  // Empty set = copy all
    CopySourceFilesFiltered(sourceDir, destDir, allExtensions);
}

void TInstaller::CopySourceFilesFiltered(const String& sourceDir, const String& destDir,
                                          const std::set<String>& extensions)
{
    LogToFile(L"CopySourceFilesFiltered: src=[" + sourceDir + L"] dst=[" + destDir + L"]");
    
    if (!DirectoryExists(sourceDir))
    {
        LogToFile(L"  Source dir does not exist, skipping");
        return;
    }
    
    if (destDir.IsEmpty())
    {
        LogToFile(L"  ERROR: Empty destination directory!");
        return;
    }
    
    try
    {
        if (!ForceDirectories(destDir))
        {
            LogToFile(L"  ERROR: ForceDirectories returned false for: " + destDir);
            return;
        }
    }
    catch (Exception& e)
    {
        LogToFile(L"  EXCEPTION in ForceDirectories: " + e.Message);
        throw;
    }
    
    TSearchRec sr;
    if (FindFirst(sourceDir + L"\\*.*", faAnyFile, sr) == 0)
    {
        do
        {
            if (sr.Name == L"." || sr.Name == L"..")
                continue;
                
            String srcPath = sourceDir + L"\\" + sr.Name;
            String dstPath = destDir + L"\\" + sr.Name;
            
            // Skip directories - don't copy subdirectories like "Icon Library"
            // They remain in original location and are added to browsing path instead
            if ((sr.Attr & faDirectory) != 0)
                continue;
            
            // If extensions set is empty, copy all files
            // Otherwise, only copy files with matching extensions
            if (extensions.empty())
            {
                CopyFile(srcPath.c_str(), dstPath.c_str(), FALSE);
            }
            else
            {
                String ext = ExtractFileExt(sr.Name).LowerCase();
                if (extensions.count(ext) > 0)
                {
                    CopyFile(srcPath.c_str(), dstPath.c_str(), FALSE);
                }
            }
        } while (FindNext(sr) == 0);
        
        FindClose(sr);
    }
}

void TInstaller::DeleteCompiledFiles(const String& dir, const std::set<String>& extensions)
{
    LogToFile(L"DeleteCompiledFiles: dir=[" + dir + L"]");
    
    if (!DirectoryExists(dir))
        return;
    
    TSearchRec sr;
    if (FindFirst(dir + L"\\*.*", faAnyFile, sr) == 0)
    {
        do
        {
            if (sr.Name == L"." || sr.Name == L"..")
                continue;
                
            String fullPath = dir + L"\\" + sr.Name;
            
            if ((sr.Attr & faDirectory) != 0)
            {
                // Recurse into subdirectories
                DeleteCompiledFiles(fullPath, extensions);
            }
            else
            {
                String ext = ExtractFileExt(sr.Name).LowerCase();
                if (extensions.count(ext) > 0)
                {
                    LogToFile(L"  Deleting: " + fullPath);
                    DeleteFile(fullPath.c_str());
                }
            }
        } while (FindNext(sr) == 0);
        
        FindClose(sr);
    }
}

void TInstaller::CleanupLibraryDir(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    String libDir = GetInstallLibraryDir(FInstallFileDir, ide, platform);
    if (libDir.IsEmpty() || !DirectoryExists(libDir))
        return;
    
    LogToFile(L"CleanupLibraryDir: [" + libDir + L"]");
    UpdateProgressState(L"Cleaning: " + libDir);
    
    // Extensions to delete from library directories
    std::set<String> compiledExtensions;
    compiledExtensions.insert(L".dcu");   // Delphi compiled units
    compiledExtensions.insert(L".hpp");   // C++ headers
    compiledExtensions.insert(L".obj");   // Object files
    compiledExtensions.insert(L".o");     // Object files (Clang)
    compiledExtensions.insert(L".a");     // Import libraries (Win64x)
    compiledExtensions.insert(L".lib");   // Import libraries (Win32/Win64)
    compiledExtensions.insert(L".bpi");   // C++Builder package import
    compiledExtensions.insert(L".res");   // Compiled resources (generated)
    
    DeleteCompiledFiles(libDir, compiledExtensions);
}

void TInstaller::CleanupAllCompiledFiles(const TIDEInfoPtr& ide)
{
    LogToFile(L"=== CleanupAllCompiledFiles for IDE: " + ide->Name + L" ===");
    LogToFile(L"  IDE BDSVersion: " + ide->BDSVersion);
    LogToFile(L"  InstallFileDir: " + FInstallFileDir);
    
    // Delete entire Library\{ver} directory (contains Win32, Win64, Win64x subfolders)
    // This is simpler and cleaner than deleting files individually
    String ideSuffix = TProfileManager::GetIDEVersionNumberStr(ide);
    if (!ideSuffix.IsEmpty() && !FInstallFileDir.IsEmpty())
    {
        String libVerDir = FInstallFileDir + L"\\Library\\" + ideSuffix;
        if (DirectoryExists(libVerDir))
        {
            LogToFile(L"Deleting entire library directory: " + libVerDir);
            UpdateProgressState(L"Deleting: " + libVerDir);
            
            // Use TDirectory::Delete with recursive flag
            try
            {
                TDirectory::Delete(libVerDir, true);
                LogToFile(L"  Successfully deleted: " + libVerDir);
            }
            catch (Exception& e)
            {
                LogToFile(L"  Failed to delete: " + e.Message);
            }
        }
        else
        {
            LogToFile(L"  Library directory does not exist: " + libVerDir);
        }
    }
    
    // Cleanup BPL directories - only delete DevExpress files (shared with other packages)
    LogToFile(L"=== Cleaning BPL directories ===");
    std::set<String> bplExtensions;
    bplExtensions.insert(L".bpl");
    bplExtensions.insert(L".lib");
    bplExtensions.insert(L".bpi");
    bplExtensions.insert(L".map");
    bplExtensions.insert(L".a");
    
    String bplDir32 = ide->GetBPLOutputPath(TIDEPlatform::Win32);
    String bplDir64 = ide->GetBPLOutputPath(TIDEPlatform::Win64);
    String bplDir64x = ide->GetBPLOutputPath(TIDEPlatform::Win64Modern);
    
    LogToFile(L"  BPL Win32 path: " + bplDir32);
    LogToFile(L"  BPL Win64 path: " + bplDir64);
    LogToFile(L"  BPL Win64x path: " + bplDir64x);
    
    DeleteDevExpressFilesFromDir(bplDir32, bplExtensions);
    DeleteDevExpressFilesFromDir(bplDir64, bplExtensions);
    DeleteDevExpressFilesFromDir(bplDir64x, bplExtensions);
    
    // Cleanup DCP directories - only delete DevExpress files
    LogToFile(L"=== Cleaning DCP directories ===");
    std::set<String> dcpExtensions;
    dcpExtensions.insert(L".dcp");
    dcpExtensions.insert(L".bpi");
    dcpExtensions.insert(L".lib");
    dcpExtensions.insert(L".a");
    dcpExtensions.insert(L".obj");
    dcpExtensions.insert(L".o");
    
    String dcpDir32 = ide->GetDCPOutputPath(TIDEPlatform::Win32);
    String dcpDir64 = ide->GetDCPOutputPath(TIDEPlatform::Win64);
    String dcpDir64x = ide->GetDCPOutputPath(TIDEPlatform::Win64Modern);
    
    LogToFile(L"  DCP Win32 path: " + dcpDir32);
    LogToFile(L"  DCP Win64 path: " + dcpDir64);
    LogToFile(L"  DCP Win64x path: " + dcpDir64x);
    
    DeleteDevExpressFilesFromDir(dcpDir32, dcpExtensions);
    DeleteDevExpressFilesFromDir(dcpDir64, dcpExtensions);
    DeleteDevExpressFilesFromDir(dcpDir64x, dcpExtensions);
    
    // Cleanup HPP directories - only delete DevExpress files
    LogToFile(L"=== Cleaning HPP directories ===");
    std::set<String> hppExtensions;
    hppExtensions.insert(L".hpp");
    
    String hppDir32 = ide->GetHPPOutputPath(TIDEPlatform::Win32);
    String hppDir64 = ide->GetHPPOutputPath(TIDEPlatform::Win64);
    String hppDir64x = ide->GetHPPOutputPath(TIDEPlatform::Win64Modern);
    
    LogToFile(L"  HPP Win32 path: " + hppDir32);
    LogToFile(L"  HPP Win64 path: " + hppDir64);
    LogToFile(L"  HPP Win64x path: " + hppDir64x);
    
    DeleteDevExpressFilesFromDir(hppDir32, hppExtensions);
    DeleteDevExpressFilesFromDir(hppDir64, hppExtensions);
    DeleteDevExpressFilesFromDir(hppDir64x, hppExtensions);
    
    LogToFile(L"=== CleanupAllCompiledFiles completed ===");
}

void TInstaller::DeleteDevExpressFilesFromDir(const String& dir, const std::set<String>& extensions)
{
    LogToFile(L"DeleteDevExpressFilesFromDir: [" + dir + L"]");
    
    if (dir.IsEmpty())
    {
        LogToFile(L"  ERROR: Empty directory path!");
        return;
    }
    
    if (!DirectoryExists(dir))
    {
        LogToFile(L"  Directory does not exist, skipping");
        return;
    }
    
    LogToFile(L"  Extensions to delete: ");
    for (const auto& ext : extensions)
    {
        LogToFile(L"    " + ext);
    }
    
    int deletedCount = 0;
    int skippedCount = 0;
    
    TSearchRec sr;
    if (FindFirst(dir + L"\\*.*", faAnyFile, sr) == 0)
    {
        do
        {
            if (sr.Name == L"." || sr.Name == L"..")
                continue;
            
            if ((sr.Attr & faDirectory) != 0)
                continue;  // Skip directories
                
            String lowerName = sr.Name.LowerCase();
            String ext = ExtractFileExt(sr.Name).LowerCase();
            
            // Check if this is a DevExpress file by prefix
            // DevExpress packages start with: dx, cx, dcldx, dclcx
            bool isDevExpress = 
                lowerName.Pos(L"dx") == 1 ||      // dxCore, dxBar, etc.
                lowerName.Pos(L"cx") == 1 ||      // cxGrid, cxEdit, etc.
                lowerName.Pos(L"dcldx") == 1 ||   // design-time packages (dcldxCore)
                lowerName.Pos(L"dclcx") == 1;     // design-time packages (dclcxGrid)
            
            if (isDevExpress)
            {
                if (extensions.count(ext) > 0)
                {
                    String fullPath = dir + L"\\" + sr.Name;
                    LogToFile(L"  Deleting: " + fullPath);
                    if (DeleteFile(fullPath.c_str()))
                    {
                        deletedCount++;
                    }
                    else
                    {
                        LogToFile(L"    FAILED to delete: " + fullPath);
                    }
                }
                else
                {
                    LogToFile(L"  Skipping (wrong ext): " + sr.Name + L" [ext=" + ext + L"]");
                    skippedCount++;
                }
            }
        } while (FindNext(sr) == 0);
        
        FindClose(sr);
    }
    else
    {
        LogToFile(L"  FindFirst failed or directory is empty");
    }
    
    LogToFile(L"  Deleted: " + String(deletedCount) + L" files, Skipped: " + String(skippedCount) + L" files");
}

void TInstaller::Install(const std::vector<TIDEInfoPtr>& ides)
{
    LogToFile(L"=== Install started (sync) ===");
    FStopped.store(false);  // Reset stop flag
    SetState(TInstallerState::Running);
    
    bool success = true;
    String errorMessage;
    
    for (const auto& ide : ides)
    {
        try
        {
            CheckStoppedState();
            InstallIDE(ide);
        }
        catch (const EAbort&)
        {
            LogToFile(L"EAbort exception caught");
            success = false;
            errorMessage = L"Operation cancelled by user";
            break;
        }
        catch (Exception& e)
        {
            LogToFile(L"EXCEPTION: " + e.Message);
            success = false;
            errorMessage = e.Message;
            throw;
        }
    }
    
    LogToFile(L"=== Install completed ===");
    SetState(TInstallerState::Normal);
    
    if (FOnComplete)
    {
        TThread::Queue(nullptr, [this, success, errorMessage]() {
            if (FOnComplete)
                FOnComplete(success, errorMessage);
        });
    }
}

void TInstaller::InstallAsync(const std::vector<TIDEInfoPtr>& ides)
{
    LogToFile(L"=== InstallAsync started ===");
    FStopped.store(false);  // Reset stop flag
    SetState(TInstallerState::Running);
    
    // Run installation in background thread
    TTask::Run([this, ides]() {
        bool success = true;
        String errorMessage;
        
        try
        {
            for (const auto& ide : ides)
            {
                CheckStoppedState();
                InstallIDE(ide);
            }
            LogToFile(L"=== InstallAsync completed successfully ===");
        }
        catch (const EAbort&)
        {
            LogToFile(L"InstallAsync: EAbort exception caught");
            success = false;
            errorMessage = L"Operation cancelled by user";
        }
        catch (Exception& e)
        {
            LogToFile(L"InstallAsync EXCEPTION: " + e.Message);
            success = false;
            errorMessage = e.Message;
        }
        
        // Update state and notify completion on main thread
        TThread::Queue(nullptr, [this, success, errorMessage]() {
            SetState(TInstallerState::Normal);
            if (FOnComplete)
                FOnComplete(success, errorMessage);
        });
    });
}

void TInstaller::Uninstall(const std::vector<TIDEInfoPtr>& ides, const TUninstallOptions& uninstallOpts)
{
    LogToFile(L"=== Uninstall started (sync) ===");
    LogToFile(L"  Uninstall32BitIDE: " + String(uninstallOpts.Uninstall32BitIDE ? L"true" : L"false"));
    LogToFile(L"  Uninstall64BitIDE: " + String(uninstallOpts.Uninstall64BitIDE ? L"true" : L"false"));
    
    FStopped.store(false);  // Reset stop flag
    SetState(TInstallerState::Running);
    
    bool success = true;
    String errorMessage;
    
    for (const auto& ide : ides)
    {
        try
        {
            CheckStoppedState();
            UninstallIDE(ide, uninstallOpts);
        }
        catch (const EAbort&)
        {
            LogToFile(L"EAbort exception caught during uninstall");
            success = false;
            errorMessage = L"Operation cancelled by user";
            break;
        }
        catch (Exception& e)
        {
            LogToFile(L"EXCEPTION during uninstall: " + e.Message);
            success = false;
            errorMessage = e.Message;
            throw;
        }
    }
    
    LogToFile(L"=== Uninstall completed ===");
    SetState(TInstallerState::Normal);
    
    if (FOnComplete)
    {
        TThread::Queue(nullptr, [this, success, errorMessage]() {
            if (FOnComplete)
                FOnComplete(success, errorMessage);
        });
    }
}

void TInstaller::UninstallAsync(const std::vector<TIDEInfoPtr>& ides, const TUninstallOptions& uninstallOpts)
{
    LogToFile(L"=== UninstallAsync started ===");
    LogToFile(L"  Uninstall32BitIDE: " + String(uninstallOpts.Uninstall32BitIDE ? L"true" : L"false"));
    LogToFile(L"  Uninstall64BitIDE: " + String(uninstallOpts.Uninstall64BitIDE ? L"true" : L"false"));
    
    FStopped.store(false);  // Reset stop flag
    SetState(TInstallerState::Running);
    
    // Run uninstallation in background thread
    TTask::Run([this, ides, uninstallOpts]() {
        bool success = true;
        String errorMessage;
        
        try
        {
            for (const auto& ide : ides)
            {
                CheckStoppedState();
                UninstallIDE(ide, uninstallOpts);
            }
            LogToFile(L"=== UninstallAsync completed successfully ===");
        }
        catch (const EAbort&)
        {
            LogToFile(L"UninstallAsync: EAbort exception caught");
            success = false;
            errorMessage = L"Operation cancelled by user";
        }
        catch (Exception& e)
        {
            LogToFile(L"UninstallAsync EXCEPTION: " + e.Message);
            success = false;
            errorMessage = e.Message;
        }
        
        // Update state and notify completion on main thread
        TThread::Queue(nullptr, [this, success, errorMessage]() {
            SetState(TInstallerState::Normal);
            if (FOnComplete)
                FOnComplete(success, errorMessage);
        });
    });
}

void TInstaller::InstallIDE(const TIDEInfoPtr& ide)
{
    // Debug output to file
    LogToFile(L"=== Starting installation for " + ide->Name + L" ===");
    LogToFile(L"InstallFileDir: [" + FInstallFileDir + L"]");
    LogToFile(L"IDE RegistryKey: [" + ide->RegistryKey + L"]");
    LogToFile(L"IDE BDSVersion: [" + ide->BDSVersion + L"]");
    LogToFile(L"IDE RootDir: [" + ide->RootDir + L"]");
    
    // Debug output to UI
    UpdateProgressState(L"=== Starting installation for " + ide->Name + L" ===");
    UpdateProgressState(L"InstallFileDir: " + FInstallFileDir);
    UpdateProgressState(L"IDE RegistryKey: " + ide->RegistryKey);
    UpdateProgressState(L"IDE BDSVersion: " + ide->BDSVersion);
    
    // First uninstall existing - clean both 32 and 64-bit registrations
    LogToFile(L"Calling UninstallIDE (cleanup)...");
    TUninstallOptions cleanupOpts;
    cleanupOpts.Uninstall32BitIDE = true;
    cleanupOpts.Uninstall64BitIDE = true;
    cleanupOpts.DeleteCompiledFiles = true;
    UninstallIDE(ide, cleanupOpts);
    LogToFile(L"UninstallIDE completed");
    
    TInstallOptionSet opts = GetOptions(ide);
    String installSourcesDir = GetInstallSourcesDir(FInstallFileDir);
    
    LogToFile(L"InstallSourcesDir: [" + installSourcesDir + L"]");
    UpdateProgressState(L"InstallSourcesDir: " + installSourcesDir);
    
    // Get DevExpress build number for version-specific fixes
    unsigned int dxBuildNumber = TProfileManager::GetDxBuildNumber(FInstallFileDir);
    
    const TComponentList& components = GetComponents(ide);
    
    // Get options from UI checkboxes
    bool registerFor32BitIDE = opts.count(TInstallOption::RegisterFor32BitIDE) > 0;
    bool registerFor64BitIDE = opts.count(TInstallOption::RegisterFor64BitIDE) > 0;
    bool compileWin32 = opts.count(TInstallOption::CompileWin32Runtime) > 0;
    bool compileWin64 = opts.count(TInstallOption::CompileWin64Runtime) > 0 && ide->SupportsWin64;
    bool compileWin64x = opts.count(TInstallOption::CompileWin64xRuntime) > 0 && ide->SupportsWin64Modern;
    bool generateCppFiles = opts.count(TInstallOption::GenerateCppFiles) > 0 && 
                            ide->Personality != TIDEPersonality::Delphi;
    
    // Win32 must be compiled for 32-bit IDE design-time packages
    if (registerFor32BitIDE && !compileWin32)
        compileWin32 = true;
    
    // Win64 must be compiled for 64-bit IDE design-time packages
    if (registerFor64BitIDE && !compileWin64 && ide->SupportsWin64)
        compileWin64 = true;
    
    // Log compilation options
    LogToFile(L"=== Compilation Options ===");
    LogToFile(L"registerFor32BitIDE: " + String(registerFor32BitIDE ? L"true" : L"false"));
    LogToFile(L"registerFor64BitIDE: " + String(registerFor64BitIDE ? L"true" : L"false"));
    LogToFile(L"compileWin32: " + String(compileWin32 ? L"true" : L"false"));
    LogToFile(L"compileWin64: " + String(compileWin64 ? L"true" : L"false"));
    LogToFile(L"compileWin64x: " + String(compileWin64x ? L"true" : L"false"));
    LogToFile(L"generateCppFiles: " + String(generateCppFiles ? L"true" : L"false"));
    LogToFile(L"IDE SupportsWin64: " + String(ide->SupportsWin64 ? L"true" : L"false"));
    LogToFile(L"IDE SupportsWin64Modern: " + String(ide->SupportsWin64Modern ? L"true" : L"false"));
    LogToFile(L"IDE Personality: " + String(ide->Personality == TIDEPersonality::Delphi ? L"Delphi" : 
                                            (ide->Personality == TIDEPersonality::CppBuilder ? L"CppBuilder" : L"RADStudio")));
    
    // ========================================
    // Phase 1: Copy source files to Library\Sources
    // ========================================
    
    // Define extensions for source files (go to Library\Sources)
    std::set<String> sourceExtensions;
    sourceExtensions.insert(L".pas");
    sourceExtensions.insert(L".inc");
    sourceExtensions.insert(L".dfm");
    sourceExtensions.insert(L".fmx");
    sourceExtensions.insert(L".res");
    sourceExtensions.insert(L".dcr");
    
    // Define extensions for resource files (needed in Library\{ver}\* for compiler)
    // Note: .dfm files are NOT needed here - compiler finds them via Search Path in Library\Sources
    // Note: .dcr files are NOT needed here - they stay in Library\Sources
    std::set<String> resourceExtensions;
    resourceExtensions.insert(L".res");
    
    for (const auto& comp : components)
    {
        if (comp->State != TComponentState::Install)
            continue;
            
        String sourcesDir = TProfileManager::GetComponentSourcesDir(
            FInstallFileDir, comp->Profile->ComponentName);
            
        UpdateProgress(ide, comp->Profile, L"Copying", L"Source Files");
        UpdateProgressState(L"Copying sources: " + sourcesDir);
        
        // Copy ALL source files to Library\Sources (one location for all)
        CopySourceFilesFiltered(sourcesDir, installSourcesDir, sourceExtensions);
        
        // Copy resource files to platform-specific library dirs
        if (compileWin32)
        {
            String libDir32 = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win32);
            CopySourceFilesFiltered(sourcesDir, libDir32, resourceExtensions);
        }
        
        if (compileWin64)
        {
            String libDir64 = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64);
            CopySourceFilesFiltered(sourcesDir, libDir64, resourceExtensions);
        }
        
        // For Win64x - create directory for compiled files
        if (compileWin64x)
        {
            String libDir64x = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64Modern);
            ForceDirectories(libDir64x);
        }
        
        // Fix for DevExpress version >= 18.2.x
        if (dxBuildNumber >= 20180200 && comp->Profile->ComponentName == L"ExpressLibrary")
        {
            String libDir32 = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win32);
            for (const auto& profile : FProfile->GetComponents())
            {
                String compSourcesDir = TProfileManager::GetComponentSourcesDir(
                    FInstallFileDir, profile->ComponentName);
                String compPackagesDir = TProfileManager::GetComponentPackagesDir(
                    FInstallFileDir, profile->ComponentName);
                    
                if (DirectoryExists(compSourcesDir) && !DirectoryExists(compPackagesDir))
                {
                    UpdateProgressState(L"Copying (18.2+ fix): " + compSourcesDir);
                    CopySourceFilesFiltered(compSourcesDir, installSourcesDir, sourceExtensions);
                    if (compileWin32)
                        CopySourceFilesFiltered(compSourcesDir, libDir32, resourceExtensions);
                    if (compileWin64)
                        CopySourceFilesFiltered(compSourcesDir, GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64), resourceExtensions);
                }
            }
            
            String pageControlDir = TProfileManager::GetComponentSourcesDir(FInstallFileDir, L"ExpressPageControl");
            if (DirectoryExists(pageControlDir))
            {
                CopySourceFilesFiltered(pageControlDir, installSourcesDir, sourceExtensions);
                if (compileWin32)
                    CopySourceFilesFiltered(pageControlDir, libDir32, resourceExtensions);
                if (compileWin64)
                    CopySourceFilesFiltered(pageControlDir, GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64), resourceExtensions);
            }
        }
    }
    
    // ========================================
    // Phase 2: Compile REQUIRED packages
    // ========================================
    for (const auto& comp : components)
    {
        if (comp->State != TComponentState::Install)
            continue;
            
        for (const auto& pkg : comp->Packages)
        {
            if (!pkg->Required)
                continue;
                
            if (compileWin32)
                CompilePackage(ide, TIDEPlatform::Win32, comp, pkg);
            
            if (compileWin64)
                CompilePackage(ide, TIDEPlatform::Win64, comp, pkg);
            
            // Win64x (Modern) - compile separately with -jf:coffi flag
            if (compileWin64x)
                CompilePackage(ide, TIDEPlatform::Win64Modern, comp, pkg);
        }
    }
    
    // ========================================
    // Phase 3: Compile OPTIONAL packages
    // ========================================
    for (const auto& comp : components)
    {
        if (comp->State != TComponentState::Install)
            continue;
            
        for (const auto& pkg : comp->Packages)
        {
            if (pkg->Required)
                continue;
                
            if (compileWin32)
                CompilePackage(ide, TIDEPlatform::Win32, comp, pkg);
            
            if (compileWin64)
                CompilePackage(ide, TIDEPlatform::Win64, comp, pkg);
            
            // Win64x (Modern) - compile separately with -jf:coffi flag
            if (compileWin64x)
                CompilePackage(ide, TIDEPlatform::Win64Modern, comp, pkg);
        }
    }
    
    // ========================================
    // Phase 4: Register design-time packages
    // ========================================
    RegisterDesignTimePackages(ide, TIDEPlatform::Win32, registerFor32BitIDE, false);
    if (registerFor64BitIDE && compileWin64)
        RegisterDesignTimePackages(ide, TIDEPlatform::Win64, false, registerFor64BitIDE);
    
    // ========================================
    // Phase 5: Add library paths
    // ========================================
    String iconLibraryDir = FInstallFileDir + L"\\ExpressLibrary\\Sources\\Icon Library";
    bool hasIconLibrary = DirectoryExists(iconLibraryDir);
    
    if (compileWin32)
        AddLibraryPaths(ide, TIDEPlatform::Win32);
    
    if (compileWin64)
        AddLibraryPaths(ide, TIDEPlatform::Win64);
    
    if (compileWin64x)
        AddLibraryPaths(ide, TIDEPlatform::Win64Modern);
    
    // Set environment variable
    SetEnvironmentVariable(ide, DX_ENV_VARIABLE, FInstallFileDir);
    
    LogToFile(L"=== Installation completed for " + ide->Name + L" ===");
}
void TInstaller::CompilePackage(const TIDEInfoPtr& ide,
                                 TIDEPlatform platform,
                                 const TComponentPtr& component,
                                 const TPackagePtr& package)
{
    CheckStoppedState();
    
    if (!package->Exists)
        return;
        
    // Check third-party dependencies
    TThirdPartyComponentSet tpc = GetThirdPartyComponents(ide);
    switch (package->Category)
    {
        case TPackageCategory::IBX:
            if (tpc.count(TThirdPartyComponent::IBX) == 0) return;
            break;
        case TPackageCategory::TeeChart:
            if (tpc.count(TThirdPartyComponent::TeeChart) == 0) return;
            break;
        case TPackageCategory::FireDAC:
            if (tpc.count(TThirdPartyComponent::FireDAC) == 0) return;
            break;
        case TPackageCategory::BDE:
            if (tpc.count(TThirdPartyComponent::BDE) == 0) return;
            if (platform != TIDEPlatform::Win32) return;  // BDE only for Win32
            break;
        default:
            break;
    }
    
    // Check platform support
    if (!TPackageCompiler::IsPlatformSupported(ide, platform))
        return;
    
    // Check if optional package dependencies are satisfied
    if (!package->Required)
    {
        // TODO: Check DependentComponents if needed
    }
    
    String platformName;
    switch (platform)
    {
        case TIDEPlatform::Win32: platformName = L"Win32"; break;
        case TIDEPlatform::Win64: platformName = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformName = L"Win64x"; break;
        default: platformName = L"Unknown"; break;
    }
    
    LogToFile(L"InstallPackage: " + platformName + L" > " + package->Name);
    LogToFile(L"  Package Usage: " + String(package->Usage == TPackageUsage::RuntimeOnly ? L"RuntimeOnly" : 
                                            (package->Usage == TPackageUsage::DesigntimeOnly ? L"DesigntimeOnly" : L"DesigntimeAndRuntime")));
    LogToFile(L"  Package Description: [" + package->Description + L"]");
    
    UpdateProgress(ide, component->Profile, 
        L"Install Package",
        platformName + L" > " + package->Name);
    
    // Setup compile options
    TCompileOptions options;
    options.PackagePath = package->FullFileName;
    options.BPLOutputDir = ide->GetBPLOutputPath(platform);
    options.DCPOutputDir = ide->GetDCPOutputPath(platform);
    options.UnitOutputDir = GetInstallLibraryDir(FInstallFileDir, ide, platform);
    
    LogToFile(L"  BPLOutputDir: [" + options.BPLOutputDir + L"]");
    LogToFile(L"  DCPOutputDir: [" + options.DCPOutputDir + L"]");
    LogToFile(L"  UnitOutputDir: [" + options.UnitOutputDir + L"]");
    
    // Debug output - show paths
    UpdateProgressState(L"BPL Output: " + options.BPLOutputDir);
    UpdateProgressState(L"DCP Output: " + options.DCPOutputDir);
    UpdateProgressState(L"Unit Output: " + options.UnitOutputDir);
    
    // Safety check - all paths must be valid
    if (options.BPLOutputDir.IsEmpty() || options.DCPOutputDir.IsEmpty() || options.UnitOutputDir.IsEmpty())
    {
        UpdateProgressState(L"ERROR: Invalid output paths for " + package->Name);
        UpdateProgressState(L"FInstallFileDir = " + FInstallFileDir);
        return;
    }
    
    options.SearchPaths->Add(GetInstallSourcesDir(FInstallFileDir));
    options.SearchPaths->Add(options.DCPOutputDir);
    // Also add library output dir to search path for finding compiled DCUs
    options.SearchPaths->Add(options.UnitOutputDir);
    
    TInstallOptionSet instOpts = GetOptions(ide);
    options.NativeLookAndFeel = instOpts.count(TInstallOption::NativeLookAndFeel) > 0;
    options.GenerateCppFiles = instOpts.count(TInstallOption::GenerateCppFiles) > 0;
    
    // Ensure output directories exist
    if (!options.BPLOutputDir.IsEmpty())
        ForceDirectories(options.BPLOutputDir);
    if (!options.DCPOutputDir.IsEmpty())
        ForceDirectories(options.DCPOutputDir);
    if (!options.UnitOutputDir.IsEmpty())
        ForceDirectories(options.UnitOutputDir);
    
    // Compile - use actual platform (dcc64x for Win64Modern)
    TCompileResult result = FCompiler->Compile(ide, platform, options);
    
    if (result.Success)
    {
        // Fix for DevExpress 18.2.x: dxSkinXxxxx.bpl should be placed in library install directory
        if (package->Name.SubString(1, 6) == L"dxSkin" && package->Name.Length() > 6)
        {
            wchar_t ch = package->Name[7];
            if (ch >= L'A' && ch <= L'Z')
            {
                String srcBpl = TPath::Combine(options.BPLOutputDir, package->Name + L".bpl");
                String dstBpl = TPath::Combine(options.UnitOutputDir, package->Name + L".bpl");
                if (FileExists(srcBpl))
                    CopyFile(srcBpl.c_str(), dstBpl.c_str(), FALSE);
            }
        }
        
        // Log what was generated
        String libPath = TPath::Combine(options.DCPOutputDir, package->Name + L".lib");
        String aPath = TPath::Combine(options.DCPOutputDir, package->Name + L".a");
        LogToFile(L"  .lib exists: " + String(FileExists(libPath) ? L"yes" : L"no"));
        LogToFile(L"  .a exists: " + String(FileExists(aPath) ? L"yes" : L"no"));
        
        LogToFile(L"  Compilation successful");
    }
    else
    {
        UpdateProgressState(L"COMPILE ERROR: " + package->Name);
        if (!result.ErrorMessage.IsEmpty())
            UpdateProgressState(result.ErrorMessage);
        SetState(TInstallerState::Error);
    }
}

//---------------------------------------------------------------------------
// Register design-time packages
//---------------------------------------------------------------------------
void TInstaller::RegisterDesignTimePackages(const TIDEInfoPtr& ide,
                                             TIDEPlatform platform,
                                             bool for32BitIDE,
                                             bool for64BitIDE)
{
    if (!for32BitIDE && !for64BitIDE)
        return;
    
    LogToFile(L"=== Registering design-time packages ===");
    LogToFile(L"  Platform: " + String(platform == TIDEPlatform::Win64 ? L"Win64" : L"Win32"));
    LogToFile(L"  for32BitIDE: " + String(for32BitIDE ? L"true" : L"false"));
    LogToFile(L"  for64BitIDE: " + String(for64BitIDE ? L"true" : L"false"));
    
    String bplDir = ide->GetBPLOutputPath(platform);
    const TComponentList& components = GetComponents(ide);
    
    for (const auto& comp : components)
    {
        if (comp->State != TComponentState::Install)
            continue;
        
        for (const auto& pkg : comp->Packages)
        {
            if (pkg->Usage == TPackageUsage::RuntimeOnly)
                continue;
            
            String bplPath = TPath::Combine(bplDir, pkg->Name + L".bpl");
            if (!FileExists(bplPath))
                continue;
            
            if (for32BitIDE)
            {
                LogToFile(L"  Registering for 32-bit IDE: " + pkg->Name);
                RegisterPackage(ide, bplPath, pkg->Description, false);
            }
            
            if (for64BitIDE)
            {
                LogToFile(L"  Registering for 64-bit IDE: " + pkg->Name);
                RegisterPackage(ide, bplPath, pkg->Description, true);
            }
        }
    }
}

//---------------------------------------------------------------------------
// Add library paths for platform
//---------------------------------------------------------------------------
void TInstaller::AddLibraryPaths(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    TInstallOptionSet opts = GetOptions(ide);
    String installSourcesDir = GetInstallSourcesDir(FInstallFileDir);
    String libDir = GetInstallLibraryDir(FInstallFileDir, ide, platform);
    String iconLibraryDir = FInstallFileDir + L"\\ExpressLibrary\\Sources\\Icon Library";
    bool hasIconLibrary = DirectoryExists(iconLibraryDir);
    bool generateCppFiles = opts.count(TInstallOption::GenerateCppFiles) > 0 && 
                            ide->Personality != TIDEPersonality::Delphi;
    
    LogToFile(L"AddLibraryPaths for platform: " + String(
        platform == TIDEPlatform::Win32 ? L"Win32" : 
        platform == TIDEPlatform::Win64 ? L"Win64" : L"Win64x"));
    LogToFile(L"  libDir: " + libDir);
    LogToFile(L"  installSourcesDir: " + installSourcesDir);
    LogToFile(L"  generateCppFiles: " + String(generateCppFiles ? L"true" : L"false"));
    
    // Add library search path (for .dcu files)
    AddToLibraryPath(ide, platform, libDir, false);
    
    // For Win64x, also add DCP path where .lib/.a files are located
    if (platform == TIDEPlatform::Win64Modern)
    {
        String dcpDir = ide->GetDCPOutputPath(platform);
        AddToLibraryPath(ide, platform, dcpDir, false);
    }
    
    // Add browsing/search path for sources
    if (opts.count(TInstallOption::AddBrowsingPath) > 0)
    {
        AddToLibraryPath(ide, platform, installSourcesDir, true);
        if (hasIconLibrary)
            AddToLibraryPath(ide, platform, iconLibraryDir, true);
    }
    else
    {
        AddToLibraryPath(ide, platform, installSourcesDir, false);
    }
    
    // Add C++ specific paths for RAD Studio and C++Builder
    if (generateCppFiles)
    {
        // Add .hpp path to C++ System Include Path
        // HPP files are in Library\{ver}\{platform} directory (same as .dcu)
        AddToCppIncludePath(ide, platform, libDir);
        
        // Also add sources to C++ Include Path for inline implementations
        AddToCppIncludePath(ide, platform, installSourcesDir);
        
        // Add DCP directory to C++ Library Path for .lib/.a files
        String dcpDir = ide->GetDCPOutputPath(platform);
        AddToCppPath(ide, platform, dcpDir, false);
        
        LogToFile(L"  Added C++ paths:");
        LogToFile(L"    IncludePath: " + libDir);
        LogToFile(L"    IncludePath: " + installSourcesDir);
        LogToFile(L"    LibraryPath: " + dcpDir);
    }
}

//---------------------------------------------------------------------------
// Remove library paths for platform
//---------------------------------------------------------------------------
void TInstaller::RemoveLibraryPaths(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    String prevInstallDir = GetEnvironmentVariable(ide, DX_ENV_VARIABLE);
    if (prevInstallDir.IsEmpty())
        return;
    
    LogToFile(L"RemoveLibraryPaths for platform: " + String(
        platform == TIDEPlatform::Win32 ? L"Win32" : 
        platform == TIDEPlatform::Win64 ? L"Win64" : L"Win64x"));
    LogToFile(L"  prevInstallDir: " + prevInstallDir);
    
    String sourcesDir = GetInstallSourcesDir(prevInstallDir);
    String libDir = GetInstallLibraryDir(prevInstallDir, ide, platform);
    String iconLibraryDir = prevInstallDir + L"\\ExpressLibrary\\Sources\\Icon Library";
    
    // Remove Delphi library paths (always - for all IDE types)
    RemoveFromLibraryPath(ide, platform, libDir, false);
    RemoveFromLibraryPath(ide, platform, sourcesDir, false);
    RemoveFromLibraryPath(ide, platform, sourcesDir, true);
    RemoveFromLibraryPath(ide, platform, iconLibraryDir, true);
    
    // For Win64x, also remove DCP path from Delphi paths
    if (platform == TIDEPlatform::Win64Modern)
    {
        String dcpDir = ide->GetDCPOutputPath(platform);
        RemoveFromLibraryPath(ide, platform, dcpDir, false);
    }
    
    // Remove C++ specific paths for RAD Studio and C++Builder
    // Note: We try to remove even if GenerateCppFiles wasn't set, 
    // because user might have changed settings between installs
    if (ide->Personality != TIDEPersonality::Delphi)
    {
        // Remove from C++ Include Path
        RemoveFromCppIncludePath(ide, platform, libDir);
        RemoveFromCppIncludePath(ide, platform, sourcesDir);
        
        // Remove from C++ Library Path
        String dcpDir = ide->GetDCPOutputPath(platform);
        RemoveFromCppPath(ide, platform, dcpDir, false);
        
        LogToFile(L"  Removed C++ paths (RAD Studio/C++Builder)");
    }
    else
    {
        LogToFile(L"  Skipped C++ paths (Delphi only)");
    }
}

//---------------------------------------------------------------------------
// Uninstall IDE
//---------------------------------------------------------------------------
void TInstaller::UninstallIDE(const TIDEInfoPtr& ide, const TUninstallOptions& uninstallOpts)
{
    LogToFile(L"=== UninstallIDE: " + ide->Name + L" ===");
    LogToFile(L"  Uninstall32BitIDE: " + String(uninstallOpts.Uninstall32BitIDE ? L"true" : L"false"));
    LogToFile(L"  Uninstall64BitIDE: " + String(uninstallOpts.Uninstall64BitIDE ? L"true" : L"false"));
    LogToFile(L"  DeleteCompiledFiles: " + String(uninstallOpts.DeleteCompiledFiles ? L"true" : L"false"));
    
    UpdateProgressState(L"Uninstalling from " + ide->Name);
    
    // Step 1: Unregister packages from registry
    if (uninstallOpts.Uninstall32BitIDE)
    {
        LogToFile(L"  Unregistering from 32-bit IDE...");
        UnregisterAllDevExpressPackages(ide, false);
    }
    
    if (uninstallOpts.Uninstall64BitIDE)
    {
        LogToFile(L"  Unregistering from 64-bit IDE...");
        UnregisterAllDevExpressPackages(ide, true);
    }
    
    // Step 2: Delete compiled files if requested
    if (uninstallOpts.DeleteCompiledFiles)
    {
        LogToFile(L"  Deleting compiled files...");
        
        // Delete package files for all platforms
        DeletePackageFiles(ide, TIDEPlatform::Win32);
        DeletePackageFiles(ide, TIDEPlatform::Win64);
        DeletePackageFiles(ide, TIDEPlatform::Win64Modern);
        
        // Cleanup library directories
        CleanupAllCompiledFiles(ide);
    }
    
    // Step 3: Remove library paths
    RemoveLibraryPaths(ide, TIDEPlatform::Win32);
    if (ide->SupportsWin64)
        RemoveLibraryPaths(ide, TIDEPlatform::Win64);
    RemoveLibraryPaths(ide, TIDEPlatform::Win64Modern);
    
    // Step 4: Clear environment variable
    SetEnvironmentVariable(ide, DX_ENV_VARIABLE, L"");
    
    LogToFile(L"=== UninstallIDE completed ===");
}

//---------------------------------------------------------------------------
// Delete package files for platform
//---------------------------------------------------------------------------
void TInstaller::DeletePackageFiles(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    LogToFile(L"DeletePackageFiles for platform: " + String(
        platform == TIDEPlatform::Win32 ? L"Win32" : 
        platform == TIDEPlatform::Win64 ? L"Win64" : L"Win64x"));
    
    if (!TPackageCompiler::IsPlatformSupported(ide, platform) && platform != TIDEPlatform::Win64Modern)
    {
        LogToFile(L"  Platform not supported, skipping");
        return;
    }
    
    String bplDir = ide->GetBPLOutputPath(platform);
    String dcpDir = ide->GetDCPOutputPath(platform);
    
    LogToFile(L"  BPL dir: " + bplDir);
    LogToFile(L"  DCP dir: " + dcpDir);
    
    int deletedCount = 0;
    
    for (const auto& profile : FProfile->GetComponents())
    {
        // Process all package lists
        TStringList* packageLists[] = { 
            profile->RequiredPackages, 
            profile->OptionalPackages, 
            profile->OutdatedPackages 
        };
        
        for (auto* pkgList : packageLists)
        {
            for (int i = 0; i < pkgList->Count; i++)
            {
                String packageName = TProfileManager::GetPackageName(pkgList->Strings[i], ide);
                
                // Delete from BPL directory
                String bplPath = TPath::Combine(bplDir, packageName + L".bpl");
                if (DeleteFile(bplPath.c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(bplPath, L".lib").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(bplPath, L".bpi").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(bplPath, L".map").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(bplPath, L".a").c_str())) deletedCount++;
                
                // Delete from DCP directory
                String dcpPath = TPath::Combine(dcpDir, packageName + L".dcp");
                if (DeleteFile(dcpPath.c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(dcpPath, L".bpi").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(dcpPath, L".obj").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(dcpPath, L".lib").c_str())) deletedCount++;
                if (DeleteFile(ChangeFileExt(dcpPath, L".a").c_str())) deletedCount++;
            }
        }
    }
    
    LogToFile(L"  Deleted " + String(deletedCount) + L" files from BPL/DCP directories");
}

void TInstaller::AddToLibraryPath(const TIDEInfoPtr& ide,
                                   TIDEPlatform platform,
                                   const String& path,
                                   bool isBrowsingPath)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    String keyPath = ide->RegistryKey + L"\\Library\\" + platformKey;
    String valueName = isBrowsingPath ? L"Browsing Path" : L"Search Path";
    
    LogToFile(L"AddToLibraryPath: [" + path + L"]");
    LogToFile(L"  Platform: " + platformKey);
    LogToFile(L"  Type: " + String(isBrowsingPath ? L"Browsing" : L"Search"));
    LogToFile(L"  Registry: HKCU\\" + keyPath + L"\\" + valueName);
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, true))
    {
        String currentPath = reg->ReadString(valueName);
        
        if (currentPath.Pos(path) == 0)
        {
            if (!currentPath.IsEmpty() && !currentPath.EndsWith(L";"))
                currentPath = currentPath + L";";
            currentPath = currentPath + path;
            reg->WriteString(valueName, currentPath);
            LogToFile(L"  SUCCESS: Path added");
        }
        else
        {
            LogToFile(L"  SKIPPED: Path already exists");
        }
        
        reg->CloseKey();
    }
    else
    {
        LogToFile(L"  ERROR: Failed to open registry key");
    }
    
    // Also add to C++Builder paths if applicable
    TInstallOptionSet opts = GetOptions(ide);
    if (opts.count(TInstallOption::GenerateCppFiles) > 0 && 
        ide->Personality != TIDEPersonality::Delphi)
    {
        AddToCppPath(ide, platform, path, isBrowsingPath);
    }
}

void TInstaller::AddToCppPath(const TIDEInfoPtr& ide,
                               TIDEPlatform platform,
                               const String& path,
                               bool isBrowsingPath)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    String valueName = isBrowsingPath ? L"BrowsingPath" : L"LibraryPath";
    
    LogToFile(L"AddToCppPath: [" + path + L"]");
    LogToFile(L"  Platform: " + platformKey);
    LogToFile(L"  ValueName: " + valueName);
    
    // For Win32, add to BOTH Modern and Classic compiler paths
    std::vector<String> keyPaths;
    keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey);
    
    if (platform == TIDEPlatform::Win32)
    {
        keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey + L"\\Classic");
    }
    
    for (const auto& keyPath : keyPaths)
    {
        LogToFile(L"  Registry: HKCU\\" + keyPath + L"\\" + valueName);
        
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKey(keyPath, true))
        {
            String currentPath = reg->ReadString(valueName);
            
            if (currentPath.Pos(path) == 0)
            {
                if (!currentPath.IsEmpty() && !currentPath.EndsWith(L";"))
                    currentPath = currentPath + L";";
                currentPath = currentPath + path;
                reg->WriteString(valueName, currentPath);
                LogToFile(L"  SUCCESS: C++ path added to " + keyPath);
            }
            else
            {
                LogToFile(L"  SKIPPED: C++ path already exists in " + keyPath);
            }
            
            reg->CloseKey();
        }
        else
        {
            LogToFile(L"  WARNING: Could not open " + keyPath);
        }
    }
}

void TInstaller::AddToCppIncludePath(const TIDEInfoPtr& ide,
                                      TIDEPlatform platform,
                                      const String& path)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    LogToFile(L"AddToCppIncludePath: [" + path + L"]");
    LogToFile(L"  Platform: " + platformKey);
    
    // For Win32, we need to add to BOTH Modern and Classic compiler paths
    // Modern: C++\Paths\Win32\IncludePath
    // Classic: C++\Paths\Win32\Classic\IncludePath
    
    std::vector<String> keyPaths;
    keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey);
    
    // For Win32, also add to Classic compiler path
    if (platform == TIDEPlatform::Win32)
    {
        keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey + L"\\Classic");
    }
    
    String valueName = L"IncludePath";
    
    for (const auto& keyPath : keyPaths)
    {
        LogToFile(L"  Registry: HKCU\\" + keyPath + L"\\" + valueName);
        
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKey(keyPath, true))
        {
            String currentPath = reg->ReadString(valueName);
            
            if (currentPath.Pos(path) == 0)
            {
                if (!currentPath.IsEmpty() && !currentPath.EndsWith(L";"))
                    currentPath = currentPath + L";";
                currentPath = currentPath + path;
                reg->WriteString(valueName, currentPath);
                LogToFile(L"  SUCCESS: C++ Include path added to " + keyPath);
            }
            else
            {
                LogToFile(L"  SKIPPED: C++ Include path already exists in " + keyPath);
            }
            
            reg->CloseKey();
        }
        else
        {
            LogToFile(L"  WARNING: Could not open " + keyPath);
        }
    }
}

void TInstaller::RemoveFromCppIncludePath(const TIDEInfoPtr& ide,
                                           TIDEPlatform platform,
                                           const String& path)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    LogToFile(L"RemoveFromCppIncludePath: [" + path + L"]");
    LogToFile(L"  Platform: " + platformKey);
    
    // For Win32, remove from BOTH Modern and Classic compiler paths
    std::vector<String> keyPaths;
    keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey);
    
    if (platform == TIDEPlatform::Win32)
    {
        keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey + L"\\Classic");
    }
    
    String valueName = L"IncludePath";
    
    for (const auto& keyPath : keyPaths)
    {
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKey(keyPath, false))
        {
            String currentPath = reg->ReadString(valueName);
            
            currentPath = StringReplace(currentPath, path + L";", L"", TReplaceFlags() << rfReplaceAll);
            currentPath = StringReplace(currentPath, L";" + path, L"", TReplaceFlags() << rfReplaceAll);
            currentPath = StringReplace(currentPath, path, L"", TReplaceFlags() << rfReplaceAll);
            
            reg->WriteString(valueName, currentPath);
            reg->CloseKey();
            LogToFile(L"  SUCCESS: C++ Include path removed from " + keyPath);
        }
    }
}

void TInstaller::RemoveFromLibraryPath(const TIDEInfoPtr& ide,
                                        TIDEPlatform platform,
                                        const String& path,
                                        bool isBrowsingPath)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    String keyPath = ide->RegistryKey + L"\\Library\\" + platformKey;
    String valueName = isBrowsingPath ? L"Browsing Path" : L"Search Path";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, false))
    {
        String currentPath = reg->ReadString(valueName);
        
        currentPath = StringReplace(currentPath, path + L";", L"", TReplaceFlags() << rfReplaceAll);
        currentPath = StringReplace(currentPath, L";" + path, L"", TReplaceFlags() << rfReplaceAll);
        currentPath = StringReplace(currentPath, path, L"", TReplaceFlags() << rfReplaceAll);
        
        reg->WriteString(valueName, currentPath);
        reg->CloseKey();
    }
    
    // Also remove from C++Builder paths
    if (ide->Personality != TIDEPersonality::Delphi)
    {
        RemoveFromCppPath(ide, platform, path, isBrowsingPath);
    }
}

void TInstaller::RemoveFromCppPath(const TIDEInfoPtr& ide,
                                    TIDEPlatform platform,
                                    const String& path,
                                    bool isBrowsingPath)
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64: platformKey = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformKey = L"Win64x"; break;
        default: platformKey = L"Win32"; break;
    }
    
    String valueName = isBrowsingPath ? L"BrowsingPath" : L"LibraryPath";
    
    // For Win32, remove from BOTH Modern and Classic compiler paths
    std::vector<String> keyPaths;
    keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey);
    
    if (platform == TIDEPlatform::Win32)
    {
        keyPaths.push_back(ide->RegistryKey + L"\\C++\\Paths\\" + platformKey + L"\\Classic");
    }
    
    for (const auto& keyPath : keyPaths)
    {
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKey(keyPath, false))
        {
            String currentPath = reg->ReadString(valueName);
            
            currentPath = StringReplace(currentPath, path + L";", L"", TReplaceFlags() << rfReplaceAll);
            currentPath = StringReplace(currentPath, L";" + path, L"", TReplaceFlags() << rfReplaceAll);
            currentPath = StringReplace(currentPath, path, L"", TReplaceFlags() << rfReplaceAll);
            
            reg->WriteString(valueName, currentPath);
            reg->CloseKey();
        }
    }
}

bool TInstaller::RegisterPackage(const TIDEInfoPtr& ide,
                                  const String& bplPath,
                                  const String& description,
                                  bool is64BitIDE)
{
    LogToFile(L"RegisterPackage: [" + bplPath + L"]");
    LogToFile(L"  Description: [" + description + L"]");
    LogToFile(L"  is64BitIDE: " + String(is64BitIDE ? L"true" : L"false"));
    
    // Check if BPL file exists
    if (!FileExists(bplPath))
    {
        LogToFile(L"  ERROR: BPL file does not exist!");
        UpdateProgressState(L"ERROR: BPL not found: " + bplPath);
        return false;
    }
    
    // IMPORTANT: Different registry keys for 32-bit and 64-bit IDE!
    // 32-bit IDE: Known Packages
    // 64-bit IDE: Known Packages x64
    String keyPath;
    if (is64BitIDE)
        keyPath = ide->RegistryKey + L"\\Known Packages x64";
    else
        keyPath = ide->RegistryKey + L"\\Known Packages";
    
    LogToFile(L"  Registry key: [HKCU\\" + keyPath + L"]");
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, true))
    {
        reg->WriteString(bplPath, description);
        reg->CloseKey();
        LogToFile(L"  SUCCESS: Package registered");
        UpdateProgressState(L"Registered: " + ExtractFileName(bplPath));
        return true;
    }
    
    LogToFile(L"  ERROR: Failed to open registry key");
    return false;
}

void TInstaller::UnregisterPackage(const TIDEInfoPtr& ide, const String& bplPath, bool is64BitIDE)
{
    // IMPORTANT: Different registry keys for 32-bit and 64-bit IDE!
    String keyPath;
    if (is64BitIDE)
        keyPath = ide->RegistryKey + L"\\Known Packages x64";
    else
        keyPath = ide->RegistryKey + L"\\Known Packages";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, false))
    {
        if (reg->ValueExists(bplPath))
            reg->DeleteValue(bplPath);
        reg->CloseKey();
    }
}

void TInstaller::UnregisterAllDevExpressPackages(const TIDEInfoPtr& ide, bool is64BitIDE)
{
    // Remove ALL DevExpress packages from the appropriate Known Packages registry key
    // Only clean the key that corresponds to the selected IDE bitness
    // 32-bit IDE uses: Known Packages
    // 64-bit IDE uses: Known Packages x64
    
    String keyPath;
    if (is64BitIDE)
        keyPath = ide->RegistryKey + L"\\Known Packages x64";
    else
        keyPath = ide->RegistryKey + L"\\Known Packages";
    
    LogToFile(L"UnregisterAllDevExpressPackages: Cleaning up " + keyPath);
    LogToFile(L"  is64BitIDE: " + String(is64BitIDE ? L"true" : L"false"));
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, false))
    {
        std::unique_ptr<TStringList> values(new TStringList());
        reg->GetValueNames(values.get());
        
        // Collect DevExpress packages to remove
        std::unique_ptr<TStringList> toRemove(new TStringList());
        
        for (int i = 0; i < values->Count; i++)
        {
            String valueName = values->Strings[i];
            
            // Check if this is a DevExpress package
            // DevExpress packages start with: dx, cx, dcldx, dclcx
            // We check for these prefixes in the filename part of the path
            String fileName = ExtractFileName(valueName).LowerCase();
            
            bool isDevExpress = 
                fileName.Pos(L"dx") == 1 ||       // dxCore, dxBar, etc.
                fileName.Pos(L"cx") == 1 ||       // cxGrid, cxEdit, etc.
                fileName.Pos(L"dcldx") == 1 ||    // design-time dx packages
                fileName.Pos(L"dclcx") == 1;      // design-time cx packages
            
            if (isDevExpress)
            {
                toRemove->Add(valueName);
            }
        }
        
        // Remove collected packages
        for (int i = 0; i < toRemove->Count; i++)
        {
            LogToFile(L"  Removing: " + toRemove->Strings[i]);
            reg->DeleteValue(toRemove->Strings[i]);
        }
        
        LogToFile(L"  Removed " + String(toRemove->Count) + L" DevExpress package registrations");
        
        reg->CloseKey();
    }
}

String TInstaller::GetEnvironmentVariable(const TIDEInfoPtr& ide, const String& name)
{
    String keyPath = ide->RegistryKey + L"\\Environment Variables";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(keyPath))
    {
        if (reg->ValueExists(name))
            return reg->ReadString(name);
    }
    
    return L"";
}

void TInstaller::SetEnvironmentVariable(const TIDEInfoPtr& ide, const String& name, const String& value)
{
    String keyPath = ide->RegistryKey + L"\\Environment Variables";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ | KEY_WRITE));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKey(keyPath, true))
    {
        if (value.IsEmpty())
        {
            if (reg->ValueExists(name))
                reg->DeleteValue(name);
        }
        else
        {
            reg->WriteString(name, value);
        }
        reg->CloseKey();
    }
}

void TInstaller::SearchNewPackages(TStringList* list)
{
    list->Clear();
    
    if (FInstallFileDir.IsEmpty())
        return;
        
    std::unique_ptr<TStringList> knownPackages(new TStringList());
    for (const auto& profile : FProfile->GetComponents())
    {
        knownPackages->AddStrings(profile->RequiredPackages);
        knownPackages->AddStrings(profile->OptionalPackages);
        knownPackages->AddStrings(profile->OutdatedPackages);
    }
    
    TSearchRec sr;
    if (FindFirst(FInstallFileDir + L"\\*.dpk", faAnyFile, sr) == 0)
    {
        do
        {
            String baseName = TPath::GetFileNameWithoutExtension(sr.Name);
            
            int len = baseName.Length();
            while (len > 0 && (baseName[len] >= L'0' && baseName[len] <= L'9'))
                len--;
                
            if (len > 0)
            {
                wchar_t lastChar = baseName[len];
                if (lastChar == L'D' || lastChar == L'd')
                    baseName = baseName.SubString(1, len - 1);
                else if (len >= 2 && baseName.SubString(len - 1, 2).UpperCase() == L"RS")
                    baseName = baseName.SubString(1, len - 2);
                else
                    baseName = baseName.SubString(1, len);
            }
                
            if (knownPackages->IndexOf(baseName) < 0)
                list->Add(sr.Name);
                
        } while (FindNext(sr) == 0);
        
        FindClose(sr);
    }
}

} // namespace DxCore
