# Continuous integration

Every push to `main` builds the project, runs the C++ unit tests, and drives
the robot through every scenario. The result appears as a tick or cross on the
commit and as the badge at the top of the README.

Workflow: [`.github/workflows/simulation-tests.yml`](../.github/workflows/simulation-tests.yml)

## What runs

1. **Checkout**, keeping `Binaries/` and `Intermediate/` from the previous run
   so the build is incremental rather than a full rebuild.
2. **`Tools/RunTests.ps1`** builds the editor target and runs every automation
   test headless.
3. **`Tools/RunScenarios.ps1`** launches the simulation as a game with no
   window or rendering, waits until its API accepts connections, runs every
   scenario through `Tools/Analysis/run_scenarios.py`, and shuts the
   simulation down. The robot's own in-engine brain drives; the harness
   only watches and judges.
4. **Logs are uploaded** as a build artifact whether the run passed or failed,
   so a red run can be diagnosed from GitHub.

A run is red if any unit test fails, if any scenario fails, or if a scenario
marked as a known failure unexpectedly passes.

## Why a self-hosted runner

GitHub's hosted runners have no Unreal Engine installation, and Epic's engine
containers are around 20 GB against roughly 14 GB of disk on a free runner. The
workflow therefore runs on a self-hosted runner: GitHub's runner agent,
installed on a Windows machine that already has Unreal Engine 5.8, Visual
Studio and Python.

The harness uses only Python's standard library, so the runner needs no Python
packages installed.

## Security on a public repository

GitHub recommends against self-hosted runners on public repositories. A
workflow runs whatever code is in the commit it builds, so a workflow triggered
by pull requests would let anyone run code on the runner's machine by forking
the repository and opening a pull request.

Three controls address this:

1. **The workflow is not triggered by pull requests.** It runs only on pushes
   to `main` and on manual dispatch, both of which require write access to the
   repository.
2. **The runner is not installed as a service.** It is started by hand with
   `run.cmd` when CI is wanted, and nothing is listening for jobs otherwise.
3. **Fork workflows require approval** for all external contributors, under
   Settings → Actions → General, so a pull request trigger added by mistake
   would still not run unattended.

The comment at the top of the workflow file explains the first point, so that
a `pull_request` trigger is not added later without understanding what it
opens.

## Running the runner

```
cd C:\Users\joelo\actions-runner
./run.cmd
```

It prints `Listening for Jobs` and picks up any queued run.

**Jobs wait for the runner.** A push made while the runner is stopped queues a
job, and GitHub cancels jobs that wait more than 24 hours for a self-hosted
runner. If that happens, start the runner and use **Run workflow** on the
Actions tab to run it again.

## Sharing the machine with the editor

The runner builds its own checkout, under the runner's `_work` folder, but
with the same installed engine as the editor.

**The build step fails while Live Coding is active**, with *Unable to build
while Live Coding is active*. Live Coding holds a lock on the engine's editor
executable rather than on one project, so an open editor with Live Coding
running blocks the runner's build too, even though it is a different folder.

To avoid it, set **Editor Preferences → General → Live Coding → Startup** to
**Manual**. Live Coding then starts only when **Ctrl+Alt+F11** is first
pressed in a session, so an editor that has only been used for play does not
block CI. Once Live Coding has been used, close the editor before pushing.

If a run fails this way, close the editor and use **Re-run jobs** on the
failed run.

The scenario step, by contrast, can run alongside the editor. It runs the
simulation on port **8766**, using the `-RoboApiPort` command-line override,
while the editor's play session uses the default **8765**.

## Running the same thing locally

Close the editor first, since the build cannot run while Live Coding is active:

```
./Tools/RunTests.ps1
./Tools/RunScenarios.ps1
```

Both exit with 0 on success and 1 on failure, exactly as CI sees them.
