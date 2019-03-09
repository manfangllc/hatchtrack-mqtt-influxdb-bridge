#!/bin/sh

# Directory which script resides.
DIR="$( cd "$(dirname "$0")" ; pwd -P )"
# Patch command we want to run.
PATCH="patch -N -s -d $DIR/../lib/mbedtls -p 1 -i $DIR/mbedtls-1.patch"

$PATCH --dry-run
# If the patch has not been applied then the $? which is the exit status for
# last command would have a success status code = 0
if [ $? -eq 0 ];
then
    # Apply the patch.
    $PATCH
fi

exit 0
