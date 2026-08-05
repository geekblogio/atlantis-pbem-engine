# The JSON report

> **Audience:** anyone reading `report.<n>.json`, or adding a field to it.
> **Provenance:** upstream-friendly.

**Read this when** you are consuming the machine-readable report, or changing what goes into it.

## It is the source of truth, not an export

`Faction::build_json_report` (`faction.cpp`) builds an `nlohmann::json` document, and
`text_report_generator.cpp` renders `report.<n>` **from that document**. The JSON is not a
serialisation of the text report — the text report is a rendering of the JSON.

Two consequences:

- a field missing from the JSON cannot appear in the text report;
- adding something to the text report means adding it to the JSON first.

## When it is written

Only when the ruleset's `REPORT_FORMAT` includes `REPORT_FORMAT_JSON` — which **every ruleset
shipped here now does**, alongside `REPORT_FORMAT_TEXT`. A ruleset that turned the flag off would
write text only, and no environment variable would change that — see [cli.md](cli.md).

The file is `report.<faction-number>.json`, one per faction that gets a report.

## There is no schema file

The document is assembled imperatively in C++. **`faction.cpp` is the authority**; everything
below is a description of it, verified against real output, not a specification the code is
checked against. If the two disagree, the code is right and this document is stale — fix it in
the same pull request, as the contribution rules require.

Fields are **conditional**. A key that is absent for one faction on one turn may be present for
another. Consumers must treat every key as optional and never index blindly.

## Top level, a player faction

Observed on a `neworigins` turn:

| Key | Type | Notes |
| --- | --- | --- |
| `engine` | object | `version`, `ruleset`, `ruleset_version`, `json_report_version` |
| `name` | string | faction name, with the trailing number stripped |
| `number` | int | faction number |
| `date` | object | `month` (name), `year` |
| `type` | object | only the faction types with a non-zero allocation, e.g. `{"magic": 1, "martial": 1}` |
| `status` | object | `mages`, `apprentices`, `quartermasters`, `regions`, each `{current, allowed}` |
| `administrative` | object | `email`, `times_sent`, `password_unset`, `show_unit_attitudes`; `password` only when set; `quit` and `inactivity_deletion_turns` when they apply |
| `attitudes` | object | `default` plus one array per attitude — `ally`, `friendly`, `neutral`, `unfriendly`, `hostile` — each of `{name, number}` |
| `unclaimed_silver` | int | |
| `errors` | array | `{message, unit:{name, number}}` — order errors from this turn |
| `events` | array | `{category, message, unit:{name, number}}` |
| `battles` | array | present only when the faction saw a battle |
| `statistics` | array | present only with `FACTION_STATISTICS`; `{name, plural, tag, amount, max, total, rank}` |
| `skill_reports` | array | `{name, tag, level, description}` |
| `item_reports` | array | `{name, tag, description}` |
| `object_reports` | array | `{name, description}` |
| `regions` | array | see below |

```json
"engine": {
  "version": "5.2.5 (beta)",
  "ruleset": "NewOrigins",
  "ruleset_version": "3.0.0 (beta)",
  "json_report_version": "1.0.1 (beta)"
}
```

The version strings carry the `(beta)` suffix. Parse them, do not compare them as opaque
strings — see [compatibility.md](compatibility.md).

## The GM report is a different shape

The world-wide report for the NPC faction (`report.1.json`, written when `GM_REPORT` is on)
carries **only** `regions`, `skill_reports`, `item_reports` and `object_reports`. No `engine`,
no `date`, no `administrative`.

A consumer that reads `report.*.json` with a glob will hit this. Either skip the GM faction
explicitly, or key off the presence of `engine`.

This is also the expensive one: it is a report for a faction that sees every region, and it
dominates a turn's runtime. `ATLANTIS_NO_GM_REPORT` turns it off.

## A region

```json
{
  "coordinates": { "x": 14, "y": 0, "z": 1, "label": "surface" },
  "terrain": "swamp",
  "province": "Nema River",
  "present": true,
  "population": { … },
  "wages": { "amount": 11.0, "max": 115 },
  "tax": …,
  "entertainment": …,
  "products":   [ { "name": "wood", "plural": "wood", "tag": "WOOD", "amount": 17 } ],
  "markets":    { "for_sale": [ … ], "wanted": [ … ] },
  "exits":      [ { "direction": "Southeast", "region": { "coordinates": …, "terrain": …, "province": … } } ],
  "structures": null,
  "units":      [ … ]
}
```

Notes that cost time if you learn them from a crash instead:

- **`wages.amount` is a float**, `wages.max` an int.
- **`structures` is `null`, not `[]`,** when a region has none.
- `exits[].region` is a *reduced* region object — coordinates, terrain, province — not a full one.
- `present` is the faction's own presence in the region.
- `population`, `tax` and `entertainment` appear according to what the faction may observe. The
  GM report's regions carry a `description` instead. **Do not assume a fixed key set.**

A unit inside `units[]` carries `faction: {name, number}`, `flags` (an object of booleans),
`capacity` (`walking`, `riding`, `flying`, `swimming`), items, skills and the rest of the
descriptor built by `Unit::build_json_descriptor`.

## Adding a field

1. Add it in `faction.cpp` (or the relevant `build_json_*` helper).
2. Decide whether the text report should show it, and if so extend
   `text_report_generator.cpp`.
3. Re-record the snapshots — reports are compared byte for byte, so this *will* be red first.
4. Bump `JSON_REPORT_VERSION` if you removed or renamed anything. Purely additive changes do
   not need a bump, but say so in the pull request.
5. Update this document in the same pull request.
