# Vendored libraries

## Reason

Vendored dependencies are necessary to ensure that any qt-multiplier commit can always be rebuild in the same way even if additional commits are added to the other repositories. Additionally, it will ensure that the daily scheduled builds will not break.

## How to update a vendored library

### Updating to the last commit

```
cd libraries/vendored/multiplier
git pull --rebase origin main

cd ..
git add multiplier
```

### Updating to a known tag

```
cd libraries/vendored/multiplier
git fetch --all --tags

git checkout -b release_v1.1.0 v1.1.0

cd ..
git add multiplier
```
