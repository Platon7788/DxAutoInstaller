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
#include "Core/ErrorTypes.h"

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
	TLabel *LblWarnings;
	TLabel *LblErrors;

    void __fastcall FormCreate(TObject *Sender);
    void __fastcall FormCloseQuery(TObject *Sender, bool &CanClose);
    void __fastcall ActStopExecute(TObject *Sender);
    void __fastcall ActCloseExecute(TObject *Sender);

private:
    DxCore::TInstaller* FInstaller;
    String FCurrentTarget;
    String FCurrentPackage;
    String FCurrentComponent;
    String FCurrentPlatform;
    std::unique_ptr<TStringList> FTargetLogs;
    int FErrorCount;
    int FWarningCount;
    int FHintCount;
    bool FIsRunning;

    // Structured issue tracking
    DxCore::TCompileIssueList FIssues;

    // Batched UI update control
    DWORD FLastUIUpdate;
    static const DWORD UI_UPDATE_INTERVAL = 50; // ms

    void SaveLogToFile();
    void UpdateCountLabels();
    void GenerateLogSummary(TStringList* log);
    
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

    // Get issue statistics
    int GetErrorCount() const { return FErrorCount; }
    int GetWarningCount() const { return FWarningCount; }
    int GetHintCount() const { return FHintCount; }
    const DxCore::TCompileIssueList& GetIssues() const { return FIssues; }
};

//---------------------------------------------------------------------------
extern PACKAGE TfrmProgress *frmProgress;
//---------------------------------------------------------------------------
#endif
