#!/usr/bin/env bash

# EDIT these according to your system configuration
export SIMDIR=$HOME/Acads/CS422/HW3/Ksim
export MODERN_ARCH=1
export MIPS_BINS_LOCATION=$HOME/bin/mips

# Not working. Use /usr/local/lib folder
#export MIPS_LIBS_LOCATION=$HOME/lib/mips

export CPU=X86
export ARCH=x86
export FLASH_TOOLS=$SIMDIR/Tools
export TWINEDIR=$SIMDIR/Tools
export FLASH_PERL_EXEC=/usr/bin/perl
export PATH=$PATH:$SIMDIR/Tools/bin/$CPU:$SIMDIR/Tools/bin:$MIPS_BINS_LOCATION
