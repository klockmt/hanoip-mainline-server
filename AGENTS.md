# hanoip-mainline-server

Mainline Linux (kernel.org 7.3) and Duranium (postmarketOS with systemd) on the Moto G60 (hanoip, SM7150), run as a **headless server reached over SSH**. The repo is public.

The rules are here. Before touching anything read `docs/STATUS.md` (what works), then `NOTES.md` (the log of the investigations); `docs/PITFALLS.md` and `docs/HOMELAB.md` say what broke and how the phone is run. The Duranium work follows `docs/DURANIUM.md`.

## Rules that always apply
- The repo check is `scripts/check-repo`: run it before every commit and push.
- Local paths and personal data live in `.env` (copy of `.env.example`). Scripts source `scripts/env.sh`; docs use `$HANOIP_*` variables. To resolve one, read `.env`.
- People who asked not to be named never appear in a tracked file or a commit message; their names are in `HANOIP_PRIVATE_NAMES` (`.env`) and `scripts/check-repo` refuses them.
- Never write an IMEI, serial, MAC, battery id, home path or the user name in any tracked file, test fixture, log or commit message. Device dumps and partition backups stay in `$HANOIP_BACKUP_DIR` / `$HANOIP_COLLECTION_DIR`, outside the repo.
- Keep `README.md` honest about which scripts were run end to end.
- Do not flash partitions beyond what the user asked for, never wipe or factory reset. Inspect the target and compare with `SHA256SUMS` first. `fastboot set_active b` + `reboot` is pre-authorized only to recover the A/B slot (see below).
- `pmbootstrap` asks for the `sudo` password and needs interactive input, so **the user runs it**: hand over ready-made commands and wait. It is not on PATH; use `$PMBOOTSTRAP`.
- When zeroing the in-use slot's `dtbo` for a test, restore the backup right afterwards, before `fastboot continue` or any reboot.
- The mainline development root lives only in the unallocated region of `super` (`sdb23`): offset 1120 MiB, 2400 MiB (approved by the user on 2026-11-07). Write it only through a loop device bounded by that offset and size, hash `super` outside the region before and after every write, and never write the LP metadata, the Android volumes or any other partition from a mainline boot. One exception (approved by the user on 2026-11-08, so that slot a is a second working slot): after a complete boot from slot a, `qbootctl -m a` (`98-hanoip-ml-slot.start`) marks every partition of slot a and writes back the GPT of each disk that holds one, LUN 4 and LUN 2 (`xbl_a`), as the 4.14 does for slot b at every boot; the guard lends it those whole disks only. `qbootctl` with no arguments is not read-only either.

## Working over SSH, securely
- Use `scripts/hanoip-ssh '<cmd>'`. It uses the dedicated key from `.env` with `IdentitiesOnly` and `BatchMode`; `HANOIP_SSH_TTY=1` only when a password prompt is needed.
- `sudo` asks for a password, which the user allowed the agent to keep in `.env` (`HANOIP_SUDO_PASSWORD`, `HANOIP_ROOT_PASSWORD`). Run root commands with `scripts/hanoip-sudo '<cmd>'` (password over the ssh stdin, command as base64); never print or log the value. Do not make `sudo` passwordless again, and keep ssh on keys only (`hanoip-sshd.conf`).
- Never print, log or commit a key, the Telegram token or owner id. The bot secrets live only on the phone, in `/etc/hanoip/telegram.env`; `telegram.env` and `*.token` are gitignored.
- SSH is accepted only over USB (`usb*`) and Tailscale (`02_hanoip-ssh.nft`); do not undo that. Do not weaken the nftables firewall, open a port, enable password login or add a public-facing service without asking. Remember `51_usb_inet.nft` already allows everything coming in over USB.
- The Telegram bot runs fixed commands only. Text from a message must never be used to build a command.
- Before a risky change over SSH (network, firewall, boot), check you still have a way back: `flash-boot-ssh` saves the old `boot_b` first; otherwise the recovery is fastboot.
- Large transfers over the USB (NCM) network take it down until a reboot. Throttle them (`pv -L 1m`, as `scripts/push-firmware` does).

## Server setup
- Since v1.0.0 (2026-11-14) the server is **Duranium** (Nura's systemd image, an immutable `/usr` with A/B updates and automatic rollback) on Linux 7.3 from kernel.org, in `userdata`, started by our U-Boot from `boot_a` or `boot_b` (`docs/DURANIUM.md`, `docs/HOMELAB.md`). The downstream 4.14 pmOS server (OpenRC) is frozen as v0.2.2 and its scripts are kept for reference; the Alpine root in `super` is the last resort.
- The screen is off by design (`hanoip-screen-off.service`). Do not use `fb0/blank` (`docs/HOMELAB.md`).
- Services (systemd, from the device package `device-motorola-hanoip-mainline`): `hanoip-charge` (75-80 % limit), `hanoip-thermal`, `hanoip-led`, `hanoip-bot`, `hanoip-dashboard` (tailnet only), `hanoip-health` (marks the slot good), `hanoip-modem` (Wi-Fi), `hanoip-usb-gadget`, `hanoip-usb-role`, `hanoip-ethernet-wifi`, `tailscaled`, `adguardhome`, `smb` and `syncthing` (these two wait for the NAS disk). The full list and how to check each is `docs/HOMELAB.md`. Open work lives in the GitHub issues (roadmap #20).
- An update is built with `scripts/pmb build --force device-motorola-hanoip` (the user runs it) and `scripts/duranium-image --image-version=YYMMDDNN`, then delivered with `systemd-sysupdate` (`docs/DURANIUM.md`, "Where it stands"). Writing `userdata` or `boot_a`/`boot_b` needs the user's word every time.
- **The mainline kernel is Linux from kernel.org**, always: the release tarball (checked against `sha256sums.asc`) or the git tag of that release, with only this project's patches, configs and board diffs on top, in `kernel/mainline` (`patches/`, `config-7.3-hanoip`, `uboot/base/`). Forks (`sm7150-mainline/linux`, fuyuki's tree, the pmOS packages) are sources for patches and references, never the base, and no kernel is built from one. Nothing is downloaded but the upstream sources. Never name anyone as the author of that base: it is this project's work.
- The 4.14 notes that still apply to its package: a package update flashes `boot` and `vendor_boot` (`deviceinfo_flash_kernel_on_update="true"`) and `scripts/test-deviceinfo` keeps `console=ttyMSM0,115200n8` first in the command line (`NOTES.md`). Never flash `boot`/`vendor_boot` by hand without a backup on the PC (`$HANOIP_BACKUP_DIR`).
- Pending on the old kernel and harmless: `systemd-tmpfiles`/`sysusers` could not read their `.d` directories on 4.14 (`openat2`). Not an issue on 7.3.

## Bugs found, and what to remember
What broke on this phone, why, and the test or file that keeps it from breaking again is in [`docs/PITFALLS.md`](docs/PITFALLS.md). Read it before changing the kernel config, the device package, the charge limit, the firewall or a flashing script.

## Tests for every bug
Prefer extending the matching `scripts/test-*` (they run without the phone); create a new `scripts/test-<thing>` in the same style when none fits.

## Commits and releases
Before pushing, check the contributors with `gh api repos/klockmt/hanoip-mainline-server/contributors`: only `klockmt` may show. The beta branch is `beta`.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.
