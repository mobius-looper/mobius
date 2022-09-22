#
# Visual Studio Definitions
#
# To switch compiler versions include the appropriate file below.
# This is also dependent on the compiler being on the path
# and the INCLUDE and LIB environment variables being set to the
# right version.  
#
# Currently the makefiles can't pick compiler versions you have
# to edit .bashrc.  Here are the definitions currently in use.
#
# # VC6 Definitions
# 
# export VC6="/cygdrive/c/Program Files/Microsoft Visual Studio"
# 
# export VC6_PATH="$VC6/common/msdev98/bin:$VC6/VC98/bin:$VC6/common/tools/WinNT:$VC6/common/tools"
# 
# export VC6_INCLUDE="C:/Program Files/Microsoft Visual Studio/VC98/atl/include;C:/Program Files/Microsoft Visual Studio/VC98/mfc/include;C:/Program Files/Microsoft Visual Studio/VC98/include"
# 
# export VC6_LIB="C:/Program Files/Microsoft Visual Studio/VC98/mfc/lib;C:/Program Files/Microsoft Visual Studio/VC98/lib"
# 
# # VC8 Definitions
# 
# export VC8_DOTNET="/cygdrive/c/Windows/Microsoft.NET/Framework/v2.0.50727"
# export VC8="/cygdrive/c/Program Files/Microsoft Visual Studio 8"
# export VC8_PATH="$VC8/Common7/IDE:$VC8/vc/bin:$VC8/Common7/tools:$VC8/Common7/tools/bin:$VC8/PlatformSDK/bin:$VC8/SDK/v2.0/bin:$DOTNET:$VC8/VC/VCPackages"
# 
# export VC8_INCLUDE="C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/INCLUDE;C:/Program Files/Microsoft Visual Studio 8/VC/INCLUDE;C:/Program Files/Microsoft Visual Studio 8/VC/PlatformSDK/include;C:/Program Files/Microsoft Visual Studio 8/SDK/v2.0/include;%INCLUDE%"
# 
# export VC8_LIB="C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/LIB;C:/Program Files/Microsoft Visual Studio 8/VC/LIB;C:/Program Files/Microsoft Visual Studio 8/VC/PlatformSDK/lib;C:/Program Files/Microsoft Visual Studio 8/SDK/v2.0/lib;%LIB%"
# 
# export VC8_LIBPATH="C:/WINDOWS/Microsoft.NET/Framework/v2.0.50727;C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/LIB"
# 
# export INCLUDE=$VC8_INCLUDE
# export LIB=$VC8_LIB
# export VC_PATH=$VC8_PATH
#
# # Note that VC bin has to be in front of MYBIN because the cygwin I now
# # have defines its own link.exe
#
# export PATH=".:$VC_PATH:......

#include common.vc6
#include common.vc8
include common.vc10
