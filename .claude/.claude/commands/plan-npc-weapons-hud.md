---
description: Plans and creates GitHub issues for the weapons system, HUD, and ammo mechanics in the LLM-NPC game. Creates a milestone with ordered issues covering C++ game objects, inventory, HUD rendering, and NPC integration.
argument-hint: Optional focus e.g. "weapons only" or "HUD only" (blank = all three systems)
---

Read .claude/skills/github/SKILL.md and .claude/skills/code-economy/SKILL.md
before doing anything.

Also read the existing codebase to understand what's already there:
```bash
find src/ -type f | head -40
grep -rn "inventory\|weapon\|ammo\|hud\|HUD" src/ 2>/dev/null | head -20
grep -rn "proposed_action\|give_item" src/ 2>/dev/null | head -10
```

This tells you what exists before planning what to build.

---

You are planning the weapons, HUD, and ammo systems for the LLM-NPC game.
Create GitHub issues scoped for overnight autonomous sessions, ordered so
each builds on the previous. All labelled ready-for-ai, assigned to you.

```python
import os
from github import Github

gh       = get_github_client()
repo     = get_repo(gh)
username = os.environ["GITHUB_USERNAME"]

create_overnight_labels(repo)

milestone = repo.create_milestone(
    title="Weapons, HUD, and Ammo Systems",
    description=(
        "Add a complete weapons and inventory system to the LLM-NPC game, "
        "including HUD rendering, ammo management, and integration with the "
        "NPC trust and proposed_action system so NPCs can give/sell weapons."
    )
)

issues = [

    # ── PHASE 1: WEAPONS ──────────────────────────────────────────────────────

    {
        "title": "Define weapon data schema and C++ WeaponDefinition struct",
        "body": """## What to change
Create the foundational weapon data layer. Every weapon in the game
is defined by a WeaponDefinition — a plain data struct with no game logic.

## Specifically
Create `src/weapons/weapon_definition.h`:
```cpp
struct WeaponDefinition {
    std::string id;           // e.g. "pistol", "rifle", "bow"
    std::string display_name; // shown in HUD and NPC dialogue
    int         damage;       // per-hit damage
    int         fire_rate;    // shots per minute
    int         range;        // effective range in game units
    int         mag_capacity; // rounds per magazine
    float       reload_time;  // seconds
    std::string ammo_type;    // e.g. "pistol_ammo", "rifle_ammo", "arrow"
};
```

Create `src/weapons/weapon_registry.h/.cpp`:
- A singleton (or injected dependency) that maps id → WeaponDefinition
- Load from a JSON file at startup (`data/weapons.json`)
- Validated on load — missing required fields = startup error

Create `data/weapons.json` with at least 3 weapons:
```json
[
  {"id": "pistol", "display_name": "Pistol", "damage": 25,
   "fire_rate": 60, "range": 50, "mag_capacity": 12,
   "reload_time": 1.5, "ammo_type": "pistol_ammo"},
  {"id": "rifle", "display_name": "Rifle", "damage": 45,
   "fire_rate": 30, "range": 200, "mag_capacity": 20,
   "reload_time": 2.5, "ammo_type": "rifle_ammo"},
  {"id": "bow", "display_name": "Bow", "damage": 60,
   "fire_rate": 15, "range": 150, "mag_capacity": 1,
   "reload_time": 0.0, "ammo_type": "arrow"}
]
```

## How to verify
Unit tests in `tests/test_weapon_registry.cpp`:
- Registry loads weapons.json without error
- Looking up "pistol" returns correct stats
- Missing required field in JSON triggers startup error
- Unknown weapon id returns std::nullopt not a crash

## Constraints
WeaponDefinition is pure data — no methods, no game logic.
Registry is read-only after startup — weapons are defined in JSON, not code.
Follow existing C++ conventions in the project.

## Code economy note
stdlib JSON parsing (nlohmann/json if already a dep, else add it) is enough.
No custom parser. No weapon class hierarchy yet — that comes later.

## Branch name
`feature/issue-N-weapon-definition-schema`

## Concept for learning materials
"Data-driven game design with JSON schemas and C++ registry patterns"
""",
    },

    {
        "title": "Implement player weapon inventory and equipped weapon state",
        "body": """## What to change
Add weapon inventory to the player — what weapons the player carries,
which is currently equipped, and basic pickup/drop operations.

## Specifically
Create `src/weapons/player_inventory.h/.cpp`:
```cpp
class PlayerInventory {
public:
    bool        add_weapon(const std::string& weapon_id);    // pickup
    bool        remove_weapon(const std::string& weapon_id); // drop
    bool        equip(const std::string& weapon_id);         // switch weapon
    std::string equipped_weapon_id() const;
    int         ammo_count(const std::string& ammo_type) const;
    bool        add_ammo(const std::string& ammo_type, int count);
    bool        consume_ammo(const std::string& ammo_type, int count);
    std::vector<std::string> carried_weapons() const;
};
```

Rules:
- Max 3 weapons carried at once (configurable constant)
- Cannot equip a weapon not in inventory
- Ammo tracked by type, not by weapon
- consume_ammo returns false if insufficient — caller decides what to do

## How to verify
Unit tests in `tests/test_player_inventory.cpp`:
- add_weapon / remove_weapon round-trip
- equip returns false for uncarried weapon
- ammo_count returns 0 for unknown type (not a crash)
- consume_ammo returns false when ammo < requested
- Cannot carry more than MAX_WEAPONS

## Constraints
No UI code in this class. Pure game logic.
Thread-safe if the game loop and UI run on separate threads.

## Code economy note
std::unordered_map for ammo counts. std::vector for carried weapons.
No custom container classes needed.

## Branch name
`feature/issue-N-player-weapon-inventory`

## Concept for learning materials
"Inventory system design: state encapsulation and invariant enforcement"
""",
    },

    {
        "title": "Implement firing mechanic and ammo consumption",
        "body": """## What to change
Add the fire action to the player — consuming ammo, applying damage,
respecting fire rate, and triggering reload when empty.

## Specifically
Create `src/weapons/weapon_controller.h/.cpp`:
```cpp
class WeaponController {
public:
    WeaponController(PlayerInventory& inventory,
                     const WeaponRegistry& registry);

    FireResult fire();    // attempt to fire equipped weapon
    void       reload();  // begin reload if equipped weapon is not full
    bool       is_reloading() const;
    float      reload_progress() const;  // 0.0 to 1.0

    void update(float delta_time);  // call each game tick for reload timer
};

enum class FireResult {
    FIRED,       // success — damage applied
    NO_WEAPON,   // nothing equipped
    NO_AMMO,     // out of ammo — trigger reload hint
    RELOADING,   // currently reloading
    RATE_LIMITED // fired too recently (fire rate limit)
};
```

## How to verify
Unit tests in `tests/test_weapon_controller.cpp`:
- FIRED result when weapon + ammo available
- NO_AMMO when ammo exhausted
- RELOADING blocks further fire attempts
- Rate limiting: fire twice in <fire_rate interval → RATE_LIMITED
- reload_progress moves from 0.0 to 1.0 over reload_time seconds

## Constraints
No rendering or UI. WeaponController takes delta_time — it is not
responsible for the game clock, only for consuming it.
Damage application is a callback or event — WeaponController does not
directly modify enemy health (that creates coupling).

## Code economy note
FireResult enum instead of bool + error code. No exceptions for game logic.

## Branch name
`feature/issue-N-weapon-firing-mechanic`

## Concept for learning materials
"Game loop timing: delta time, rate limiting, and state machines"
""",
    },

    {
        "title": "Integrate weapons with NPC proposed_action system",
        "body": """## What to change
Allow NPCs to give, sell, or withhold weapons via the proposed_action
system. This is the connection between the LLM dialogue layer and the
C++ inventory layer.

## Specifically
Add to the C++ action validation layer (wherever proposed_action is handled):
- New action type: `"give_weapon"`
  ```json
  {
    "type": "give_weapon",
    "parameters": {"weapon_id": "pistol"},
    "trust_required": 60
  }
  ```
- Validation: weapon_id must exist in WeaponRegistry
- Execution: calls PlayerInventory::add_weapon() if trust threshold met
- Response to LLM: inject current inventory into NPC context so the NPC
  knows what the player already carries

Add to the NPC system prompt template for weapon-relevant NPCs (blacksmith,
guard, merchant):
```
PLAYER INVENTORY: {weapon_list}
You may offer weapons via proposed_action give_weapon if trust >= 60.
You know what the player carries and can comment on it naturally.
```

Update the game-state-auditor checklist to include give_weapon validation.

## How to verify
Integration test: simulate NPC dialogue → proposed_action give_weapon →
validation layer → PlayerInventory receives weapon.
Test: trust below threshold → action rejected → inventory unchanged.
Test: unknown weapon_id → action rejected → error logged.

## Constraints
Trust threshold for give_weapon is always checked in C++ from game state —
never trust the value the LLM returns in trust_required.
Run game-state-auditor agent before committing this issue.

## Code economy note
Reuse the existing proposed_action validation pattern exactly.
No new validation framework — just one more case in the existing switch.

## Branch name
`feature/issue-N-npc-weapon-actions`

## Concept for learning materials
"Trust-gated NPC actions: bridging LLM output and game state safely"
""",
    },

    # ── PHASE 2: AMMO ─────────────────────────────────────────────────────────

    {
        "title": "Add ammo pickup items and world spawning",
        "body": """## What to change
Add ammo as a pickup item that spawns in the world and adds to
PlayerInventory when collected.

## Specifically
Create `src/world/pickup_item.h`:
```cpp
struct PickupItem {
    std::string id;
    std::string type;       // "ammo", "weapon", "health"
    std::string subtype;    // ammo_type or weapon_id
    int         quantity;
    float       x, y;       // world position
    bool        collected;
};
```

Create `src/world/pickup_spawner.h/.cpp`:
- Reads spawn points from `data/world/spawns.json`
- Respawns ammo at defined intervals
- On collection: calls PlayerInventory::add_ammo()

Add to `data/world/spawns.json`:
```json
[
  {"type": "ammo", "subtype": "pistol_ammo", "quantity": 30,
   "x": 100, "y": 200, "respawn_seconds": 60},
  {"type": "ammo", "subtype": "rifle_ammo",  "quantity": 20,
   "x": 300, "y": 150, "respawn_seconds": 90}
]
```

## How to verify
Unit tests:
- Spawner loads spawns.json without error
- Pickup collected → inventory ammo count increases
- Collected pickup is marked collected and not collectable again
- Respawn timer resets collected state after interval

## Constraints
Collision detection for pickup is out of scope — just a position check.
Spawner does not render anything — pure game logic.

## Code economy note
Spawn data in JSON, not hardcoded. Same JSON pattern as weapons.json.

## Branch name
`feature/issue-N-ammo-pickup-spawning`

## Concept for learning materials
"World item spawning systems and respawn timer patterns"
""",
    },

    # ── PHASE 3: HUD ──────────────────────────────────────────────────────────

    {
        "title": "Add HUD overlay: current weapon and ammo count display",
        "body": """## What to change
Add a HUD overlay that shows the currently equipped weapon name and
current ammo count. Updates in real-time from PlayerInventory state.

## Specifically
Create `src/gui/hud/weapon_hud.h/.cpp` (PyQt6 QWidget overlay):
- Positioned bottom-right of the game window
- Shows: weapon icon (or name if no icon), current ammo / mag capacity
- Updates every game tick via a signal from PlayerInventory
- Visually distinct states:
  - Normal: white text
  - Low ammo (< 20% of mag): yellow text
  - No ammo: red text + "RELOAD" indicator
  - Reloading: animated reload progress bar

Layout:
```
[PISTOL]  8 / 12
```

## How to verify
Visual test: `tests/visual/test_hud_weapon.py`
- `test_hud_normal_state()` → golden: hud_weapon_normal.png
- `test_hud_low_ammo_state()` → golden: hud_weapon_low_ammo.png
- `test_hud_no_ammo_state()` → golden: hud_weapon_no_ammo.png
Unit test: HUD updates when inventory signal fires.

## Constraints
Read .claude/skills/visual-testing/SKILL-visual-testing.md before writing tests.
Golden images generated on EC2 (not Mac) to match CI rendering.
HUD must not block mouse events — use Qt::WindowTransparentForInput.

## Code economy note
Qt signal/slot for inventory updates — no polling.
Text rendering only for now; icons are a separate future issue.

## Branch name
`feature/issue-N-hud-weapon-ammo-display`

## Concept for learning materials
"Real-time HUD design with Qt signals and overlay widgets"
""",
    },

    {
        "title": "Add HUD overlay: NPC relationship indicator for nearby NPCs",
        "body": """## What to change
Add a HUD element showing trust/relationship level for the nearest NPC
when the player is within interaction range. Gives the player visibility
into NPC state without breaking immersion.

## Specifically
Add to the HUD:
- Appears only when an NPC is within interaction range
- Shows NPC name + a relationship bar (0-100 scale)
- Color coding: red (hostile <25), yellow (neutral 25-60), green (friendly >60)
- Disappears when player moves out of range or NPC walks away

```
[BLACKSMITH GUARD]  ████████░░  Trust: 78
```

Signal flow: GameWorld → NearestNPCChanged signal → HUD::update_npc_indicator()

## How to verify
Visual tests:
- `test_hud_npc_hostile()` → golden: hud_npc_hostile.png
- `test_hud_npc_neutral()` → golden: hud_npc_neutral.png
- `test_hud_npc_friendly()` → golden: hud_npc_friendly.png
- `test_hud_npc_hidden()` → golden confirms indicator is absent

## Constraints
Read visual-testing skill before writing tests.
NPC name must come from game state, not hardcoded.
HUD does not query NPC position directly — receives it via signal.

## Code economy note
Reuse the HUD widget pattern from the weapon HUD issue.
One QWidget for both HUD elements, not two separate overlays.

## Branch name
`feature/issue-N-hud-npc-relationship-indicator`

## Concept for learning materials
"Qt overlay HUD design and signal-driven UI updates"
""",
    },
]

# ─── Create all issues in order ───────────────────────────────────────────────
created = []
for spec in issues:
    result = create_issue(
        repo,
        title=spec["title"],
        body=spec["body"],
        labels=["ready-for-ai"],
        assignees=[username],
        milestone=milestone
    )
    issue_obj = repo.get_issue(result["number"])
    updated_body = spec["body"].replace("issue-N-", f"issue-{result['number']}-")
    issue_obj.edit(body=updated_body)
    created.append(result)
    print(f"  ✅ #{result['number']}: {spec['title']}")

print(f"\n{len(created)} issues created in: {milestone.title}")
print("\nOrder matters — each issue depends on the previous:")
for i, issue in enumerate(created, 1):
    print(f"  {i}. #{issue['number']} {issue['title']}")
print(f"\nRun: bash ~/scripts/nightly-github.sh npc")
```
