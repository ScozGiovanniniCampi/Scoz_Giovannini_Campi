# Scoz_Giovannini_Campi

This repository contains the material for the Operating Systems laboratory project "Library". The assignment describes a distributed library system written in C and Bash, where multiple library processes exchange books and serve user requests through IPC.

## Project Scope

The project is centered around a shared book catalog seeded from [code/books.csv](code/books.csv). Each record contains a title, an author, and a publication year. At bootstrap time, that catalog is split across the library processes, and each library manages its own local inventory while coordinating with the others when a requested book is not available locally.

The assignment also requires:

- a C implementation for the library processes
- Bash scripts for user interaction and system management
- a `Makefile` as the build entry point (use `make build`, `make run`, and `make clean`).
- a short report in PDF format describing the design choices

## Current Repository Layout

```text
.
├── README.md
├── LICENSE
├── COMMIT_GUIDE.md
└── code/
    └── books.csv
```

## Data Set

The catalog file already included in the repository is a simple CSV with the following columns:

- `Title`
- `Author`
- `Year`

The assignment states that each book exists exactly once in the system, so the catalog should be treated as a single shared source of truth that gets partitioned across libraries at startup.

## Development Notes

- Keep implementation files under `code/` unless you have a reason to reorganize the project.
- Prefer a clear separation between the library process, the user script, and the management script.
- Use the `Makefile` as the entry point and ensure it supports `build`, `clean`, and `run` as required by the assignment.
- The course instructions require the final submission archive to include `report.pdf` and the `code/` folder.

## Commit Style

Commit messages should follow the Conventional Commits specification and align with the [SemVer specification](https://semver.org/). See [COMMIT_GUIDE.md](COMMIT_GUIDE.md) for a project-specific summary and examples.

## License

This project uses the MIT License. See [LICENSE](LICENSE).