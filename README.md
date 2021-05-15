# OBS Pthread Text

## Introduction

This plugin displays text from a file with many advanced features.

* Markup
* Text alignment
  * Left, center, and right
  * Justification
* Outline
  * Configurable width, color, and opacity
  * Blur
* Transition
  * Fade-in, fade-out, cross-fade
  * Slide
* Threaded glyph drawing
  * Lower priority to draw glyphs so that other sources and encoders are not impacted
  * More frequent polling of the text file
* Automatic line-break supporting East Asian languages

### Markups

See [the Pango Markup Language](https://developer.gnome.org/pygtk/stable/pango-markup-language.html)
for detailed markup tags available.

### Updating text

This plugin can set the text by setting or from a text file.
To have transition, the text should be updated from a progam, not by typing on the setting window.

#### Updating text using obs-websocket

You can use `SetSourceSettings` request for [obs-websocket](https://github.com/Palakis/obs-websocket/).
Request fields will be as below for example.
```
{"sourceName": "source-name", "sourceSettings": {"text": "your new text"}}
```

#### Updating text file

This plugin checks these file attributes; inode, mtime, and size.
Recommended flow to update the text is as below.
1. Set the source file in the property of this plugin. Let's say ```/dev/shm/text.txt``` for example.
2. Write to a temporary file.
   ```your_program > /dev/shm/text.txt~```
3. Move the temporary file to the target file.
   ```mv /dev/shm/text.txt~ /dev/shm/text.txt```
   This step is atomic so that the plugin won't read the middle state.

### Furture plan

* support Mac OS and Windows,
* use inotify instead of polling the file,
* and feature requests.
