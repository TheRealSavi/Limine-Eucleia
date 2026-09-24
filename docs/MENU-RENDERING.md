# Menu behaviour and rendering

Menu presentation is extracted from `common/menu.c`. The Eucleia GUI and original
terminal menu implement the same rendering interface. The `eucleia.conf`
companion selects the GUI; missing or disabled companions retain the terminal menu.
See [GUI.md](GUI.md) for theme loading and framebuffer implementation details.

## Source map

| File | Responsibility |
| --- | --- |
| `common/menu.c` | Configuration policy, entry filtering, default/last/one-shot selection, keyboard and mouse actions, expansion, timeout decisions, editor invocation and boot dispatch |
| `common/menu_model.c` | Visible depth-first projection of the entry tree, independent of drawing |
| `common/menu_renderer.h` | Read-only menu view, visual options and renderer interface |
| `common/menu_renderer.c` | Renderer selection and terminal fallback |
| `common/menu_terminal.c` | Existing centred layout, tree connectors, scrolling, colours, branding, help, comments, countdown and hit testing |
| `common/menu_gui.c`, `common/ui/` | GUI layout, framebuffer drawing, proportional text, theme validation and GUI pointer |
| `common/lib/config.c` | Config parsing and ownership of `struct menu_entry` trees |
| `common/lib/term.c` | Terminal lifecycle, firmware/text fallback and shared ANSI colour formatting |
| `common/lib/gterm.c` | Graphical terminal setup, font/wallpaper configuration and Flanterm integration |
| `common/lib/fb.c`, `common/drivers/gop.c` | Framebuffer information and UEFI display modes |
| `common/lib/image.c` | Image loading/decoding used for wallpapers |
| `common/drivers/mouse.c` | Pointer devices, events, coordinates and cursor sprite support |
| `common/lib/getchar.c` | Key decoding and timed input waits |
| `common/lib/bli.c` | UEFI Boot Loader Interface variables |
| `common/lib/panic.s2.c` | Error presentation and return to menu/editor |
| `common/protos/` | Actual boot protocol implementations |

## The rendering interface

The controller builds a `menu_model`, resolves selection from it and passes a
`menu_view` to a renderer. It does not ask drawing code to find boot entries.

```text
configuration tree -> visible model -> controller -> renderer
                                        |              |
                                        |         layout + pixels/text
                                        |
                                  editor / boot / firmware
```

`menu_row` contains borrowed names and presentation metadata: depth, parent row,
directory/expansion state and sibling connectors. The renderer cannot access
entry bodies or execute a boot. The controller alone maps a visible index to
`struct menu_entry *`.

| Operation | Contract |
| --- | --- |
| `init(style)` | Initialise renderer state after terminal setup; return false when unavailable |
| `draw(view)` | Build a complete menu frame, keeping the selected index visible |
| `present()` | Display the prepared frame and pointer |
| `timeout(milliseconds)` | Display and present remaining time; zero clears the countdown |
| `hit_test(x, y, &index)` | Return a visible index for an input position; no selection or boot side effects |
| `leave()` | Restore terminal output for editor, boot messages or firmware actions |

The controller retains timeout parsing, waiting, cancellation and automatic boot.
Only its visual representation moves behind the rendering interface. Expansion,
wrapping, Home/End, numeric shortcuts, wheel policy and boot decisions stay in
the controller.

`menu_renderer_start(preferred, style)` selects the preferred renderer if its
initialisation succeeds, otherwise initialises the existing terminal renderer.
The controller supplies the GUI renderer, whose initialisation discovers
`eucleia.conf` beside the selected boot configuration. Missing, disabled or
invalid UI configuration selects the terminal renderer. The controller does
not parse presentation properties. See [CONFIGURATION.md](CONFIGURATION.md).

The fallback still uses Flanterm when graphics are available, or the existing
firmware/text/serial terminal when they are not. A preferred renderer that fails
must release partial resources and leave that terminal usable.

## Ownership and lifecycle

- Initialise `menu_model` to zero. `menu_model_update()` rebuilds its visible
  projection after expansion changes, and reuses buffers when the count is
  unchanged. An empty tree releases the projection. Names remain owned by the
  config tree; callers must not retain row pointers across an update.
- `menu_model_count()` is also used by default-path lookup, so visible indices
  share the same traversal rules. Firmware/architecture filtering remains a
  controller-supplied predicate.
- Renderers borrow a view only during `draw()`. They may keep layout indices and
  scroll state but must not keep its row or comment pointers.
- Style strings remain valid for the menu session. The terminal renderer copies
  the style values; it does not own the branding string.
- `leave()` suspends menu presentation. A later `draw()` must be able to resume
  it after the user cancels editing. It is not a final destructor.
- A failed boot re-enters `_menu(false)` through the existing rewind mechanism.
  The controller creates a fresh model and initialises the renderer again.
- A zero timeout can boot before renderer initialisation; the controller handles
  the absent renderer on that path.

## GUI and remaining scope

The Eucleia adapter owns pixel layout, proportional text, image composition,
selection decoration and a GUI pointer. `mouse_set_canvas()` selects pixel input
coordinates; leaving the GUI restores terminal cells. The controller hit-tests
both click edges against the same entry. Pointer conversion is host-tested with
synthetic firmware events; actual input delivery with this OVMF image remains
an integration gap.

The configuration editor remains inside `common/menu.c` and uses the existing
terminal. A themed editor is separate work. Error reporting and protocol loading
messages also continue using the terminal. Protocol code and downloaded
Flanterm sources remain outside the graphical changes.

There is one renderer instance for the active menu session. The interface is not
a multi-window framework or a theme format. A graphical implementation may reveal
small additions it needs, but boot actions should remain controller-owned.

## Verification

See `tests/README.md` for reproducible host and QEMU checks. Runtime comparisons
use an EFI binary saved before the refactor and fresh, isolated VM images.
No test writes the host EFI partition or host firmware variables.

The complete-frame drawing contract normalises one visual edge case: an empty
menu revealed from quiet mode now shows branding and hides the text cursor,
matching the normal empty menu. Selection and boot policy are unchanged.
