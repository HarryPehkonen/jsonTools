#!/usr/bin/env bash
# Temporary end-to-end check of the CLI shims (not part of the deliverable).
set -u
cd "$(dirname "$0")/build"

echo -n 'filter|sort|len|select: '
echo '{"items":[{"n":"bob","age":20},{"n":"ann","age":30},{"n":"cy","age":10}]}' \
  | ./jtFilter /items age --gt 15 \
  | ./jtSort /items n \
  | ./jtLen /items /count \
  | ./jtSelect /items /count

echo -n 'sort --desc-for:      '
echo '[{"a":1,"b":1},{"a":1,"b":2},{"a":0,"b":0}]' | ./jtSort a --desc-for b

echo -n 'natural sort:         '
echo '[10,9,100]' | ./jtSort

echo -n 'new|set|zip:          '
./jtNew '{}' | ./jtSet /k '["a","b"]' | ./jtSet /v '[1,2]' | ./jtZip /k /v

echo -n 'remove|get|keys:      '
echo '{"a":{"b":1,"c":2},"junk":9}' | ./jtRemove /junk | ./jtGet /a | ./jtKeys

echo -n 'type:                 '
echo '{"a":[1]}' | ./jtType /a

echo -n 'get --default:        '
echo '{"a":1}' | ./jtGet /nope --default '"fallback"'

echo -n 'len -p:               '
echo '{"items":[1,2]}' | ./jtLen /items /meta/count -p

echo -n 'zip --overwrite:      '
echo '{"k":["a","a"],"v":[1,2]}' | ./jtZip /k /v --overwrite

echo '--- error paths (stderr + exit code) ---'
for cmd in "./jtRemove /usr" "./jtGet /nope" "./jtSelect /a/n /b/n" "./jtSort n"; do
  echo -n "$cmd -> "
  echo '{"user":1,"a":{"n":1},"b":{"n":2}}' | $cmd 2>&1 >/dev/null
  echo "   (exit $?)"
done

echo -n 'zip length mismatch -> '
echo '{"k":["a","b"],"v":[1]}' | ./jtZip /k /v 2>&1 >/dev/null
echo -n 'filter type clash   -> '
echo '[{"a":"x"}]' | ./jtFilter a --gt 1 2>&1 >/dev/null
echo -n 'sort mixed types    -> '
echo '[1,"a"]' | ./jtSort 2>&1 >/dev/null
echo -n 'set out-of-range     -> '
echo '{"a":[1,2]}' | ./jtSet /a/5 9 2>&1 >/dev/null
echo "   (exit $?)"
echo -n 'set non-index        -> '
echo '{"a":[1,2]}' | ./jtSet /a/name 9 2>&1 >/dev/null
echo "   (exit $?)"
echo -n 'move out-of-range    -> '
echo '{"src":1,"dst":[1,2]}' | ./jtMove /src /dst/5 2>&1 >/dev/null
echo "   (exit $?)"
echo -n 'set scalar tunnel    -> '
echo '{"a":5}' | ./jtSet /a/b 1 2>&1 >/dev/null
echo "   (exit $?)"
echo -n 'pretty:'
echo '{"a":1}' | ./jtGet / --pretty
