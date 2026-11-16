---
name: handoff
description: Send folders and files to another laptop on the local network with Handoff (rsync over SSH), or check whether this laptop is ready to receive. Use when asked to copy, sync, move, or migrate files to another machine over Wi-Fi or SSH.
---

# Handoff

Handoff sends files to another laptop with rsync over SSH. Every command
exits 0 on success and 1 on failure, with the reason on stderr, and takes
`--json` for one line of JSON on stdout.

## Send

```sh
handoff send --to user@192.168.1.20 --dry-run --json ~/Documents ~/Pictures
handoff send --to user@192.168.1.20 --json ~/Documents ~/Pictures
```

- Without `--into`, each path lands at the same place under the other home
  folder (`~/Documents/tax` to `~/Documents/tax`), so paths must be inside
  the home folder.
- `--into FOLDER` puts everything in that folder instead, relative to the
  other home folder.
- Files that are newer on the other laptop are kept unless
  `--overwrite-newer` is given. Nothing is ever deleted on either side.
- Run with `--dry-run` first and tell the user how many files and bytes would
  move before sending for real.
- It logs in with the user's SSH key. For a password, set it in the
  `HANDOFF_PASSWORD` environment variable; never put it on the command line.
- An interrupted send carries on where it stopped when run again.

## Receive

```sh
handoff status --json
```

Reports `ready`, and the `target` (user@address) the sender should use.
Starting to receive needs the user's password, so ask them to open Handoff,
press R, then Space; it cannot be done from the command line.
