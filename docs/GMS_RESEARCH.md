# Combat Arms GMS research

This document records the current Fireteam reverse-engineering status for Combat Arms `.GMS` mission files. The raw Combat Arms files are local research inputs and are **not** committed to this repository.

## Current evidence

The supplied `GMS.zip` contains 305 `.GMS` files. The Fireteam/co-op files currently under study are:

- `BLACKLUNG_CP.GMS`
- `CABINFEVER_CP.GMS`
- `DESERTFOX_CP.GMS`
- `NEMEXISHQ_CP.GMS`
- `SANDHOG_CP.GMS`

The mission files are not plain Bute text. They are encrypted or obfuscated, but their structure leaks useful information:

| File | Bytes | Size mod 8 | Clear tail | Repeated 8-byte occurrences |
| --- | ---: | ---: | --- | ---: |
| `BLACKLUNG_CP.GMS` | 29053 | 5 | `= 6\r\n` | 1344 |
| `CABINFEVER_CP.GMS` | 38331 | 3 | `6\r\n` | 2537 |
| `DESERTFOX_CP.GMS` | 16176 | 0 | none | 523 |
| `NEMEXISHQ_CP.GMS` | 13921 | 1 | `5` | 363 |
| `SANDHOG_CP.GMS` | 11411 | 3 | `6\r\n` | 330 |

The repeated-block signal is strongest when the files are split at offset zero into 8-byte blocks. Archive-wide validation strengthens this substantially: **266 of the 305 GMS files are not divisible by 8, and all 266 of those files have a completely printable ASCII/control-character tail**. This is consistent with a transform that processes complete 8-byte blocks from offset zero and leaves the final 1-7 bytes untouched. Together with the repeated ciphertext blocks, this strongly supports an 8-byte ECB-style `CryBytes` transform over otherwise normal text rather than CBC encryption over the entire file.

The Combat Arms reference `CShell.dll` and `Lithtech.exe` both contain a complete Blowfish implementation (including the standard Blowfish P-array constants). This makes **Blowfish ECB the leading GMS hypothesis**. The exact GMS key or key derivation is not yet recovered.

A client-side Blowfish wrapper was traced far enough to recover its key-schedule behavior. In this build that wrapper passes a **fixed key length of 4 bytes** into the Blowfish key setup. Direct tests of `seungho`, and of its first four bytes `seun`, using both standard Blowfish block ordering and the native DWORD ordering seen in the CA implementation did **not** produce valid GMS plaintext. The reported `seungho` key may belong to another CA build/tool/server path, but it is not confirmed for these files.

The historical Attributes decryptor is now preserved more precisely from the original working code:

- Twofish engine
- CBC mode
- 256-bit key: `620C724A2FF22C975B5A2B9C21430820227B3D2800193AAA4CF3128803AC3ABD`
- 128-bit IV: `56B83E3F68B60F0F29357BED335E5642`
- BouncyCastle `TwofishEngine` + `CbcBlockCipher`
- no PKCS padding wrapper
- decrypt processes only `floor(fileLength / 16)` complete blocks
- encrypt zero-fills the last partial 16-byte input block and always emits complete 16-byte blocks

That implementation is a useful control case, but it does **not** match GMS. Twofish's block size is 16 bytes, true CBC should not leak large numbers of repeated ciphertext blocks, and the historical encryptor emits lengths divisible by 16. The GMS corpus instead shows strong repetition at 8-byte boundaries and 266 files with readable 1-7 byte remainders. Do not reuse the Attributes cipher for GMS without new evidence.

## Client vs. server loader

The client reference binary contains `Attributes\\GMS.txt` and a `GMSButeMgr` initialization path. The nearby byte-by-byte obfuscation was traced and resolves literally to the string **`GMSButeMgr`**; it is a manager/registration name, not a recovered encryption key. That manager handles the global GMS attribute/config table. It does not expose an obvious loader for map mission files such as `CABINFEVER_CP.GMS`.

Current working conclusion: the `_CP.GMS` mission-file decrypt/load path is probably in the Combat Arms **DServer/server-side code or binary**, not in the two client reference binaries currently attached to this project.

If a matching DServer binary or server source becomes available, search first for:

- `.GMS` / `GMS\\`
- `CryBytes`
- Blowfish key setup / 8-byte block loops
- `CooperativeMission`
- Fireteam mission/spawn/wave managers

## Cross-checks from decrypted attributes

`GAMEROOM-DEC.TXT` explicitly maps Cabin Fever to:

```text
GMS_COOP = "GMS\\CabinFever_CP.GMS"
GMS_SCOP = "GMS\\CabinFever_SCP.GMS"
```

The decrypted data also confirms Fireteam difficulty and no-friendly-fire behavior. Those sources should remain the cross-check for any values recovered from GMS.

Do **not** invent round counts, spawn counts, enemy weights, boss timing, or difficulty multipliers while the original GMS rules are still recoverable.

## Inspection tool

Run the repository helper against the local archive:

```bat
powershell -ExecutionPolicy Bypass -File scripts\inspect-gms.ps1
```

It defaults to `assets-local\GMS.zip` and `*_CP.GMS`.

Useful variants:

```bat
powershell -ExecutionPolicy Bypass -File scripts\inspect-gms.ps1 -Pattern *.GMS
powershell -ExecutionPolicy Bypass -File scripts\inspect-gms.ps1 -Path assets-local\GMS.zip -Json
```

The tool intentionally performs structural inspection only. It reports both 8-byte and 16-byte repetition so the GMS format can be compared directly against the historical Twofish/CBC Attributes implementation. Once the exact GMS Blowfish key/key derivation is recovered, decryption support can be added without changing the research workflow.
