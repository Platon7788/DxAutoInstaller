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
// IDE Type determines design-time package compilation:
// - 32-bit IDE: design-time compiled with dcc32, output to bin/
// - 64-bit IDE: design-time compiled with dcc64, output to bin64/
//
// Target platforms determine runtime library compilation:
// - CompileWin32Runtime: compile with dcc32 for Win32 target
// - CompileWin64Runtime: compile with dcc64 for Win64 target
// - CompileWin64ModernRuntime: DEPRECATED - not used (no dcc64x for Delphi)
//   Win64x support is automatic when InstallToCppBuilder + CompileWin64Runtime
//---------------------------------------------------------------------------
enum class TInstallOption
{
    AddBrowsingPath,
    NativeLookAndFeel,
    CompileWin32Runtime,        // Compile runtime for Win32 target
    CompileWin64Runtime,        // Compile runtime for Win64 target
    CompileWin64ModernRuntime,  // DEPRECATED: Win64x is auto-generated from Win64
    InstallToCppBuilder,        // Generate C++ headers/libs (enables Win64x support)
    Use64BitIDE,                // IDE runs as 64-bit (design-time via dcc64)
    UseBothIDE                  // Install for both 32-bit and 64-bit IDE
};

typedef std::set<TInstallOption> TInstallOptionSet;

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
    
    // Internal methods
    void DoSetInstallFileDir(const String& value);
    void DetectThirdPartyComponents(const TIDEInfoPtr& ide);
    void BuildComponentList(const TIDEInfoPtr& ide);
    String FindPackageFile(const String& packagesDir, 
                           const String& pkgBaseName, 
                           const String& ideSuffix);
    
    void InstallIDE(const TIDEInfoPtr& ide);
    void InstallPackage(const TIDEInfoPtr& ide, 
                        TIDEPlatform platform,
                        const TComponentPtr& component,
                        const TPackagePtr& package,
                        bool isDesignTime);
                        
    void UninstallIDE(const TIDEInfoPtr& ide);
    void UninstallPackage(const TIDEInfoPtr& ide,
                          TIDEPlatform platform,
                          const TComponentProfilePtr& component,
                          const String& packageBaseName);
    
    void UpdateProgress(const TIDEInfoPtr& ide,
                        const TComponentProfilePtr& component,
                        const String& task,
                        const String& target);
    void UpdateProgressState(const String& stateText);
    void OnCompilerOutput(const String& line);
    
    void CheckStoppedState();
    void SetState(TInstallerState value);
    
    // Registry helpers
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
    void CleanupLibraryDir(const TIDEInfoPtr& ide, TIDEPlatform platform);
    void CleanupAllCompiledFiles(const TIDEInfoPtr& ide);
    
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
    void Uninstall(const std::vector<TIDEInfoPtr>& ides);
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
    const wchar_t* const AddBrowsingPath = L"Add Browsing Path";
    const wchar_t* const NativeLookAndFeel = L"Use Native Look and Feel as Default";
    const wchar_t* const CompileWin32Runtime = L"Compile Win32 Runtime Libraries";
    const wchar_t* const CompileWin64Runtime = L"Compile Win64 Runtime Libraries";
    const wchar_t* const CompileWin64ModernRuntime = L"Compile Win64 Modern (Clang) Libraries";
    const wchar_t* const InstallToCppBuilder = L"Install to C++Builder (generate .hpp)";
    const wchar_t* const Use64BitIDE = L"64-bit IDE (design-time packages)";
}

} // namespace DxCore

#endif
