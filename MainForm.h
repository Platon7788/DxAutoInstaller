//---------------------------------------------------------------------------
// MainForm - Main application form
//---------------------------------------------------------------------------
#ifndef MainFormH
#define MainFormH

#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Buttons.hpp>
#include <Vcl.ActnList.hpp>
#include <Vcl.ImgList.hpp>
#include <Vcl.Dialogs.hpp>
#include <Vcl.CheckLst.hpp>
#include <System.Actions.hpp>
#include <System.ImageList.hpp>

#include "Core/Installer.h"
#include "ProgressForm.h"

//---------------------------------------------------------------------------
class TfrmMain : public TForm
{
__published:
    TPanel *PanelTop;
    TLabel *LblAppName;
    TLabel *LblVersion;
    TPageControl *PageFuns;
    TTabSheet *TabInstall;
    TTabSheet *TabUninstall;
    TTabSheet *TabTools;
    TTabSheet *TabAbout;
    TPanel *PanelBottom;
    TButton *BtnRun;
    TButton *BtnExit;
    TActionList *ActionList;
    TAction *ActInstall;
    TAction *ActUninstall;
    TAction *ActExit;
    
    // Install tab
    TLabel *LblSourceDir;
    TEdit *EditSourceDir;
    TButton *BtnBrowse;
    TLabel *LblVersion2;
    TEdit *EditDxVersion;
    
    // Component CheckListBox
    TCheckListBox *CheckListComponents;
    
    // Right panel
    TPanel *PanelRight;
    TGroupBox *GroupIDESelect;
    TCheckListBox *CheckListIDEs;
    
    // IDE Options group
    TGroupBox *GroupIDEOptions;
    TCheckBox *ChkIDE32;
    TCheckBox *ChkIDE64;
    TLabel *LblIDEType;
    
    // Target platforms group
    TGroupBox *GroupTargets;
    TCheckBox *ChkTargetWin32;
    TCheckBox *ChkTargetWin64;
    
    // Other options
    TGroupBox *GroupOtherOptions;
    TCheckBox *ChkNativeLookAndFeel;
    TCheckBox *ChkGenerateCpp;
    TCheckBox *ChkHideBase;
    
    // Uninstall tab
    TLabel *LblSelectIDE;
    TCheckListBox *CheckListUninstall;
    TGroupBox *GroupUninstallOptions;
    TCheckBox *ChkUninstall32;
    TCheckBox *ChkUninstall64;
    
    // Tools tab
    TGroupBox *GroupProfile;
    TLabel *LblCurrentProfile;
    TButton *BtnExportProfile;
    TButton *BtnDeleteProfile;
    TGroupBox *GroupSearch;
    TButton *BtnSearchNew;
    TMemo *MemoSearchResults;
    
    // About tab
    TLabel *LblAboutTitle;
    TLabel *LblAboutDesc;
    TLabel *LblDownload;
    TLinkLabel *LinkDownload;
    TLabel *LblEmail;
    TLinkLabel *LinkEmail;
    TMemo *MemoChangelog;
    
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall FormDestroy(TObject *Sender);
    void __fastcall ActInstallExecute(TObject *Sender);
    void __fastcall ActUninstallExecute(TObject *Sender);
    void __fastcall ActExitExecute(TObject *Sender);
    void __fastcall BtnBrowseClick(TObject *Sender);
    void __fastcall PageFunsChange(TObject *Sender);
    void __fastcall BtnExportProfileClick(TObject *Sender);
    void __fastcall BtnDeleteProfileClick(TObject *Sender);
    void __fastcall BtnSearchNewClick(TObject *Sender);
    void __fastcall LinkClick(TObject *Sender, const UnicodeString Link, TSysLinkType LinkType);
    void __fastcall ChkHideBaseClick(TObject *Sender);
    void __fastcall CheckListIDEsClick(TObject *Sender);
    void __fastcall CheckListComponentsClickCheck(TObject *Sender);
    void __fastcall ChkIDE64Click(TObject *Sender);
    void __fastcall TargetCheckBoxClick(TObject *Sender);
    
private:
    std::unique_ptr<DxCore::TInstaller> FInstaller;
    TfrmProgress *FProgressForm;
    DxCore::TIDEInfoPtr FSelectedIDE;  // Currently selected IDE for options
    
    void InitializeIDEList();
    void InitializeUninstallList();
    void InitializeProfileInfo();
    void RefreshIDEList();
    void RefreshComponentList();
    void RunInstaller(bool install);
    void UpdateControlStates();
    void UpdateOptionsForSelectedIDE();
    void SaveOptionsForSelectedIDE();
    void UpdateTargetCheckboxes();
    
    std::vector<DxCore::TIDEInfoPtr> GetSelectedIDEs(TCheckListBox* checkList);
    
    // Progress callbacks
    void __fastcall OnProgress(const DxCore::TIDEInfoPtr& ide,
                               const DxCore::TComponentProfilePtr& component,
                               const String& task,
                               const String& target);
    void __fastcall OnProgressState(const String& stateText);
    
public:
    __fastcall TfrmMain(TComponent* Owner);
};

//---------------------------------------------------------------------------
extern PACKAGE TfrmMain *frmMain;
//---------------------------------------------------------------------------
#endif
