#!/usr/bin/env python3
"""Generate the private Pokémon move table header inside PlatformIO's build tree."""

from pathlib import Path
import runpy


Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
output_dir = Path(env.subst("$BUILD_DIR")) / "generated" / "pokemon"
output_path = output_dir / "PokemonMoves.generated.h"
generator_path = project_dir / "scripts" / "generate_pokemon_moves.py"
source_path = project_dir / "scripts" / "data" / "pokemon-moves.csv"

generator = runpy.run_path(str(generator_path))
rendered = generator["generate"](generator["load_moves"](source_path))

output_dir.mkdir(parents=True, exist_ok=True)
if not output_path.exists() or output_path.read_text(encoding="utf-8") != rendered:
    output_path.write_text(rendered, encoding="utf-8", newline="\n")

env.Append(CPPPATH=[str(output_dir)])
