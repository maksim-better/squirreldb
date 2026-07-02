# SquirrelDB

![logo](logo.jpg)

A toy SQL database built in C, following [Let's Build a Simple Database](https://cstack.github.io/db_tutorial/) by cstack.

## Build

```bash
gcc main.c -o db
```

## Usage

```bash
./db
```

A REPL prompt `db ->` will appear. Supported commands:

| Command | Description |
|---------|-------------|
| `.exit` | Exit the program |
| `insert` | Insert a row (not yet implemented) |
| `select` | Select rows (not yet implemented) |

## TODO

- [ ] Table / page storage
- [ ] B-tree implementation
- [ ] Cursor abstraction
- [ ] Persist to file
