# Per Wall Loop Color Index

In Advanced mode, open **Multimaterial → Filament for Features** and enter filament
numbers in **Per Wall Loop Color Index**, beside **Walls**. This setting can
also be overridden for an object or part.

The comma-separated numbers refer to the project's ordinary filament slots.
The first number selects the exterior wall loop, the second selects the next
loop inward, and so on. Additional loops repeat the last number. For example,
with four walls, `2,1,3` produces `2,1,3,3` from outside to inside. A single
number selects that filament for every indexed wall loop.

Leave the field empty to use the regular Walls filament. Enter positive whole
numbers for available physical filament slots; zero, missing entries, and
unavailable filament numbers are rejected. This setting selects existing
filaments and does not change the number of walls.

Filament reordering updates the numbers while retaining each wall's assignment.
Deleting an assigned filament clears the affected override, returning it to
the inherited setting or the regular Walls filament. Assignments to surviving
filaments are remapped to their new slot numbers.

## Limits

- Gap fill and thin-wall paths without a loop index use the regular Walls
  filament.
- Painted regions and regions whose Walls filament is a mixed filament ignore
  this setting.
- Changing materials within a layer adds tool changes and purge waste. Use
  compatible materials and the appropriate purge settings for the project.

## Development checks

The Catch2 `[WallLoopFilaments]` tests cover parsing, repeat behavior, filament
remapping, complete-loop splitting, configuration inheritance, painting,
validation, tool ordering, prime-tower normalization, and generated G-code. The G-code cases cover Classic,
Arachne, a single-entry list, an empty list, and flushing into infill.

After building the Windows test targets, run:

```powershell
& build_windows/tests/libslic3r/Release/libslic3r_tests.exe '[WallLoopFilaments]'
& build_windows/tests/fff_print/Release/fff_print_tests.exe '[WallLoopFilaments]'
```

The parser and remapping tests can also be built independently of the slicer's
GUI dependencies:

```powershell
cmake -S tests/wall_loop_filaments -B build_wall_loop_tests
cmake --build build_wall_loop_tests --config Release
ctest --test-dir build_wall_loop_tests -C Release --output-on-failure
```
