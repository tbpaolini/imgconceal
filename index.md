---
layout: default
title: Home
---
# imgconceal: image steganography on JPEG and PNG

**imgconceal** is a free and open source steganography tool for hiding files inside JPEG and PNG images. The images with hidden data look the same to the human eye as a regular image. A password can be used so only those who you want can extract the data.

Features:
* Compression and encryption of the hidden data.
* Can hide multiple files in a single image.
* Can hide other files after on the same image.
* Preserves the metadata of the cover image and the hiddes files.

## Downloads - version {{ site.data.imgconceal.version }}:
* [imgconceal for Windows]({{ site.data.imgconceal.download }}v{{ site.data.imgconceal.version }}{{ site.data.imgconceal.windows }})
* [imgconceal for Linux]({{ site.data.imgconceal.download }}v{{ site.data.imgconceal.version }}{{ site.data.imgconceal.linux }})

This is a self-contained command line program, it requires no installation and has no dependencies. It is licensed under the [MIT License](https://github.com/tbpaolini/imgconceal/blob/master/License.txt).

## Quickstart

This is a command-line tool, so it needs to be run from a terminal. For Hiding a file in an image:
```shell
imgconceal -i "path to cover image" -h "path to file being hidden" -p "password for extraction"
```
And for extracting the file hidden in the image:
```shell
imgconceal -e "path to cover image" -p "password for extraction"
```

For all commands, please refer to [How to use](/help) and the [Documentation](/docs). You can also try the program on some [example images]({{ 'examples' | relative_url}}).