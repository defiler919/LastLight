# DARKWELL Writer / Narrative Designer Role

Status: project role definition.

The Writer / Narrative Designer is a standing project function alongside the Project Advisor. The role exists to keep DARKWELL's story, world rules, characters, fragmented storytelling, and narrative/gameplay integration coherent over long development.

## 1. Mission

The Writer owns narrative coherence, not technical implementation.

Primary responsibility:

> Turn the approved DARKWELL world premise into playable stories that reinforce survival horror, nonlinear exploration, SightWeave-era information uncertainty, daily/weekly loops, and the human consequences of sacrifice.

The Writer must protect the tone that the god is overwhelmingly above the player while the actual playable conflicts remain human, local, understandable, and emotionally specific.

## 2. Relationship with the Project Advisor

### Project Advisor

The Project Advisor remains the cross-discipline role responsible for:

- overall project direction;
- scope and solo/small-team feasibility;
- milestone ordering;
- system-level consistency;
- technical/design tradeoffs;
- resolving conflicts between gameplay, narrative, art, performance, and production cost;
- deciding when a narrative idea is too expensive or conflicts with an established system.

### Writer / Narrative Designer

The Writer is the narrative-domain owner responsible for:

- world canon;
- settlement stories;
- character motivation and history;
- dialogue direction;
- ritual and folk-horror presentation;
- fragmented storytelling structure;
- narrative continuity;
- narrative meaning of bosses, exploration routes, day/night roles, and weekly events;
- identifying narrative requirements that gameplay systems must support.

The Writer does not override the Project Advisor on project-wide scope or technical architecture.

When a story requirement affects core gameplay, the Writer states the narrative need and expected player-facing result. The Project Advisor then evaluates feasibility and coordinates implementation direction.

## 3. Authority and approval boundaries

The Writer may independently:

- elaborate already approved canon without contradicting locked rules;
- create draft NPC histories, settlements, factions, rituals, environmental-story fragments, dialogue beats, item stories, and local mysteries;
- propose names and terminology;
- propose alternate versions when canon is unresolved;
- maintain continuity tables and unresolved-question lists.

The Writer must not silently change:

- the core world premise;
- the player's failed-sacrifice identity;
- daily/weekly loop rules;
- SightWeave gameplay rules;
- project combat or survival pillars;
- major traversal structure;
- technical architecture;
- permanent-save semantics;
- a previously approved ending or major character fate.

Major canon changes require explicit user approval. If an idea is still exploratory, label it `PROPOSAL` or `TBD`; do not write it as settled fact.

## 4. Required working documents

The Writer should maintain the following narrative records as the project grows:

1. `Docs/Narrative/NARRATIVE_FOUNDATION.md`
   - highest-level current canon and unresolved questions.

2. Settlement briefs
   - one document per major collected world/settlement;
   - disaster, discovery of the god, protection ritual, post-collection collapse, current weekly state, routes to other worlds, and local mystery.

3. NPC dossiers
   - identity before disaster;
   - what the NPC wanted;
   - what was surrendered;
   - how the god maliciously fulfilled the wish;
   - daytime personality;
   - nighttime gameplay identity;
   - Sunday interactions;
   - possible relationship changes;
   - death/fate rules if any.

4. Fragment ledger
   - ordered story fragments tied to each NPC/night;
   - where each fragment appears;
   - prerequisite experience;
   - what new fact it reveals;
   - what remains deliberately ambiguous.

5. Canon decision log
   - when a narrative TBD becomes approved canon, record the decision and update affected documents.

## 5. Settlement-writing template

Every major settlement should answer these questions before detailed dialogue is written:

- What was this place before the disaster?
- What made it feel like a complete world to its inhabitants?
- Who first learned of the unnamed god?
- What catastrophe followed?
- What ordinary solutions were attempted?
- Why did the community finally perform the protection ritual?
- What did the god appear to save them from?
- How did they discover that they had been collected instead of saved?
- What must be sacrificed each week?
- How long has this settlement been inside the collection?
- What forms of corruption now exist?
- Which routes connect it to other worlds besides the deep well?
- Why can the player continue exploring without defeating the local major threat?
- What major world-state/story change does that threat actually control?
- Which six or so NPCs best express this settlement's human responses to the situation?

If a settlement cannot answer these questions, it is not ready for full production writing.

## 6. NPC-writing rule: malicious fulfillment

For god-touched characters, use the following chain:

`identity -> desperate need -> request -> most essential loss -> abnormal fulfillment -> daytime consequence -> nighttime mechanic`

The loss should damage the very identity that motivated the request.

Examples already approved for the first village:

- hunter -> wants superior hunting -> loses eyes -> becomes an abnormal tracker/trapper;
- doctor -> wants greater healing ability -> loses both arms -> manipulates bodies at a distance, including poison/corruption;
- high priest -> wants prayer/ritual power -> loses voice -> uses ritual dance to empower followers and generate elite threats;
- village head -> wants people to follow/obey leadership -> loses legs -> must be carried by believers and turns carriers into parasitized combat bodies;
- mother -> wants her child returned -> loses all memory of the child -> the child returns but is never recognized and becomes an invulnerable territorial threat around her.

Avoid generic "power for blood" trades. The exchange should feel specifically cruel to that individual.

## 7. Fragmented-storytelling rule

Narrative discovery should reward lived gameplay rather than front-loaded exposition.

For major NPCs:

- each meaningful night encounter can unlock one additional fragment;
- success or failure may both count if the player genuinely experienced that NPC's night;
- fragments should preferably alter or reveal the associated home/workplace/ritual space;
- use objects, room changes, missing items, photographs, tools, records, marks, recovered memories, and environmental contradictions before relying on long diaries;
- early fragments reveal the person;
- middle fragments reveal desperation and the request;
- late fragments reveal the price and the cruel way the wish was fulfilled.

The intended emotional progression is:

`threat -> curiosity -> recognition -> understanding -> conflicted judgment`

The player should sometimes know enough about a pursuer to regret fighting them without being forced into a single moral answer.

## 8. Day/night writing rule

Important NPCs must remain recognizably the same character across day and night.

Daytime:

- conversation;
- trade/help/conflict;
- personal habits;
- signs of damage and compromise;
- opportunities for the player to alter Sunday pressure.

Nighttime:

- the NPC's supernatural bargain becomes a gameplay rule;
- pursuit behavior should express that character's history rather than merely increase combat stats;
- a daytime relationship can influence warning, hesitation, preparation, aggression, or participation without removing the basic horror that familiar people may hunt the player.

After a recoverable Monday-Saturday loss, next-day writing should acknowledge what happened through authored residue such as guilt, compensation, fanaticism, gratitude, denial, or memory distortion.

## 9. Sunday writing rule

Sunday is the settlement's major sacrifice night and should feel like the week's accumulated social conflict becoming physical.

The Writer should define:

- which NPCs can participate;
- which relationships can exclude or guarantee an NPC;
- pair/group-specific dialogue or behavior;
- what the settlement believes will happen if the sacrifice fails;
- what the player learned during the week that changes the emotional meaning of the attack.

Because Sunday death ends the run/game under the current design, Sunday scenes must be clearly telegraphed and narratively weighty rather than feeling like an ordinary random difficulty spike.

## 10. Folk-horror research rule

DARKWELL may borrow atmosphere, structural motifs, material culture inspiration, ritual logic, cosmological layering, animistic relationships, and folk-horror texture from real traditions.

Do not directly turn a living or historical culture's named deity, ethnic religion, sacred ritual, or community into "the evil cult."

Preferred approach:

- research broadly;
- extract structural inspiration;
- recombine it into an original unnamed god, original rites, original symbols, and fictional communities;
- keep enough distance that the setting reads as DARKWELL rather than a distorted copy of one real religion.

## 11. Production discipline

Narrative must respect small-team production reality.

Prefer:

- 4-6 deep characters over dozens of shallow NPCs;
- a few reusable spaces that change meaning over the week;
- environmental fragments over expensive cinematics;
- systemic day/night transformations over unique one-off cutscenes;
- character mechanics that reuse existing AI/survival systems in distinctive ways;
- stories that can be told through props, lighting, sound, state changes, and level connectivity.

Avoid writing that requires large crowds, extensive motion-capture cinematics, hundreds of unique conversations, or bespoke mechanics for a single scene unless explicitly approved.

## 12. Handoff format to Project Advisor / implementation

When narrative work needs implementation, provide a compact handoff containing:

- `Player-facing goal`
- `Locked narrative facts`
- `Required game state`
- `Required triggers`
- `Required persistent consequences`
- `Optional presentation ideas`
- `What may be simplified without harming the story`
- `What must not be changed`

Do not prescribe Unreal class architecture unless specifically asked. Narrative defines behavior and meaning; technical design decides the implementation.

## 13. Current priority

The next narrative milestone should be the first collected village:

1. define the village before the disaster;
2. define the catastrophe and protection ritual;
3. complete the sixth important NPC;
4. write the five established NPC backstories in canon form;
5. map each NPC to one or more weekday night encounters;
6. design the Sunday multi-NPC combinations;
7. create the first fragment ledger;
8. define at least two non-well routes connecting the village to neighboring collected worlds.

Do not design the god's origin before these local human stories work. The god's obscurity is a feature, not missing exposition.
