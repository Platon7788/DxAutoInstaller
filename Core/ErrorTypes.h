//---------------------------------------------------------------------------
// ErrorTypes - Structured error and warning tracking
//---------------------------------------------------------------------------
#ifndef ErrorTypesH
#define ErrorTypesH

#include <System.hpp>
#include <System.Classes.hpp>
#include <vector>
#include <memory>

namespace DxCore
{

//---------------------------------------------------------------------------
// Error severity levels
//---------------------------------------------------------------------------
enum class TErrorSeverity
{
    Hint,       // Compiler hints (low priority)
    Warning,    // Warnings (medium priority)
    Error,      // Errors (high priority)
    Fatal       // Fatal errors (critical)
};

//---------------------------------------------------------------------------
// Error source - where the error originated
//---------------------------------------------------------------------------
enum class TErrorSource
{
    Compiler,       // Delphi compiler (dcc32/dcc64)
    Linker,         // Linker errors
    Installer,      // DxAutoInstaller internal errors
    Configuration,  // Profile.ini or package configuration
    FileSystem,     // File/directory access errors
    Registry,       // Windows registry errors
    Unknown         // Unclassified errors
};

//---------------------------------------------------------------------------
// Error type - specific error category
//---------------------------------------------------------------------------
enum class TErrorType
{
    // Compiler errors
    MissingPackage,     // E2202: Required package 'X' not found
    MissingUnit,        // F2613: Unit 'X' not found
    PackageRecompile,   // E2225: Never-build package must be recompiled
    SyntaxError,        // Syntax errors in code
    TypeMismatch,       // Type mismatch errors

    // Installer errors
    CompilationFailed,  // General compilation failure
    FileNotFound,       // Source file not found
    DirectoryNotFound,  // Directory not found
    CopyFailed,         // File copy failed
    RegistrationFailed, // Package registration failed

    // Configuration errors
    InvalidProfile,     // Invalid Profile.ini entry
    DependencyOrder,    // Wrong dependency order
    CircularDependency, // Circular dependency detected

    // Other
    GeneralError,       // Unclassified error
    GeneralWarning,     // Unclassified warning
    GeneralHint         // Unclassified hint
};

//---------------------------------------------------------------------------
// Structured compile error/warning information
//---------------------------------------------------------------------------
struct TCompileIssue
{
    // Basic info
    TErrorSeverity Severity;
    TErrorSource Source;
    TErrorType Type;

    // Error details
    String ErrorCode;       // E2202, F2613, W1000, etc.
    String Message;         // Full error message
    String ShortMessage;    // Brief description

    // Location info
    String PackageName;     // Package being compiled
    String ComponentName;   // DevExpress component name
    String FileName;        // Source file with error
    int LineNumber;         // Line number in file
    String Platform;        // Win32, Win64, Win64x

    // Log info
    int LogLineNumber;      // Line number in log file

    // Constructor
    TCompileIssue()
        : Severity(TErrorSeverity::Error),
          Source(TErrorSource::Unknown),
          Type(TErrorType::GeneralError),
          LineNumber(0),
          LogLineNumber(0)
    {}

    // Get severity as string
    String GetSeverityStr() const
    {
        switch (Severity)
        {
            case TErrorSeverity::Hint: return L"HINT";
            case TErrorSeverity::Warning: return L"WARNING";
            case TErrorSeverity::Error: return L"ERROR";
            case TErrorSeverity::Fatal: return L"FATAL";
            default: return L"UNKNOWN";
        }
    }

    // Get source as string
    String GetSourceStr() const
    {
        switch (Source)
        {
            case TErrorSource::Compiler: return L"Compiler";
            case TErrorSource::Linker: return L"Linker";
            case TErrorSource::Installer: return L"Installer";
            case TErrorSource::Configuration: return L"Configuration";
            case TErrorSource::FileSystem: return L"FileSystem";
            case TErrorSource::Registry: return L"Registry";
            default: return L"Unknown";
        }
    }

    // Get type as string
    String GetTypeStr() const
    {
        switch (Type)
        {
            case TErrorType::MissingPackage: return L"Missing Package";
            case TErrorType::MissingUnit: return L"Missing Unit";
            case TErrorType::PackageRecompile: return L"Package Recompile Required";
            case TErrorType::SyntaxError: return L"Syntax Error";
            case TErrorType::TypeMismatch: return L"Type Mismatch";
            case TErrorType::CompilationFailed: return L"Compilation Failed";
            case TErrorType::FileNotFound: return L"File Not Found";
            case TErrorType::DirectoryNotFound: return L"Directory Not Found";
            case TErrorType::CopyFailed: return L"Copy Failed";
            case TErrorType::RegistrationFailed: return L"Registration Failed";
            case TErrorType::InvalidProfile: return L"Invalid Profile";
            case TErrorType::DependencyOrder: return L"Dependency Order";
            case TErrorType::CircularDependency: return L"Circular Dependency";
            default: return L"General";
        }
    }

    // Format for log output
    String FormatForLog() const
    {
        String result;
        result = L"[" + GetSeverityStr() + L"] ";

        if (!ComponentName.IsEmpty())
            result = result + ComponentName + L" > ";
        if (!PackageName.IsEmpty())
            result = result + PackageName;
        if (!Platform.IsEmpty())
            result = result + L" (" + Platform + L")";

        result = result + L"\n";
        result = result + L"  Type: " + GetTypeStr() + L"\n";
        result = result + L"  Source: " + GetSourceStr() + L"\n";

        if (!ErrorCode.IsEmpty())
            result = result + L"  Code: " + ErrorCode + L"\n";
        if (!FileName.IsEmpty())
        {
            result = result + L"  File: " + FileName;
            if (LineNumber > 0)
                result = result + L":" + String(LineNumber);
            result = result + L"\n";
        }

        result = result + L"  Message: " + Message + L"\n";

        if (LogLineNumber > 0)
            result = result + L"  Log Line: " + String(LogLineNumber) + L"\n";

        return result;
    }
};

typedef std::shared_ptr<TCompileIssue> TCompileIssuePtr;
typedef std::vector<TCompileIssuePtr> TCompileIssueList;

//---------------------------------------------------------------------------
// Error parser - parses compiler output to extract structured info
//---------------------------------------------------------------------------
class TErrorParser
{
public:
    // Parse a line of compiler output and return structured issue (or nullptr)
    static TCompileIssuePtr ParseLine(const String& line,
                                       const String& currentPackage,
                                       const String& currentComponent,
                                       const String& currentPlatform,
                                       int logLineNumber);

    // Check if line contains an error/warning/hint
    static bool IsErrorLine(const String& line);
    static bool IsWarningLine(const String& line);
    static bool IsHintLine(const String& line);

private:
    // Extract error code from message (E2202, F2613, W1000, etc.)
    static String ExtractErrorCode(const String& line);

    // Extract file name and line number from message
    static bool ExtractFileLocation(const String& line, String& fileName, int& lineNumber);

    // Determine error type from error code
    static TErrorType DetermineErrorType(const String& errorCode, const String& message);
};

} // namespace DxCore

#endif
