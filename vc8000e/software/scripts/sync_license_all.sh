#!/bin/sh

script=$SYSTEM_MODEL_HOME/../software/scripts/sync_license.pl
if [ ! -f $script ]; then
  echo "Please set \$SYSTEM_MODEL_HOME to the right directory"
  exit -1
fi
cd $SYSTEM_MODEL_HOME/../software/linux_reference/kernel_module
pwd
for f in gpl/*; do
  perl $script $(basename $f) $f
done
cd $SYSTEM_MODEL_HOME/../software/linux_reference/memalloc
pwd
for f in gpl/*; do
  perl $script $(basename $f) $f
done

