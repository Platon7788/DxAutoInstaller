//---------------------------------------------------------------------------
// MainForm implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "MainForm.h"
#include <Vcl.FileCtrl.hpp>
#include <IOUtils.hpp>
#include <ShellAPI.h>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMain *frmMain;

//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent* Owner)
    : TForm(Owner),
      FSelectedIDE(nullptr)
{
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormCreate(TObject *Sender)
{
    Caption = Application->Title;
    LblAppName->Caption = Application->Title;
    LblVersion->Caption = L"v1.0.0 C++ Edition";
    
    // Initialize installer
    FInstaller = std::make_unique<DxCore::TInstaller>();
    FInstaller->Initialize();
    FInstaller->SetOnProgress(OnProgress);
    FInstaller->SetOnProgressState(OnProgressState);
    
    // Create progress form
    FProgressForm = new TfrmProgress(this);
    FProgressForm->SetInstaller(FInstaller.get());
    
    // Initialize UI
    PageFuns->ActivePage = TabInstall;
    InitializeIDEList();
    InitializeUninstallList();
    InitializeProfileInfo();
    
    // Initial state
    UpdateControlStates();
    
    // Setup links
    LinkDownload->Caption = L"<a href=\"https://github.com/Platon7788/DxAutoInstaller\">GitHub Repository</a>";
    LinkEmail->Caption = L"<a href=\"mailto:vteme777@gmail.com\">vteme777@gmail.com</a>";
    
    // Load changelog
    MemoChangelog->Lines->Add(L"v1.0.0 - C++ Edition (2025)");
    MemoChangelog->Lines->Add(L"");
    MemoChangelog->Lines->Add(L"  Author: Platon (vteme777@gmail.com)");
    MemoChangelog->Lines->Add(L"  Based on original Delphi version by Delphier");
    MemoChangelog->Lines->Add(L"");
    MemoChangelog->Lines->Add(L"  NEW FEATURES:");
    MemoChangelog->Lines->Add(L"  - Rewritten in C++Builder (x64 Modern / Clang)");
    MemoChangelog->Lines->Add(L"  - No JCL dependency - direct Windows Registry access");
    MemoChangelog->Lines->Add(L"  - No DevExpress UI dependency - standard VCL");
    MemoChangelog->Lines->Add(L"  - Win64 Modern (Clang/LLVM) platform support");
    MemoChangelog->Lines->Add(L"  - 64-bit IDE support for design-time packages");
    MemoChangelog->Lines->Add(L"  - Both 32 and 64-bit IDE installation in one pass");
    MemoChangelog->Lines->Add(L"  - RAD Studio 12/13 support");
    MemoChangelog->Lines->Add(L"  - Clean uninstall - removes all compiled files");
    MemoChangelog->Lines->Add(L"  - Optimized file copying (sources to one location)");
    MemoChangelog->Lines->Add(L"  - DevExpress VCL 25.1.x support");
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormDestroy(TObject *Sender)
{
    delete FProgressForm;
}

//---------------------------------------------------------------------------
void TfrmMain::InitializeIDEList()
{
    CheckListIDEs->Clear();
    
    auto detector = FInstaller->GetIDEDetector();
    for (int i = 0; i < detector->GetCount(); i++)
    {
        auto ide = detector->GetIDE(i);
        CheckListIDEs->Items->AddObject(ide->Name, reinterpret_cast<TObject*>(static_cast<intptr_t>(i)));
    }
    
    if (CheckListIDEs->Items->Count > 0)
    {
        CheckListIDEs->ItemIndex = 0;
        FSelectedIDE = detector->GetIDE(0);
    }
}

//---------------------------------------------------------------------------
void TfrmMain::InitializeUninstallList()
{
    CheckListUninstall->Clear();
    
    auto detector = FInstaller->GetIDEDetector();
    for (int i = 0; i < detector->GetCount(); i++)
    {
        auto ide = detector->GetIDE(i);
        CheckListUninstall->Items->AddObject(ide->Name, reinterpret_cast<TObject*>(static_cast<intptr_t>(i)));
    }
}

//---------------------------------------------------------------------------
void TfrmMain::InitializeProfileInfo()
{
    auto profile = FInstaller->GetProfile();
    
    if (profile->IsCustomProfile())
        LblCurrentProfile->Caption = L"Current Profile: <Custom>";
    else
        LblCurrentProfile->Caption = L"Current Profile: <Built-in>";
        
    String customFile = DxCore::TProfileManager::GetCustomProfileFileName();
    BtnDeleteProfile->Visible = FileExists(customFile);
    BtnExportProfile->Visible = !FileExists(customFile);
}

//---------------------------------------------------------------------------
void TfrmMain::RefreshComponentList()
{
    CheckListComponents->Clear();
    
    if (FSelectedIDE == nullptr)
        return;
    
    bool hideBase = ChkHideBase->Checked;
    const auto& components = FInstaller->GetComponents(FSelectedIDE);
    
    if (components.empty())
    {
        CheckListComponents->Items->Add(L"[No components found - check source directory]");
        return;
    }
    
    for (const auto& comp : components)
    {
        // Show all components for now, including base
        if (hideBase && comp->Profile->IsBase)
            continue;
        
        String itemText = comp->Profile->ComponentName;
        bool canCheck = true;
        bool isChecked = false;
        
        switch (comp->State)
        {
            case DxCore::TComponentState::Install:
                itemText = itemText + L" [" + String(comp->GetExistsPackageCount()) + L" packages]";
                isChecked = true;
                break;
            case DxCore::TComponentState::NotInstall:
                itemText = itemText + L" [" + String(comp->GetExistsPackageCount()) + L" packages]";
                break;
            case DxCore::TComponentState::NotFound:
                itemText = itemText + L" [not found]";
                canCheck = false;
                break;
            case DxCore::TComponentState::NotSupported:
                itemText = itemText + L" [not supported]";
                canCheck = false;
                break;
            case DxCore::TComponentState::Missing:
                itemText = itemText + L" [missing deps]";
                break;
        }
        
        int idx = CheckListComponents->Items->Add(itemText);
        CheckListComponents->Items->Objects[idx] = reinterpret_cast<TObject*>(comp.get());
        CheckListComponents->Checked[idx] = isChecked;
        CheckListComponents->ItemEnabled[idx] = canCheck;
    }
}

//---------------------------------------------------------------------------
void TfrmMain::RefreshIDEList()
{
    for (int i = 0; i < CheckListIDEs->Items->Count; i++)
    {
        intptr_t ideIndex = reinterpret_cast<intptr_t>(CheckListIDEs->Items->Objects[i]);
        auto ide = FInstaller->GetIDEDetector()->GetIDE(static_cast<int>(ideIndex));
        
        const auto& components = FInstaller->GetComponents(ide);
        int installCount = 0;
        for (const auto& comp : components)
        {
            if (comp->State == DxCore::TComponentState::Install)
                installCount++;
        }
        
        CheckListIDEs->Items->Strings[i] = ide->Name + L" (" + String(installCount) + L")";
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::CheckListIDEsClick(TObject *Sender)
{
    int idx = CheckListIDEs->ItemIndex;
    if (idx < 0)
        return;
    
    if (FSelectedIDE != nullptr)
        SaveOptionsForSelectedIDE();
    
    intptr_t ideIndex = reinterpret_cast<intptr_t>(CheckListIDEs->Items->Objects[idx]);
    FSelectedIDE = FInstaller->GetIDEDetector()->GetIDE(static_cast<int>(ideIndex));
    
    UpdateControlStates();
    UpdateOptionsForSelectedIDE();
    
    if (!EditSourceDir->Text.IsEmpty())
        RefreshComponentList();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::CheckListComponentsClickCheck(TObject *Sender)
{
    // Update component states based on checkbox
    for (int i = 0; i < CheckListComponents->Items->Count; i++)
    {
        DxCore::TComponent* comp = reinterpret_cast<DxCore::TComponent*>(CheckListComponents->Items->Objects[i]);
        if (comp != nullptr && CheckListComponents->ItemEnabled[i])
        {
            comp->State = CheckListComponents->Checked[i] ? 
                DxCore::TComponentState::Install : DxCore::TComponentState::NotInstall;
        }
    }
    RefreshIDEList();
}

//---------------------------------------------------------------------------
void TfrmMain::UpdateControlStates()
{
    bool hasIDE = (FSelectedIDE != nullptr);
    bool hasSourceDir = !EditSourceDir->Text.IsEmpty();
    
    // IDE Options - 32-bit always enabled, 64-bit optional for RS12+
    GroupIDEOptions->Enabled = hasIDE;
    if (hasIDE)
    {
        int bdsNum = StrToIntDef(FSelectedIDE->BDSVersion.SubString(1, 
            FSelectedIDE->BDSVersion.Pos(L".") - 1), 0);
        ChkIDE64->Enabled = (bdsNum >= 23);  // 64-bit IDE available from RS12
    }
    else
    {
        ChkIDE64->Enabled = false;
    }
    
    // Target platforms
    GroupTargets->Enabled = hasIDE && hasSourceDir;
    if (hasIDE && hasSourceDir)
    {
        ChkTargetWin32->Enabled = true;
        ChkTargetWin64->Enabled = FSelectedIDE->SupportsWin64;
        ChkTargetWin64x->Enabled = FSelectedIDE->SupportsWin64Modern;
    }
    else
    {
        ChkTargetWin32->Enabled = false;
        ChkTargetWin64->Enabled = false;
        ChkTargetWin64x->Enabled = false;
    }
    
    // Other options
    GroupOtherOptions->Enabled = hasIDE && hasSourceDir;
    if (hasIDE && hasSourceDir)
    {
        ChkNativeLookAndFeel->Enabled = true;
        ChkGenerateCpp->Enabled = FSelectedIDE->Personality != DxCore::TIDEPersonality::Delphi;
        ChkHideBase->Enabled = true;
    }
    else
    {
        ChkNativeLookAndFeel->Enabled = false;
        ChkGenerateCpp->Enabled = false;
        ChkHideBase->Enabled = false;
    }
    
    CheckListComponents->Enabled = hasSourceDir;
    ActInstall->Enabled = hasSourceDir;
}

//---------------------------------------------------------------------------
void TfrmMain::UpdateOptionsForSelectedIDE()
{
    if (FSelectedIDE == nullptr)
        return;
    
    DxCore::TInstallOptionSet opts = FInstaller->GetOptions(FSelectedIDE);
    
    // IDE options - 32-bit always checked, 64-bit optional
    ChkIDE64->Checked = opts.count(DxCore::TInstallOption::RegisterFor64BitIDE) > 0;
    
    // Target platforms
    ChkTargetWin32->Checked = opts.count(DxCore::TInstallOption::CompileWin32Runtime) > 0;
    ChkTargetWin64->Checked = opts.count(DxCore::TInstallOption::CompileWin64Runtime) > 0;
    ChkTargetWin64x->Checked = opts.count(DxCore::TInstallOption::CompileWin64xRuntime) > 0;
    
    // Other options
    ChkNativeLookAndFeel->Checked = opts.count(DxCore::TInstallOption::NativeLookAndFeel) > 0;
    ChkGenerateCpp->Checked = opts.count(DxCore::TInstallOption::GenerateCppFiles) > 0;
    ChkGenerateCpp->Enabled = FSelectedIDE->Personality != DxCore::TIDEPersonality::Delphi;
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::ChkIDE64Click(TObject *Sender)
{
    // When 64-bit IDE is enabled, ensure Win64 runtime is also enabled
    if (ChkIDE64->Checked && !ChkTargetWin64->Checked)
        ChkTargetWin64->Checked = true;
}

//---------------------------------------------------------------------------
void TfrmMain::SaveOptionsForSelectedIDE()
{
    if (FSelectedIDE == nullptr)
        return;
    
    DxCore::TInstallOptionSet opts;
    
    // IDE registration - 32-bit always, 64-bit optional
    opts.insert(DxCore::TInstallOption::RegisterFor32BitIDE);
    if (ChkIDE64->Checked)
        opts.insert(DxCore::TInstallOption::RegisterFor64BitIDE);
    
    // Target platforms
    if (ChkTargetWin32->Checked && ChkTargetWin32->Enabled)
        opts.insert(DxCore::TInstallOption::CompileWin32Runtime);
    if (ChkTargetWin64->Checked && ChkTargetWin64->Enabled)
        opts.insert(DxCore::TInstallOption::CompileWin64Runtime);
    if (ChkTargetWin64x->Checked && ChkTargetWin64x->Enabled)
        opts.insert(DxCore::TInstallOption::CompileWin64xRuntime);
    
    // Other options
    opts.insert(DxCore::TInstallOption::AddBrowsingPath);
    if (ChkNativeLookAndFeel->Checked)
        opts.insert(DxCore::TInstallOption::NativeLookAndFeel);
    if (ChkGenerateCpp->Checked && ChkGenerateCpp->Enabled)
        opts.insert(DxCore::TInstallOption::GenerateCppFiles);
    
    FInstaller->SetOptions(FSelectedIDE, opts);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::TargetCheckBoxClick(TObject *Sender)
{
    // Ensure at least one target is selected
    if (!ChkTargetWin32->Checked && !ChkTargetWin64->Checked)
    {
        TCheckBox* cb = dynamic_cast<TCheckBox*>(Sender);
        if (cb != nullptr)
            cb->Checked = true;
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::BtnBrowseClick(TObject *Sender)
{
    String dir;
    if (SelectDirectory(L"Select DevExpress Source Directory", L"", dir))
    {
        EditSourceDir->Text = dir;
        
        unsigned int buildNum = DxCore::TProfileManager::GetDxBuildNumber(dir);
        EditDxVersion->Text = DxCore::TProfileManager::GetDxBuildNumberAsVersion(buildNum);
        
        Screen->Cursor = crHourGlass;
        try
        {
            FInstaller->SetInstallFileDir(dir);
            RefreshIDEList();
            RefreshComponentList();
            UpdateControlStates();
        }
        __finally
        {
            Screen->Cursor = crDefault;
        }
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::PageFunsChange(TObject *Sender)
{
    if (PageFuns->ActivePage == TabInstall)
    {
        BtnRun->Action = ActInstall;
        BtnRun->Visible = true;
    }
    else if (PageFuns->ActivePage == TabUninstall)
    {
        BtnRun->Action = ActUninstall;
        BtnRun->Visible = true;
    }
    else
    {
        BtnRun->Visible = false;
    }
}

//---------------------------------------------------------------------------
std::vector<DxCore::TIDEInfoPtr> TfrmMain::GetSelectedIDEs(TCheckListBox* checkList)
{
    std::vector<DxCore::TIDEInfoPtr> result;
    
    for (int i = 0; i < checkList->Items->Count; i++)
    {
        if (checkList->Checked[i])
        {
            intptr_t ideIndex = reinterpret_cast<intptr_t>(checkList->Items->Objects[i]);
            result.push_back(FInstaller->GetIDEDetector()->GetIDE(static_cast<int>(ideIndex)));
        }
    }
    
    return result;
}

//---------------------------------------------------------------------------
void TfrmMain::RunInstaller()
{
    SaveOptionsForSelectedIDE();
    
    if (FInstaller->GetIDEDetector()->AnyIDERunning())
    {
        ShowMessage(L"Please close all running IDEs before continuing.");
        return;
    }
    
    auto ides = GetSelectedIDEs(CheckListIDEs);
    if (ides.empty())
    {
        ShowMessage(L"Please select at least one IDE.");
        return;
    }
    
    Hide();
    try
    {
        FProgressForm->Initialize();
        FInstaller->Install(ides);
    }
    __finally
    {
        Show();
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::ActInstallExecute(TObject *Sender)
{
    if (EditSourceDir->Text.IsEmpty())
    {
        ShowMessage(L"Please select DevExpress source directory first.");
        return;
    }
    
    RunInstaller();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::ActUninstallExecute(TObject *Sender)
{
    auto ides = GetSelectedIDEs(CheckListUninstall);
    if (ides.empty())
    {
        ShowMessage(L"Please select at least one IDE.");
        return;
    }
    
    // Build uninstall options from checkboxes
    DxCore::TUninstallOptions uninstallOpts;
    uninstallOpts.Uninstall32BitIDE = ChkUninstall32->Checked;
    uninstallOpts.Uninstall64BitIDE = ChkUninstall64->Checked;
    uninstallOpts.DeleteCompiledFiles = true;
    
    if (!uninstallOpts.Uninstall32BitIDE && !uninstallOpts.Uninstall64BitIDE)
    {
        ShowMessage(L"Please select at least one IDE type to uninstall.");
        return;
    }
    
    Hide();
    try
    {
        FProgressForm->Initialize();
        FInstaller->Uninstall(ides, uninstallOpts);
    }
    __finally
    {
        Show();
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::ActExitExecute(TObject *Sender)
{
    Close();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::BtnExportProfileClick(TObject *Sender)
{
    String fileName = DxCore::TProfileManager::GetCustomProfileFileName();
    FInstaller->GetProfile()->ExportBuiltInProfile(fileName);
    InitializeProfileInfo();
    ShowMessage(L"Profile exported to: " + fileName);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::BtnDeleteProfileClick(TObject *Sender)
{
    String fileName = DxCore::TProfileManager::GetCustomProfileFileName();
    DeleteFile(fileName);
    InitializeProfileInfo();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::BtnSearchNewClick(TObject *Sender)
{
    if (EditSourceDir->Text.IsEmpty())
    {
        ShowMessage(L"Please select DevExpress source directory first.");
        return;
    }
    
    Screen->Cursor = crHourGlass;
    std::unique_ptr<TStringList> list(new TStringList());
    try
    {
        FInstaller->SearchNewPackages(list.get());
        MemoSearchResults->Lines->Assign(list.get());
        
        if (list->Count == 0)
            MemoSearchResults->Lines->Add(L"No new packages found.");
    }
    __finally
    {
        Screen->Cursor = crDefault;
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::LinkClick(TObject *Sender, const UnicodeString Link, TSysLinkType LinkType)
{
    ShellExecute(Handle, L"open", Link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::ChkHideBaseClick(TObject *Sender)
{
    RefreshComponentList();
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnProgress(const DxCore::TIDEInfoPtr& ide,
                                      const DxCore::TComponentProfilePtr& component,
                                      const String& task,
                                      const String& target)
{
    FProgressForm->UpdateProgress(ide, component, task, target);
}

//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnProgressState(const String& stateText)
{
    FProgressForm->UpdateProgressState(stateText);
}
