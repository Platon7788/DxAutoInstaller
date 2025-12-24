//---------------------------------------------------------------------------
// ProgressForm implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "ProgressForm.h"
#include <IOUtils.hpp>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmProgress *frmProgress;

//---------------------------------------------------------------------------
__fastcall TfrmProgress::TfrmProgress(TComponent* Owner)
    : TForm(Owner),
      FInstaller(nullptr),
      FIsRunning(false)
{
    FTargetLogs = std::make_unique<TStringList>();
}

//---------------------------------------------------------------------------
void __fastcall TfrmProgress::FormCreate(TObject *Sender)
{
    Caption = Application->Title;
}

//---------------------------------------------------------------------------
void __fastcall TfrmProgress::FormCloseQuery(TObject *Sender, bool &CanClose)
{
    // Allow close only if not running
    CanClose = !FIsRunning;
    
    if (!CanClose)
    {
        // Ask user if they want to stop
        if (Application->MessageBox(
                L"Operation is in progress. Do you want to stop it?",
                L"Confirm",
                MB_ICONQUESTION | MB_YESNO) == IDYES)
        {
            if (FInstaller)
                FInstaller->Stop();
        }
    }
}

//---------------------------------------------------------------------------
void __fastcall TfrmProgress::ActStopExecute(TObject *Sender)
{
    if (FInstaller)
        FInstaller->Stop();
}

//---------------------------------------------------------------------------
void __fastcall TfrmProgress::ActCloseExecute(TObject *Sender)
{
    Close();
}

//---------------------------------------------------------------------------
void TfrmProgress::Initialize()
{
    MemoLogs->Clear();
    FTargetLogs->Clear();
    FCurrentTarget = L"";
    FErrorCount = 0;
    FIsRunning = true;
    BtnAction->Action = ActStop;
    
    // Log start
    MemoLogs->Lines->Add(L"Installation started at " + FormatDateTime(L"yyyy-mm-dd hh:nn:ss", Now()));
    MemoLogs->Lines->Add(L"");
    
    Show();
}

//---------------------------------------------------------------------------
void TfrmProgress::UpdateProgress(const DxCore::TIDEInfoPtr& ide,
                                   const DxCore::TComponentProfilePtr& component,
                                   const String& task,
                                   const String& target)
{
    if (!FInstaller)
        return;
        
    if (FInstaller->GetState() == DxCore::TInstallerState::Running)
        BtnAction->Action = ActStop;
        
    // Build title
    String title = ide->Name;
    if (component)
        title = title + L" > " + component->ComponentName;
    if (!task.IsEmpty())
        title = title + L" > " + task;
        
    // Check if target changed
    if (FCurrentTarget != target)
    {
        FTargetLogs->Clear();
        MemoLogs->Lines->Add(StringOfChar(L'-', 80));
    }
    FCurrentTarget = target;
    
    if (!target.IsEmpty())
        title = title + L" > " + target;
        
    LblTitle->Caption = title;
}

//---------------------------------------------------------------------------
void TfrmProgress::UpdateProgressState(const String& stateText)
{
    if (!FInstaller)
        return;
    
    // Add timestamp
    String timestamp = FormatDateTime(L"hh:nn:ss", Now());
    String logLine = L"[" + timestamp + L"] " + stateText;
    
    MemoLogs->Lines->Add(logLine);
    FTargetLogs->Add(logLine);
    
    // Scroll to bottom
    SendMessage(MemoLogs->Handle, EM_LINESCROLL, 0, MemoLogs->Lines->Count);
    
    // Check for errors in the state text
    if (stateText.Pos(L"ERROR") > 0 || stateText.Pos(L"FAILED") > 0)
    {
        FErrorCount++;
    }
}

//---------------------------------------------------------------------------
void TfrmProgress::OnComplete(bool success, const String& message)
{
    FIsRunning = false;
    BtnAction->Action = ActClose;
    
    if (success)
    {
        LblTitle->Caption = L"Finished!";
        MemoLogs->Lines->Add(L"");
        MemoLogs->Lines->Add(L"=== Operation completed successfully ===");
    }
    else
    {
        LblTitle->Caption = L"Stopped";
        MemoLogs->Lines->Add(L"");
        MemoLogs->Lines->Add(L"=== Operation stopped: " + message + L" ===");
    }
    
    // Save log to file
    SaveLogToFile();
}

//---------------------------------------------------------------------------
void TfrmProgress::SaveLogToFile()
{
    String logDir = TPath::GetDirectoryName(Application->ExeName);
    String logFile = TPath::Combine(logDir, L"DxAutoInstaller.log");
    
    try
    {
        // Add summary header
        std::unique_ptr<TStringList> fullLog(new TStringList());
        fullLog->Add(L"========================================");
        fullLog->Add(L"DxAutoInstaller Log");
        fullLog->Add(L"Date: " + FormatDateTime(L"yyyy-mm-dd hh:nn:ss", Now()));
        fullLog->Add(L"========================================");
        fullLog->Add(L"");
        fullLog->AddStrings(MemoLogs->Lines);
        fullLog->Add(L"");
        fullLog->Add(L"========================================");
        fullLog->Add(L"Summary: " + String(FErrorCount) + L" error(s)");
        fullLog->Add(L"========================================");
        
        fullLog->SaveToFile(logFile);
    }
    catch (...)
    {
        // Ignore log save errors
    }
}
