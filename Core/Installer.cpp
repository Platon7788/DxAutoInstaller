//---------------------------------------------------------------------------
// Installer implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "Installer.h"
#include <Registry.hpp>
#include <IOUtils.hpp>
#include <Vcl.Forms.hpp>
#include <DateUtils.hpp>
#include <fstream>

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
    
    LogToFile(L"=== DxAutoInstaller Started ===");
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
        opts.insert(TInstallOption::AddBrowsingPath);
        opts.insert(TInstallOption::NativeLookAndFeel);
        opts.insert(TInstallOption::CompileWin32Runtime);
        
        if (ide->SupportsWin64)
            opts.insert(TInstallOption::CompileWin64Runtime);
        // Note: CompileWin64ModernRuntime is NOT set - Win64x is auto-generated
        // when InstallToCppBuilder + CompileWin64Runtime are both enabled
        if (ide->IDEBitness == TIDEBitness::IDE64)
            opts.insert(TInstallOption::Use64BitIDE);
            
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
    // CompileWin64ModernRuntime is deprecated - Win64x is auto-generated
    opts.erase(TInstallOption::CompileWin64ModernRuntime);
    if (ide->Personality == TIDEPersonality::Delphi)
        opts.erase(TInstallOption::InstallToCppBuilder);
        
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
    SetState(TInstallerState::Stopped);
}

void TInstaller::CheckStoppedState()
{
    Application->ProcessMessages();
    if (FState == TInstallerState::Stopped)
    {
        SetState(TInstallerState::Normal);
        Abort();
    }
}

void TInstaller::UpdateProgress(const TIDEInfoPtr& ide,
                                 const TComponentProfilePtr& component,
                                 const String& task,
                                 const String& target)
{
    if (FOnProgress)
        FOnProgress(ide, component, task, target);
}

void TInstaller::UpdateProgressState(const String& stateText)
{
    if (FOnProgressState)
        FOnProgressState(stateText);
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
        
        if (platform == TIDEPlatform::Win64)
            result = result + L"\\" + PlatformNames::Win64;
        else if (platform == TIDEPlatform::Win64Modern)
            result = result + L"\\" + PlatformNames::Win64Modern;
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
    LogToFile(L"CleanupAllCompiledFiles for IDE: " + ide->Name);
    
    // Cleanup library directories
    CleanupLibraryDir(ide, TIDEPlatform::Win32);
    if (ide->SupportsWin64)
        CleanupLibraryDir(ide, TIDEPlatform::Win64);
    CleanupLibraryDir(ide, TIDEPlatform::Win64Modern);
    
    // Cleanup BPL directories
    std::set<String> bplExtensions;
    bplExtensions.insert(L".bpl");
    bplExtensions.insert(L".lib");
    bplExtensions.insert(L".bpi");
    bplExtensions.insert(L".map");
    
    String bplDir32 = ide->GetBPLOutputPath(TIDEPlatform::Win32);
    String bplDir64 = ide->GetBPLOutputPath(TIDEPlatform::Win64);
    
    // Only delete DevExpress files from BPL dirs
    DeleteDevExpressFilesFromDir(bplDir32, bplExtensions);
    DeleteDevExpressFilesFromDir(bplDir64, bplExtensions);
    
    // Cleanup DCP directories
    std::set<String> dcpExtensions;
    dcpExtensions.insert(L".dcp");
    dcpExtensions.insert(L".bpi");
    
    String dcpDir32 = ide->GetDCPOutputPath(TIDEPlatform::Win32);
    String dcpDir64 = ide->GetDCPOutputPath(TIDEPlatform::Win64);
    String dcpDir64x = ide->GetDCPOutputPath(TIDEPlatform::Win64Modern);
    
    DeleteDevExpressFilesFromDir(dcpDir32, dcpExtensions);
    DeleteDevExpressFilesFromDir(dcpDir64, dcpExtensions);
    DeleteDevExpressFilesFromDir(dcpDir64x, dcpExtensions);
}

void TInstaller::DeleteDevExpressFilesFromDir(const String& dir, const std::set<String>& extensions)
{
    LogToFile(L"DeleteDevExpressFilesFromDir: [" + dir + L"]");
    
    if (!DirectoryExists(dir))
        return;
    
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
            
            // Check if this is a DevExpress file
            bool isDevExpress = 
                lowerName.Pos(L"dx") == 1 ||      // dxCore, dxBar, etc.
                lowerName.Pos(L"cx") == 1 ||      // cxGrid, cxEdit, etc.
                lowerName.Pos(L"dcldx") == 1 ||   // design-time packages
                lowerName.Pos(L"dclcx") == 1;
            
            if (isDevExpress && extensions.count(ext) > 0)
            {
                String fullPath = dir + L"\\" + sr.Name;
                LogToFile(L"  Deleting DevExpress file: " + fullPath);
                DeleteFile(fullPath.c_str());
            }
        } while (FindNext(sr) == 0);
        
        FindClose(sr);
    }
}

void TInstaller::Install(const std::vector<TIDEInfoPtr>& ides)
{
    LogToFile(L"=== Install started ===");
    SetState(TInstallerState::Running);
    
    for (const auto& ide : ides)
    {
        try
        {
            InstallIDE(ide);
        }
        catch (const EAbort&)
        {
            LogToFile(L"EAbort exception caught");
            break;
        }
        catch (Exception& e)
        {
            LogToFile(L"EXCEPTION: " + e.Message);
            throw;
        }
    }
    
    LogToFile(L"=== Install completed ===");
    SetState(TInstallerState::Normal);
}

void TInstaller::Uninstall(const std::vector<TIDEInfoPtr>& ides)
{
    SetState(TInstallerState::Running);
    
    for (const auto& ide : ides)
    {
        try
        {
            UninstallIDE(ide);
        }
        catch (const EAbort&)
        {
            break;
        }
    }
    
    SetState(TInstallerState::Normal);
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
    
    // First uninstall existing
    LogToFile(L"Calling UninstallIDE...");
    UninstallIDE(ide);
    LogToFile(L"UninstallIDE completed");
    
    TInstallOptionSet opts = GetOptions(ide);
    String installSourcesDir = GetInstallSourcesDir(FInstallFileDir);
    
    LogToFile(L"InstallSourcesDir: [" + installSourcesDir + L"]");
    UpdateProgressState(L"InstallSourcesDir: " + installSourcesDir);
    
    // Get DevExpress build number for version-specific fixes
    unsigned int dxBuildNumber = TProfileManager::GetDxBuildNumber(FInstallFileDir);
    
    const TComponentList& components = GetComponents(ide);
    
    // Check IDE bitness options
    bool useBothIDE = opts.count(TInstallOption::UseBothIDE) > 0;
    bool use64BitIDE = opts.count(TInstallOption::Use64BitIDE) > 0;
    
    // Determine which platforms to compile based on IDE bitness and user options:
    // - Both IDE: compile Win32 for 32-bit IDE design-time, Win64 for 64-bit IDE design-time
    // - 64-bit IDE only: compile ONLY Win64/Win64x (NOT Win32)
    // - 32-bit IDE only: compile Win32 always, Win64/Win64x if user enabled them
    bool compileWin32 = useBothIDE || (!use64BitIDE && opts.count(TInstallOption::CompileWin32Runtime) > 0);
    bool compileWin64 = useBothIDE || use64BitIDE || (opts.count(TInstallOption::CompileWin64Runtime) > 0 && ide->SupportsWin64);
    
    // Win64x (Modern C++) support for C++Builder:
    // - We DON'T compile packages for Win64x (no dcc64x compiler for Delphi packages)
    // - Instead, when compiling Win64 with -JL flag, we:
    //   1. Copy .hpp files from Win64 to Win64x folder
    //   2. Generate .a import libraries using mkexp.exe from .bpl files
    // This is handled in InstallPackage() when platform == Win64 && GenerateCppFiles
    bool generateWin64xLibs = opts.count(TInstallOption::InstallToCppBuilder) > 0 && 
                              ide->Personality != TIDEPersonality::Delphi &&
                              compileWin64;
    
    // Log compilation options
    LogToFile(L"=== Compilation Options ===");
    LogToFile(L"useBothIDE: " + String(useBothIDE ? L"true" : L"false"));
    LogToFile(L"use64BitIDE: " + String(use64BitIDE ? L"true" : L"false"));
    LogToFile(L"compileWin32: " + String(compileWin32 ? L"true" : L"false"));
    LogToFile(L"compileWin64: " + String(compileWin64 ? L"true" : L"false"));
    LogToFile(L"generateWin64xLibs: " + String(generateWin64xLibs ? L"true" : L"false"));
    LogToFile(L"IDE SupportsWin64: " + String(ide->SupportsWin64 ? L"true" : L"false"));
    LogToFile(L"IDE Personality: " + String(ide->Personality == TIDEPersonality::Delphi ? L"Delphi" : 
                                            (ide->Personality == TIDEPersonality::CppBuilder ? L"CppBuilder" : L"RADStudio")));
    
    // ========================================
    // Phase 1: Copy source files to Library\Sources ONLY
    // Compiled files (.dcu, .hpp, etc.) go to Library\{ver}\* during compilation
    // Only .dfm, .res and .dcr files need to be in Library\{ver}\* for compiler
    // ========================================
    
    // Define extensions for source files (go to Library\Sources)
    std::set<String> sourceExtensions;
    sourceExtensions.insert(L".pas");
    sourceExtensions.insert(L".inc");
    sourceExtensions.insert(L".dfm");
    sourceExtensions.insert(L".fmx");
    sourceExtensions.insert(L".res");
    sourceExtensions.insert(L".dcr");  // Component resource files (palette icons)
    
    // Define extensions for resource files (needed in Library\{ver}\* for compiler)
    std::set<String> resourceExtensions;
    resourceExtensions.insert(L".dfm");
    resourceExtensions.insert(L".res");
    resourceExtensions.insert(L".dcr");  // Component resource files (palette icons)
    
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
        
        // Copy ONLY .dfm and .res to library dirs (needed for compilation)
        String libDir32 = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win32);
        if (compileWin32)
        {
            CopySourceFilesFiltered(sourcesDir, libDir32, resourceExtensions);
        }
        
        if (compileWin64)
        {
            String libDir64 = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64);
            CopySourceFilesFiltered(sourcesDir, libDir64, resourceExtensions);
        }
        
        // For Win64x - only create directory, .hpp and .a will be generated from Win64
        if (generateWin64xLibs)
        {
            String libDir64x = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64Modern);
            ForceDirectories(libDir64x);
        }
        
        // Fix for DevExpress version >= 18.2.x
        if (dxBuildNumber >= 20180200 && comp->Profile->ComponentName == L"ExpressLibrary")
        {
            for (const auto& profile : FProfile->GetComponents())
            {
                String compSourcesDir = TProfileManager::GetComponentSourcesDir(
                    FInstallFileDir, profile->ComponentName);
                String compPackagesDir = TProfileManager::GetComponentPackagesDir(
                    FInstallFileDir, profile->ComponentName);
                    
                if (DirectoryExists(compSourcesDir) && !DirectoryExists(compPackagesDir))
                {
                    UpdateProgressState(L"Copying (18.2+ fix): " + compSourcesDir);
                    // Sources go to Library\Sources
                    CopySourceFilesFiltered(compSourcesDir, installSourcesDir, sourceExtensions);
                    // Resources go to Library\{ver}\*
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
        
        // ========================================
        // Compile REQUIRED packages for this component
        // Based on IDE bitness:
        // - 32-bit IDE: compile Win32 only
        // - 64-bit IDE: compile Win64/Win64x only
        // ========================================
        for (const auto& pkg : comp->Packages)
        {
            if (!pkg->Required)
                continue;
                
            // Win32 - compile only for 32-bit IDE
            if (compileWin32)
            {
                InstallPackage(ide, TIDEPlatform::Win32, comp, pkg, false);
            }
            
            // Win64 - compile only for 64-bit IDE
            // Win64x libraries are auto-generated when compiling Win64 with -JL flag
            if (compileWin64)
            {
                InstallPackage(ide, TIDEPlatform::Win64, comp, pkg, false);
            }
        }
    }
    
    // ========================================
    // Phase 2: Compile OPTIONAL packages
    // ========================================
    for (const auto& comp : components)
    {
        if (comp->State != TComponentState::Install)
            continue;
            
        for (const auto& pkg : comp->Packages)
        {
            if (pkg->Required)
                continue;
                
            // Win32 - compile only for 32-bit IDE
            if (compileWin32)
            {
                InstallPackage(ide, TIDEPlatform::Win32, comp, pkg, false);
            }
            
            // Win64 - compile only for 64-bit IDE
            if (compileWin64)
            {
                InstallPackage(ide, TIDEPlatform::Win64, comp, pkg, false);
            }
        }
    }
    
    // ========================================
    // Phase 3: Add library paths
    // Based on IDE bitness - add paths only for compiled platforms
    // ========================================
    
    // Add Icon Library path to browsing path (icons are not copied, just referenced)
    String iconLibraryDir = FInstallFileDir + L"\\ExpressLibrary\\Sources\\Icon Library";
    bool hasIconLibrary = DirectoryExists(iconLibraryDir);
    
    if (compileWin32)
    {
        String libDir = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win32);
        AddToLibraryPath(ide, TIDEPlatform::Win32, libDir, false);
        if (opts.count(TInstallOption::AddBrowsingPath) > 0)
        {
            AddToLibraryPath(ide, TIDEPlatform::Win32, installSourcesDir, true);
            if (hasIconLibrary)
                AddToLibraryPath(ide, TIDEPlatform::Win32, iconLibraryDir, true);
        }
        else
            AddToLibraryPath(ide, TIDEPlatform::Win32, installSourcesDir, false);
    }
    
    if (compileWin64)
    {
        String libDir = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64);
        AddToLibraryPath(ide, TIDEPlatform::Win64, libDir, false);
        if (opts.count(TInstallOption::AddBrowsingPath) > 0)
        {
            AddToLibraryPath(ide, TIDEPlatform::Win64, installSourcesDir, true);
            if (hasIconLibrary)
                AddToLibraryPath(ide, TIDEPlatform::Win64, iconLibraryDir, true);
        }
        else
            AddToLibraryPath(ide, TIDEPlatform::Win64, installSourcesDir, false);
        
        // Add Win64x paths for C++Builder Modern support
        // When C++ option is enabled, we generate .hpp and .a files for Win64x
        if (generateWin64xLibs)
        {
            String libDir64x = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64Modern);
            AddToLibraryPath(ide, TIDEPlatform::Win64Modern, libDir64x, false);
            if (opts.count(TInstallOption::AddBrowsingPath) > 0)
            {
                AddToLibraryPath(ide, TIDEPlatform::Win64Modern, installSourcesDir, true);
                if (hasIconLibrary)
                    AddToLibraryPath(ide, TIDEPlatform::Win64Modern, iconLibraryDir, true);
            }
            else
                AddToLibraryPath(ide, TIDEPlatform::Win64Modern, installSourcesDir, false);
        }
    }
    
    // Set environment variable
    SetEnvironmentVariable(ide, DX_ENV_VARIABLE, FInstallFileDir);
}

void TInstaller::InstallPackage(const TIDEInfoPtr& ide,
                                 TIDEPlatform platform,
                                 const TComponentPtr& component,
                                 const TPackagePtr& package,
                                 bool /* unused */)
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
        case TIDEPlatform::Win64: platformName = L"Win64"; break;
        case TIDEPlatform::Win64Modern: platformName = L"Win64x"; break;
        default: platformName = L"Win32"; break;
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
    options.GenerateCppFiles = instOpts.count(TInstallOption::InstallToCppBuilder) > 0;
    
    // Get IDE bitness options for later use in registration
    bool useBothIDE = instOpts.count(TInstallOption::UseBothIDE) > 0;
    bool use64BitIDE = instOpts.count(TInstallOption::Use64BitIDE) > 0;
    
    // Ensure output directories exist
    if (!options.BPLOutputDir.IsEmpty())
        ForceDirectories(options.BPLOutputDir);
    if (!options.DCPOutputDir.IsEmpty())
        ForceDirectories(options.DCPOutputDir);
    if (!options.UnitOutputDir.IsEmpty())
        ForceDirectories(options.UnitOutputDir);
    
    // Compile
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
        
        // Copy .hpp files to Win64x folder for C++Builder Modern support
        // When compiling Win64 with -JL, also copy hpp to Win64x folder
        // and generate .a import libraries for Modern C++ using mkexp
        if (platform == TIDEPlatform::Win64 && options.GenerateCppFiles)
        {
            String win64xLibDir = GetInstallLibraryDir(FInstallFileDir, ide, TIDEPlatform::Win64Modern);
            if (!win64xLibDir.IsEmpty())
            {
                ForceDirectories(win64xLibDir);
                
                // Copy all .hpp files from Win64 to Win64x (headers are identical)
                TSearchRec sr;
                if (FindFirst(options.UnitOutputDir + L"\\*.hpp", faAnyFile, sr) == 0)
                {
                    do
                    {
                        String srcHpp = options.UnitOutputDir + L"\\" + sr.Name;
                        String dstHpp = win64xLibDir + L"\\" + sr.Name;
                        CopyFile(srcHpp.c_str(), dstHpp.c_str(), FALSE);
                    } while (FindNext(sr) == 0);
                    FindClose(sr);
                }
                
                // Copy .bpi files from Win64 DCP dir to Win64x DCP dir
                // BPI files are needed for C++Builder linking
                String win64xDcpDir = ide->GetDCPOutputPath(TIDEPlatform::Win64Modern);
                if (!win64xDcpDir.IsEmpty())
                {
                    ForceDirectories(win64xDcpDir);
                    
                    String srcBpi = options.DCPOutputDir + L"\\" + package->Name + L".bpi";
                    if (FileExists(srcBpi))
                    {
                        String dstBpi = win64xDcpDir + L"\\" + package->Name + L".bpi";
                        CopyFile(srcBpi.c_str(), dstBpi.c_str(), FALSE);
                    }
                }
                
                // Generate .a import library for Win64x (Modern C++) using mkexp from .bpl
                // mkexp.exe creates import library from BPL for use with bcc64x
                // Modern C++ linker uses .a files instead of .lib
                String mkexpPath = ide->RootDir + L"\\bin64\\mkexp.exe";
                if (FileExists(mkexpPath))
                {
                    String bplPath = options.BPLOutputDir + L"\\" + package->Name + L".bpl";
                    String libPath = win64xLibDir + L"\\" + package->Name + L".a";
                    
                    if (FileExists(bplPath))
                    {
                        // mkexp.exe output.a input.bpl
                        String cmdLine = L"\"" + mkexpPath + L"\" \"" + libPath + L"\" \"" + bplPath + L"\"";
                        
                        UpdateProgressState(L"Generating Win64x import lib: " + package->Name + L".a");
                        
                        STARTUPINFOW si = {0};
                        si.cb = sizeof(si);
                        si.dwFlags = STARTF_USESHOWWINDOW;
                        si.wShowWindow = SW_HIDE;
                        
                        PROCESS_INFORMATION pi = {0};
                        std::vector<wchar_t> cmdBuffer(cmdLine.Length() + 1);
                        wcscpy(cmdBuffer.data(), cmdLine.c_str());
                        
                        if (CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE,
                                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
                        {
                            WaitForSingleObject(pi.hProcess, 30000);  // 30 sec timeout
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                        }
                    }
                }
            }  // end if (!win64xLibDir.IsEmpty())
        }  // end if (platform == Win64 && GenerateCppFiles)
        
        // Register design-time packages
        // Design-time packages have Usage != RuntimeOnly
        //
        // For "Both IDE" mode: register to BOTH Known Packages and Known Packages x64
        // For single IDE mode: register only to the appropriate key
        bool isDesignTimePlatform32 = (platform == TIDEPlatform::Win32 && (useBothIDE || !use64BitIDE));
        bool isDesignTimePlatform64 = (platform == TIDEPlatform::Win64 && (useBothIDE || use64BitIDE));
        
        LogToFile(L"  useBothIDE: " + String(useBothIDE ? L"true" : L"false"));
        LogToFile(L"  use64BitIDE: " + String(use64BitIDE ? L"true" : L"false"));
        LogToFile(L"  platform: " + String(platform == TIDEPlatform::Win64 ? L"Win64" : L"Win32"));
        LogToFile(L"  isDesignTimePlatform32: " + String(isDesignTimePlatform32 ? L"true" : L"false"));
        LogToFile(L"  isDesignTimePlatform64: " + String(isDesignTimePlatform64 ? L"true" : L"false"));
        LogToFile(L"  package->Usage: " + String(package->Usage == TPackageUsage::RuntimeOnly ? L"RuntimeOnly" : 
                                                  (package->Usage == TPackageUsage::DesigntimeOnly ? L"DesigntimeOnly" : L"Both")));
        
        if (package->Usage != TPackageUsage::RuntimeOnly)
        {
            // Register for 32-bit IDE (Known Packages)
            if (isDesignTimePlatform32)
            {
                String bplPath = TPath::Combine(options.BPLOutputDir, package->Name + L".bpl");
                LogToFile(L"  Registering for 32-bit IDE: " + bplPath);
                RegisterPackage(ide, bplPath, package->Description, false);
            }
            
            // Register for 64-bit IDE (Known Packages x64)
            if (isDesignTimePlatform64)
            {
                String bplPath = TPath::Combine(options.BPLOutputDir, package->Name + L".bpl");
                LogToFile(L"  Registering for 64-bit IDE: " + bplPath);
                RegisterPackage(ide, bplPath, package->Description, true);
            }
        }
        else
        {
            LogToFile(L"  Skipping registration (runtime-only package)");
        }
    }
    else
    {
        UpdateProgressState(L"COMPILE ERROR: " + package->Name);
        if (!result.ErrorMessage.IsEmpty())
            UpdateProgressState(result.ErrorMessage);
        SetState(TInstallerState::Error);
    }
}

void TInstaller::UninstallIDE(const TIDEInfoPtr& ide)
{
    LogToFile(L"=== UninstallIDE: " + ide->Name + L" ===");
    UpdateProgressState(L"Uninstalling from " + ide->Name);
    
    // Get the IDE bitness option to know which registry key to clean
    TInstallOptionSet opts = GetOptions(ide);
    bool useBothIDE = opts.count(TInstallOption::UseBothIDE) > 0;
    bool use64BitIDE = opts.count(TInstallOption::Use64BitIDE) > 0;
    
    // Remove DevExpress packages from the appropriate Known Packages registry
    // For "Both IDE" mode: clean both registry keys
    if (useBothIDE || !use64BitIDE)
        UnregisterAllDevExpressPackages(ide, false);  // 32-bit IDE
    if (useBothIDE || use64BitIDE)
        UnregisterAllDevExpressPackages(ide, true);   // 64-bit IDE
    
    // Get previous install directory before clearing it
    String prevInstallDir = GetEnvironmentVariable(ide, DX_ENV_VARIABLE);
    
    // Delete BPL and DCP files for each package
    for (const auto& profile : FProfile->GetComponents())
    {
        for (int i = 0; i < profile->RequiredPackages->Count; i++)
        {
            UninstallPackage(ide, TIDEPlatform::Win32, profile, profile->RequiredPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64, profile, profile->RequiredPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64Modern, profile, profile->RequiredPackages->Strings[i]);
        }
        
        for (int i = 0; i < profile->OptionalPackages->Count; i++)
        {
            UninstallPackage(ide, TIDEPlatform::Win32, profile, profile->OptionalPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64, profile, profile->OptionalPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64Modern, profile, profile->OptionalPackages->Strings[i]);
        }
        
        for (int i = 0; i < profile->OutdatedPackages->Count; i++)
        {
            UninstallPackage(ide, TIDEPlatform::Win32, profile, profile->OutdatedPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64, profile, profile->OutdatedPackages->Strings[i]);
            UninstallPackage(ide, TIDEPlatform::Win64Modern, profile, profile->OutdatedPackages->Strings[i]);
        }
    }
    
    // Cleanup compiled files from BPL/DCP directories (system folders)
    // This catches any files that weren't deleted by UninstallPackage
    CleanupAllCompiledFiles(ide);
    
    if (prevInstallDir.IsEmpty())
        return;
        
    // Remove library paths for all platforms
    String sourcesDir = GetInstallSourcesDir(prevInstallDir);
    String iconLibraryDir = prevInstallDir + L"\\ExpressLibrary\\Sources\\Icon Library";
    
    // Win32
    String libDir = GetInstallLibraryDir(prevInstallDir, ide, TIDEPlatform::Win32);
    RemoveFromLibraryPath(ide, TIDEPlatform::Win32, libDir, false);
    RemoveFromLibraryPath(ide, TIDEPlatform::Win32, sourcesDir, false);
    RemoveFromLibraryPath(ide, TIDEPlatform::Win32, sourcesDir, true);
    RemoveFromLibraryPath(ide, TIDEPlatform::Win32, iconLibraryDir, true);
    
    // Win64
    if (ide->SupportsWin64)
    {
        libDir = GetInstallLibraryDir(prevInstallDir, ide, TIDEPlatform::Win64);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64, libDir, false);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64, sourcesDir, false);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64, sourcesDir, true);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64, iconLibraryDir, true);
    }
    
    // Win64 Modern
    if (ide->SupportsWin64Modern)
    {
        libDir = GetInstallLibraryDir(prevInstallDir, ide, TIDEPlatform::Win64Modern);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64Modern, libDir, false);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64Modern, sourcesDir, false);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64Modern, sourcesDir, true);
        RemoveFromLibraryPath(ide, TIDEPlatform::Win64Modern, iconLibraryDir, true);
    }
    
    SetEnvironmentVariable(ide, DX_ENV_VARIABLE, L"");
}

void TInstaller::UninstallPackage(const TIDEInfoPtr& ide,
                                   TIDEPlatform platform,
                                   const TComponentProfilePtr& component,
                                   const String& packageBaseName)
{
    CheckStoppedState();
    
    if (!TPackageCompiler::IsPlatformSupported(ide, platform))
        return;
        
    String packageName = TProfileManager::GetPackageName(packageBaseName, ide);
    String bplDir = ide->GetBPLOutputPath(platform);
    String dcpDir = ide->GetDCPOutputPath(platform);
    String libDir = GetInstallLibraryDir(FInstallFileDir, ide, platform);
    
    String bplPath = TPath::Combine(bplDir, packageName + L".bpl");
    
    UpdateProgress(ide, component, L"Uninstall", bplPath);
    
    // Unregister from BOTH registry keys (32-bit and 64-bit IDE)
    UnregisterPackage(ide, bplPath, false);  // Known Packages (32-bit)
    UnregisterPackage(ide, bplPath, true);   // Known Packages x64 (64-bit)
    
    // Delete BPL and related files from BPL directory
    DeleteFile(bplPath.c_str());
    DeleteFile(ChangeFileExt(bplPath, L".lib").c_str());
    DeleteFile(ChangeFileExt(bplPath, L".bpi").c_str());
    DeleteFile(ChangeFileExt(bplPath, L".map").c_str());
    
    // Delete DCP and BPI from DCP directory
    String dcpPath = TPath::Combine(dcpDir, packageName + L".dcp");
    String bpiPath = TPath::Combine(dcpDir, packageName + L".bpi");
    DeleteFile(dcpPath.c_str());
    DeleteFile(bpiPath.c_str());
    
    // Delete compiled files from Library directory (.dcu, .hpp, .a, .obj)
    // Note: We can't easily know which .dcu files belong to which package,
    // so we rely on CleanupAllCompiledFiles() to clean the entire directory
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
    if (opts.count(TInstallOption::InstallToCppBuilder) > 0 && 
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
    
    // C++Builder uses "C++\\Paths" section
    String keyPath = ide->RegistryKey + L"\\C++\\Paths\\" + platformKey;
    String valueName = isBrowsingPath ? L"BrowsingPath" : L"LibraryPath";
    
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
        }
        
        reg->CloseKey();
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
    
    String keyPath = ide->RegistryKey + L"\\C++\\Paths\\" + platformKey;
    String valueName = isBrowsingPath ? L"BrowsingPath" : L"LibraryPath";
    
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
            String lowerName = valueName.LowerCase();
            
            // Check if this is a DevExpress package
            // DevExpress packages typically contain: dx, cx, dcldx, dclcx
            if (lowerName.Pos(L"\\dx") > 0 || 
                lowerName.Pos(L"\\cx") > 0 ||
                lowerName.Pos(L"\\dcldx") > 0 ||
                lowerName.Pos(L"\\dclcx") > 0 ||
                lowerName.Pos(L"dxcore") > 0 ||
                lowerName.Pos(L"cxlibrary") > 0 ||
                lowerName.Pos(L"dxskin") > 0 ||
                lowerName.Pos(L"dxbar") > 0 ||
                lowerName.Pos(L"dxribbon") > 0 ||
                lowerName.Pos(L"dxlayout") > 0 ||
                lowerName.Pos(L"dxspreadsheet") > 0 ||
                lowerName.Pos(L"dxrichedit") > 0 ||
                lowerName.Pos(L"dxpdf") > 0 ||
                lowerName.Pos(L"dxgantt") > 0 ||
                lowerName.Pos(L"dxmap") > 0 ||
                lowerName.Pos(L"dxchart") > 0 ||
                lowerName.Pos(L"dxgauge") > 0 ||
                lowerName.Pos(L"dxps") > 0 ||
                lowerName.Pos(L"dxflowchart") > 0 ||
                lowerName.Pos(L"dxorgchart") > 0 ||
                lowerName.Pos(L"dxtree") > 0 ||
                lowerName.Pos(L"cxgrid") > 0 ||
                lowerName.Pos(L"cxscheduler") > 0 ||
                lowerName.Pos(L"cxpivot") > 0 ||
                lowerName.Pos(L"cxvgrid") > 0 ||
                lowerName.Pos(L"cxtreelist") > 0 ||
                lowerName.Pos(L"cxedit") > 0)
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
