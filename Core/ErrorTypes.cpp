//---------------------------------------------------------------------------
// ErrorTypes implementation - No VCL dependencies, pure C++ with System types
//---------------------------------------------------------------------------
#pragma hdrstop
#include "ErrorTypes.h"
#include <regex>
#include <string>
#include <cwctype>

namespace DxCore
{

//---------------------------------------------------------------------------
// Helper: Convert System::String to std::wstring
//---------------------------------------------------------------------------
static std::wstring ToStdWString(const String& s)
{
    if (s.IsEmpty())
        return std::wstring();
    return std::wstring(s.c_str(), s.Length());
}

//---------------------------------------------------------------------------
// TErrorParser implementation
//---------------------------------------------------------------------------
bool TErrorParser::IsErrorLine(const String& line)
{
    String upper = line.UpperCase();
    return upper.Pos(L"ERROR") > 0 ||
           upper.Pos(L"FATAL") > 0 ||
           upper.Pos(L"FAILED") > 0 ||
           upper.Pos(L": E2") > 0 ||   // Delphi error codes like ": E2202"
           upper.Pos(L": F2") > 0;     // Fatal error codes like ": F2613"
}

bool TErrorParser::IsWarningLine(const String& line)
{
    String upper = line.UpperCase();
    if (IsErrorLine(line))
        return false;
    return upper.Pos(L"WARNING") > 0 ||
           upper.Pos(L": W1") > 0 ||
           upper.Pos(L": W2") > 0;
}

bool TErrorParser::IsHintLine(const String& line)
{
    String upper = line.UpperCase();
    if (IsErrorLine(line) || IsWarningLine(line))
        return false;
    return upper.Pos(L"HINT") > 0 ||
           upper.Pos(L": H2") > 0;
}

String TErrorParser::ExtractErrorCode(const String& line)
{
    // Look for patterns like E2202, F2613, W1000, H2164
    // Use std::regex instead of TRegEx
    try
    {
        std::wstring ws = ToStdWString(line);
        std::wregex pattern(L"[EFWH]\\d{4}");
        std::wsmatch match;

        if (std::regex_search(ws, match, pattern))
        {
            return String(match[0].str().c_str());
        }
    }
    catch (...)
    {
        // Regex failed, try manual extraction
    }

    // Manual fallback: look for common error code patterns
    const wchar_t* prefixes[] = { L": E2", L": F2", L": W1", L": W2", L": H2" };

    for (const wchar_t* prefix : prefixes)
    {
        int pos = line.Pos(prefix);
        if (pos > 0)
        {
            // Extract 5 characters starting from the letter (skip ": ")
            int codeStart = pos + 2;
            if (codeStart + 5 <= line.Length())
            {
                String code = line.SubString(codeStart, 5);
                // Verify format: letter + 4 digits
                if (code.Length() >= 5 &&
                    std::iswdigit(code[2]) && std::iswdigit(code[3]) &&
                    std::iswdigit(code[4]) && std::iswdigit(code[5]))
                {
                    return code;
                }
            }
        }
    }

    return L"";
}

bool TErrorParser::ExtractFileLocation(const String& line, String& fileName, int& lineNumber)
{
    fileName = L"";
    lineNumber = 0;

    // Look for pattern: filename.ext(number)
    // Use std::regex
    try
    {
        std::wstring ws = ToStdWString(line);
        std::wregex pattern(L"([\\w\\.]+\\.(?:dpk|pas|inc|dfm))\\((\\d+)\\)");
        std::wsmatch match;

        if (std::regex_search(ws, match, pattern))
        {
            fileName = String(match[1].str().c_str());
            lineNumber = std::stoi(match[2].str());
            return true;
        }
    }
    catch (...)
    {
        // Regex failed, try manual extraction
    }

    // Manual fallback: look for .dpk( or .pas(
    const wchar_t* extensions[] = { L".dpk(", L".pas(", L".inc(" };

    for (const wchar_t* ext : extensions)
    {
        int pos = line.Pos(ext);
        if (pos > 0)
        {
            // Find start of filename (go backwards until space or bracket)
            int start = pos;
            while (start > 1)
            {
                wchar_t ch = line[start];
                if (ch == L' ' || ch == L']' || ch == L'[')
                {
                    start++;
                    break;
                }
                start--;
            }
            if (start < 1) start = 1;

            // Extract filename including extension (4 chars for .dpk/.pas/.inc)
            int extLen = 4;
            fileName = line.SubString(start, pos - start + extLen);

            // Extract line number after (
            int numStart = pos + extLen + 1;
            int numEnd = numStart;
            while (numEnd <= line.Length() && std::iswdigit(line[numEnd]))
                numEnd++;

            if (numEnd > numStart)
            {
                String numStr = line.SubString(numStart, numEnd - numStart);
                lineNumber = StrToIntDef(numStr, 0);
            }

            return !fileName.IsEmpty();
        }
    }

    return false;
}

TErrorType TErrorParser::DetermineErrorType(const String& errorCode, const String& message)
{
    String upper = message.UpperCase();

    // Check error code first
    if (errorCode == L"E2202")
        return TErrorType::MissingPackage;
    if (errorCode == L"F2613")
        return TErrorType::MissingUnit;
    if (errorCode == L"E2225")
        return TErrorType::PackageRecompile;

    // Check message content
    if (upper.Pos(L"REQUIRED PACKAGE") > 0 && upper.Pos(L"NOT FOUND") > 0)
        return TErrorType::MissingPackage;
    if (upper.Pos(L"UNIT") > 0 && upper.Pos(L"NOT FOUND") > 0)
        return TErrorType::MissingUnit;
    if (upper.Pos(L"SYNTAX ERROR") > 0)
        return TErrorType::SyntaxError;
    if (upper.Pos(L"TYPE MISMATCH") > 0)
        return TErrorType::TypeMismatch;
    if (upper.Pos(L"COMPILE ERROR") > 0 || upper.Pos(L"COMPILATION FAILED") > 0)
        return TErrorType::CompilationFailed;
    if (upper.Pos(L"FILE NOT FOUND") > 0)
        return TErrorType::FileNotFound;
    if (upper.Pos(L"DIRECTORY") > 0 && upper.Pos(L"NOT") > 0)
        return TErrorType::DirectoryNotFound;

    // Default based on first letter of error code
    if (!errorCode.IsEmpty())
    {
        wchar_t first = errorCode[1];
        if (first == L'E' || first == L'F')
            return TErrorType::GeneralError;
        if (first == L'W')
            return TErrorType::GeneralWarning;
        if (first == L'H')
            return TErrorType::GeneralHint;
    }

    return TErrorType::GeneralError;
}

// Helper: Find substring position starting from a given index
static int PosFrom(const String& haystack, const String& needle, int startPos)
{
    if (startPos < 1 || startPos > haystack.Length())
        return 0;

    String sub = haystack.SubString(startPos, haystack.Length() - startPos + 1);
    int pos = sub.Pos(needle);
    if (pos > 0)
        return pos + startPos - 1;
    return 0;
}

TCompileIssuePtr TErrorParser::ParseLine(const String& line,
                                          const String& currentPackage,
                                          const String& currentComponent,
                                          const String& currentPlatform,
                                          int logLineNumber)
{
    // Determine severity
    bool isError = IsErrorLine(line);
    bool isWarning = IsWarningLine(line);
    bool isHint = IsHintLine(line);

    if (!isError && !isWarning && !isHint)
        return nullptr;

    auto issue = std::make_shared<TCompileIssue>();

    // Set severity
    if (isError)
    {
        issue->Severity = line.UpperCase().Pos(L"FATAL") > 0 ?
                          TErrorSeverity::Fatal : TErrorSeverity::Error;
    }
    else if (isWarning)
        issue->Severity = TErrorSeverity::Warning;
    else
        issue->Severity = TErrorSeverity::Hint;

    // Extract error code
    issue->ErrorCode = ExtractErrorCode(line);

    // Extract file location
    String fileName;
    int lineNum;
    if (ExtractFileLocation(line, fileName, lineNum))
    {
        issue->FileName = fileName;
        issue->LineNumber = lineNum;
    }

    // Set context info
    issue->PackageName = currentPackage;
    issue->ComponentName = currentComponent;
    issue->Platform = currentPlatform;
    issue->LogLineNumber = logLineNumber;

    // Set message (strip timestamp if present)
    issue->Message = line;
    int bracketPos = line.Pos(L"] ");
    if (bracketPos > 0 && bracketPos < 15)  // Timestamp like [12:34:56]
        issue->Message = line.SubString(bracketPos + 2, line.Length() - bracketPos - 1);

    // Determine error type
    issue->Type = DetermineErrorType(issue->ErrorCode, issue->Message);

    // Determine source
    if (!issue->ErrorCode.IsEmpty())
        issue->Source = TErrorSource::Compiler;
    else if (line.UpperCase().Pos(L"COMPILE ERROR") > 0)
        issue->Source = TErrorSource::Installer;
    else
        issue->Source = TErrorSource::Unknown;

    // Create short message by extracting key info
    if (issue->Type == TErrorType::MissingPackage || issue->Type == TErrorType::MissingUnit)
    {
        // Extract name from "Required package 'X' not found" or "Unit 'X' not found"
        int startQuote = issue->Message.Pos(L"'");
        if (startQuote > 0)
        {
            int endQuote = PosFrom(issue->Message, L"'", startQuote + 1);
            if (endQuote > startQuote)
            {
                String name = issue->Message.SubString(startQuote + 1, endQuote - startQuote - 1);
                if (issue->Type == TErrorType::MissingPackage)
                    issue->ShortMessage = L"Missing package: " + name;
                else
                    issue->ShortMessage = L"Missing unit: " + name;
            }
        }
    }

    if (issue->ShortMessage.IsEmpty())
        issue->ShortMessage = issue->Message;

    return issue;
}

} // namespace DxCore
