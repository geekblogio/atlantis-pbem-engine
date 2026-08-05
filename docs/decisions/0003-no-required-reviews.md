# 0003 — No required GitHub reviews

**Status:** accepted, 2026-08-05.

## Context

Every change must be reviewed before it is committed. The obvious encoding is
`required_pull_request_reviews: { required_approving_review_count: 1 }`.

## Decision

`required_pull_request_reviews: null`. The review requirement is enforced in the working
agreement, not by GitHub.

## Why

**GitHub does not let an author approve their own pull request.** On a single-maintainer
repository, requiring one approval makes every pull request permanently unmergeable. It would
not add a review; it would replace the workflow with an obstacle and an eventual override.

The actual requirement — recorded as a non-negotiable in `CLAUDE.md` — is stronger than a
GitHub approval anyway: the complete diff is presented and approved **before the commit exists**,
not afterwards.

## Consequences

The protection settings do not, on their own, prove that anything was reviewed. That evidence
lives in the working agreement and the pull request bodies. `required_conversation_resolution`
is on, `enforce_admins` is on, so the parts GitHub *can* enforce are enforced for everyone.
