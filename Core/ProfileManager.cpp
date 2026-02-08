//---------------------------------------------------------------------------
// ProfileManager implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "ProfileManager.h"
#include <IOUtils.hpp>
#include <System.IniFiles.hpp>
#include <Vcl.Forms.hpp>

namespace DxCore
{

//---------------------------------------------------------------------------
// TProfileManager implementation
//---------------------------------------------------------------------------
TProfileManager::TProfileManager()
{
}

TProfileManager::~TProfileManager()
{
}

void TProfileManager::LoadFromFile(const String& fileName)
{
    FFileName = fileName;
    LoadComponents();
}

void TProfileManager::LoadFromResource()
{
    String customFile = GetCustomProfileFileName();
    if (!FileExists(customFile))
        ExportBuiltInProfile(customFile);
    
    FFileName = customFile;
    LoadComponents();
}

void TProfileManager::ExportBuiltInProfile(const String& fileName)
{
    TResourceStream* rs = nullptr;
    try
    {
        rs = new TResourceStream(
            reinterpret_cast<NativeUInt>(HInstance),
            L"PROFILE",
            RT_RCDATA
        );
        
        std::unique_ptr<TFileStream> fs(new TFileStream(fileName, fmCreate));
        fs->CopyFrom(rs, rs->Size);
    }
    catch (...)
    {
        // Resource not found - will use empty profile
    }
    
    delete rs;
}

void TProfileManager::LoadComponents()
{
    FComponents.clear();
    
    if (!FileExists(FFileName))
        return;
        
    std::unique_ptr<TIniFile> ini(new TIniFile(FFileName));
    std::unique_ptr<TStringList> sections(new TStringList());
    
    ini->ReadSections(sections.get());
    
    for (int i = 0; i < sections->Count; i++)
    {
        String section = sections->Strings[i].Trim();
        
        auto profile = std::make_shared<TComponentProfile>();
        profile->ComponentName = section;
        
        StrToList(ini->ReadString(section, ProfileKeys::RequiredPackages, L""), 
                  profile->RequiredPackages);
        StrToList(ini->ReadString(section, ProfileKeys::OptionalPackages, L""), 
                  profile->OptionalPackages);
        StrToList(ini->ReadString(section, ProfileKeys::OutdatedPackages, L""), 
                  profile->OutdatedPackages);
        profile->IsBase = ini->ReadBool(section, ProfileKeys::IsBase, false);
        
        FComponents.push_back(profile);
    }
}

void TProfileManager::StrToList(const String& s, TStringList* list)
{
    list->Clear();
    list->CommaText = s;
    
    for (int i = list->Count - 1; i >= 0; i--)
    {
        if (list->Strings[i].Trim().IsEmpty())
            list->Delete(i);
    }
}

bool TProfileManager::IsCustomProfile() const
{
    // Profile is always stored next to the executable now.
    // Consider it "custom" (user-edited) when file timestamp
    // differs from the initial export, but for simplicity
    // always return true since the file is always external.
    return FileExists(FFileName);
}

String TProfileManager::GetCustomProfileFileName()
{
    return TPath::Combine(
        TPath::GetDirectoryName(Application->ExeName),
        L"Profile.ini"
    );
}

String TProfileManager::GetIDEVersionNumberStr(const TIDEInfoPtr& ide)
{
    // Package suffix mapping (BDS version -> Package suffix)
    // RAD Studio 12 Athens: BDS 23.0 -> suffix 290
    // RAD Studio 13 Florence: BDS 37.0 -> suffix 370
    
    String bdsVersion = ide->BDSVersion;
    
    if (bdsVersion == L"23.0")
        return L"290";  // RAD Studio 12
    else if (bdsVersion == L"37.0")
        return L"370";  // RAD Studio 13
    else
    {
        // Future versions - try to derive suffix
        // BDS 23 -> 290, BDS 37 -> 370 (multiply by ~12.6 and round to nearest 10)
        int dotPos = bdsVersion.Pos(L".");
        if (dotPos > 0)
        {
            int bdsNum = StrToIntDef(bdsVersion.SubString(1, dotPos - 1), 0);
            // For unknown versions, use BDS * 10 + 60 as approximation
            return String(bdsNum * 10 + 60);
        }
        return L"290";  // fallback
    }
}

String TProfileManager::GetComponentDir(const String& installFileDir, const String& componentName)
{
    return TPath::Combine(installFileDir, componentName);
}

String TProfileManager::GetComponentSourcesDir(const String& installFileDir, const String& componentName)
{
    return TPath::Combine(GetComponentDir(installFileDir, componentName), L"Sources");
}

String TProfileManager::GetComponentPackagesDir(const String& installFileDir, const String& componentName)
{
    return TPath::Combine(GetComponentDir(installFileDir, componentName), L"Packages");
}

String TProfileManager::GetPackageName(const String& packageBaseName, const TIDEInfoPtr& ide)
{
    return packageBaseName + GetIDEVersionNumberStr(ide);
}

String TProfileManager::GetPackageFullFileName(const String& installFileDir,
                                                const String& componentName,
                                                const String& packageBaseName,
                                                const TIDEInfoPtr& ide)
{
    String packagesDir = GetComponentPackagesDir(installFileDir, componentName);
    String packageName = GetPackageName(packageBaseName, ide);
    return TPath::Combine(packagesDir, packageName + L".dpk");
}

unsigned int TProfileManager::GetDxBuildNumber(const String& installFileDir)
{
    String sourceFile = TPath::Combine(installFileDir, 
        L"ExpressCore Library\\Sources\\dxCore.pas");
        
    if (!FileExists(sourceFile))
        return 0;
        
    const String VERSION_IDENT = L"dxVersion = ";
    const String BUILD_NUMBER_IDENT = L"dxBuildNumber: Cardinal = ";
    
    std::unique_ptr<TStringList> lines(new TStringList());
    lines->LoadFromFile(sourceFile);
    
    for (int i = 0; i < lines->Count; i++)
    {
        String line = lines->Strings[i].Trim();
        
        int pos = line.Pos(VERSION_IDENT);
        if (pos == 0)
            pos = line.Pos(BUILD_NUMBER_IDENT);
            
        if (pos > 0)
        {
            int eqPos = line.Pos(L"=");
            int semiPos = line.Pos(L";");
            
            if (eqPos > 0 && semiPos > eqPos)
            {
                String numStr = line.SubString(eqPos + 1, semiPos - eqPos - 1).Trim();
                return static_cast<unsigned int>(StrToIntDef(numStr, 0));
            }
        }
    }
    
    return 0;
}

String TProfileManager::GetDxBuildNumberAsVersion(unsigned int buildNumber)
{
    if (buildNumber == 0)
        return L"n/a";
        
    int minor = buildNumber % 10000;
    int major = (buildNumber / 10000) % 100;
    int release = minor / 100;
    int patch = minor % 100;
    
    String result = String(major) + L"." + String(release);
    if (patch != 0)
        result = result + L"." + String(patch);
        
    return result;
}

} // namespace DxCore
