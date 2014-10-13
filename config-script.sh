#! /bin/sh
#
# This script generates configuration directives for runover.

# The machine script. This can probably be left alone.

echo "machinescript /etc/runover/machine-script.sh"

# If we can detect if we are running under DQS or another batch #
# system, created a "jobname" directive to pass this information to
# runover.

