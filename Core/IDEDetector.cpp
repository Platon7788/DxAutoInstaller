//---------------------------------------------------------------------------
// IDEDetector implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "IDEDetector.h"
#include <Registry.hpp>
#include <Winapi.TlHelp32.hpp>
#include <IOUtils.hpp>

namespace DxCore
{

//---------------------------------------------------------------------------
// TIDEInfo implementation
//---------------------------------------------------------------------------
TIDEInfo::TIDEInfo()
    : SupportsWin32(true),
      SupportsWin64(false),
      SupportsWin64Modern(false),
      Personality(TIDEPersonality::Both),
      IDEBitness(TIDEBitness::IDE32)
{
}

String TIDEInfo::GetDCC32Path() const
{
    return TPath::Combine(BinDir, L"dcc32.exe");
}

String TIDEInfo::GetDCC64Path() const
{
    return TPath::Combine(BinDir, L"dcc64.exe");
}

String TIDEInfo::GetDCC64XPath() const
{
    // dcc64x.exe is in bin64 folder, not bin
    return TPath::Combine(RootDir, L"bin64\\dcc64x.exe");
}

// Helper function to expand IDE macros like $(BDSCOMMONDIR), $(Platform)
static String ExpandIDEMacros(const String& path, const String& bdsVersion, const String& platform)
{
    String result = path;
    
    // Expand $(BDSCOMMONDIR) - typically C:\Users\Public\Documents\Embarcadero\Studio\XX.0
    if (result.Pos(L"$(BDSCOMMONDIR)") > 0)
    {
        String publicDocs = L"C:\\Users\\Public\\Documents\\Embarcadero\\Studio";
        wchar_t publicPath[MAX_PATH];
        if (GetEnvironmentVariableW(L"PUBLIC", publicPath, MAX_PATH) > 0)
        {
            publicDocs = String(publicPath) + L"\\Documents\\Embarcadero\\Studio";
        }
        String bdsCommonDir = publicDocs + L"\\" + bdsVersion;
        result = StringReplace(result, L"$(BDSCOMMONDIR)", bdsCommonDir, TReplaceFlags() << rfReplaceAll);
    }
    
    // Expand $(Platform)
    if (result.Pos(L"$(Platform)") > 0)
    {
        result = StringReplace(result, L"$(Platform)", platform, TReplaceFlags() << rfReplaceAll);
    }
    
    // Expand $(BDSBIN) - bin directory
    if (result.Pos(L"$(BDSBIN)") > 0)
    {
        // This would need RootDir, but we don't have it here
        // For now, just remove it
        result = StringReplace(result, L"$(BDSBIN)", L"", TReplaceFlags() << rfReplaceAll);
    }
    
    return result;
}

String TIDEInfo::GetBPLOutputPath(TIDEPlatform platform) const
{
    // Determine platform name for macro expansion and subfolder
    String platformName;
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64:
            platformKey = L"Win64";
            platformName = L"Win64";
            break;
        case TIDEPlatform::Win64Modern:
            platformKey = L"Win64x";
            platformName = L"Win64x";
            break;
        default:
            platformKey = L"Win32";
            platformName = L"Win32";
    }
    
    // Try to read from registry
    if (!RegistryKey.IsEmpty())
    {
        String keyPath = RegistryKey + L"\\Library\\" + platformKey;
        
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKeyReadOnly(keyPath))
        {
            if (reg->ValueExists(L"Package DPL Output"))
            {
                String path = reg->ReadString(L"Package DPL Output");
                reg->CloseKey();
                if (!path.IsEmpty())
                {
                    // Expand macros like $(BDSCOMMONDIR) and $(Platform)
                    path = ExpandIDEMacros(path, BDSVersion, platformName);
                    if (!path.IsEmpty() && path.Pos(L"$") == 0)  // No unexpanded macros
                        return path;
                }
            }
            reg->CloseKey();
        }
    }
    
    // Fallback to Public Documents path
    String publicDocs = L"C:\\Users\\Public\\Documents\\Embarcadero\\Studio";
    
    wchar_t publicPath[MAX_PATH];
    if (GetEnvironmentVariableW(L"PUBLIC", publicPath, MAX_PATH) > 0)
    {
        publicDocs = String(publicPath) + L"\\Documents\\Embarcadero\\Studio";
    }
    
    String bdsVer = BDSVersion.IsEmpty() ? L"23.0" : BDSVersion;
    String result = publicDocs + L"\\" + bdsVer + L"\\Bpl";
    
    // For Win64, add subfolder
    if (platform == TIDEPlatform::Win64)
        result = result + L"\\Win64";
    else if (platform == TIDEPlatform::Win64Modern)
        result = result + L"\\Win64x";
    
    return result;
}

String TIDEInfo::GetDCPOutputPath(TIDEPlatform platform) const
{
    // Determine platform name for macro expansion and subfolder
    String platformName;
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64:
            platformKey = L"Win64";
            platformName = L"Win64";
            break;
        case TIDEPlatform::Win64Modern:
            platformKey = L"Win64x";
            platformName = L"Win64x";
            break;
        default:
            platformKey = L"Win32";
            platformName = L"Win32";
    }
    
    // Try to read from registry
    if (!RegistryKey.IsEmpty())
    {
        String keyPath = RegistryKey + L"\\Library\\" + platformKey;
        
        std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
        reg->RootKey = HKEY_CURRENT_USER;
        
        if (reg->OpenKeyReadOnly(keyPath))
        {
            if (reg->ValueExists(L"Package DCP Output"))
            {
                String path = reg->ReadString(L"Package DCP Output");
                reg->CloseKey();
                if (!path.IsEmpty())
                {
                    // Expand macros like $(BDSCOMMONDIR) and $(Platform)
                    path = ExpandIDEMacros(path, BDSVersion, platformName);
                    if (!path.IsEmpty() && path.Pos(L"$") == 0)  // No unexpanded macros
                        return path;
                }
            }
            reg->CloseKey();
        }
    }
    
    // Fallback to Public Documents path
    String publicDocs = L"C:\\Users\\Public\\Documents\\Embarcadero\\Studio";
    
    wchar_t publicPath[MAX_PATH];
    if (GetEnvironmentVariableW(L"PUBLIC", publicPath, MAX_PATH) > 0)
    {
        publicDocs = String(publicPath) + L"\\Documents\\Embarcadero\\Studio";
    }
    
    String bdsVer = BDSVersion.IsEmpty() ? L"23.0" : BDSVersion;
    String result = publicDocs + L"\\" + bdsVer + L"\\Dcp";
    
    // For Win64, add subfolder
    if (platform == TIDEPlatform::Win64)
        result = result + L"\\Win64";
    else if (platform == TIDEPlatform::Win64Modern)
        result = result + L"\\Win64x";
    
    return result;
}
String TIDEInfo::GetHPPOutputPath(TIDEPlatform platform) const
{
    // HPP files go to the hpp folder in Public Documents
    String publicDocs = L"C:\\Users\\Public\\Documents\\Embarcadero\\Studio";
    
    wchar_t publicPath[MAX_PATH];
    if (GetEnvironmentVariableW(L"PUBLIC", publicPath, MAX_PATH) > 0)
    {
        publicDocs = String(publicPath) + L"\\Documents\\Embarcadero\\Studio";
    }
    
    String bdsVer = BDSVersion.IsEmpty() ? L"23.0" : BDSVersion;
    String result = publicDocs + L"\\" + bdsVer + L"\\hpp";
    
    // For Win64, add subfolder
    if (platform == TIDEPlatform::Win64)
        result = result + L"\\Win64";
    else if (platform == TIDEPlatform::Win64Modern)
        result = result + L"\\Win64x";
    
    return result;
}

String TIDEInfo::GetDesignTimeBPLPath() const
{
    // Design-time packages must match IDE bitness
    // Use the same BPL output path as runtime packages
    if (IDEBitness == TIDEBitness::IDE64)
        return GetBPLOutputPath(TIDEPlatform::Win64);
    else
        return GetBPLOutputPath(TIDEPlatform::Win32);
}

TIDEPlatform TIDEInfo::GetDesignTimePlatform() const
{
    // Design-time packages platform depends on IDE bitness
    if (IDEBitness == TIDEBitness::IDE64)
        return TIDEPlatform::Win64;
    else
        return TIDEPlatform::Win32;
}

String TIDEInfo::GetLibrarySearchPath(TIDEPlatform platform) const
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64:
            platformKey = L"Win64";
            break;
        case TIDEPlatform::Win64Modern:
            platformKey = L"Win64x";
            break;
        default:
            platformKey = L"Win32";
    }
    
    String keyPath = RegistryKey + L"\\Library\\" + platformKey;
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(keyPath))
    {
        if (reg->ValueExists(L"Search Path"))
            return reg->ReadString(L"Search Path");
    }
    
    return L"";
}

String TIDEInfo::GetLibraryBrowsingPath(TIDEPlatform platform) const
{
    String platformKey;
    switch (platform)
    {
        case TIDEPlatform::Win64:
            platformKey = L"Win64";
            break;
        case TIDEPlatform::Win64Modern:
            platformKey = L"Win64x";
            break;
        default:
            platformKey = L"Win32";
    }
    
    String keyPath = RegistryKey + L"\\Library\\" + platformKey;
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(keyPath))
    {
        if (reg->ValueExists(L"Browsing Path"))
            return reg->ReadString(L"Browsing Path");
    }
    
    return L"";
}

bool TIDEInfo::IsRunning() const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;
        
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    
    bool found = false;
    if (Process32FirstW(snapshot, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, L"bds.exe") == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }
    
    CloseHandle(snapshot);
    return found;
}

//---------------------------------------------------------------------------
// TIDEDetector implementation
//---------------------------------------------------------------------------
TIDEDetector::TIDEDetector()
{
}

TIDEDetector::~TIDEDetector()
{
}

void TIDEDetector::Detect()
{
    FIDEs.clear();
    ScanRegistry();
}

void TIDEDetector::ScanRegistry()
{
    const String baseKey = L"SOFTWARE\\Embarcadero\\BDS";
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(baseKey))
    {
        std::unique_ptr<TStringList> subKeys(new TStringList());
        reg->GetKeyNames(subKeys.get());
        
        for (int i = 0; i < subKeys->Count; i++)
        {
            String version = subKeys->Strings[i];
            if (version.Pos(L".") > 0)
            {
                // Only support RAD Studio 12+ (BDS 23.0+)
                int bdsNum = StrToIntDef(version.SubString(1, version.Pos(L".") - 1), 0);
                if (bdsNum < 23)
                    continue;  // Skip older versions
                
                TIDEInfoPtr ide = ParseIDEFromRegistry(version);
                if (ide && !ide->RootDir.IsEmpty())
                {
                    FIDEs.push_back(ide);
                }
            }
        }
        
        reg->CloseKey();
    }
}

TIDEInfoPtr TIDEDetector::ParseIDEFromRegistry(const String& bdsVersion)
{
    String keyPath = L"SOFTWARE\\Embarcadero\\BDS\\" + bdsVersion;
    
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (!reg->OpenKeyReadOnly(keyPath))
        return nullptr;
        
    TIDEInfoPtr ide = std::make_shared<TIDEInfo>();
    ide->BDSVersion = bdsVersion;
    ide->RegistryKey = keyPath;
    ide->Name = GetIDENameFromVersion(bdsVersion);
    
    if (reg->ValueExists(L"RootDir"))
        ide->RootDir = reg->ReadString(L"RootDir");
    else
        return nullptr;
        
    ide->BinDir = TPath::Combine(ide->RootDir, L"bin");
    
    // Check available compilers
    ide->SupportsWin32 = FileExists(ide->GetDCC32Path());
    ide->SupportsWin64 = FileExists(ide->GetDCC64Path());
    
    // Win64 Modern (dcc64x) - NOT AVAILABLE for Delphi packages!
    // dcc64x.exe does not exist - it's only bcc64x.exe for C++Builder
    // For Delphi packages (.dpk), we only have dcc32 and dcc64
    ide->SupportsWin64Modern = false;
    
    // Check if IDE supports 64-bit mode (RAD Studio 12+)
    // For RS12+, 64-bit IDE is always available as an option
    if (Supports64BitIDE(bdsVersion))
    {
        // 64-bit IDE is available - user can choose
        // Default to 32-bit, user selects in UI
        ide->IDEBitness = TIDEBitness::IDE32;
    }
    
    // Detect personality - check multiple possible key names
    reg->CloseKey();
    String personalityKey = keyPath + L"\\Personalities";
    if (reg->OpenKeyReadOnly(personalityKey))
    {
        std::unique_ptr<TStringList> values(new TStringList());
        reg->GetValueNames(values.get());
        
        bool hasDelphi = false;
        bool hasCpp = false;
        
        for (int i = 0; i < values->Count; i++)
        {
            String val = values->Strings[i].LowerCase();
            if (val.Pos(L"delphi") > 0)
                hasDelphi = true;
            if (val.Pos(L"cplus") > 0 || val.Pos(L"c++") > 0 || val.Pos(L"bcb") > 0)
                hasCpp = true;
        }
        
        if (hasDelphi && hasCpp)
            ide->Personality = TIDEPersonality::Both;
        else if (hasCpp)
            ide->Personality = TIDEPersonality::CppBuilder;
        else if (hasDelphi)
            ide->Personality = TIDEPersonality::Delphi;
        else
            ide->Personality = TIDEPersonality::Both;  // Default to Both for RAD Studio
            
        reg->CloseKey();
    }
    else
    {
        // Can't read personalities - assume RAD Studio (Both)
        ide->Personality = TIDEPersonality::Both;
    }
    
    // Extract product version
    int bdsNum = StrToIntDef(bdsVersion.SubString(1, bdsVersion.Pos(L".") - 1), 0);
    if (bdsNum == 23)
        ide->ProductVersion = L"12.0";  // RAD Studio 12
    else if (bdsNum == 37)
        ide->ProductVersion = L"13.0";  // RAD Studio 13
    else if (bdsNum >= 20)
        ide->ProductVersion = String(bdsNum - 7) + L".0";
    
    return ide;
}

String TIDEDetector::GetIDENameFromVersion(const String& bdsVersion)
{
    // RAD Studio 12+ supported
    if (bdsVersion == L"23.0") return L"RAD Studio 12 Athens";
    if (bdsVersion == L"37.0") return L"RAD Studio 13 Florence";
    
    // Future versions - show BDS version
    return L"RAD Studio (BDS " + bdsVersion + L")";
}

bool TIDEDetector::SupportsWin64Modern(const String& bdsVersion)
{
    int bdsNum = StrToIntDef(bdsVersion.SubString(1, bdsVersion.Pos(L".") - 1), 0);
    return bdsNum >= 23;  // BDS 23.0 = RAD Studio 12
}

bool TIDEDetector::Supports64BitIDE(const String& bdsVersion)
{
    int bdsNum = StrToIntDef(bdsVersion.SubString(1, bdsVersion.Pos(L".") - 1), 0);
    return bdsNum >= 23;  // 64-bit IDE available from RAD Studio 12
}

TIDEInfoPtr TIDEDetector::GetIDE(int index) const
{
    if (index >= 0 && index < static_cast<int>(FIDEs.size()))
        return FIDEs[index];
    return nullptr;
}

TIDEInfoPtr TIDEDetector::FindByName(const String& name) const
{
    for (const auto& ide : FIDEs)
    {
        if (ide->Name == name)
            return ide;
    }
    return nullptr;
}

TIDEInfoPtr TIDEDetector::FindByVersion(const String& bdsVersion) const
{
    for (const auto& ide : FIDEs)
    {
        if (ide->BDSVersion == bdsVersion)
            return ide;
    }
    return nullptr;
}

bool TIDEDetector::AnyIDERunning() const
{
    for (const auto& ide : FIDEs)
    {
        if (ide->IsRunning())
            return true;
    }
    return false;
}

String TIDEDetector::GetRegistryValue(const String& keyPath, const String& valueName)
{
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    
    if (reg->OpenKeyReadOnly(keyPath))
    {
        if (reg->ValueExists(valueName))
            return reg->ReadString(valueName);
    }
    
    return L"";
}

bool TIDEDetector::RegistryKeyExists(const String& keyPath)
{
    std::unique_ptr<TRegistry> reg(new TRegistry(KEY_READ));
    reg->RootKey = HKEY_CURRENT_USER;
    return reg->KeyExists(keyPath);
}

} // namespace DxCore
