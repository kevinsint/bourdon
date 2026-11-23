# How to release a version of BOURDON

A release is a tag `vX.YY` on `main`, a GitHub release with the package, and the page
[kevinsint.live/apps/bourdon](https://kevinsint.live/apps/bourdon/) brought up to date. Do the steps in this
order. All commands run from the repository's root.

## Before

- The version was installed and played on an FM-1.
- The work is on `main`. Never push the remote `archive` or its branches to `origin`.

## Steps

1. **The version.** Set `APP_VERSION` in `app/app_id.h` (`"V0.28"`), and the same version in the "State" section of
   `README.md`. `README.md`, `pannel-description.md` and `app/style.h` say what this version does: the page's text is
   made from their changes since the last tag.
2. **The tests.**
   ```sh
   tests/run_host_tests.sh
   ```
3. **The build and its test.** `tests/build_test.sh` builds `build/sint_fm1_bourdon.fwsc` and checks it.
   ```sh
   tests/build_test.sh
   shasum -a 256 build/sint_fm1_bourdon.fwsc
   ```
   Keep the SHA-256: it goes in the release notes.
4. **Commit and push**, over SSH.
   ```sh
   git push origin main
   ```
5. **The tag.**
   ```sh
   git tag v0.28
   git push origin v0.28
   ```
6. **The GitHub release**, with the package and the Apache licence text (the package holds three files of JieLi's
   AC79 SDK: `bsp/LICENSING.md`).
   ```sh
   gh release create v0.28 build/sint_fm1_bourdon.fwsc bsp/LICENSES/Apache-2.0.txt \
       --repo kevinsint/bourdon --title "BOURDON V0.28" --notes-file <notes>
   ```
   The notes follow the last release's: one line on what BOURDON is and where the install steps are, the package's name,
   its SHA-256, "In this version", "Known limits", and the line on the Apache License 2.0.
7. **The screens for the page**, drawn by the tag's own code:
   ```sh
   scripts/web_screens.sh v0.28
   ```
   They go to `build/web_screens/v0.28/` as PNG, 480 x 480. The page uses these names; do not change one without a
   word to the Web session: `8_play`, `9_play_links`, `9_play_shift`, `10_mixer`, `11_mixer_channel`, `14_eoc_delay`,
   `5_eoc_reverb`, `28_eoc_comp`, `6_info`.
8. **Tell the Web session**: the tag, one line on what is new for the one who plays, and the names of the new
   screens the page must show. The Web session then runs the site's `scripts/bourdon-update.sh <tag>` (repository
   `kevinsint/site`), which reads this repository on ava over SSH.

## After

- The release has two files, and the SHA-256 in its notes is the package's:
  `gh release view v0.28 --repo kevinsint/bourdon`.
- The page shows the new version.
