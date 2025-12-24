//---------------------------------------------------------------------------
// PackageCompiler implementation
//---------------------------------------------------------------------------
#pragma hdrstop
#include "PackageCompiler.h"
#include <IOUtils.hpp>
#include <Winapi.Windows.hpp>

namespace DxCore
{

//---------------------------------------------------------------------------
// TCompileOptions implementation
//---------------------------------------------------------------------------
TCompileOptions::TCompileOptions()
    : GenerateCppFiles(false),
      NativeLookAndFeel(false)
{
    SearchPaths = new TStringList();
    Defines = new TStringList();
}

TCompileOptions::~TCompileOptions()
{
    delete SearchPaths;
    delete Defines;
}

//---------------------------------------------------------------------------
// TPackageCompiler implementation
//---------------------------------------------------------------------------
TPackageCompiler::TPackageCompiler()
    : FOnOutput(nullptr)
{
}

TPackageCompiler::~TPackageCompiler()
{
}

void TPackageCompiler::OutputLine(const String& line)
{
    if (FOnOutput)
        FOnOutput(line);
}

String TPackageCompiler::GetCompilerPath(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    switch (platform)
    {
        case TIDEPlatform::Win32:
            return ide->GetDCC32Path();
        case TIDEPlatform::Win64:
            return ide->GetDCC64Path();
        case TIDEPlatform::Win64Modern:
            // For Win64Modern we still use dcc64 (Delphi compiler)
            // but with -jf:coffi flag to generate COFF .lib files
            // dcc64x doesn't exist - it's bcc64x for C++ only
            return ide->GetDCC64Path();
        default:
            return L"";
    }
}

bool TPackageCompiler::IsPlatformSupported(const TIDEInfoPtr& ide, TIDEPlatform platform)
{
    switch (platform)
    {
        case TIDEPlatform::Win32:
            return ide->SupportsWin32;
        case TIDEPlatform::Win64:
            return ide->SupportsWin64;
        case TIDEPlatform::Win64Modern:
            // Win64Modern uses dcc64 with -jf:coffi flag
            // So it's supported if Win64 is supported
            return ide->SupportsWin64;
        default:
            return false;
    }
}

TCompileResult TPackageCompiler::Compile(const TIDEInfoPtr& ide,
                                          TIDEPlatform platform,
                                          const TCompileOptions& options)
{
    TCompileResult result;
    
    if (!IsPlatformSupported(ide, platform))
    {
        result.Success = false;
        result.ErrorMessage = L"Platform not supported by this IDE";
        return result;
    }
    
    String compilerPath = GetCompilerPath(ide, platform);
    if (!FileExists(compilerPath))
    {
        result.Success = false;
        result.ErrorMessage = L"Compiler not found: " + compilerPath;
        return result;
    }
    
    String cmdLine = BuildCommandLine(ide, platform, options);
    String workDir = TPath::GetDirectoryName(options.PackagePath);
    
    OutputLine(L"Compiling: " + TPath::GetFileName(options.PackagePath));
    OutputLine(L"Compiler: " + compilerPath);
    
    result = ExecuteCompiler(compilerPath, cmdLine, workDir);
    
    return result;
}

String TPackageCompiler::BuildCommandLine(const TIDEInfoPtr& ide,
                                           TIDEPlatform platform,
                                           const TCompileOptions& options)
{
    String cmd;
    
    cmd = L"\"" + options.PackagePath + L"\"";
    
    // Debug options (disable)
    cmd = cmd + L" " + CompilerOptions::NO_DEBUG_INFO;
    cmd = cmd + L" " + CompilerOptions::NO_LOCAL_SYMBOLS;
    cmd = cmd + L" " + CompilerOptions::NO_SYMBOL_REF;
    
    // Quiet and build all
    cmd = cmd + L" " + CompilerOptions::QUIET;
    cmd = cmd + L" " + CompilerOptions::BUILD_ALL;
    
    // Output directories
    if (!options.BPLOutputDir.IsEmpty())
        cmd = cmd + L" -LE\"" + options.BPLOutputDir + L"\"";
        
    if (!options.DCPOutputDir.IsEmpty())
        cmd = cmd + L" -LN\"" + options.DCPOutputDir + L"\"";
        
    if (!options.UnitOutputDir.IsEmpty())
    {
        cmd = cmd + L" " + CompilerOptions::UNIT_OUTPUT_DIR + L"\"" + options.UnitOutputDir + L"\"";
        cmd = cmd + L" " + CompilerOptions::UNIT_OUTPUT_DIR_OLD + L"\"" + options.UnitOutputDir + L"\"";
    }
    
    // Search paths
    if (!options.DCPOutputDir.IsEmpty())
        cmd = cmd + L" " + CompilerOptions::UNIT_SEARCH_PATH + L"\"" + options.DCPOutputDir + L"\"";
        
    for (int i = 0; i < options.SearchPaths->Count; i++)
    {
        cmd = cmd + L" " + CompilerOptions::UNIT_SEARCH_PATH + L"\"" + options.SearchPaths->Strings[i] + L"\"";
        cmd = cmd + L" " + CompilerOptions::RESOURCE_PATH + L"\"" + options.SearchPaths->Strings[i] + L"\"";
    }
    
    // Unit aliases
    cmd = cmd + L" " + CompilerOptions::UNIT_ALIAS + L"WinTypes=Windows;WinProcs=Windows;DbiTypes=BDE;DbiProcs=BDE";
    
    // Namespace search paths
    cmd = cmd + L" " + CompilerOptions::NAMESPACE_SEARCH + 
          L"Winapi;System.Win;Data.Win;Datasnap.Win;Web.Win;Soap.Win;Xml.Win;" +
          L"Bde;Vcl;Vcl.Imaging;Vcl.Touch;Vcl.Samples;Vcl.Shell;System;Xml;" +
          L"Data;Datasnap;Web;Soap;IBX;VclTee;";
    
    // Defines
    if (options.NativeLookAndFeel)
        cmd = cmd + L" " + CompilerOptions::DEFINE + L"USENATIVELOOKANDFEELASDEFAULT";
        
    for (int i = 0; i < options.Defines->Count; i++)
        cmd = cmd + L" " + CompilerOptions::DEFINE + options.Defines->Strings[i];
    
    // C++Builder options
    if (options.GenerateCppFiles)
    {
        // -JL generates .hpp, .bpi, .bpl and import library:
        // - For Win32: generates .lib (OMF format)
        // - For Win64: generates .a (ELF format)
        // - For Win64Modern: generates .lib (COFF format) with -jf:coffi flag
        cmd = cmd + L" " + CompilerOptions::GENERATE_CPP;
        
        // For Win64Modern (Win64x), add -jf:coffi to generate COFF .lib files
        // and -DDX_WIN64_MODERN define
        if (platform == TIDEPlatform::Win64Modern)
        {
            cmd = cmd + L" " + CompilerOptions::GENERATE_COFF;
            cmd = cmd + L" " + CompilerOptions::DEFINE + L"DX_WIN64_MODERN";
        }
        
        if (!options.DCPOutputDir.IsEmpty())
        {
            cmd = cmd + L" " + CompilerOptions::BPI_OUTPUT_DIR + L"\"" + options.DCPOutputDir + L"\"";
            cmd = cmd + L" " + CompilerOptions::OBJ_OUTPUT_DIR + L"\"" + options.DCPOutputDir + L"\"";
        }
        
        if (!options.UnitOutputDir.IsEmpty())
            cmd = cmd + L" " + CompilerOptions::HPP_OUTPUT_DIR + L"\"" + options.UnitOutputDir + L"\"";
    }
    
    return cmd;
}

TCompileResult TPackageCompiler::ExecuteCompiler(const String& compilerPath,
                                                  const String& cmdLine,
                                                  const String& workDir)
{
    TCompileResult result;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        result.Success = false;
        result.ErrorMessage = L"Failed to create pipe";
        return result;
    }
    
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {0};
    
    String fullCmd = L"\"" + compilerPath + L"\" " + cmdLine;
    
    std::vector<wchar_t> cmdBuffer(fullCmd.Length() + 1);
    wcscpy(cmdBuffer.data(), fullCmd.c_str());
    
    BOOL created = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );
    
    CloseHandle(hWritePipe);
    
    if (!created)
    {
        CloseHandle(hReadPipe);
        result.Success = false;
        result.ErrorMessage = L"Failed to start compiler";
        return result;
    }
    
    String output;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        String line = String(buffer);
        output = output + line;
        
        int pos;
        while ((pos = output.Pos(L"\n")) > 0)
        {
            String singleLine = output.SubString(1, pos - 1).Trim();
            if (!singleLine.IsEmpty())
                OutputLine(singleLine);
            output = output.SubString(pos + 1, output.Length() - pos);
        }
    }
    
    if (!output.Trim().IsEmpty())
        OutputLine(output.Trim());
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    result.ExitCode = static_cast<int>(exitCode);
    result.Success = (exitCode == 0);
    result.Output = output;
    
    if (!result.Success)
        result.ErrorMessage = L"Compilation failed with exit code " + String(static_cast<int>(exitCode));
    
    return result;
}

TCompileResult TPackageCompiler::GenerateCoffLib(const TIDEInfoPtr& ide,
                                                  const String& bplPath,
                                                  const String& libOutputPath)
{
    TCompileResult result;
    
    // Get mkexp.exe path
    String mkexpPath = ide->GetMkExpPath();
    if (!FileExists(mkexpPath))
    {
        result.Success = false;
        result.ErrorMessage = L"mkexp.exe not found: " + mkexpPath;
        return result;
    }
    
    // Check if BPL exists
    if (!FileExists(bplPath))
    {
        result.Success = false;
        result.ErrorMessage = L"BPL file not found: " + bplPath;
        return result;
    }
    
    // Ensure output directory exists
    String outputDir = TPath::GetDirectoryName(libOutputPath);
    if (!outputDir.IsEmpty())
        ForceDirectories(outputDir);
    
    // Build command line: mkexp.exe -p <output.lib> <input.bpl>
    // -p flag tells mkexp that input is a PE file (BPL is a PE DLL)
    String cmdLine = L"-p \"" + libOutputPath + L"\" \"" + bplPath + L"\"";
    
    OutputLine(L"Generating COFF .lib: " + TPath::GetFileName(libOutputPath));
    OutputLine(L"From BPL: " + TPath::GetFileName(bplPath));
    OutputLine(L"Using: " + mkexpPath);
    
    // Execute mkexp
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        result.Success = false;
        result.ErrorMessage = L"Failed to create pipe";
        return result;
    }
    
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {0};
    
    String fullCmd = L"\"" + mkexpPath + L"\" " + cmdLine;
    
    std::vector<wchar_t> cmdBuffer(fullCmd.Length() + 1);
    wcscpy(cmdBuffer.data(), fullCmd.c_str());
    
    String workDir = TPath::GetDirectoryName(bplPath);
    
    BOOL created = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );
    
    CloseHandle(hWritePipe);
    
    if (!created)
    {
        CloseHandle(hReadPipe);
        result.Success = false;
        result.ErrorMessage = L"Failed to start mkexp.exe";
        return result;
    }
    
    String output;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        String line = String(buffer);
        output = output + line;
        
        int pos;
        while ((pos = output.Pos(L"\n")) > 0)
        {
            String singleLine = output.SubString(1, pos - 1).Trim();
            if (!singleLine.IsEmpty())
                OutputLine(singleLine);
            output = output.SubString(pos + 1, output.Length() - pos);
        }
    }
    
    if (!output.Trim().IsEmpty())
        OutputLine(output.Trim());
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    result.ExitCode = static_cast<int>(exitCode);
    result.Success = (exitCode == 0) && FileExists(libOutputPath);
    result.Output = output;
    
    if (!result.Success)
    {
        if (!FileExists(libOutputPath))
            result.ErrorMessage = L"mkexp.exe did not create output file: " + libOutputPath;
        else
            result.ErrorMessage = L"mkexp.exe failed with exit code " + String(static_cast<int>(exitCode));
    }
    
    return result;
}

} // namespace DxCore
