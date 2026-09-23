# Wallkan

> **Status:** Early Development / Work-in-Progress

A Wayland native Vulkan powered wallpaper daemon in C11.

## Planned features:
- Full Shadertoy compatibility
- Extended push constants
- Video via FFmpeg 6.1+ vulkan video decode
- Support for binding video directly to an iChannel of a shader pass

## Current Status:
- Simple IPC
- Wayland setup
- Vulkan device & swapchain setup

## Building
```bash
meson setup build
meson compile -C build
```
