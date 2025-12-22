object frmProgress: TfrmProgress
  Left = 0
  Top = 0
  Caption = 'DxAutoInstaller'
  ClientHeight = 400
  ClientWidth = 700
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  Position = poScreenCenter
  OnCreate = FormCreate
  OnCloseQuery = FormCloseQuery
  PixelsPerInch = 96
  TextHeight = 15
  
  object PanelTop: TPanel
    Left = 0
    Top = 0
    Width = 700
    Height = 50
    Align = alTop
    BevelOuter = bvNone
    Color = clWhite
    ParentBackground = False
    TabOrder = 0
    object LblTitle: TLabel
      Left = 16
      Top = 16
      Width = 668
      Height = 20
      AutoSize = False
      Caption = 'Initializing...'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clNavy
      Font.Height = -13
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentFont = False
    end
  end
  
  object PanelLogs: TPanel
    Left = 0
    Top = 50
    Width = 700
    Height = 300
    Align = alClient
    BevelOuter = bvNone
    Padding.Left = 8
    Padding.Top = 8
    Padding.Right = 8
    Padding.Bottom = 8
    TabOrder = 1
    object MemoLogs: TMemo
      Left = 8
      Top = 8
      Width = 684
      Height = 284
      Align = alClient
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Consolas'
      Font.Style = []
      ParentFont = False
      ReadOnly = True
      ScrollBars = ssVertical
      TabOrder = 0
    end
  end
  
  object PanelBottom: TPanel
    Left = 0
    Top = 350
    Width = 700
    Height = 50
    Align = alBottom
    BevelOuter = bvNone
    TabOrder = 2
    object BtnAction: TButton
      Left = 596
      Top = 12
      Width = 90
      Height = 28
      Action = ActClose
      TabOrder = 0
    end
  end
  
  object ActionList: TActionList
    Left = 40
    Top = 360
    object ActStop: TAction
      Caption = 'Stop'
      OnExecute = ActStopExecute
    end
    object ActClose: TAction
      Caption = 'Close'
      OnExecute = ActCloseExecute
    end
  end
end
