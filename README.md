# SteamVRAdaptiveBrightness

Overview
-
Small utility that continuisly adjusts the screen brightness of the valve index via SteamVR. This allowes for much better black levels in night scenes. This feels much more natural and it makes bright scenes much brighter in comparision. First time I felt blinded in VR lol.

How to use:
-
1. Download compiled build from releases.
2. Start this utility and SteamVR
3. SteamVR settings > Startup / Shutdown > Choose Startup Overlay Apps
4. Toggle AdaptiveBrightness to On (If it isn't already).

How to compile yourself:
-
1. Clone this repository.
2. Dowload the OpenVR SDK/API from valve from https://github.com/ValveSoftware/openvr
3. Download the Windows SDK https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
4. In Visual Studio go to your property pages.
5.   Configuration Properties > VC++ Directories > Include Directories > [...]\openvr-master\headers & C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um (Path to the headers folders)
6.   Configuration Properties > VC++ Directories > Libary Directories > [...]\openvr-master\lib\win64 & C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64 (Path to the libary folders)
7.   Linker > Input > Additional Dependencies > openvr_api.lib & d3d11.lib & d3dcompiler.lib (.lib files)
8. Ctrl + B (You should be able to build now)
9. Copy openvr_api.dll from [...]\openvr-master\bin\win64 into the build folder next to the exe.

Notice
-
Use this software at your own risk. I have no idea if this could caus any damage to your hardware or your body.
