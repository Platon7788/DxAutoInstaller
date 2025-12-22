//---------------------------------------------------------------------------
// ProfileManager - Manages DevExpress component profiles (INI format)
//---------------------------------------------------------------------------
#ifndef ProfileManagerH
#define ProfileManagerH

#include <System.hpp>
#include <System.Classes.hpp>
#include <System.IniFiles.hpp>
#include "Component.h"
#include "IDEDetector.h"

namespace DxCore
{

//---------------------------------------------------------------------------
// Profile Manager
//---------------------------------------------------------------------------
class TProfileManager
{
private:
    String FFileName;
    TComponentProfileList FComponents;
    
    void LoadComponents();
    void StrToList(const String& s, TStringList* list);
    
public:
    TProfileManager();
    ~TProfileManager();
    
    // Load profile from file
    void LoadFromFile(const String& fileName);
    void LoadFromResource();  // Load built-in profile
    
    // Export built-in profile to file
    void ExportBuiltInProfile(const String& fileName);
    
    // Properties
    const String& GetFileName() const { return FFileName; }
    const TComponentProfileList& GetComponents() const { return FComponents; }
    bool IsCustomProfile() const;
    
    // Path helpers
    static String GetCustomProfileFileName();
    static String GetIDEVersionNumberStr(const TIDEInfoPtr& ide);
    static String GetComponentDir(const String& installFileDir, const String& componentName);
    static String GetComponentSourcesDir(const String& installFileDir, const String& componentName);
    static String GetComponentPackagesDir(const String& installFileDir, const String& componentName);
    static String GetPackageName(const String& packageBaseName, const TIDEInfoPtr& ide);
    static String GetPackageFullFileName(const String& installFileDir, 
                                          const String& componentName,
                                          const String& packageBaseName,
                                          const TIDEInfoPtr& ide);
    
    // DevExpress version detection
    static unsigned int GetDxBuildNumber(const String& installFileDir);
    static String GetDxBuildNumberAsVersion(unsigned int buildNumber);
};

//---------------------------------------------------------------------------
// Profile INI keys
//---------------------------------------------------------------------------
namespace ProfileKeys
{
    const String RequiredPackages = L"RequiredPackages";
    const String OptionalPackages = L"OptionalPackages";
    const String OutdatedPackages = L"OutdatedPackages";
    const String IsBase = L"IsBase";
}

} // namespace DxCore

#endif
