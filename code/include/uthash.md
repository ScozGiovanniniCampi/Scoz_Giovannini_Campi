# uthash usage (custom)

> Note: This document is custom-made for this repository and is NOT the
> official uthash documentation. For the official reference see:
> https://troydhanson.github.io/uthash/

This short guide shows the minimal pattern to use a struct with `uthash`.

1) Define your element with a `UT_hash_handle` field:

```c
struct user {
		int id;               /* key */
		char name[32];
		UT_hash_handle hh;    /* makes this structure hashable */
};
```

2) Include the header (in this repo the header is at `code/include/`):

```c
#include "include/uthash.h"  /* or <uthash.h> if installed system-wide */
```

3) Common operations (illustrative snippets):

```c
struct user *users = NULL;          /* hash table head */

/* add */
struct user *u = malloc(sizeof *u);
u->id = 1; strcpy(u->name, "Alice");
HASH_ADD_INT(users, id, u);

/* find */
struct user *s;
int findid = 1;
HASH_FIND_INT(users, &findid, s);
if (s) printf("found %s\n", s->name);

/* iterate and delete safely */
struct user *iter, *tmp;
HASH_ITER(hh, users, iter, tmp) {
	printf("%d: %s\n", iter->id, iter->name);
	HASH_DEL(users, iter);
	free(iter);
}
```

4) Compilation

From the repository root (this project keeps the header under `code/include`):

```bash
gcc -Icode/include -o code/uthash_example code/uthash_example.c
```

5) Notes
- This file is a concise, repo-specific usage note and not a replacement for
	the official documentation or the header's comments.
- If you prefer system-wide availability, copy `code/include/uthash.h` to
	`/usr/local/include/` (requires sudo).
