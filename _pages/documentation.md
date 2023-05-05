---
layout: default
title: Documentation
permalink: /docs/
---

<style type="text/css">
    table {
        border-spacing: 0 1em;
    }
    
    table tr td:first-child {
        vertical-align: top;
        white-space: nowrap;
        font-weight: bold;
        padding-right: 0.5em;
    }
</style>

# Documentation

These are all the command-line arguments that `imgconceal` accepts:

|`-c`, `--check=IMAGE` | Check if a given JPEG or PNG image contains data hidden by this program, and estimate how much data can still be hidden on the image. If a password was used to hide the data, you should also use the `--password` option. |
|`-e`, `--extract=IMAGE` | Extracts from the cover image the files that were hidden on it by this program.The extracted files will have the same names and timestamps as when they were hidden. |
|`-h`, `--hide=FILE` | Path to the file being hidden in the cover image. This option can be specified multiple times in order to hide more than one file. You can also pass more than one path to this option in order to hide multiple files. If there is no enough space in the cover image, some files may fail being hidden (files specified first have priority when trying to hide). The default behavior is to overwrite the existing previously hidden files, to avoid that add the `--append` option. |
|`-i`, `--input=IMAGE` | Path to the cover image (the JPEG or PNG file where to hide another file). Please use the `--output` option to specify where to save the modified image. |
|`-o`, `--output=IMAGE` | Path to where to save the image with hidden data. If this option is not used, the output file will be named automatically (a number is added to the name of the original file). |
|`-a`, `--append` |  When hiding a file with the `--hide` option, append the new file instead of overwriting the existing hidden files. For this option to work, the password must be the same as the one used for the previous files |
|`-p`, `--password=TEXT` | Password for encrypting and scrambling the hidden data. This option should be used alongside `--hide`, `--extract`, or `--check`. The password may contain any character that your terminal allows you to input (if it has spaces, please enclose the password between quotation marks). If you do not want to have a password, please use `--no-password` instead of this option. |
|`-n`, `--no-password` | Do not use a password for encrypting and scrambling the hidden data. That means the data will be able to be extracted without needing a password. This option can be used with `--hide`, `--extract`, or `--check`. |
|`-s`, `--silent` |  Do not print any progress information (errors are still shown). |
|`-v`, `--verbose` | Print detailed progress information. |
|`--algorithm` | Print a summary of the [algorithm used by imgconceal]({{ 'algorithm' | relative_url}}), then exit. |
|`-?`, `--help` |  give this help list |
|`--usage` | give a short usage message |
|`-V`, `--version` | print program version |