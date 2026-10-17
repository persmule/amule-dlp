#!/usr/bin/env bash
# Exercise DELETE /shared/{hash} on this phase's own disposable fixture only.
set -euo pipefail
API="${HOST:-localhost:4713}/api/v1"
if [[ -z "${AMULE_SHARED_DIR:-}" ]]; then
  echo 'SKIP: shared deletion requires AMULE_SHARED_DIR for a disposable fixture'
  exit 0
fi
command -v jq >/dev/null
FIXTURE=$(mktemp "$AMULE_SHARED_DIR/amuleapi-delete-XXXXXXXX.txt")
BODY=$(mktemp)
trap 'rm -f "$FIXTURE" "$BODY"' EXIT
head -c 1024 /dev/urandom > "$FIXTURE"
NAME=${FIXTURE##*/}
TOKEN=$(curl -fsS -H 'Content-Type: application/json' \
  -d "$(jq -nc --arg p "${ADMIN_PASS:-adminpass}" '{password:$p}')" \
  "$API/auth/login?include_token=true" | jq -er .token)
GUEST=$(curl -fsS -H 'Content-Type: application/json' \
  -d "$(jq -nc --arg p "${GUEST_PASS:-guestpass}" '{password:$p}')" \
  "$API/auth/login?include_token=true" | jq -er .token)
curl -fsS -X POST -H "Authorization: Bearer $TOKEN" "$API/shared_reload" >/dev/null
HASH=''
for ((i=0; i<30; i++)); do
  HASH=$(curl -fsS -H "Authorization: Bearer $TOKEN" \
    "$API/shared?limit=1000000000" \
    | jq -r --arg n "$NAME" '.shared[] | select(.name==$n) | .hash' | head -1)
  [[ -n "$HASH" ]] && break
  sleep 1
done
[[ -n "$HASH" ]] || { echo 'FAIL: disposable fixture was not shared'; exit 1; }
TARGET=$(curl -fsS -H "Authorization: Bearer $TOKEN" "$API/shared/$HASH" \
  | jq -ec '{confirmed_target:{ecid,name,directory,incomplete}}')
# Verify the record still names our exact fixture before exercising destructive writes.
[[ $(jq -r '.confirmed_target.name' <<<"$TARGET") == "$NAME" ]]
request() {
  local want=$1 label=$2
  shift 2
  local status
  status=$(curl -sS --max-time 10 -o "$BODY" -w '%{http_code}' "$@")
  [[ "$status" == "$want" ]] || { echo "FAIL: $label: wanted $want, got $status"; cat "$BODY"; exit 1; }
  echo "PASS: $label"
}
request 401 'unauthenticated deletion denied' -X DELETE "$API/shared/$HASH"
request 403 'guest deletion denied' -X DELETE -H "Authorization: Bearer $GUEST" \
  -H 'Content-Type: application/json' -d "$TARGET" "$API/shared/$HASH"
request 400 'bodyless deletion denied' -X DELETE -H "Authorization: Bearer $TOKEN" "$API/shared/$HASH"
request 400 'missing confirmation denied' -X DELETE -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' -d '{}' "$API/shared/$HASH"
STALE=$(jq -c '.confirmed_target.name="stale.txt"' <<<"$TARGET")
request 409 'stale target denied' -X DELETE -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' -d "$STALE" "$API/shared/$HASH"
[[ $(jq -r '.error.code' "$BODY") == selection_changed && -f "$FIXTURE" ]]
request 204 'confirmed standalone shared file deleted' -X DELETE -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' -d "$TARGET" "$API/shared/$HASH"
[[ ! -s "$BODY" && ! -e "$FIXTURE" ]]
request 404 'shared detail immediately gone' -H "Authorization: Bearer $TOKEN" "$API/shared/$HASH"
