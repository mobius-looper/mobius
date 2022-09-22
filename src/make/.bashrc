#
# Cygwin bash init file for Windows XP/Vista
# 
# Some of this is necessary to build Mobius but there is a lot of 
# legacy shit that has nothing to do with Mobius in here that needs to
# be weeded out and put into the makefiles.  We should have zero
# or relatively obvious dependencies on the environment.
# 

echo "Reading $HOME/.bashrc"

# VC6 Definitions

export VC6="/cygdrive/c/Program Files/Microsoft Visual Studio"

export VC6_PATH="$VC6/common/msdev98/bin:$VC6/VC98/bin:$VC6/common/tools/WinNT:$VC6/common/tools"

export VC6_INCLUDE="C:/Program Files/Microsoft Visual Studio/VC98/atl/include;C:/Program Files/Microsoft Visual Studio/VC98/mfc/include;C:/Program Files/Microsoft Visual Studio/VC98/include"

export VC6_LIB="C:/Program Files/Microsoft Visual Studio/VC98/mfc/lib;C:/Program Files/Microsoft Visual Studio/VC98/lib"

# VC8 Definitions

export VC8_DOTNET="/cygdrive/c/Windows/Microsoft.NET/Framework/v2.0.50727"
export VC8="/cygdrive/c/Program Files/Microsoft Visual Studio 8"
export VC8_PATH="$VC8/Common7/IDE:$VC8/vc/bin:$VC8/Common7/tools:$VC8/Common7/tools/bin:$VC8/PlatformSDK/bin:$VC8/SDK/v2.0/bin:$DOTNET:$VC8/VC/VCPackages"

export VC8_INCLUDE="C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/INCLUDE;C:/Program Files/Microsoft Visual Studio 8/VC/INCLUDE;C:/Program Files/Microsoft Visual Studio 8/VC/PlatformSDK/include;C:/Program Files/Microsoft Visual Studio 8/SDK/v2.0/include;%INCLUDE%"

export VC8_LIB="C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/LIB;C:/Program Files/Microsoft Visual Studio 8/VC/LIB;C:/Program Files/Microsoft Visual Studio 8/VC/PlatformSDK/lib;C:/Program Files/Microsoft Visual Studio 8/SDK/v2.0/lib;%LIB%"

export VC8_LIBPATH="C:/WINDOWS/Microsoft.NET/Framework/v2.0.50727;C:/Program Files/Microsoft Visual Studio 8/VC/ATLMFC/LIB"

# select Visual Studio

#export INCLUDE=$VC6_INCLUDE
#export LIB=$VC6_LIB
#export VC_PATH=$VC6_PATH

export INCLUDE=$VC8_INCLUDE
export LIB=$VC8_LIB
export VC_PATH=$VC8_PATH

# adjust the prompt
PS1='[$PWD] '
set -o emacs

# set this for cywin make to use Unix style makes, necessary for some
# java stuff
set MAKE_MODE=UNIX

alias purge="rm -f *.*~ *~"
alias nmkae="nmake"
alias ma="nmake"

# now that we have Tompson's hd, we don't need this
#alias hd='od -t x2 -A d'

# necessary to build Doc2Html and older muse Java code
export JLHOME="c:/larson/dev/muse"

export JL_CVSROOT=":local:c:/larson/dev/cvsroot"
export MAC_CVSROOT=":pserver:jeff@mac:/Users/Jeff/cvsroot cvs login"
export CVSROOT=$MAC_CVSROOT

alias jlcvs="export CVSROOT=$JL_CVSROOT"
alias maccvs="export CVSROOT=$MAC_CVSROOT;export CVS_RSH="

# seems to be necessary to explicitly set this, not reading /etc/man.config?
#export MANPATH="/cygdrive/c/cygwin/usr/man"

export mobius=/cygdrive/c/larson/dev/mobius/src/mobius

export JAVA_HOME="/cygdrive/c/work/java/jdk15.0_09"
export JAVABIN="/cygdrive/c/work/java/jdk15.0_09/bin"

export MYBIN="/cygdrive/c/cygwin/bin:/cygdrive/c/larson/bin:c/cygdrive/c/larson/dev/bin"

export WINBIN="/cygdrive/c/Windows/system32:/cygdrive/c/Windows"

# upgraded 5/19/2009
#export UTILBIN="/cygdrive/c/Program Files/Subversion/bin"
export UTILBIN="/cygdrive/c/Program Files/CollabNet Subversion Client"

export MYSQLBIN="/cygdrive/c/Program Files/MySQL/MySQL Server 5.0/bin"

export WSHOME=c:/work/idm
export WSBIN=/cygdrive/c/work/idm/bin

# this *must not* use /cygdrive/c the console.sh script uses this
# to buidl CLASSPATH
#export SPROOT="c:/work/complianceiq/branches/2.0"
#export SPROOT="c:/work/complianceiq/branches/jpmc"
#export SPROOT="c:/work/complianceiq/branches/poc84"
#export SPROOT="c:/work/complianceiq/branches/2.5p"
#export SPROOT="c:/work/complianceiq/branches/poc811"
#export SPROOT="c:/work/complianceiq/branches/3.1p"
export SPROOT="c:/work/complianceiq/trunk"
export SPHOME=$SPROOT/build

export CYGSPROOT="/cygdrive/c/work/complianceiq/trunk"
#export CYGSPROOT="/cygdrive/c/work/complianceiq/branches/3.1p"
#export CYGSPROOT="/cygdrive/c/work/complianceiq/branches/poc811"
#export CYGSPROOT="/cygdrive/c/work/complianceiq/branches/2.5p"
#export CYGSPROOT="/cygdrive/c/work/complianceiq/branches/2.0"
#export CYGSPROOT="/cygdrive/c/work/complianceiq/branches/poc84"

export WORKBIN=$CYGSPROOT/src/bin:/cygdrive/c/work/ant/apache-ant-1.7.1/bin

# Note that VC bin has to be in front of MYBIN because the cygwin I now
# have defines its own link.exe

export PATH=".:$VC_PATH:$MYBIN:$WORKBIN:$WSBIN:$JAVABIN:$MYSQLBIN:$WINBIN:$UTILBIN"
#export PATH=".:$MYBIN:$WORKBIN:$JAVABIN"

# simplified path hacked when trying to debug the newline translation problem
#export PATH=".:/cygdrive/c/cygwin/bin:/cygdrive/c/larson/bin:c/cygdrive/c/larson/dev/bin:/cygdrive/c/work/ant/apache-ant-1.7.1/bin:/cygdrive/c/Program Files/Subversion/bin:$CYGSPROOT/script:/cygdrive/c/work/java/jdk15.0_09/bin:/cygdrive/c/Program Files/MySQL/MySQL Server 5.0/bin:/cygdrive/c/Windows/system32:/cygdrive/c/Windows"

# need to conver this to /cygdrive convention to avoid confusing 
# console.sh
#export CLASSPATH=


