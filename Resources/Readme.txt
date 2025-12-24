DxAutoInstaller C++ Edition
===========================
DevExpress VCL Components Automatic Installer

Author: Platon
Based on: https://github.com/Delphier/DxAutoInstaller
Original Copyright(c) 2023 by faceker@gmail.com

Supported:
- RAD Studio 12 Athens
- RAD Studio 13 Florence
- DevExpress VCL 25.1.2

Changelog
--------------------------
v2.0.0	2025/12/24
	Asynchronous installation - UI stays responsive during compilation.
	Win64x separate compilation with -jf:coffi -DDX_WIN64_MODERN flags.
	C++ Include/Library paths - automatic registration for all platforms.
	Win32 Classic compiler support - paths for Modern and Classic.
	Improved logging - detailed logs for debugging.
	Better error handling - graceful stop and error recovery.

v1.0.0	2025/01/xx
	Complete rewrite in C++Builder (x64 Modern / Clang).
	No JCL dependency - direct Windows Registry access.
	No DevExpress UI dependency - standard VCL components.
	
	New features:
	- Win64 Modern (Clang/LLVM) platform support
	- 64-bit IDE support for design-time packages
	- Separate IDE type selection (32-bit / 64-bit)
	- Target platform selection per IDE type
	- RAD Studio 12/13 only support
	- DevExpress VCL 25.1.2 profile
	- ExpressAI components support
	- ExpressHtmlEditor support
	
	Compilation strategy:
	- 32-bit IDE: design-time via dcc32, targets: Win32/Win64/Win64x
	- 64-bit IDE: design-time via dcc64, targets: Win64/Win64x only

Previous versions (Delphi edition by faceker@gmail.com):
--------------------------
v2.3.8	2024/06/15 - DevExpress VCL 24.1.3
v2.3.7	2023/11/20 - RAD Studio 12 Athens supported
v2.3.6	2023/06/25 - dxSkinWXI skin added
v2.3.5	2022/10/20 - DevExpress VCL 22.1.2
v2.3.4	2022/03/18 - Bugfix for component selecting
v2.3.3	2021/11/09 - Delphi 11 Alexandria supported
v2.3.2	2021/06/17 - DevExpress VCL 20.2.8
v2.3.1	2020/06/09 - DevExpress VCL 20.1.2
v2.3	2020/06/08 - Delphi 10.4 supported
v2.2.3	2019/12/14 - DevExpress VCL 19.2.2
v2.2.2	2019/07/26 - Skin names fix for v18.2.x
v2.2.1	2019/07/24 - DevExpress VCL 19.1.3
v2.2	2019/03/05 - DevExpress VCL 18.2.x compatible
v2.1.11	2018/12/02 - Delphi 10.3 Rio supported
v2.1.10	2018/06/28 - ExpressEntityMapping Framework
v2.0	2014/08/29 - Initial release
