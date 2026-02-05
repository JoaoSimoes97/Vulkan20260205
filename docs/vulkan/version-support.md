# Vulkan version support

## Version overview

| Version | Released   | Use case |
|---------|------------|----------|
| **1.0** | 2016       | Legacy; avoid for new code. |
| **1.1** | 2018       | Wide compatibility. |
| **1.2** | 2020       | **Recommended baseline** — desktop, MoltenVK, most Android. |
| **1.3** | 2022       | Newer GPUs and mid/high-end Android from ~2022. |
| **1.4** | Dec 2024   | Latest; drivers and MoltenVK adding support. |

## Desktop GPUs (Vulkan 1.3)

**Day-one Vulkan 1.3 drivers** (Windows & Linux):

- **NVIDIA** — GeForce RTX 30 series (Ampere) and newer; driver 510+.
- **AMD** — RDNA 2 (RX 6000) and RDNA 3 (RX 7000); Mesa RADV on Linux.
- **Intel** — Arc (Alchemist) desktop and mobile; Iris Xe; UHD 7xx+; Mesa ANV on Linux.

Older GPUs often get 1.3 in driver updates; check your driver release notes.

## Desktop GPUs (Vulkan 1.4)

Vulkan 1.4 was released **December 2024**. Support is rolling out in vendor and Mesa drivers; check release notes for your GPU and OS. Core additions include streaming transfers, push descriptors, dynamic rendering local reads, scalar block layouts, and 8K rendering with up to eight render targets.

## Android (Vulkan 1.3)

- **Qualcomm Adreno** — Vulkan 1.3 on newer Snapdragon SoCs (e.g. 8 Gen 2+, 8 Gen 3).
- **ARM Mali** — Vulkan 1.3 on recent Mali-G (e.g. G710, G715) and newer.
- **Samsung / Exynos** — Depends on SoC and driver; newer Exynos with Mali support 1.3 where Mali does.

**Rough rule:** Vulkan 1.3 on Android is common on **mid/high-end devices from ~2022 onward**. Older or low-end devices often stay on 1.1 or 1.2.

**Vulkan 1.4 on Android** — Too new for broad device lists; check [Vulkan Hardware Database](https://vulkan.gpuinfo.org/) and device/driver docs.

**Check at runtime:** Use `vkGetPhysicalDeviceProperties` → `apiVersion` or enumerate extensions; don’t assume 1.3 or 1.4 on Android.

## MoltenVK (macOS / iOS)

- **1.2** — Supported.
- **1.3** — Supported (MoltenVK 1.3+).
- **1.4** — In progress; extensions and core 1.4 features are being added. Use latest MoltenVK and check release notes.

## Reference

- [Vulkan Hardware Database (vulkan.gpuinfo.org)](https://vulkan.gpuinfo.org/) — filter by platform (Android, Linux, Windows) and API version.
- [Khronos Vulkan roadmap](https://docs.vulkan.org/spec/latest/appendices/roadmap.html) — version milestones.

## This project

We request **Vulkan 1.4** at instance creation (latest). The loader/driver may report a lower version if 1.4 is not supported; use the reported `apiVersion` (e.g. from `vkGetPhysicalDeviceProperties`) before using 1.4-only features and fall back or require a minimum version as needed.
