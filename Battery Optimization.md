# Framework 13 Pro — Battery Optimization

A technical reference for configuring a Framework Laptop 13 Pro (Intel Core Ultra Series 3, Panther Lake) on Debian forky/sid with KDE Plasma (Wayland). Describes the end-state configuration only. All commands assume root where `sudo` is used.

## Hardware baseline

| Component | Value |
|---|---|
| System | Framework A7 Laptop 13 Pro (Intel Core Ultra Series 3) |
| SoC | Intel Core Ultra X7 358H |
| GPU | Intel Arc B390 (integrated, Panther Lake) |
| Panel | 2880×1920, 120 Hz max, 60 Hz min, HDR-capable, 4S battery |
| Wi-Fi | Intel CNVi (Core Ultra Series 3), `iwlmld` driver |
| Battery | ATC `FRANEDA`, 4640 mAh, `PNP0C0A` (no `power_now`; compute from V×A) |
| Kernel | 7.1.x amd64 (Debian) |

## Target metrics

| Metric | Value |
|---|---|
| Idle power (40% brightness, 60 Hz) | ~5.5 W |
| Package idle C-state | C10 (≥70% residency) |
| Power-draw derivation | `V_now × I_now`, where `V_now` is µV and `I_now` is µA → divide by 1e12 for W |

## 1. Wi-Fi power save

`/etc/modprobe.d/iwlwifi.conf`:

```
options iwlwifi power_save=1 power_level=5
```

`power_save` and `power_level` are load-time module parameters; they are read-only after the module loads. Enforce at next boot, or apply live:

```sh
sudo modprobe -r iwlwifi && sudo modprobe iwlwifi
```

Runtime power save (persists across suspend, must be asserted if a network manager resets it):

```sh
sudo iw dev wlp0s20f3 set power_save on
```

## 2. TLP (device runtime power management)

Installation (mandates removal of the conflicting `power-profiles-daemon`):

```sh
sudo apt install tlp tlp-rdw
sudo systemctl stop power-profiles-daemon
sudo systemctl mask power-profiles-daemon
sudo systemctl enable --now tlp
sudo tlp start
```

End-state expectations:

- `sudo tlp-stat -s` reports `tlp = enabled`, no `power-profiles-daemon running` warnings.
- CPU governor `powersave`, EPP `balance_power`, `intel_pstate` active.
- TLP profile `balanced/BAT`.

## 3. GPU idle — Panel Self-Refresh + FrameBuffer Compression

`/etc/default/grub.d/50-framework-battery.cfg`:

```
GRUB_CMDLINE_LINUX_DEFAULT="$GRUB_CMDLINE_LINUX_DEFAULT i915.enable_psr=1 i915.enable_fbc=1"
```

Apply:

```sh
sudo update-grub
sudo reboot
```

Verify boot flags:

```sh
cat /proc/cmdline   # contains i915.enable_psr=1 i915.enable_fbc=1
```

This restores deep package C10 residency. Without it the SoC idles shallower and burns several extra watts.

## 4. ASPM (no action required)

All PCIe links already negotiate ASPM L1 with L1.1/L1.2 substates at the NVMe root port. Do **not** add `pcie_aspm=force`. Confirm with:

```sh
sudo lspci -vv | grep -E 'ASPM|L1SubCtl1'
```

## 5. powertop auto-tune (persistent)

`/etc/systemd/system/powertop.service`:

```ini
[Unit]
Description=Powertop auto-tune
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/usr/sbin/powertop --auto-tune
RemainAfterExit=true

[Install]
WantedBy=multi-user.target
```

```sh
sudo systemctl enable --now powertop.service
```

One-time calibration (AC power, machine idle ~5 min, lid open):

```sh
sudo powertop --calibrate
```

## 6. Display refresh-rate automation (AC ↔ battery)

Refresh rate and brightness are the only two measured display levers; both cost ~1.8–1.9 W over their tested range. Refresh rate is automated; brightness is not (auto-brightness is unsupported on this panel) and remains a manual control.

### 6.1 Script

`/usr/local/bin/framework-refresh.sh`:

```bash
#!/usr/bin/env bash
set -u

OUTPUT="eDP-1"
AC_ONLINE="/sys/class/power_supply/ACAD/online"
[ -e "$AC_ONLINE" ] || AC_ONLINE="/sys/class/power_supply/AC/online"
[ -e "$AC_ONLINE" ] || exit 0

ac=$(cat "$AC_ONLINE" 2>/dev/null || echo 1)

if [ "$ac" = "1" ]; then
    kscreen-doctor "output.${OUTPUT}.mode.2880x1920@120"
else
    kscreen-doctor "output.${OUTPUT}.mode.2880x1920@60"
fi
```

```sh
sudo install -m 0755 /dev/stdin /usr/local/bin/framework-refresh.sh <<'EOF'
#!/usr/bin/env bash
set -u
OUTPUT="eDP-1"
AC_ONLINE="/sys/class/power_supply/ACAD/online"
[ -e "$AC_ONLINE" ] || AC_ONLINE="/sys/class/power_supply/AC/online"
[ -e "$AC_ONLINE" ] || exit 0
ac=$(cat "$AC_ONLINE" 2>/dev/null || echo 1)
if [ "$ac" = "1" ]; then
    kscreen-doctor "output.${OUTPUT}.mode.2880x1920@120"
else
    kscreen-doctor "output.${OUTPUT}.mode.2880x1920@60"
fi
EOF
```

### 6.2 User service (Wayland session inheritance)

`~/.config/systemd/user/framework-refresh.service`:

```ini
[Unit]
Description=Switch refresh rate based on power source
After=graphical-session.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/framework-refresh.sh
```

```sh
systemctl --user daemon-reload
```

### 6.3 udev trigger

`/etc/udev/rules.d/99-framework-refresh.rules`:

```
SUBSYSTEM=="power_supply", ATTR{type}=="Mains", ATTR{online}=="0", RUN+="/usr/bin/systemctl --user --machine=c10l@.host start framework-refresh.service"
SUBSYSTEM=="power_supply", ATTR{type}=="Mains", ATTR{online}=="1", RUN+="/usr/bin/systemctl --user --machine=c10l@.host start framework-refresh.service"
```

```sh
sudo udevadm control --reload-rules
```

The script runs in the user session (`--machine=c10l@.host`) so `kscreen-doctor` inherits `$WAYLAND_DISPLAY` and `$XDG_RUNTIME_DIR`. A bare `RUN+=` (root, no session) cannot reach the Plasma session.

## 7. Not configured (measured and rejected)

| Item | Reason |
|---|---|
| HDR / wide-gamut toggle | ~0.3 W, within noise, not worth automating |
| `pcie_aspm=force` | ASPM already optimal |
| Charge-threshold (80% cap) | Not exposed by this battery's EC; check again after BIOS updates |

## Validation

```sh
# Confirm power-management services
sudo tlp-stat -s          # tlp enabled, no power-profiles-daemon conflict
systemctl is-enabled powertop.service tlp.service

# Confirm GPU flags
cat /proc/cmdline          # i915.enable_psr=1 i915.enable_fbc=1

# Idle measurement (battery, 40% brightness, 60 Hz, 90s idle)
cat /sys/class/power_supply/BAT1/voltage_now
cat /sys/class/power_supply/BAT1/current_now   # expect V×A ≈ 5.5 W

# Refresh-rate automation fires on power-source change
kscreen-doctor -o | grep '2880'   # '*' marker moves 60.00 ↔ 120.00
```
