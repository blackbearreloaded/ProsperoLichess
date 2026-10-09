# Build output

Every build creates and validates the app folder `dist/<TITLE_ID>/` and
archives it as `dist/<TITLE_ID>.zip`. There is no other output: the project
builds no filesystem image and no signed retail PKG/FPKG container.

```bash
make app
```

On Windows, `./build.ps1` runs the same build through WSL.

The build uses Python's standard-library `zipfile` module to archive
`dist/<TITLE_ID>/` as `<TITLE_ID>.zip`, then stores every entry as mode 0777
(`tools/zip-open-modes.py`): the console only starts an app whose files are
open to all. The ZIP is a distribution convenience, not a console filesystem
format; extract it before directory deployment. Tagged GitHub Releases and CI
builds attach this ZIP and its `SHA256SUMS`.

An image an older build left on the console (`<TITLE_ID>.ffpkg`) is not
replaced by a folder deployment. Remove it with `make undeploy` so it is not
mistaken for the current app.
