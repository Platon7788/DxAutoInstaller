//---------------------------------------------------------------------------
// Installer - Main installation/uninstallation logic
//
// IMPORTANT: Platform compilation strategy for RAD Studio 12+
//
// 1. Design-time packages (dcl*.bpl):
//    - Must match IDE bitness (32 or 64 bit)
//    - For 64-bit IDE: compile with dcc64.exe, output to bin64
//    - For 32-bit IDE: compile with dcc32.exe, output to bin
//
// 2. Runtime packages (non-dcl):
//    - Compile for each target platform user wants to develop for
//    - Win32: dcc32.exe
//    - Win64: dcc64.exe  
//    - Win64Modern: NOT directly compiled! (dcc64x doesn't exist for Delphi)
//
// 3. C++Builder Modern (Win64x/bcc64x) support:
//    - When "Install to C++Builder" is enabled and Win64 is compiled:
//      a) .hpp files are copied from Win64 to Win64x folder (identical)
//      b) .bpi files are copied from Win64 DCP to Win64x DCP folder
//      c) .a import libraries are generated using mkexp.exe from .bpl
//    - This allows C++Builder Modern projects to link against DevExpress
//
// 4. Library paths:
//    - Each platform has separate search/browsing paths in registry
//    - Win32: HKCU\...\Library\Win32
//    - Win64: HKCU\...\Library\Win64
//    - Win64x: HKCU\...\Library\Win64x (for C++Builder Modern)
//---------------------------------------------------------------------------
#ifndef InstallerH
#define InstallerH

#include <System.hpp>
#include <System.Classes.hpp>
#include <Vcl.Forms.hpp>
#include <set>
#include <map>
#include "IDEDetector.h"
#include "Component.h"
#include "ProfileManager.h"
#include "PackageCompiler.h"

namespace DxCore
{

//---------------------------------------------------------------------------
// Install options
//
// Design-time packages must match IDE bitness:
// - 32-bit IDE loads BPLs from Bpl\, registered in "Known Packages"
// - 64-bit IDE loads BPLs from Bpl\Win64\, registered in "Known Packages x64"
//
// Runtime packages are compiled for target platforms (independent of IDE bitness):
// - Win32: compiled with dcc32, used when building Win32 applications
// - Win64: compiled with dcc64, used when building Win64 applications
// - Win64x: NOT compiled (no dcc64x), artifacts generated from Win64 via mkexp
//
// C++Builder support:
// - When GenerateCppFiles is enabled, -JL flag generates .hpp, .bpi, .lib
// - For Win64x (Modern C++), .hpp/.bpi are copied from Win64, .a generated via mkexp
//---------------------------------------------------------------------------
enum class TInstallOption
{
    // Design-time registration (which IDE bitness to support)
    RegisterFor32BitIDE,        // Register design-time packages for 32-bit IDE (always enabled)
    RegisterFor64BitIDE,        // Register design-time packages for 64-bit IDE (optional)
    
    // Runtime compilation (which target platforms to support)
    CompileWin32Runtime,        // Compile runtime packages for Win32 target
    CompileWin64Runtime,        // Compile runtime packages for Win64 target
    
    // C++Builder support
    GenerateCppFiles,           // Generate .hpp/.bpi/.lib (and .a for Win64x via mkexp)
    
    // Other options
    AddBrowsingPath,            // Add source paths to IDE browsing path
    NativeLookAndFeel           // Use native look and feel as default skin
};

typedef std::set<TInstallOption> TInstallOptionSet;

//---------------------------------------------------------------------------
// Uninstall options - which IDE registrations to remove
//---------------------------------------------------------------------------
struct TUninstallOptions
{
    bool Uninstall32BitIDE;     // Remove from 32-bit IDE (Known Packages)
    bool Uninstall64BitIDE;     // Remove from 64-bit IDE (Known Packages x64)
    bool DeleteCompiledFiles;   // Delete all compiled files (.bpl, .dcp, .dcu, etc.)
    
    TUninstallOptions() 
        : Uninstall32BitIDE(true), 
          Uninstall64BitIDE(false),
          DeleteCompiledFiles(true) {}
};

//---------------------------------------------------------------------------
// Installer state
//---------------------------------------------------------------------------
enum class TInstallerState
{
    Normal,
    Running,
    Stopped,
    Error
};

//---------------------------------------------------------------------------
// Progress callback types
//---------------------------------------------------------------------------
typedef void __fastcall (__closure *TProgressCallback)(
    const TIDEInfoPtr& ide,
    const TComponentProfilePtr& component,
    const String& task,
    const String& target);
    
typedef void __fastcall (__closure *TProgressStateCallback)(const String& stateText);

//---------------------------------------------------------------------------
// Installer class
//---------------------------------------------------------------------------
class TInstaller
{
public:
    static const wchar_t* const DX_ENV_VARIABLE;  // "DXVCL"
    
private:
    std::unique_ptr<TIDEDetector> FIDEDetector;
    std::unique_ptr<TProfileManager> FProfile;
    std::unique_ptr<TPackageCompiler> FCompiler;
    
    String FInstallFileDir;
    TInstallerState FState;
    
    // Per-IDE data (key = BDS version string)
    std::map<String, TComponentList> FComponents;
    std::map<String, TInstallOptionSet> FOptions;
    std::map<String, TThirdPartyComponentSet> FThirdPartyComponents;
    
    // Callbacks
    TProgressCallback FOnProgress;
    TProgressStateCallback FOnProgressState;
    
    // Internal methods - Setup
    void DoSetInstallFileDir(const String& value);
    void DetectThirdPartyComponents(const TIDEInfoPtr& ide);
    void BuildComponentList(const TIDEInfoPtr& ide);
    String FindPackageFile(const String& packagesDir, 
                           const String& pkgBaseName, 
                           const String& ideSuffix);
    
    // Internal methods - Installation
    void InstallIDE(const TIDEInfoPtr& ide);
    void CopyAllSourcesToLibrary();
    void CopyResourcesToLibraryDir(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void CompileAllPackages(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void CompilePackage(const TIDEInfoPtr& ide, 
                        TIDEPlatform platform,
                        const TComponentPtr& component,
                        const TPackagePtr& package);
    void RegisterDesignTimePackages(const TIDEInfoPtr& ide, 
                                     TIDEPlatform platform,
                                     bool for32BitIDE, 
                                     bool for64BitIDE);
    void GenerateWin64xArtifacts(const TIDEInfoPtr& ide);
    
    // Internal methods - Uninstallation  
    void UninstallIDE(const TIDEInfoPtr& ide, const TUninstallOptions& opts);
    void DeletePackageFiles(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void CleanupLibraryDir(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void CleanupAllCompiledFiles(const TIDEInfoPtr& ide);
    
    // Progress callbacks
    void UpdateProgress(const TIDEInfoPtr& ide,
                        const TComponentProfilePtr& component,
                        const String& task,
                        const String& target);
    void UpdateProgressState(const String& stateText);
    void OnCompilerOutput(const String& line);
    
    void CheckStoppedState();
    void SetState(TInstallerState value);
    
    // Registry helpers
    void AddLibraryPaths(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void RemoveLibraryPaths(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void AddToLibraryPath(const TIDEInfoPtr& ide, 
                          TIDEPlatform platform,
                          const String& path,
                          bool isBrowsingPath);
    void RemoveFromLibraryPath(const TIDEInfoPtr& ide,
                               TIDEPlatform platform,
                               const String& path,
                               bool isBrowsingPath);
    void AddToCppPath(const TIDEInfoPtr& ide,
                      TIDEPlatform platform,
                      const String& path,
                      bool isBrowsingPath);
    void RemoveFromCppPath(const TIDEInfoPtr& ide,
                           TIDEPlatform platform,
                           const String& path,
                           bool isBrowsingPath);
    bool RegisterPackage(const TIDEInfoPtr& ide,
                         const String& bplPath,
                         const String& description,
                         bool is64BitIDE);
    void UnregisterPackage(const TIDEInfoPtr& ide, const String& bplPath, bool is64BitIDE);
    void UnregisterAllDevExpressPackages(const TIDEInfoPtr& ide, bool is64BitIDE);
    
    String GetEnvironmentVariable(const TIDEInfoPtr& ide, const String& name);
    void SetEnvironmentVariable(const TIDEInfoPtr& ide, const String& name, const String& value);
    
    // File operations
    void CopySourceFiles(const String& sourceDir, const String& destDir);
    void CopySourceFilesFiltered(const String& sourceDir, const String& destDir, 
                                  const std::set<String>& extensions);
    void DeleteCompiledFiles(const String& dir, const std::set<String>& extensions);
    void DeleteDevExpressFilesFromDir(const String& dir, const std::set<String>& extensions);
    
public:
    TInstaller();
    ~TInstaller();
    
    // Initialize
    void Initialize();
    
    // Properties
    TIDEDetector* GetIDEDetector() const { return FIDEDetector.get(); }
    TProfileManager* GetProfile() const { return FProfile.get(); }
    
    String GetInstallFileDir() const { return FInstallFileDir; }
    void SetInstallFileDir(const String& value);
    
    TInstallerState GetState() const { return FState; }
    
    // Get components for IDE
    const TComponentList& GetComponents(const TIDEInfoPtr& ide) const;
    
    // Get/Set options for IDE
    TInstallOptionSet GetOptions(const TIDEInfoPtr& ide) const;
    void SetOptions(const TIDEInfoPtr& ide, const TInstallOptionSet& options);
    
    // Get/Set third-party components for IDE
    TThirdPartyComponentSet GetThirdPartyComponents(const TIDEInfoPtr& ide) const;
    void SetThirdPartyComponents(const TIDEInfoPtr& ide, const TThirdPartyComponentSet& components);
    
    // Install/Uninstall
    void Install(const std::vector<TIDEInfoPtr>& ides);
    void Uninstall(const std::vector<TIDEInfoPtr>& ides, const TUninstallOptions& uninstallOpts);
    void Stop();
    
    // Search for new packages not in profile
    void SearchNewPackages(TStringList* list);
    
    // Path helpers
    static String GetInstallLibraryDir(const String& installFileDir, 
                                        const TIDEInfoPtr& ide = nullptr,
                                        TIDEPlatform platform = TIDEPlatform::Win32);
    static String GetInstallSourcesDir(const String& installFileDir);
    
    // Callbacks
    void SetOnProgress(TProgressCallback callback) { FOnProgress = callback; }
    void SetOnProgressState(TProgressStateCallback callback) { FOnProgressState = callback; }
};

//---------------------------------------------------------------------------
// Install option names for UI
//---------------------------------------------------------------------------
namespace InstallOptionNames
{
    const wchar_t* const RegisterFor32BitIDE = L"32-bit IDE (design-time packages)";
    const wchar_t* const RegisterFor64BitIDE = L"64-bit IDE (design-time packages)";
    const wchar_t* const CompileWin32Runtime = L"Compile Win32 Runtime Libraries";
    const wchar_t* const CompileWin64Runtime = L"Compile Win64 Runtime Libraries";
    const wchar_t* const GenerateCppFiles = L"Generate C++ files (.hpp/.bpi/.a)";
    const wchar_t* const AddBrowsingPath = L"Add Browsing Path";
    const wchar_t* const NativeLookAndFeel = L"Use Native Look and Feel as Default";
}

} // namespace DxCore

#endif
