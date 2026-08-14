#!/usr/bin/env bash
#
# upstream-guard — a PreToolUse guard that travels with this repository.
#
# Nothing reaches Atlantis-PBEM/Atlantis without an explicit decision. That is a
# standing rule in CLAUDE.md, and on 2026-08-14 an agent broke it: `gh pr create`
# without --repo opened a pull request against upstream, because gh silently
# defaults to a fork's PARENT repository. A rule that lives only in prose gets
# broken by whoever did not check a tool's default that day.
#
# Registered in .claude/settings.json, so it applies in every clone of this
# repository on every machine — unlike a guard in ~/.claude, which protects one
# laptop and nothing else.
#
# Three tiers:
#
#   deny   the accident shape — a GitHub write with no --repo at all, inside a
#          clone whose remotes can reach upstream. Never intentional; the fix is
#          to name the repository. Also: any attempt to touch this guard.
#   ask    a deliberate, explicitly upstream-targeted operation. A permission
#          prompt names the repository and the human decides.
#   allow  reads of upstream, writes that name the fork, and everything in a
#          clone that cannot reach upstream at all.
#
# What this does NOT cover, stated plainly: a human typing `gh` in a terminal;
# a session with hooks disabled; any checkout without this file. It is the
# strongest portable layer available, not a proof.
#
# To change or lift it, edit this file by hand. Agents are denied the file tools
# and any shell command that names it.

set -uo pipefail

FORK='geekblogio/atlantis-pbem-engine'
UPSTREAM='Atlantis-PBEM'

FORK_RE="(-R|--repo)[[:space:]=]+${FORK}"
ANY_REPO_RE='(-R|--repo)[[:space:]=]+[A-Za-z0-9._-]+/[A-Za-z0-9._-]+'

# Machine-independent: match by path suffix, never by absolute path.
GUARD_RE='\.claude/hooks/upstream-guard\.sh|\.claude/settings\.json|atlantis-upstream-guard'

input=$(cat)
tool=$(printf '%s' "$input" | jq -r '.tool_name // ""')

emit() {
  jq -nc --arg d "$1" --arg r "$2" \
    '{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:$d,permissionDecisionReason:$r}}'
  exit 0
}
deny() { emit deny "$1"; }
ask()  { emit ask  "$1"; }

# The gh-default hazard exists only where a remote can actually reach upstream.
# Everywhere else this guard stays out of the way entirely.
in_fork_clone() {
  git remote -v 2>/dev/null | grep -qi "$UPSTREAM"
}

case "$tool" in
  Write|Edit|NotebookEdit)
    path=$(printf '%s' "$input" | jq -r '.tool_input.file_path // ""')
    if printf '%s' "$path" | grep -Eq "$GUARD_RE"; then
      deny "BLOCKED: $path is the upstream guard itself. Agents may not modify it by any means. To change or lift it, a human edits the file by hand."
    fi
    exit 0
    ;;
  Bash)
    cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')
    ;;
  *)
    exit 0
    ;;
esac

# --- deny: the guard protects itself, from the shell as well ---------------
if printf '%s' "$cmd" | grep -Eq "$GUARD_RE"; then
  deny "BLOCKED: this command names the upstream guard's own files. Agents may not move, copy or rewrite them. A human edits them by hand."
fi

# --- deny: the accident shape — a GitHub write with no --repo at all -------
GH_WRITE='gh[[:space:]]+(pr[[:space:]]+(create|merge|close|comment|edit|review|reopen|ready|lock|unlock)'
GH_WRITE+='|issue[[:space:]]+(create|close|comment|edit|reopen|delete|pin|unpin|transfer|develop|lock|unlock)'
GH_WRITE+='|release[[:space:]]+(create|delete|edit|upload)'
GH_WRITE+='|repo[[:space:]]+(delete|edit|archive|unarchive|rename|fork|sync|deploy-key)'
GH_WRITE+='|workflow[[:space:]]+(run|enable|disable)'
GH_WRITE+='|secret[[:space:]]+(set|delete)|variable[[:space:]]+(set|delete)'
GH_WRITE+='|label[[:space:]]+(create|delete|edit|clone)'
GH_WRITE+='|run[[:space:]]+(cancel|rerun|delete)|cache[[:space:]]+delete'
GH_WRITE+='|gist[[:space:]]+(create|delete|edit|rename))'

if printf '%s' "$cmd" | grep -Eq "$GH_WRITE"; then
  if ! printf '%s' "$cmd" | grep -Eq "$ANY_REPO_RE"; then
    if in_fork_clone; then
      deny "BLOCKED: a GitHub write with no --repo, in a clone that can reach ${UPSTREAM}/Atlantis. gh defaults to the fork's PARENT repository — that is how pull request #297 was opened by accident. Re-run with --repo ${FORK}."
    fi
  fi
fi

# --- ask: deliberate upstream operations need a human ----------------------
if printf '%s' "$cmd" | grep -Eq '(^|[;&|(]|[[:space:]])git([[:space:]]+-[^[:space:]]+)*[[:space:]]+push' \
   && printf '%s' "$cmd" | grep -Eq "(upstream|$UPSTREAM)"; then
  ask "This pushes git refs toward ${UPSTREAM}/Atlantis. Nothing reaches upstream without an explicit decision — approve only if you mean it."
fi

if printf '%s' "$cmd" | grep -qF "$UPSTREAM"; then
  READ_OK='gh[[:space:]]+(pr[[:space:]]+(view|list|diff|checks|status)'
  READ_OK+='|issue[[:space:]]+(view|list|status)|repo[[:space:]]+(view|list)'
  READ_OK+='|release[[:space:]]+(view|list)|run[[:space:]]+(view|list|watch)'
  READ_OK+='|search|browse|label[[:space:]]+list)'
  READ_OK+='|git[[:space:]]+(fetch|ls-remote|log|diff|show|remote)'

  # gh api reads freely; a write method is a write.
  if printf '%s' "$cmd" | grep -Eq 'gh[[:space:]]+api'; then
    if printf '%s' "$cmd" | grep -Eq '(-X|--method)[[:space:]=]+(POST|PUT|PATCH|DELETE)'; then
      ask "This is a gh api WRITE against ${UPSTREAM}/Atlantis. Approve only if you mean to change something upstream."
    fi
    exit 0
  fi

  if ! printf '%s' "$cmd" | grep -Eq "$READ_OK"; then
    ask "This command targets ${UPSTREAM}/Atlantis and is not a recognised read. Nothing reaches upstream without an explicit decision — approve only if you mean it."
  fi
fi

exit 0
