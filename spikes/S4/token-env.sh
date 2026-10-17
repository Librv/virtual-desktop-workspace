#!/bin/sh
# Launch kate and dolphin with an activation token, then show the activation
# related environment of the new processes. Run via dev-session.sh run.
here=$(cd "$(dirname "$0")" && pwd)
"$here/build/s4launch" ${S4_TOKEN_FLAG---token} --wait 3000 org.kde.kate org.kde.dolphin &
sleep 1.5
for p in $(pgrep -x kate) $(pgrep -x dolphin); do
    echo "pid $p: $(tr '\0' '\n' </proc/$p/environ | grep -E 'ACTIVATION|STARTUP' | tr '\n' ' ')"
done
wait
