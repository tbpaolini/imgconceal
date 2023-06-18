---
layout: default
title: Changelog
permalink: /changelog/
last_modified_at: 2023-06-17 23:04:29 -0300
---

# Changelog

## [Version 1.0.4 ](https://github.com/tbpaolini/imgconceal/releases/tag/v1.0.4)- June 17, 2023
- **BIG UPDATE:** Added support for hiding data on still WebP images.
- Tweaked help text to clarify about the `--output` option.
- The timer that controls the progress monitor's refresh rate is now thread safe.
- Now all verbose status messages are flushed to the terminal, so they do not
- Fixed bug where the program tried to access freed memory on exit.

## [Version 1.0.3](https://github.com/tbpaolini/imgconceal/releases/tag/v1.0.3) - May 23, 2023
- Added the ability to choose the output folder for the extracted files, which can be done with the `--output` option.
- When extracting hidden files on Windows, forbidden characters on the filename are now replaced by underscores. This is being done because Linux allows on filenames some characters that Windows doesn't, and we want to keep interoperable both the Windows and Linux versions of this program.
- When checking an image, and the `--verbose` option is enabled, more line breaks where added to the terminal's output in order to improve readability.
- The progress monitor (`--verbose` option) is now updated at most once 1/6 second, in order to prevent it from slowing down the whole program.
- Added an icon to the executable on Windows.

## [Version 1.0.2](https://github.com/tbpaolini/imgconceal/releases/tag/v1.0.2) - May 3, 2023
- Now it is possible to pass multiple file paths to the `--hide` (or `-h`) argument.
- Now when using the `--append` option while hiding files, the operation will fail if no existing hidden files can be found. This prevents the existing data from being overwritten in case of a wrong password.
- When an invalid command line option is passed to the program, now the error message says which option was wrong.
- Check if the paths provided by the user are directories, so the program can give the appropriate error message.

## [Version 1.0.1](https://github.com/tbpaolini/imgconceal/releases/tag/v1.0.1) - April 30, 2023
- Windows version now handles properly special characters on terminal.
- Handling special characters on passwords consistently across Windows and Linux (the password string is now encoded to UTF-8 on both systems).
- More descriptive error messages if a file could not be opened, detailing the reason for that.

## [Version 1.0.0](https://github.com/tbpaolini/imgconceal/releases/tag/v1.0.0) - April 25, 2023
- Initial release.