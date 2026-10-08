# Pull-request builds

Every pull request is built in full by the [Build workflow](../.github/workflows/tooling.yml):
lint, host tests, and the same packaging a release gets. The result is an installable copy
of ProsperoLichess, kept for 14 days, that a reviewer can put on a console before merging.

## What a pull request produces

| | Pull request | Tag, or a run started by hand |
| --- | --- | --- |
| Artifact name | `ProsperoLichess-PR<number>-<commit>` | `ProsperoLichess-<commit>` |
| `<commit>` | First seven characters of the pull request's own head commit | The full commit that was built |
| Label file in the app folder | `PR <number>, <commit>` | None |
| `contentVersion` | Unchanged | Unchanged |

For example, pull request 12 at commit `1ae2fd0…` is uploaded as `ProsperoLichess-PR12-1ae2fd0`
and its app folder says `PR 12, 1ae2fd0`.

Two details are deliberate:

- **The commit is the pull request's head**, not `github.sha`. For a pull request,
  `github.sha` is a temporary merge commit that appears nowhere on the pull request's page,
  so an artifact named after it cannot be matched to what is being reviewed.
- **The version is not touched.** A test build reports the same `contentVersion` as the
  release it is based on, so the in-app update check and the update itself behave exactly as they will after the
  merge. The label is a separate file.

## Getting the build

1. Open the pull request, then **Checks** and the **Build** run (or the run's page under
   **Actions**).
2. Download the artifact named `ProsperoLichess-PR<number>-<commit>` from the run's **Artifacts**
   list. GitHub requires a signed-in account with access to the repository for this.
3. Unpack it: it holds `PPSA99009.zip` (the app folder) and `SHA256SUMS`. Check the ZIP with
   `sha256sum -c SHA256SUMS`, then install as described in [Deployment](DEPLOYMENT.md).

## The label file

`tools/build.sh` writes the environment variable `BUILD_LABEL` as one line to
`build-label.txt` at the root of the app folder, next to `eboot.bin`
(`dist/PPSA99009/build-label.txt` after a build, `/app0/build-label.txt` on the console).
The file is in the ZIP. The workflow sets the variable for pull requests
only. A build without it writes no file, so a release never carries one.

`BUILD_LABEL` must be 1 to 40 characters from letters, digits, spaces and `, . _ # -`. The
build refuses anything else before compiling, so the text is safe to show as it is.

The app does not show the label yet: to see which build is installed, read the file from
the app folder. When a screen shows the version, it can read the same file and show nothing
when it is missing. Do not put the label into `param.json` or compare it with anything: it
is for people.

The same works on your PC, for a build you want to tell apart on the console:

```bash
BUILD_LABEL="pacing test 2" make
```

## Keeping it safe

The name used for tags and runs started by hand, `ProsperoLichess-<commit>`, appears twice in the
workflow (the "Name this build" step, and the release job's download); change both together.
A tag never takes the pull-request branch, so the release job finds its build as before.

Pull-request runs have a read-only token and no secrets, including for forks. Do not move
this build to `pull_request_target` to post links or comments: that event runs with write
access and secrets, and building a contributor's code under it hands both to that code.
