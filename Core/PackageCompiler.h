//---------------------------------------------------------------------------
// PackageCompiler - Compiles Delphi packages using DCC32/DCC64/DCC64X
//---------------------------------------------------------------------------
#ifndef PackageCompilerH
#define PackageCompilerH

#include <System.hpp>
#include <System.Classes.hpp>
#include "IDEDetector.h"
#include "Component.h"

namespace DxCore
{

//---------------------------------------------------------------------------
// Compile result
//---------------------------------------------------------------------------
struct TCompileResult
{
    bool Success;
    int ExitCode;
    String Output;
    String ErrorMessage;
    
    TCompileResult() : Success(false), ExitCode(-1) {}
};

//---------------------------------------------------------------------------
// Compile options
//---------------------------------------------------------------------------
struct TCompileOptions
{
    String PackagePath;           // Full path to .dpk file
    String BPLOutputDir;          // BPL output directory
    String DCPOutputDir;          // DCP output directory
    String UnitOutputDir;         // DCU output directory
    TStringList* SearchPaths;     // Unit search paths
    TStringList* Defines;         // Conditional defines
    bool GenerateCppFiles;        // -JL for C++Builder
    bool NativeLookAndFeel;       // -DUSENATIVELOOKANDFEELASDEFAULT
    
    TCompileOptions();
    ~TCompileOptions();
};

//---------------------------------------------------------------------------
// Output callback type
//---------------------------------------------------------------------------
#include <functional>
typedef std::function<void(const String&)> TOutputCallback;

//---------------------------------------------------------------------------
// Package Compiler
//---------------------------------------------------------------------------
class TPackageCompiler
{
private:
    TOutputCallback FOnOutput;
    
    String BuildCommandLine(const TIDEInfoPtr& ide, 
                            TIDEPlatform platform,
                            const TCompileOptions& options);
    TCompileResult ExecuteCompiler(const String& compilerPath, 
                                    const String& cmdLine,
                                    const String& workDir);
    void OutputLine(const String& line);
    
public:
    TPackageCompiler();
    ~TPackageCompiler();
    
    // Compile a package
    TCompileResult Compile(const TIDEInfoPtr& ide,
                           TIDEPlatform platform,
                           const TCompileOptions& options);
    
    // Output callback
    void SetOnOutput(TOutputCallback callback) { FOnOutput = callback; }
    
    // Get compiler path for platform
    static String GetCompilerPath(const TIDEInfoPtr& ide, TIDEPlatform platform);
    
    // Check if platform is supported
    static bool IsPlatformSupported(const TIDEInfoPtr& ide, TIDEPlatform platform);
};

//---------------------------------------------------------------------------
// Compiler command line options
//---------------------------------------------------------------------------
namespace CompilerOptions
{
    // Debug options (disable for release)
    const String NO_DEBUG_INFO = L"-$D-";
    const String NO_LOCAL_SYMBOLS = L"-$L-";
    const String NO_SYMBOL_REF = L"-$Y-";
    
    // Output options
    const String QUIET = L"-Q";
    const String BUILD_ALL = L"-B";
    
    // Path options
    const String UNIT_SEARCH_PATH = L"-U";      // -U"path"
    const String RESOURCE_PATH = L"-R";          // -R"path"
    const String UNIT_OUTPUT_DIR = L"-NU";       // -NU"path" (XE2+)
    const String UNIT_OUTPUT_DIR_OLD = L"-N0";   // -N0"path" (XE2-)
    
    // C++Builder options
    const String GENERATE_CPP = L"-JL";          // Generate .lib, .bpi, .hpp
    const String BPI_OUTPUT_DIR = L"-NB";        // -NB"path"
    const String HPP_OUTPUT_DIR = L"-NH";        // -NH"path"
    const String OBJ_OUTPUT_DIR = L"-NO";        // -NO"path"
    
    // Namespace options
    const String NAMESPACE_SEARCH = L"-NS";
    const String UNIT_ALIAS = L"-A";
    
    // Define options
    const String DEFINE = L"-D";
}

} // namespace DxCore

#endif
