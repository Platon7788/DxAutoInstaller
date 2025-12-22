//---------------------------------------------------------------------------
// DxAutoInstaller C++ Edition v1.0.0
// DevExpress VCL Components Automatic Installer
//
// Author: Platon (vteme777@gmail.com)
// GitHub: https://github.com/Platon7788/DxAutoInstaller
//
// Based on original Delphi version by Delphier
// https://github.com/Delphier/DxAutoInstaller
//
// Rewritten from Delphi to C++Builder (x64 Modern / Clang)
// No JCL dependency - direct Windows Registry access
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
#include <tchar.h>
//---------------------------------------------------------------------------
#include <Vcl.Styles.hpp>
#include <Vcl.Themes.hpp>
#include "MainForm.h"
int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
    try
    {
        Application->Initialize();
        Application->Title = L"DxAutoInstaller";
        TStyleManager::TrySetStyle("Glossy");
		Application->CreateForm(__classid(TfrmMain), &frmMain);
		Application->Run();
    }
    catch (Exception &exception)
    {
        Application->ShowException(&exception);
    }
    catch (...)
    {
        try
        {
            throw Exception(L"");
        }
        catch (Exception &exception)
        {
            Application->ShowException(&exception);
        }
    }
    return 0;
}
//---------------------------------------------------------------------------

