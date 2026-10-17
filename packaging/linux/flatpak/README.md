# Flatpak

Manifest for building aMule as a Flatpak. Targets `org.gnome.Platform`
which provides GTK3 / glib / pixman / fontconfig / libpng / zlib / freetype.

`flatpak-external-data-checker` runs daily against the manifest (set up
in the Flathub repo's CI) and opens PRs when upstream releases for
`cryptopp`, `libupnp`, `libgd`, `wxWidgets`, or aMule itself are detected.

## Files

- `org.amule.aMule.yaml.in` — template; structural changes go here.
- `org.amule.aMule.yaml` — generated from `.in` + `versions.env`
  via `envsubst`. Don't hand-edit; it gets regenerated.

## App ID

`org.amule.aMule` is the canonical id — matches the AppStream component
id, the .desktop filename, the Wayland app_id (set in CamuleApp::OnInit
via g_set_prgname), and the macOS bundle id. Override via `APP_ID=` in
`packaging/linux/versions.env` if you ever need to fork; the manifest
template filename has to match `${APP_ID}.yaml.in`.

## Local build + smoke test

```sh
# Install runtime + SDK + binfmt for both arches (one-time)
packaging/linux/build.sh setup-cross-arch

# Build native (host arch)
packaging/linux/build.sh flatpak

# Cross-arch builds (uses QEMU emulation, slow)
packaging/linux/build.sh flatpak x86_64
packaging/linux/build.sh flatpak aarch64

# Result: ./dist/aMule-<version>-Linux-<arch>.flatpak (single bundle)

# Optional: install + run via the local repo
flatpak install --user --reinstall ./dist/aMule-<version>-Linux-<arch>.flatpak
flatpak run org.amule.aMule
```

To regenerate the rendered manifest without building:

```sh
packaging/linux/build.sh render-flatpak-manifest
```

To produce all 4 artifacts (appimage+flatpak × x86_64+aarch64) in one
go: `packaging/linux/build.sh all`.

## Automated GUI smoke tests

GUI smoke tests run automatically on every master push that triggers the
**Packaging** workflow. Releases do not run them. For other branches, manually
dispatch **Packaging**, set `only` to `flatpak`, and enable `flatpak_smoke`.
Manual smoke builds produce one x86_64 bundle; automatic packaging keeps both
architectures. The tests install the same x86_64 artifact in three isolated
sessions on Ubuntu 26.04:

- X11 with Xvfb/Openbox, with no Wayland display, exercising `fallback-x11`.
- GNOME Shell/Mutter headless Wayland, with Xwayland disabled.
- KWin Wayland nested in Xvfb, with the app forced to native Wayland.

To rerun the desktop jobs without rebuilding, also set `flatpak_smoke_run_id`
to an earlier Packaging run ID in the same repository. That run must have an
unexpired `flatpak-x86_64` artifact. Its source revision and URL are recorded in
each desktop job's `build.json`; reusing a bundle does not rebuild application
code from the selected workflow branch.

The driver checks the installed permissions and absence of `/dev/dri` inside
the sandbox without overriding device permissions. It uses a fresh aMule
configuration with first-run setup completed and networking disabled. It opens
the main window, sends navigation and Page Down keys, opens Preferences,
resizes the display/window, and requests a clean exit. GNOME uses a virtual
keyboard; X11 and KWin's nested display use XTest input.

Download `flatpak-smoke-*` artifacts to review screenshots, application and
compositor logs, package versions, and the permissions actually tested. These
artifacts are kept for 7 days. Any `drawing failure` line in `amule.log` fails
the job; unrelated `Gtk-WARNING` lines do not fail it. A pass
requires a nonblank display and opening Preferences through keyboard input;
screenshots still need human review for missing or incorrectly drawn widgets.
These jobs use software rendering and do not establish scrolling smoothness or
GPU-driver compatibility. They test the desktop versions shipped by Ubuntu
26.04, not a pinned GNOME 49 or a complete Plasma desktop session.

For local reproduction, install the packages listed in
`.github/workflows/flatpak-smoke.yml`, install the bundle for the current user,
and run on a disposable desktop account (the output directory must be new):

```sh
dbus-run-session -- python3 packaging/linux/flatpak/smoke.py \
    gnome-wayland "$HOME/amule-smoke-results"
```

Use `x11` or `kde-wayland` for the other sessions. GNOME's test instance enables
its compositor scripting interface on the isolated session bus to drive input
and capture screenshots.

## Pending before first build

The `libupnp` SHA256 in `packaging/linux/versions.env` is a placeholder.
On the first local build:

```sh
sha256sum <path-to-libupnp-release-tarball>
```

…and paste it into `versions.env`. After that, `flatpak-external-data-checker`
keeps all hashes in sync.

## Flathub submission

Once the manifest builds locally:

1. Open a PR against `flathub/flathub` adding a `new-pr/<app-id>` directory
   referencing the rendered manifest.
2. Address reviewer feedback (license, runtime version, finish-args).

After acceptance Flathub creates a `flathub/<app-id>` repo and rebuilds
on every manifest commit, and on every aMule release tag.
