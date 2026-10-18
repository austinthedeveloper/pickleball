# Releasing

Releases are GitHub Releases: a `vX.Y.Z` tag on `main` with the ROM attached.
ROMs stay out of git (`build/` and `dist/` are ignored); each release keeps
its own copy as a checkpoint.

## One-time setup

```sh
brew install gh
gh auth login        # GitHub.com, HTTPS, log in with a browser
```

## Each release

1. Merge the work into `main` and push it.
2. Bump `VERSION_NAME` in the `Makefile` (the tag is `v` + that), commit, and
   push.
3. Optionally write `docs/releases/vX.Y.Z.md` as the intro to the notes.
4. Run:

   ```sh
   make publish NOTES=docs/releases/vX.Y.Z.md   # or just: make publish
   ```

`make publish` refuses to run unless the tree is clean, `main` matches
`origin/main`, the tag is new, and `gh` is logged in. A dirty tree would give
the ROM a `-dirty` build ID that matches no commit. Then it:

- runs `make check` and `make dist`;
- writes `dist/release-notes.md`: the intro, the commits since the previous
  tag, and the ROM's SHA-256 (`tools/release_notes.py`);
- tags and pushes `vX.Y.Z`;
- creates the release with the `.sfc`, `.sha256`, `build-report.md`, and the
  player guide attached: `dist/README.md`, which `make dist` copies from
  `docs/PLAYER_GUIDE.md` (not the repository's README).

The `.sym` and `.map` debug files stay local in `dist/`.
