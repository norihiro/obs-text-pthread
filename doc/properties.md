# PThread Text Properties

## Text Properties

### Font
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Font | `font` | object | n/a | size=64 |

This property specifies font for the text.
Still the font can be changed by mark up in the text.

### Text
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Text | `text` | string | can be multiline | "" |

This property will be taken if _Read text from a file_ is unchecked.
If _Pango mark-up_ is checked, the text has to have a correct mark-up syntax.

### Read text from a file
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Read text from a file | `from_file` | bool | false, true | false |

If checked, the text will be retreived from a file specified by _Text file_ property.

### Text file
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Text file | `text_file` | string | Absolute file path | |

This property will be taken if _Read text from a file_ is checked.
If _Pango mark-up_ is checked, the text has to have a correct mark-up syntax.

### Pango mark-up
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Pango mark-up | `markup` | bool | false, true | true |

If checked, the text is parsed by pango as a mark-up language.
See [the Pango Markup Language](https://developer.gnome.org/pygtk/stable/pango-markup-language.html)
for details.

## Color and Size Properties

### Color
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Color | `color` | int | 0-4294967295 | #FFFFFFFF (solid white) |

This property selects the default color of the text.
Still the mark-up can overwrite the color.
Transparent color value is also accepted.

### Width and Height
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Width | `width` | int | 0-16384 | 1920 |
| Height | `height` | int | 0-16384 | 1080 |

These properties specify the canvas size when drawing the text.

### Automatically shrink size
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Automatically shrink size | `shrink_size` | bool | false, true | true |

If checked, shrinks the canvas size to eliminate empty spaces.

## Text Layouting Properties

### Alignment
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Alignment | `align` | int | 0-6 | 0 (left) |

This property specifies horizontal alignment and justification.
Justification will happen if the text in a line is too long and need to wrap the text.
From the API, available numbers are shown as below.

| Value | Description |
| ----- | ----------- |
| 0 | Align to left |
| 1 | Align to center |
| 2 | Align to right |
| 4 | Justification (can be added to one of above values.) |

### Calculate the bidirectonal base direction
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Calculate the bidirectonal base direction | `auto_dir` | bool | false, true | true |

When this flag is true (the default), then paragraphs in layout that begin with strong right-to-left characters (Arabic and Hebrew principally), will have right-to-left layout, paragraphs with letters from other scripts will have left-to-right layout. Paragraphs with only neutral characters get their direction from the surrounding paragraphs.
When false, the layout is left-to-right.

See [`pango_layout_set_auto_dir`](https://developer.gnome.org/pango/1.46/pango-Layout-Objects.html#pango-layout-set-auto-dir)
for detailed description.

### Wrap text
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Wrap text | `wrapmode` | int | 0-2 | 0 (word) |

| Value | Selection | Description |
| ----- | --------- | ----------- |
| 0 | Word | wrap lines at word boundaries. |
| 1 | Char | wrap lines at character boundaries. |
| 2 | WordChar | wrap lines at word boundaries, but fall back to character boundaries if there is not enough space for a full word.

### Indent
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Indent | `indent` | int | -32767 - +32767 | 0 |

This property sets the width to indent each paragraph.

### Ellipsize
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Ellipsize | `ellipsize` | int | 0 - 3 | 0 (none) |

This parameter controls ellipsization, if long line is given, some characters are removed in order to make it fit to the width.

| Value | Selection | Description |
| 0 | None | No ellipsization. |
| 1 | Start | Omit characters at the start of the text. |
| 2 | Middle | Omit characters in the middle of the text. |
| 3 | End | Omit characters at the end of the text. |

### Line spacing
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Line spacing | `spacing` | int | -65536 - +65536 | 0 |

This parameter controls additional spacing between lines.

## Decoration Properties

This plugin provides two decorations; outline and shadow.
The outline makes the text thicker with different color.
The shadow has the same figure as the text and the outline and stays underneath them and drawn by yet another color.

### Outline
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Outline | `outline` | bool | false, true | false |
| Outline color | `outline_color` | int | | `#FF000000` (black) |
| Outline width | `outline_width` | int | 0 - 65536 | 0 |
| Outline blur | `outline_blur` | int | 0 - 65536 | 0 |
| Outline shape | `outline_shape` | int | | Round |

The width specifies the width of outline in pixels.
The blur will composite several outline that have different width from `width+blur` to `width-blur`.
You should set non-zero value for at least width or blur.

Outline shape takes one of these choice.
- Round: (default)
- Bevel: The outline becomes octagonal shape at the corner.
- Rect: The outline becomes rectangle shape at the corner.
- Sharp: The outline becomes pointed sharp at acute corner. The maximum length of the acute corner is limited to 4 since there is no limit if the corner is super acute. Usually it should not reach the limit.

See also [examples](https://github.com/norihiro/obs-text-pthread/wiki/properties#outline-shape).

### Shadow
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Shadow | `shadow` | bool | false, true | false |
| Shadow color | `shadow_color` | int | | `#FF000000` (black) |
| Shadow offset x | `shadow_x` | int | -65536 - +65536 | 2 |
| Shadow offset y | `shadow_y` | int | -65536 - +65536 | 3 |
<!-- TODO: implement them
| Shadow width | `shadow_width` | int | | |
| Shadow blur | `shadow_blur` | int | | |
| Shadow shape | `shadow_shape` | int | | |
-->

If enabled, a shadow is drawn underneath the text and the outline.
The color of the shadow can be specified by `Shadow color` property.
The location of the shadow is specified by x and y coordinate. Please note that nothing will be displayed if both x and y are zero.

## Transition Properties

### Transition Alignment
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
| Transition alignment (horizontal) | `align_transition.h` | int | 0, 1, 2 | 0 (left) |
| Transition alignment (vertical)   | `align_transition.v` | int | 8, 16, 32 | 8 (top) |

This parameter is used if crossfade or slide transition is enabled.
Please set as same as the alignment setting for the scene item.

### Fade
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
Fadein time [ms] | `fadein_ms` | int | 0 - 4294 | 0 |
Fadeout time [ms] | `fadeout_ms` | int | 0 - 4294 | 0 |
Crossfade time [ms] | `crossfade_ms` | int | 0 - 4294 | 0 |

These parameters enable fade-in/out and cross fade if set to non-zero value.
Fadein is applied if the text is set from blank to non-blank.
Fadeout is applied if the text is set from non-blank to blank.
Crossfade is applied if the text is set from non-blank to non-blank.

### Slide
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
Slide \[px/s\] | `slide_pxps` | int | 0 - 65500 | 0 |

This parameter enables slide transition if set to non-zero value.
When the text is updated, old text will slide upward and the new text will slide from the bottom.

Set 0 to disable slide. Default is 0.

Slide feature is under development. Behavior might change in the next release.

## Post-production Support Properties

### Save as PNG
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
Save as PNG | `save_file` | bool | false, true | false |

Each text will be saved as a PNG file if this property is checked.
The PNG image has alpha channel so that you can import the file to your non-linear video editor and overlay on your video.

Default is disabled.

### Directory to save
| Name | Key | Type | Range | Default |
| ---- | --- | ---- | ----- | ------- |
Directory to save | `save_file_dir` | string | Absolute path to a directory | |

Specify the directory to save the PNG files.
