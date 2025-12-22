//---------------------------------------------------------------------------
// Component implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "Component.h"
#include <IOUtils.hpp>

namespace DxCore
{

//---------------------------------------------------------------------------
// DPK file parsing constants
//---------------------------------------------------------------------------
const String DPK_DESCRIPTION_IDENT = L"{$DESCRIPTION '";
const String DPK_DESIGNTIME_ONLY = L"{$DESIGNONLY";
const String DPK_RUNTIME_ONLY = L"{$RUNONLY";
const String DPK_REQUIRES_IDENT = L"requires";

//---------------------------------------------------------------------------
// TPackage implementation
//---------------------------------------------------------------------------
TPackage::TPackage(const String& fullFileName)
    : FullFileName(fullFileName),
      Category(TPackageCategory::Normal),
      Usage(TPackageUsage::DesigntimeAndRuntime),
      Exists(false),
      Required(true)
{
    Requires = new TStringList();
    
    // Extract name without extension
    Name = TPath::GetFileNameWithoutExtension(fullFileName);
    
    // Check if file exists
    Exists = FileExists(fullFileName);
    
    // Detect category from name
    DetectCategory();
    
    // Parse DPK if exists
    if (Exists)
        ParseDPKFile();
}

TPackage::~TPackage()
{
    delete Requires;
}

void TPackage::DetectCategory()
{
    String upperName = Name.UpperCase();
    
    if (upperName.Pos(L"IBX") > 0)
        Category = TPackageCategory::IBX;
    else if (upperName.Pos(L"TEECHART") > 0)
        Category = TPackageCategory::TeeChart;
    else if (upperName.Pos(L"FIREDAC") > 0)
        Category = TPackageCategory::FireDAC;
    else if (upperName.Pos(L"BDE") > 0)
        Category = TPackageCategory::BDE;
    else
        Category = TPackageCategory::Normal;
}

void TPackage::ParseDPKFile()
{
    if (!Exists)
        return;
        
    std::unique_ptr<TStringList> dpk(new TStringList());
    dpk->LoadFromFile(FullFileName);
    
    bool inRequiresPart = false;
    
    for (int i = 0; i < dpk->Count; i++)
    {
        String line = dpk->Strings[i].Trim();
        String upperLine = line.UpperCase();
        
        if (inRequiresPart)
        {
            // Parse requires section
            if (line.Pos(L",") > 0)
            {
                String pkg = StringReplace(line, L",", L"", TReplaceFlags());
                Requires->Add(pkg.Trim());
            }
            else
            {
                String pkg = StringReplace(line, L";", L"", TReplaceFlags());
                Requires->Add(pkg.Trim());
                break;  // End of requires section
            }
        }
        else
        {
            // Parse options
            if (line.Pos(DPK_DESCRIPTION_IDENT) > 0)
            {
                int start = line.Pos(DPK_DESCRIPTION_IDENT) + DPK_DESCRIPTION_IDENT.Length();
                int end = line.LastDelimiter(L"'");
                if (end > start)
                    Description = line.SubString(start, end - start);
            }
            // Check for {$DESIGNONLY} or {$DESIGNONLY ON}
            else if (upperLine.Pos(L"{$DESIGNONLY") > 0)
            {
                Usage = TPackageUsage::DesigntimeOnly;
            }
            // Check for {$RUNONLY} or {$RUNONLY ON}
            else if (upperLine.Pos(L"{$RUNONLY") > 0)
            {
                Usage = TPackageUsage::RuntimeOnly;
            }
            else if (line.LowerCase() == DPK_REQUIRES_IDENT)
            {
                inRequiresPart = true;
            }
        }
    }
}

void TPackage::ReadOptions()
{
    ParseDPKFile();
}

//---------------------------------------------------------------------------
// TComponentProfile implementation
//---------------------------------------------------------------------------
TComponentProfile::TComponentProfile()
    : IsBase(false)
{
    RequiredPackages = new TStringList();
    OptionalPackages = new TStringList();
    OutdatedPackages = new TStringList();
}

TComponentProfile::~TComponentProfile()
{
    delete RequiredPackages;
    delete OptionalPackages;
    delete OutdatedPackages;
}

//---------------------------------------------------------------------------
// TComponent implementation
//---------------------------------------------------------------------------
TComponent::TComponent(TComponentProfilePtr profile)
    : Profile(profile),
      State(TComponentState::Install)
{
}

TComponent::~TComponent()
{
}

int TComponent::GetExistsPackageCount() const
{
    int count = 0;
    for (const auto& pkg : Packages)
    {
        if (pkg->Exists)
            count++;
    }
    return count;
}

bool TComponent::IsMissingDependents() const
{
    for (const auto* parent : ParentComponents)
    {
        if (parent->State != TComponentState::Install && 
            parent->State != TComponentState::NotInstall)
        {
            return true;
        }
    }
    return false;
}

void TComponent::SetState(TComponentState value)
{
    if (State == value)
        return;
        
    // Can only change state if currently editable
    if (State != TComponentState::Install && State != TComponentState::NotInstall)
        return;
        
    // Propagate state changes
    switch (value)
    {
        case TComponentState::Install:
            // When installing, also install parent components
            for (auto* parent : ParentComponents)
                parent->SetState(TComponentState::Install);
            break;
            
        case TComponentState::NotInstall:
            // When not installing, also don't install sub components
            for (auto* sub : SubComponents)
                sub->SetState(TComponentState::NotInstall);
            break;
            
        default:
            break;
    }
    
    State = value;
}

} // namespace DxCore
