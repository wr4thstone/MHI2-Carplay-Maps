#!/bin/sh
# Read-only discovery matching the MOI3 0334 display manager's sysfs interface.
# Optional first argument overrides the sysfs directory for offline fixtures.
set -u

root=${1:-/sys/devices/platform/avb_video}
if [ ! -d "$root" ]; then
    printf 'AVB video directory unavailable: %s\n' "$root" >&2
    exit 2
fi

found=0
unreadable=0
for talker in "$root"/*talker*; do
    [ -d "$talker" ] || continue
    found=1
    printf 'talker=%s\n' "${talker##*/}"
    for attribute in ip_dst_6_addr rtp_dst_port device_path; do
        path=$talker/$attribute
        if [ -r "$path" ]; then
            printf '%s=' "$attribute"
            cat "$path" || exit 3
            printf '\n'
        else
            printf '%s=<unreadable>\n' "$attribute"
            unreadable=1
        fi
    done
    printf '\n'
done

if [ "$found" -eq 0 ]; then
    printf 'No talker directories found.\n' >&2
    exit 2
fi
[ "$unreadable" -eq 0 ] || exit 3
exit 0
