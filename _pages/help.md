---
layout: default
title: Help
permalink: /help/
---

# Help

## Basic usage

This is a command line program, so you need to run `imgconceal` on a terminal. Currently there is no graphical user interface (GUI), though that is not off the table for a future update.

You can just put the executable on the folder where you want to run it, or add the executable's folder to the `PATH` enviroment variable of your system so you can run `imgconceal` from anywhere.

### Hiding

To hide a file in a image using a password, run this command:

```shell
imgconceal --input "path to the cover image" --hide "path to the file being hidden" --password "the password to be used for extraction"
```

If there is enough space in the cover image to hide the file, the image with hidden data will be saved to a new file (the original image and the hidden files are left untouched). The new image is named automatically (by adding a number to the original name), but if you want to specify the name you can add the command line argument `--output "new image name"`.

The `--hide "..."` argument can receive multiple paths (or be provided multiple times) in order to hide more than one file in a single image. Size permitting, as many files as possible will be hidden (you get a status message which files succeeded or not). For example:

```shell
# Providing multiple files to be hidden in a single cover image
imgconceal --input "cover image" --hide "file 1" "file 2" "file 3" --password "password for extraction"

# This also works for hiding multiple files
imgconceal --input "cover image" --hide "file 1" --hide "file 2" --hide "file 3" --password "password for extraction"
```

If you do not want to use a password, you can provide the argument `--no-password` instead of `--password "..."`. It is important to note that if you lose the password, **there is no way of recovering it**. The password is not stored, it is used instead for generating the encryption key, and without that key it's not possible to decrypt the data.

If you do not provide the `--no-password` or `--password "..."` options, then you are asked to type the password on the terminal. In this case, the typed characters are not shown, and you can just type nothing to use no password. Afterwards, you are asked to type the password again to confirm.

### Extraction

To extract the files hidden in an image, run this command:

```shell
imgconceal --extract "path to the cover image" --password "the same password used when hiding"
```

If you didn't use a password for hiding, you can pass the argument `--no-password` instead of `--password "..."`. If the password is correct, the hidden files will be saved to the current folder.

If a extracted file has the same name as one that already exists, it will be renamed automatically (so no previously existing file is overwritten). You get status messages informing the names of the extracted files.

### Checking a image for hidden data

It is possible to check if an image contains data hidden by *imgconceal*, without extracting the files. Just run this command:

```shell
imgconceal --check "path to the cover image" --password "the same password used when hiding"
```

Again, `--no-password` can be used instead of `--password "..."`.

If there are hidden files, they will be listed, alongside with their timestamps and names. If there is no hidden files or the password is wrong, the approximate amount of bytes that can be hidden in the image will be shown.

This program cannot differentiate between a wrong password and non-existing hidden data.

## Advanced usage

It's possible to abbreviate a command line argument by just writing a `-` and its first letter. For example, `--input` can be `-i`, and `--extract` can be `-e`.

Examples (the arguments' order does not matter):
```shell
# Hiding a file on an image
imgconceal -i "input image" -h "file being hidden" - o "output image" -p "password for unhiding"

# Extracting a hidden file from an image
imgconceal -e "input image" -p "same password as before"
```

You can add the argument `--verbose` (or `-v`) to any operation in order to display the progress of each step performed during the hiding, extraction, or checking. Alternatively, you can add `--silent` (or `-s`) in order to print no status messages at all (errors are still shown).

When hiding a file, the default behavior is to overwrite the existing hidden files on the cover image. You can avoid that by adding the `--append` (or `-a`) argument. In order for appending to work, **the password used must be the same** as used for the previous files, otherwise the operation will fail (the existing files remain untouched).

You can run `imgconceal --help` in order to see all available command line arguments and their descriptions, or just check the [Documentation]({{ 'docs' | relative_url}}). We also provide [some images]({{ 'examples' | relative_url}}) for you to test the program on.