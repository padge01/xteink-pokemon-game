# Pokémon Game for Xteink X3 Repository Design

> The artwork policy below is superseded by
> [Full X3 install and artwork release design](2026-08-31-full-install-artwork-release-design.md).

## Project

- Repository: `padge01/xteink-pokemon-game`
- Display name: **Pokémon Game for Xteink X3**
- Description: **An unofficial reading-powered Pokémon companion game for the Xteink X3, built on CrossInk.**
- Visibility: public
- License: MIT for project code
- Status: beta
- First planned release: `v0.1.0-beta.1`

## Boundaries

- This is a complete firmware repository, not a CrossInk plugin.
- This project is independently maintained.
- It is not a CrossInk or CrossPoint pull request.
- It does not change `padge01/CrossInk`.
- X3 is the only supported device at launch.
- X4 and X4 Pro remain unsupported until separately built and physically tested.
- No GitHub release is published until the staged X3 firmware passes physical acceptance.

## Git History and Branches

- Preserve the existing CrossInk and CrossPoint commit history.
- Publish the current Pokémon work as `main` after its release changes are reviewed and committed.
- Use short-lived `feat/*` and `fix/*` branches for later work.
- Keep CrossInk configured as an upstream remote for future updates.
- Do not create a permanent `develop` branch at launch.

## Public Files

- Replace the current CrossInk README with a direct project README.
- Add `NOTICE.md` for upstream and asset attribution.
- Keep the inherited MIT `LICENSE` and required copyright notices.
- Keep the user beta guide and technical file-format documentation.
- Add GitHub issue templates for bugs and device-test reports.
- Add a release checklist for X3 firmware and SD-card assets.
- Keep generated firmware, private archives, converted Pokémon images and local save data out of Git.

## README Style

- Use short statements and bullet points.
- Lead with what the firmware does and which device is supported.
- Avoid marketing language.
- Avoid vague claims such as "safe," "complete," or "fully tested."
- State exactly which checks and hardware tests were run.
- Keep installation steps numbered and explicit.
- Keep recovery and save-backup instructions beside installation.

## README Content

- Project summary and beta status
- X3 support statement
- Feature list
- Explanation of credited reading and progression
- Installation and SD-card layout
- Save backup and rollback instructions
- Artwork setup
- Game rules
- Build and test commands
- Known limitations
- Credits
- Trademark and no-affiliation notice

## Artwork Policy

- Do not commit or publish Pokémon artwork, rendered Pokédex cards, or generated artwork packs.
- Public source and GitHub releases contain code and conversion tools only.
- Users obtain source artwork themselves and run the conversion tools locally.
- Attribution records provenance; it does not grant redistribution permission.
- Link to u/xDaftTurtle's original Reddit post and X3 archive.
- Credit u/xDaftTurtle / dmellok and Tesserae for the Pokédex screen design and rendered cards.
- Do not rehost u/xDaftTurtle's rendered cards without their explicit permission.
- Include local conversion and packaging scripts.
- Credit PokeAPI Sprites for Pokémon icons.
- Credit PokéSprite for evolution-stone images.
- State that Pokémon artwork and related names belong to Nintendo, Creatures Inc., GAME FREAK Inc. and The Pokémon Company.

## Upstream Credits

- CrossInk by uxjulia: firmware base
- CrossPoint Reader: original upstream firmware
- Joshua Miller's `crosspoint-reader-companion`: companion concept and real-reading session-credit behaviour
- Preserve all applicable upstream MIT notices

## GitHub Setup

- Create `padge01/xteink-pokemon-game` as a new public repository without generated starter files.
- Push only after the repository-facing documentation and ignore rules have been reviewed.
- Set the approved description.
- Enable Issues.
- Add repository topics for Xteink, X3, e-ink, e-reader, CrossInk and ESP32-C3.
- Do not create a release during initial repository creation.

## Launch Sequence

1. Draft and review the README, notice, issue templates and release checklist locally.
2. Audit tracked files for generated artwork, private archives, save data and credentials.
3. Commit the current firmware repairs and repository documentation in reviewable commits.
4. Create the public GitHub repository.
5. Push the reviewed branch as `main`.
6. Flash and physically accept the staged X3 firmware.
7. Publish `v0.1.0-beta.1` only after physical acceptance and a final release audit.
