//---------------------------------------------------------------------------
// ProgressForm implementation - Optimized with structured error tracking
//---------------------------------------------------------------------------
#pragma hdrstop
#include "ProgressForm.h"
#include <IOUtils.hpp>
#include <DateUtils.hpp>
#include <map>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmProgress *frmProgress;

//---------------------------------------------------------------------------
__fastcall TfrmProgress::TfrmProgress(TComponent* Owner)
    : TForm(Owner),
      FInstaller(nullptr),
      FErrorCount(0),
      FWarningCount(0),
      FHintCount(0),
      FIsRunning(false),
      FLastUIUpdate(0)
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
    CanClose = !FIsRunning;

    if (!CanClose)
    {
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
    FIssues.clear();
    FCurrentTarget = L"";
    FCurrentPackage = L"";
    FCurrentComponent = L"";
    FCurrentPlatform = L"";
    FErrorCount = 0;
    FWarningCount = 0;
    FHintCount = 0;
    FIsRunning = true;
    FLastUIUpdate = 0;
    BtnAction->Action = ActStop;

    UpdateCountLabels();

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

    // Update current context for error parsing
    if (component)
        FCurrentComponent = component->ComponentName;

    // Extract platform and package from target if present
    if (!target.IsEmpty())
    {
        // Target format: "Win32 > PackageName" or just "PackageName"
        int arrowPos = target.Pos(L" > ");
        if (arrowPos > 0)
        {
            FCurrentPlatform = target.SubString(1, arrowPos - 1);
            FCurrentPackage = target.SubString(arrowPos + 3, target.Length() - arrowPos - 2);
        }
        else
        {
            FCurrentPackage = target;
        }
    }

    // Build title
    String title = ide->Name;
    if (component)
        title = title + L" > " + component->ComponentName;
    if (!task.IsEmpty())
        title = title + L" > " + task;

    // Check if target changed - add separator
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

    int lineNumber = MemoLogs->Lines->Count + 1;

    // Add to memo (with batched scrolling for performance)
    MemoLogs->Lines->BeginUpdate();
    try
    {
        MemoLogs->Lines->Add(logLine);
    }
    __finally
    {
        MemoLogs->Lines->EndUpdate();
    }

    FTargetLogs->Add(logLine);

    // Batched scroll - only scroll every UI_UPDATE_INTERVAL ms
    DWORD now = GetTickCount();
    if (now - FLastUIUpdate >= UI_UPDATE_INTERVAL)
    {
        SendMessage(MemoLogs->Handle, EM_LINESCROLL, 0, MemoLogs->Lines->Count);
        FLastUIUpdate = now;
        Application->ProcessMessages(); // Keep UI responsive
    }

    // Parse for errors/warnings using structured parser
    auto issue = DxCore::TErrorParser::ParseLine(
        stateText,
        FCurrentPackage,
        FCurrentComponent,
        FCurrentPlatform,
        lineNumber);

    if (issue)
    {
        FIssues.push_back(issue);

        switch (issue->Severity)
        {
            case DxCore::TErrorSeverity::Error:
            case DxCore::TErrorSeverity::Fatal:
                FErrorCount++;
                break;
            case DxCore::TErrorSeverity::Warning:
                FWarningCount++;
                break;
            case DxCore::TErrorSeverity::Hint:
                FHintCount++;
                break;
        }

        UpdateCountLabels();
    }
}

//---------------------------------------------------------------------------
void TfrmProgress::OnComplete(bool success, const String& message)
{
    FIsRunning = false;
    BtnAction->Action = ActClose;

    // Final scroll
    SendMessage(MemoLogs->Handle, EM_LINESCROLL, 0, MemoLogs->Lines->Count);

    MemoLogs->Lines->Add(L"");

    if (success)
    {
        if (FErrorCount > 0)
        {
            LblTitle->Caption = Format(L"Finished with %d error(s)", ARRAYOFCONST((FErrorCount)));
            MemoLogs->Lines->Add(Format(L"=== Completed with %d error(s), %d warning(s), %d hint(s) ===",
                ARRAYOFCONST((FErrorCount, FWarningCount, FHintCount))));
        }
        else if (FWarningCount > 0)
        {
            LblTitle->Caption = Format(L"Finished with %d warning(s)", ARRAYOFCONST((FWarningCount)));
            MemoLogs->Lines->Add(Format(L"=== Completed with %d warning(s), %d hint(s) ===",
                ARRAYOFCONST((FWarningCount, FHintCount))));
        }
        else
        {
            LblTitle->Caption = L"Finished successfully!";
            MemoLogs->Lines->Add(L"=== Operation completed successfully ===");
        }
    }
    else
    {
        LblTitle->Caption = L"Stopped";
        MemoLogs->Lines->Add(L"=== Operation stopped: " + message + L" ===");
    }

    // Show log file path
    String logFile = DxCore::TInstaller::GetCurrentLogFileName();
    if (!logFile.IsEmpty())
    {
        MemoLogs->Lines->Add(L"");
        MemoLogs->Lines->Add(L"Log file: " + logFile);
    }

    UpdateCountLabels();
    SaveLogToFile();
}

//---------------------------------------------------------------------------
void TfrmProgress::UpdateCountLabels()
{
    if (LblErrors)
    {
        LblErrors->Caption = Format(L"Errors: %d", ARRAYOFCONST((FErrorCount)));
        if (FErrorCount > 0)
            LblErrors->Font->Color = clRed;
        else
            LblErrors->Font->Color = clWindowText;
    }

    if (LblWarnings)
    {
        LblWarnings->Caption = Format(L"Warnings: %d", ARRAYOFCONST((FWarningCount)));
        if (FWarningCount > 0)
            LblWarnings->Font->Color = static_cast<TColor>(0x000080FF); // Orange
        else
            LblWarnings->Font->Color = clWindowText;
    }
}

//---------------------------------------------------------------------------
void TfrmProgress::GenerateLogSummary(TStringList* log)
{
    log->Add(L"");
    log->Add(L"================================================================================");
    log->Add(L"                              INSTALLATION SUMMARY");
    log->Add(L"================================================================================");
    log->Add(L"");
    log->Add(Format(L"Total Errors:   %d", ARRAYOFCONST((FErrorCount))));
    log->Add(Format(L"Total Warnings: %d", ARRAYOFCONST((FWarningCount))));
    log->Add(Format(L"Total Hints:    %d", ARRAYOFCONST((FHintCount))));
    log->Add(L"");

    // Group issues by severity
    DxCore::TCompileIssueList errors, warnings, hints;
    for (const auto& issue : FIssues)
    {
        switch (issue->Severity)
        {
            case DxCore::TErrorSeverity::Error:
            case DxCore::TErrorSeverity::Fatal:
                errors.push_back(issue);
                break;
            case DxCore::TErrorSeverity::Warning:
                warnings.push_back(issue);
                break;
            case DxCore::TErrorSeverity::Hint:
                hints.push_back(issue);
                break;
        }
    }

    // Quick reference: errors grouped by component
    if (!errors.empty())
    {
        log->Add(L"--------------------------------------------------------------------------------");
        log->Add(L"ERRORS BY COMPONENT (quick reference)");
        log->Add(L"--------------------------------------------------------------------------------");

        std::map<String, std::vector<size_t>> errorsByComponent;
        for (size_t i = 0; i < errors.size(); i++)
        {
            String comp = errors[i]->ComponentName.IsEmpty() ? L"(unknown)" : errors[i]->ComponentName;
            errorsByComponent[comp].push_back(i);
        }

        for (const auto& pair : errorsByComponent)
        {
            log->Add(L"");
            log->Add(L"  " + pair.first + L" (" + String(static_cast<int>(pair.second.size())) + L" errors):");
            for (size_t idx : pair.second)
            {
                const auto& e = errors[idx];
                String line = L"    ";
                if (!e->Platform.IsEmpty())
                    line = line + L"[" + e->Platform + L"] ";
                if (!e->PackageName.IsEmpty())
                    line = line + e->PackageName + L": ";
                if (!e->ErrorCode.IsEmpty())
                    line = line + e->ErrorCode + L" ";
                line = line + e->ShortMessage;
                log->Add(line);
            }
        }
        log->Add(L"");
    }

    // Quick reference: warnings grouped by component
    if (!warnings.empty())
    {
        log->Add(L"--------------------------------------------------------------------------------");
        log->Add(L"WARNINGS BY COMPONENT (quick reference)");
        log->Add(L"--------------------------------------------------------------------------------");

        std::map<String, std::vector<size_t>> warningsByComponent;
        for (size_t i = 0; i < warnings.size(); i++)
        {
            String comp = warnings[i]->ComponentName.IsEmpty() ? L"(unknown)" : warnings[i]->ComponentName;
            warningsByComponent[comp].push_back(i);
        }

        for (const auto& pair : warningsByComponent)
        {
            log->Add(L"");
            log->Add(L"  " + pair.first + L" (" + String(static_cast<int>(pair.second.size())) + L" warnings):");
            for (size_t idx : pair.second)
            {
                const auto& w = warnings[idx];
                String line = L"    ";
                if (!w->Platform.IsEmpty())
                    line = line + L"[" + w->Platform + L"] ";
                if (!w->PackageName.IsEmpty())
                    line = line + w->PackageName + L": ";
                if (!w->ErrorCode.IsEmpty())
                    line = line + w->ErrorCode + L" ";
                line = line + w->ShortMessage;
                log->Add(line);
            }
        }
        log->Add(L"");
    }

    // Detailed errors section
    if (!errors.empty())
    {
        log->Add(L"================================================================================");
        log->Add(L"ERRORS - DETAILED (" + String(static_cast<int>(errors.size())) + L")");
        log->Add(L"================================================================================");
        log->Add(L"");

        for (size_t i = 0; i < errors.size(); i++)
        {
            const auto& e = errors[i];
            log->Add(Format(L"--- Error #%d ---", ARRAYOFCONST((static_cast<int>(i + 1)))));
            log->Add(L"  Component:  " + (e->ComponentName.IsEmpty() ? L"(unknown)" : e->ComponentName));
            log->Add(L"  Package:    " + (e->PackageName.IsEmpty() ? L"(unknown)" : e->PackageName));
            log->Add(L"  Platform:   " + (e->Platform.IsEmpty() ? L"(unknown)" : e->Platform));
            log->Add(L"  Type:       " + e->GetTypeStr());
            log->Add(L"  Source:     " + e->GetSourceStr());
            if (!e->ErrorCode.IsEmpty())
                log->Add(L"  Code:       " + e->ErrorCode);
            if (!e->FileName.IsEmpty())
            {
                String loc = e->FileName;
                if (e->LineNumber > 0)
                    loc = loc + L":" + String(e->LineNumber);
                log->Add(L"  Location:   " + loc);
            }
            log->Add(L"  Message:    " + e->Message);
            log->Add(L"");
        }
    }

    // Detailed warnings section
    if (!warnings.empty())
    {
        log->Add(L"================================================================================");
        log->Add(L"WARNINGS - DETAILED (" + String(static_cast<int>(warnings.size())) + L")");
        log->Add(L"================================================================================");
        log->Add(L"");

        for (size_t i = 0; i < warnings.size(); i++)
        {
            const auto& w = warnings[i];
            log->Add(Format(L"--- Warning #%d ---", ARRAYOFCONST((static_cast<int>(i + 1)))));
            log->Add(L"  Component:  " + (w->ComponentName.IsEmpty() ? L"(unknown)" : w->ComponentName));
            log->Add(L"  Package:    " + (w->PackageName.IsEmpty() ? L"(unknown)" : w->PackageName));
            log->Add(L"  Platform:   " + (w->Platform.IsEmpty() ? L"(unknown)" : w->Platform));
            log->Add(L"  Type:       " + w->GetTypeStr());
            if (!w->ErrorCode.IsEmpty())
                log->Add(L"  Code:       " + w->ErrorCode);
            log->Add(L"  Message:    " + w->Message);
            log->Add(L"");
        }
    }

    // Hints section (condensed)
    if (!hints.empty())
    {
        log->Add(L"================================================================================");
        log->Add(L"HINTS (" + String(static_cast<int>(hints.size())) + L")");
        log->Add(L"================================================================================");
        log->Add(L"");

        for (const auto& h : hints)
        {
            String line = L"  ";
            if (!h->PackageName.IsEmpty())
                line = line + h->PackageName + L": ";
            line = line + h->Message;
            log->Add(line);
        }
        log->Add(L"");
    }

    // Final summary with log file path
    log->Add(L"================================================================================");
    if (FErrorCount == 0 && FWarningCount == 0)
    {
        log->Add(L"Installation completed successfully without errors or warnings.");
    }
    else if (FErrorCount > 0)
    {
        log->Add(L"ATTENTION: Installation completed with errors!");
        log->Add(L"");
        log->Add(L"Common causes:");
        log->Add(L"  - Missing dependencies: Check that required packages compile before dependent ones");
        log->Add(L"  - Wrong compilation order: Check Profile.ini component order");
        log->Add(L"  - Missing source files: Verify DevExpress installation is complete");
        log->Add(L"  - Design-time units in runtime package: Units using DesignIntf must be in dcl*.dpk only");
        log->Add(L"  - Missing prerequisite package: Some addons require pre-installed packages (e.g., SynEdit for ExpressSynEdit)");
    }
    else
    {
        log->Add(L"Installation completed with warnings. Review warnings above.");
    }
    log->Add(L"================================================================================");
}

//---------------------------------------------------------------------------
void TfrmProgress::SaveLogToFile()
{
    // Append structured summary to the main log file (created by Installer.cpp)
    try
    {
        std::unique_ptr<TStringList> summary(new TStringList());
        GenerateLogSummary(summary.get());

        // Append each summary line to the main log file
        for (int i = 0; i < summary->Count; i++)
        {
            DxCore::TInstaller::AppendToLogFile(summary->Strings[i]);
        }

        // Close the log file so it can be read immediately
        DxCore::TInstaller::CloseLogFile();
    }
    catch (...)
    {
        // Ignore log save errors
    }
}
