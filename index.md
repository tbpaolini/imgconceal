---
layout: default
title: Home
last_modified_at: 2023-06-18 14:21:33 -0300
---
# imgconceal: image steganography on JPEG, PNG and WebP

**imgconceal** is a free and open source steganography tool for hiding files inside JPEG, PNG and WebP images. The images with hidden data look the same to the human eye as a regular image. A password can be used so only those who you want can extract the data.

Features:
* Available for **Windows** and **Linux**.
* Self-contained (requires no installation and has no dependencies).
* Fast and small executable (2.7 MB).
* Compression and encryption of the hidden data.
* Can hide multiple files in a single image.
* Preserves the metadata of the cover image and of the hiddes files.

<p style="padding-top: 0.5em;">
    <a href="https://github.com/tbpaolini/imgconceal/releases/tag/v{{ site.data.imgconceal.version }}" id="download" title="Get the latest release of imgconceal" rel="external" target="_blank">
    Download - version {{ site.data.imgconceal.version }}
    </a>
</p>

This free software is licensed under the <a href="https://github.com/tbpaolini/imgconceal/blob/master/License.txt" rel="license" target="_blank">MIT License</a>.

## Quickstart

This is a command-line tool. For Hiding a file in an image, run on a terminal:
```shell
imgconceal -i "path to cover image" -h "path to file being hidden" -p "password for extraction"
```
And for extracting the file hidden in the image:
```shell
imgconceal -e "path to cover image" -p "password for extraction"
```

For all commands, please refer to [How to use]({{ 'help/' | relative_url }}) and the [Documentation]({{ 'docs/' | relative_url }}). You can also try the program on some [example images]({{ 'examples/' | relative_url}}).
