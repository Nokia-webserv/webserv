#!/usr/bin/env bash
# Usage: ./dbg.sh [-c | -u] file.cpp [file2.cpp ...]
#   -c comment, -u uncomment, no flag toggle
set -u

mode=toggle
case "${1:-}" in
  -c) mode=comment; shift ;;
  -u) mode=uncomment; shift ;;
esac

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 [-c|-u] file..." >&2
  exit 1
fi

rc=0
for f in "$@"; do
  if [ ! -f "$f" ]; then
    echo "$f: file not found" >&2
    rc=1
    continue
  fi

  tmp=$(mktemp) || exit 1
  if awk -v mode="$mode" -v file="$f" '
    function is_start(s) { return s ~ /^[ \t]*\/\/[ \t]*DEBUG[ \t]+START([ \t]|$)/ }
    function is_end(s)   { return s ~ /^[ \t]*\/\/[ \t]*DEBUG[ \t]+END([ \t]|$)/ }

    NR == FNR {
      if (is_start($0)) {
        if (inside) err = file ":" FNR ": nested DEBUG START"
        inside = 1; start_line = FNR; blocks++
      } else if (is_end($0)) {
        if (!inside) err = file ":" FNR ": DEBUG END without DEBUG START"
        inside = 0
      } else if (inside && $0 !~ /^[ \t\r]*$/) {
        total++
        if ($0 ~ /^[ \t]*\/\//) commented++
      }
      next
    }

    !decided {
      decided = 1
      if (!err && inside) err = file ":" start_line ": DEBUG START without DEBUG END"
      if (err) { print err > "/dev/stderr"; bad = 1; exit 1 }
      action = mode
      if (mode == "toggle") action = (total > 0 && total == commented) ? "uncomment" : "comment"
    }

    is_start($0) { inside = 1; print; next }
    is_end($0)   { inside = 0; print; next }
    !inside || $0 ~ /^[ \t\r]*$/ { print; next }

    {
      match($0, /^[ \t]*/)
      indent = substr($0, 1, RLENGTH)
      rest = substr($0, RLENGTH + 1)
      if (action == "comment") {
        if (rest ~ /^\/\//) print
        else print indent "// " rest
      } else if (rest ~ /^\/\//) {
        rest = substr(rest, 3)
        sub(/^ /, "", rest)
        print indent rest
      } else print
    }

    END {
      if (NR == FNR && inside && !err) err = file ":" start_line ": DEBUG START without DEBUG END"
      if (blocks == 0) print file ": no DEBUG blocks found" > "/dev/stderr"
    }
  ' "$f" "$f" > "$tmp"; then
    cat "$tmp" > "$f" || rc=1
  else
    rc=1
  fi
  rm -f "$tmp"
done
exit "$rc"
