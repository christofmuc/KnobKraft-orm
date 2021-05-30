cd winsparkle

rem Download nuget if required
wget https://dist.nuget.org/win-x86-commandline/v5.7.0/nuget.exe

rem Need to have nuget on the path
nuget install packages.config -OutputDirectory packages

rem Then run msbuild, need Command Line for Visual Studio for this to work
msbuild WinSparkle-2017.sln /t:Build /p:Configuration=Debug;WindowsTargetPlatformVersion=10.0.18362.0;Platform=x64

rem Problem is to specify the exact same Windows SDK version as you have (is a CMake output). Could also use the outer property when called from a solution,
rem but how to nest solutions?
