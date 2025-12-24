//---------------------------------------------------------------------------
// IDEDetector - Detects installed RAD Studio IDEs
// Direct Windows Registry access - no JCL dependency
//
// SUPPORTED: RAD Studio 12 Athens (BDS 23.0), RAD Studio 13 Florence (BDS 37.0)
// Older versions are ignored.
//---------------------------------------------------------------------------
#ifndef IDEDetectorH
#define IDEDetectorH

#include <System.hpp>
#include <System.Classes.hpp>
#include <System.SysUtils.hpp>
#include <System.Generics.Collections.hpp>
#include <vector>
#include <memory>
#include <set>

namespace DxCore
{

//---------------------------------------------------------------------------
// IDE Platform enumeration
// 
// For Delphi packages (.dpk) compilation:
// - Win32: dcc32.exe
// - Win64: dcc64.exe
// - Win64Modern: NOT SUPPORTED for Delphi! (dcc64x doesn't exist)
//
// Note: bcc64x.exe is for C++Builder only, not for Delphi packages.
// DevExpress VCL packages are Delphi packages, so we use dcc32/dcc64.
//
// For IDE itself (RAD Studio 12+):
// - IDE can run as 32-bit or 64-bit process
// - Design-time packages must match IDE bitness
// - If IDE is 64-bit, design-time BPLs must be 64-bit (compiled with dcc64)
//---------------------------------------------------------------------------
enum class TIDEPlatform
{
    Win32,
    Win64,
    Win64Modern  // LLVM/Clang - RAD Studio 12+
};

//---------------------------------------------------------------------------
// IDE Personality (Delphi, C++Builder, or both)
//---------------------------------------------------------------------------
enum class TIDEPersonality
{
    Delphi,
    CppBuilder,
    Both  // RAD Studio
};

//---------------------------------------------------------------------------
// IDE Bitness - whether IDE itself runs as 32 or 64 bit
//---------------------------------------------------------------------------
enum class TIDEBitness
{
    IDE32,      // IDE runs as 32-bit process
    IDE64       // IDE runs as 64-bit process (RAD Studio 12+)
};

//---------------------------------------------------------------------------
// IDE Information structure
//---------------------------------------------------------------------------
class TIDEInfo
{
public:
    String Name;              // "RAD Studio 12 Athens"
    String BDSVersion;        // "23.0"
    String ProductVersion;    // "12.0"
    String RootDir;           // Installation directory
    String BinDir;            // Bin directory (compilers)
    TIDEPersonality Personality;
    TIDEBitness IDEBitness;   // 32 or 64 bit IDE
    
    // Supported platforms for compilation
    bool SupportsWin32;
    bool SupportsWin64;
    bool SupportsWin64Modern;
    
    // Registry paths
    String RegistryKey;       // Full registry key path
    
    // Compiler paths
    String GetDCC32Path() const;
    String GetDCC64Path() const;
    String GetDCC64XPath() const;  // Modern compiler
    String GetMkExpPath() const;   // mkexp.exe for generating import library from .bpl (use -p flag)
    
    // Output paths - for compiled packages
    String GetBPLOutputPath(TIDEPlatform platform) const;
    String GetDCPOutputPath(TIDEPlatform platform) const;
    String GetHPPOutputPath(TIDEPlatform platform) const;  // For C++Builder
    
    // Design-time BPL path - depends on IDE bitness!
    // For 64-bit IDE, design-time packages go to Win64 BPL folder
    String GetDesignTimeBPLPath() const;
    TIDEPlatform GetDesignTimePlatform() const;
    
    // Library paths (from registry)
    String GetLibrarySearchPath(TIDEPlatform platform) const;
    String GetLibraryBrowsingPath(TIDEPlatform platform) const;
    
    // Check if IDE is running
    bool IsRunning() const;
    
    TIDEInfo();
};

typedef std::shared_ptr<TIDEInfo> TIDEInfoPtr;
typedef std::vector<TIDEInfoPtr> TIDEList;

//---------------------------------------------------------------------------
// IDE Detector class
//---------------------------------------------------------------------------
class TIDEDetector
{
private:
    TIDEList FIDEs;
    
    void ScanRegistry();
    TIDEInfoPtr ParseIDEFromRegistry(const String& bdsVersion);
    String GetRegistryValue(const String& keyPath, const String& valueName);
    bool RegistryKeyExists(const String& keyPath);
    
    // Version mapping
    static String GetIDENameFromVersion(const String& bdsVersion);
    static bool SupportsWin64Modern(const String& bdsVersion);
    static bool Supports64BitIDE(const String& bdsVersion);
    
public:
    TIDEDetector();
    ~TIDEDetector();
    
    // Detect all installed IDEs
    void Detect();
    
    // Access detected IDEs
    const TIDEList& GetIDEs() const { return FIDEs; }
    int GetCount() const { return static_cast<int>(FIDEs.size()); }
    TIDEInfoPtr GetIDE(int index) const;
    TIDEInfoPtr FindByName(const String& name) const;
    TIDEInfoPtr FindByVersion(const String& bdsVersion) const;
    
    // Check if any IDE is running
    bool AnyIDERunning() const;
};

//---------------------------------------------------------------------------
// BDS Version constants (only supported versions)
//
// RAD Studio 12 Athens:   BDS 23.0, Package suffix 290
// RAD Studio 13 Florence: BDS 37.0, Package suffix 370
//---------------------------------------------------------------------------
namespace BDSVersions
{
    const wchar_t* const BDS_23_0 = L"23.0";  // RAD Studio 12 Athens (suffix 290)
    const wchar_t* const BDS_37_0 = L"37.0";  // RAD Studio 13 Florence (suffix 370)
    
    const int MIN_SUPPORTED_BDS = 23;  // Minimum supported BDS version
}

//---------------------------------------------------------------------------
// Platform names for paths
//---------------------------------------------------------------------------
namespace PlatformNames
{
    const wchar_t* const Win32 = L"Win32";
    const wchar_t* const Win64 = L"Win64";
    const wchar_t* const Win64Modern = L"Win64x";
}

} // namespace DxCore

#endif
