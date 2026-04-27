# Commit Guide

This project follows [Conventional Commits v1.0.0](https://www.conventionalcommits.org/en/v1.0.0/) and the [SemVer specification](https://semver.org/). The goal is to keep the history readable and make future automation easier.

## Format

```text
<type>[optional scope][!]: <description>

[optional body]

[optional footer(s)]
```

## Recommended Types

- `feat`: a new feature
- `fix`: a bug fix
- `docs`: documentation-only changes
- `refactor`: code changes that do not add features or fix bugs
- `test`: test additions or updates
- `chore`: maintenance work
- `build`: build system or dependency changes
- `ci`: continuous integration changes
- `perf`: performance improvements

## Rules of Thumb

- Use the type to describe the main intent of the commit.
- Add a scope when it helps locate the change, for example `library`, `user`, `bootstrap`, or `docs`.
- Keep the description short, imperative, and lowercase when possible.
- Use `!` in the header, or a `BREAKING CHANGE:` footer, when the commit introduces a breaking change.
- Put extra context in the body when the change is not obvious from the summary alone.
- Keep unrelated changes in separate commits whenever possible.

## Examples For This Project

```text
feat(library): add search across remote catalogs
```

```text
fix(bootstrap): preserve catalog order when splitting books.csv
```

```text
docs(readme): explain build and run workflow
```

```text
feat(api)!: change borrow response format

BREAKING CHANGE: user scripts must parse the new response payload.
```

```text
chore: update ignore rules for generated catalogs
```

## Practical Workflow

1. Make one logical change per commit.
2. Run the relevant build or test command before committing.
3. If a commit message is wrong, fix it before publishing the branch when possible.
4. Prefer `docs:` commits for documentation updates so they are easy to scan later.

## References

- Specification: https://www.conventionalcommits.org/en/v1.0.0/
- SemVer specification: https://semver.org/
- Semantic versioning context: patch for `fix`, minor for `feat`, major for breaking changes