# mespprc_c

`mespprc_c` is the transferable native-build package for the MESPPRC research
prototype.

## What is in this folder

- Standalone Python module sources mirroring the current `mespprc` package
- Generated C translation units for each major module:
  - `instance.c`
  - `label.c`
  - `route.c`
  - `phase1.c`
  - `phase2_dp.c`
  - `phase2_ip.c`
  - `instance_generator.c`
- In-folder build metadata:
  - `setup.py`
  - `pyproject.toml`

## Intended use on a machine with the required native toolchain

From inside this `mespprc_c` directory:

```bash
python -m pip install .
```

Or for an in-place editable/native development install:

```bash
python -m pip install -e .
```

If the compiled extension modules build successfully, Python will load the compiled
package modules instead of the `.py` source fallbacks.

## Requirements

- Python 3.11+
- A working C extension build toolchain for the target platform
- The Python headers/libs for the target interpreter
- `pulp>=3.3` available at runtime for the Phase 2 IP solver

## Status behavior

At runtime, `mespprc_c.__init__` reports:

- `C_BINDINGS_AVAILABLE = True` when compiled extension modules are loaded
- `C_BINDINGS_AVAILABLE = False` when Python source fallback modules are loaded
