# Pokemon Companion V2 Clean Baseline Evidence

This records the non-Pokemon CrossInk/CrossPoint 1.6-parity reference used for all V2 size and hardware comparisons. It is a clean reference build, not an instrumented diagnostic build.

## Source

- Base commit: `1ef5a97ac5153cc6c7a5a9d7ffd6c3de637d3a4f`
- Build commit: `6b7a41e7` (`feat/pokemon-companion-v2`; only plans and host-test harness repairs differ from the base)
- FreeInk SDK: `1ff020263cd2202ea79ce3eb811f5ac8489b8cde`
- Lucide submodule: `c81680e066f45b640743ca78ae36cdedda3f0318`
- PlatformIO Core: `6.1.19`
- Environment: `default` (ESP32-C3 X3/X4, no Pokemon compile flag)

## Verification

- Host suite: 188/188 tests passed, zero failures.
- Command: `pio run -e default`
- Result: success.
- Static RAM reported by PlatformIO: 57,780 of 327,680 bytes (17.6%).
- Linked flash sections reported by PlatformIO: 6,254,295 of 6,553,600 bytes (95.4%).
- Final firmware image: 6,268,448 bytes.
- OTA application-partition headroom: 285,152 bytes.
- Hard V2 minimum headroom: 131,072 bytes.
- Unreviewed V2 growth limit: 102,400 bytes, leaving 182,752 bytes if fully used.

## Preserved local artifacts

The binary and ELF are copied outside disposable `.pio` output to the sibling directory `.worktrees/pokemon-companion-v2-baseline-artifacts/`:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware-x3-x4-clean-6b7a41e7.bin` | 6,268,448 | `20bece0c9f328066fefabc412877fa6997f14d15d3259080afc220278276336f` |
| `firmware-clean-6b7a41e7.elf` | 84,725,652 | `08adfe4b9ab6d37b0e41d240ce029c1876e05b882d4d35786a0803bcae8b6fe8` |

`SHA256SUMS.txt` beside those files is the machine-readable checksum record. Rebuilds after later commits are not expected to be byte-identical because the embedded version string changes; compare size and behavior against these preserved artifacts.
