//---------------------------------------------------------------------------
// ProgressForm - Installation progress display
//---------------------------------------------------------------------------
#ifndef ProgressFormH
#define ProgressFormH

#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.ActnList.hpp>
#include <System.Actions.hpp>

#include "Core/Installer.h"

//---------------------------------------------------------------------------
class TfrmProgress : public TForm
{
__published:
    TPanel *PanelTop;
    TLabel *LblTitle;
    TPanel *PanelLogs;
    TMemo *MemoLogs;
    TPanel *PanelBottom;
    TButton *BtnAction;
    TActionList *ActionList;
    TAction *ActStop;
    TAction *ActClose;
    
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall FormCloseQuery(TObject *Sender, bool &CanClose);
    void __fastcall ActStopExecute(TObject *Sender);
    void __fastcall ActCloseExecute(TObject *Sender);
    
private:
    DxCore::TInstaller* FInstaller;
    String FCurrentTarget;
    std::unique_ptr<TStringList> FTargetLogs;
    int FErrorCount;
    bool FIsRunning;  // Track if operation is in progress
    
    void SaveLogToFile();
    
public:
    __fastcall TfrmProgress(TComponent* Owner);
    
    void SetInstaller(DxCore::TInstaller* installer) { FInstaller = installer; }
    
    void Initialize();
    void UpdateProgress(const DxCore::TIDEInfoPtr& ide,
                        const DxCore::TComponentProfilePtr& component,
                        const String& task,
                        const String& target);
    void UpdateProgressState(const String& stateText);
    void OnComplete(bool success, const String& message);
};

//---------------------------------------------------------------------------
extern PACKAGE TfrmProgress *frmProgress;
//---------------------------------------------------------------------------
#endif
