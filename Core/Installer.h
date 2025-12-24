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
//    - Win32: dcc32.exe with -JL flag
//    - Win64: dcc64.exe with -JL flag (generates .a files)
//    - Win64Modern: dcc64.exe with -JL -jf:coffi -DDX_WIN64_MODERN flags
//      (generates COFF .lib files for bcc64x/ld.lld linker)
//
// 3. C++Builder Modern (Win64x/bcc64x) support:
//    - Win64x IS compiled separately with special flags!
//    - Uses dcc64.exe (same as Win64) but with additional flags:
//      -jf:coffi - generate COFF format .lib instead of ELF .a
//      -DDX_WIN64_MODERN - define for conditional compilation
//    - Output goes to separate Win64x directories
//
// 4. Library paths:
//    - Each platform has separate search/browsing paths in registry
//    - Win32: HKCU\...\Library\Win32
//    - Win64: HKCU\...\Library\Win64
//    - Win64x: HKCU\...\Library\Win64x (for C++Builder Modern)
//
// 5. Threading model:
//    - Heavy work (compilation, file copying) runs in background thread
//    - UI updates are synchronized via TThread::Queue
//    - Stop flag is atomic for thread-safe cancellation
//---------------------------------------------------------------------------
#ifndef InstallerH
#define InstallerH

#include <System.hpp>
#include <System.Classes.hpp>
#include <Vcl.Forms.hpp>
#include <set>
#include <map>
#include <atomic>
#include <functional>
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
// - Win32: compiled with dcc32 -JL, generates .lib (OMF format)
// - Win64: compiled with dcc64 -JL, generates .a (ELF format)
// - Win64x: compiled with dcc64 -JL -jf:coffi -DDX_WIN64_MODERN, generates .lib (COFF format)
//
// C++Builder support:
// - When GenerateCppFiles is enabled, -JL flag generates .hpp, .bpi, .lib/.a
// - Win32 generates .lib (OMF format for classic bcc32)
// - Win64 generates .a (ELF format for classic bcc64)
// - Win64x generates .lib (COFF format for modern bcc64x/ld.lld)
//---------------------------------------------------------------------------
enum class TInstallOption
{
    // Design-time registration (which IDE bitness to support)
    RegisterFor32BitIDE,        // Register design-time packages for 32-bit IDE
    RegisterFor64BitIDE,        // Register design-time packages for 64-bit IDE
    
    // Runtime compilation (which target platforms to support)
    CompileWin32Runtime,        // Compile runtime packages for Win32 target
    CompileWin64Runtime,        // Compile runtime packages for Win64 target (ELF, .a)
    CompileWin64xRuntime,       // Compile runtime packages for Win64x target (COFF, .lib)
    
    // C++Builder support
    GenerateCppFiles,           // Generate .hpp/.bpi/.lib/.a
    
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

typedef std::function<void(bool success, const String& message)> TCompletionCallback;

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
    std::atomic<bool> FStopped{false};  // Thread-safe stop flag
    
    // Per-IDE data (key = BDS version string)
    std::map<String, TComponentList> FComponents;
    std::map<String, TInstallOptionSet> FOptions;
    std::map<String, TThirdPartyComponentSet> FThirdPartyComponents;
    
    // Callbacks
    TProgressCallback FOnProgress;
    TProgressStateCallback FOnProgressState;
    TCompletionCallback FOnComplete;
    
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
    void AddToCppIncludePath(const TIDEInfoPtr& ide,
                              TIDEPlatform platform,
                              const String& path);
    void RemoveFromCppIncludePath(const TIDEInfoPtr& ide,
                                   TIDEPlatform platform,
                                   const String& path);
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
    
    // Install/Uninstall (synchronous - for backward compatibility)
    void Install(const std::vector<TIDEInfoPtr>& ides);
    void Uninstall(const std::vector<TIDEInfoPtr>& ides, const TUninstallOptions& uninstallOpts);
    
    // Install/Uninstall (asynchronous - runs in background thread)
    void InstallAsync(const std::vector<TIDEInfoPtr>& ides);
    void UninstallAsync(const std::vector<TIDEInfoPtr>& ides, const TUninstallOptions& uninstallOpts);
    
    void Stop();
    bool IsStopped() const { return FStopped.load(); }
    
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
    void SetOnComplete(TCompletionCallback callback) { FOnComplete = callback; }
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
