//---------------------------------------------------------------------------
// Component - DevExpress component and package definitions
//---------------------------------------------------------------------------
#ifndef ComponentH
#define ComponentH

#include <System.hpp>
#include <System.Classes.hpp>
#include <System.Generics.Collections.hpp>
#include <vector>
#include <memory>
#include <set>
#include <map>

namespace DxCore
{

//---------------------------------------------------------------------------
// Package category (for third-party dependencies)
//---------------------------------------------------------------------------
enum class TPackageCategory
{
    Normal,
    IBX,        // InterBase Express
    TeeChart,   // TeeChart
    FireDAC,    // FireDAC
    BDE         // Borland Database Engine (legacy)
};

//---------------------------------------------------------------------------
// Package usage type
//---------------------------------------------------------------------------
enum class TPackageUsage
{
    DesigntimeOnly,
    RuntimeOnly,
    DesigntimeAndRuntime
};

//---------------------------------------------------------------------------
// Component state
//---------------------------------------------------------------------------
enum class TComponentState
{
    Install,        // Will be installed
    NotInstall,     // Will not be installed
    NotFound,       // Source files not found
    NotSupported,   // Not supported by this IDE
    Missing         // Missing dependencies
};

//---------------------------------------------------------------------------
// Package information
//---------------------------------------------------------------------------
class TPackage
{
public:
    String FullFileName;      // Full path to .dpk file
    String Name;              // Package name without extension
    String Description;       // From {$DESCRIPTION}
    TPackageCategory Category;
    TPackageUsage Usage;
    TStringList* Requires;    // Required packages
    bool Exists;              // File exists
    bool Required;            // Is required package (not optional)
    
    TPackage(const String& fullFileName);
    ~TPackage();
    
    void ReadOptions();       // Parse .dpk file
    
private:
    void DetectCategory();
    void ParseDPKFile();
};

typedef std::shared_ptr<TPackage> TPackagePtr;
typedef std::vector<TPackagePtr> TPackageList;

//---------------------------------------------------------------------------
// Component profile (from INI)
//---------------------------------------------------------------------------
class TComponentProfile
{
public:
    String ComponentName;
    TStringList* RequiredPackages;
    TStringList* OptionalPackages;
    TStringList* OutdatedPackages;
    bool IsBase;              // Base component (always needed)
    
    TComponentProfile();
    ~TComponentProfile();
};

typedef std::shared_ptr<TComponentProfile> TComponentProfilePtr;
typedef std::vector<TComponentProfilePtr> TComponentProfileList;

//---------------------------------------------------------------------------
// Component (runtime representation)
//---------------------------------------------------------------------------
class TComponent
{
public:
    TComponentProfilePtr Profile;
    TPackageList Packages;
    TComponentState State;
    
    // Dependencies
    std::vector<TComponent*> ParentComponents;  // Components this depends on
    std::vector<TComponent*> SubComponents;     // Components that depend on this
    
    TComponent(TComponentProfilePtr profile);
    ~TComponent();
    
    int GetExistsPackageCount() const;
    bool IsMissingDependents() const;
    void SetState(TComponentState value);
};

typedef std::shared_ptr<TComponent> TComponentPtr;
typedef std::vector<TComponentPtr> TComponentList;

//---------------------------------------------------------------------------
// Third-party components detection
//---------------------------------------------------------------------------
enum class TThirdPartyComponent
{
    IBX,
    TeeChart,
    FireDAC,
    BDE
};

// Use std::set for third-party components
typedef std::set<TThirdPartyComponent> TThirdPartyComponentSet;

} // namespace DxCore

#endif
