# imgconceal: steganography on JPEG and PNG images

*imgconceal* is a tool for image steganography, that can hide files inside JPEG, PNG and WebP images. A password an be used for extracting the data later. The image with hidden data looks the same to the human eye as the regular image.

**Downloads:**
* [imgconceal for Windows](https://github.com/tbpaolini/imgconceal/releases/download/v1.0.4/imgconceal.exe) (2.60 MB)
* [imgconceal for Linux](https://github.com/tbpaolini/imgconceal/releases/download/v1.0.4/imgconceal) (2.76 MB)

This is a command line program that is available for both Windows and Linux operating systems. *imgconceal* is a standalone executable, requiring no installation or shared libraries (DLL or SO).

## Basic usage

This is a command line program, so you need to run `imgconceal` on a terminal. Currently there is no graphical user interface (GUI), though that is not off the table for a future update.

You can just put the executable on the folder where you want to run it, or add the executable's folder to the `PATH` environment variable of your system so you can run `imgconceal` from anywhere.

### Hiding

To hide a file in an image using a password, run this command:

```shell
./imgconceal --input "path to the cover image" --hide "path to the file being hidden" --password "the password to be used for extraction"
```

If there is enough space in the cover image to hide the file, the image with hidden data will be saved to a new file (the original image and the hidden files are left untouched). The new image is named automatically (by adding a number to the original name), but if you want to specify the name you can add the command line argument `--output "new image name"`.

The `--hide "..."` argument can receive multiple paths (or be provided multiple times) in order to hide more than one file in a single image. Size permitting, as many files as possible will be hidden (you get a status message which files succeeded or not). For example:

```shell
# Providing multiple files to be hidden in a single cover image
./imgconceal --input "cover image" --hide "file 1" "file 2" "file 3" --password "password for extraction"

# This also works for hiding multiple files
./imgconceal --input "cover image" --hide "file 1" --hide "file 2" --hide "file 3" --password "password for extraction"
```

If you do not want to use a password, you can provide the argument `--no-password` instead of `--password "..."`. It is important to note that if you lose the password, **there is no way of recovering it**. The password is not stored, it is used instead for generating the encryption key, and without that key it's not possible to decrypt the data.

If you do not provide the `--no-password` or `--password "..."` options, then you are asked to type the password on the terminal. In this case, the typed characters are not shown, and you can just type nothing to use no password. Afterwards, you are asked to type the password again to confirm.

### Extraction

To extract the files hidden in an image, run this command:

```shell
./imgconceal --extract "path to the cover image" --password "the same password used when hiding"
```

If you didn't use a password for hiding, you can pass the argument `--no-password` instead of `--password "..."`. If the password is correct, the hidden files will be saved to the current folder.

By default, the files are extracted to the current directory. If you prefer to extract the files to somewhere else, you can use the `--output` option:

```shell
./imgconceal --extract "image" --output "folder" --password "your password"
```

Either way, if a extracted file has the same name as one that already exists, it will be renamed automatically (so no previously existing file is overwritten). You get status messages informing the names of the extracted files.

*Tip:* You can check our [example images](https://tbpaolini.github.io/imgconceal/examples/) to test extracting data from them.

### Checking an image for hidden data

It is possible to check if an image contains data hidden by *imgconceal*, without extracting the files. Just run this command:

```shell
./imgconceal --check "path to the cover image" --password "the same password used when hiding"
```

Again, `--no-password` can be used instead of `--password "..."`.

If there are hidden files, they will be listed, alongside with their timestamps and names. If there is no hidden files or the password is wrong, the approximate amount of bytes that can be hidden in the image will be shown.

This program cannot differentiate between a wrong password and non-existing hidden data.

## Advanced usage

It is possible to abbreviate a command line argument by just writing a `-` and its first letter. For example, `--input` can be `-i`, and `--extract` can be `-e`.

Examples (the arguments' order does not matter):
```shell
# Hiding a file on an image
./imgconceal -i "input image" -h "file being hidden" - o "output image" -p "password for unhiding"

# Extracting a hidden file from an image
./imgconceal -e "input image" -p "same password as before"
```

You can add the argument `--verbose` (or `-v`) to any operation in order to display the progress of each step performed during the hiding, extraction, or checking. Alternatively, you can add `--silent` (or `-s`) in order to print no status messages at all (errors are still shown).

When hiding a file, the default behavior is to overwrite the existing hidden files on the cover image. You can avoid that by adding the `--append` (or `-a`) argument. In order for appending to work, **the password used must be the same** as used for the previous files, otherwise the operation will fail (the existing files remain untouched).

You can run `./imgconceal --help` in order to see all available command line arguments and their descriptions. For convenience's sake, here is the full help text:

```txt
Usage: imgconceal [OPTION...]

Steganography tool for hiding and extracting files on JPEG, PNG and WebP
images. Multiple files can be hidden in a single cover image, and the hidden
data can be (optionally) protected with a password.

Hiding a file on an image:
  imgconceal --input=IMAGE --hide=FILE [--output=NEW_IMAGE] [--append]
[--password=TEXT | --no-password]

Extracting a hidden file from an image:
  imgconceal --extract=IMAGE [--output=FOLDER] [--password=TEXT |
--no-password]

Check if an image has data hidden by this program:
  imgconceal --check=IMAGE [--password=TEXT | --no-password]

All options:

  -c, --check=IMAGE          Check if a given JPEG, PNG or WebP image contains
                             data hidden by this program, and estimate how much
                             data can still be hidden on the image. If a
                             password was used to hide the data, you should
                             also use the '--password' option. 
  -e, --extract=IMAGE        Extracts from the cover image the files that were
                             hidden on it by this program. The extracted files
                             will have the same names and timestamps as when
                             they were hidden. You can also use the '--output'
                             option to specify the folder where the files are
                             extracted into.
  -h, --hide=FILE            Path to the file being hidden in the cover image.
                             This option can be specified multiple times in
                             order to hide more than one file. You can also
                             pass more than one path to this option in order to
                             hide multiple files. If there is no enough space
                             in the cover image, some files may fail being
                             hidden (files specified first have priority when
                             trying to hide). The default behavior is to
                             overwrite the existing previously hidden files, to
                             avoid that add the '--append' option.
  -i, --input=IMAGE          Path to the cover image (the JPEG, PNG or WebP
                             file where to hide another file). You can also use
                             the '--output' option to specify the name in which
                             to save the modified image.
  -o, --output=PATH          When hiding files in an image, this is the
                             filename where to save the image with hidden data
                             (if this option is not used, the new image is
                             named automatically). When extracting files from
                             an image, this option is the directory where to
                             save the extracted files (if not used, the files
                             are extracted to the current working directory).
  -a, --append               When hiding a file with the '--hide' option,
                             append the new file instead of overwriting the
                             existing hidden files. For this option to work,
                             the password must be the same as the one used for
                             the previous files.
  -p, --password=TEXT        Password for encrypting and scrambling the hidden
                             data. This option should be used alongside
                             '--hide', '--extract', or '--check'. The password
                             may contain any character that your terminal
                             allows you to input (if it has spaces, please
                             enclose the password between quotation marks). If
                             you do not want to have a password, please use
                             '--no-password' instead of this option.
  -n, --no-password          Do not use a password for encrypting and
                             scrambling the hidden data. That means the data
                             will be able to be extracted without needing a
                             password. This option can be used with '--hide',
                             '--extract', or '--check'.
  -s, --silent               Do not print any progress information (errors are
                             still shown).
  -v, --verbose              Print detailed progress information.
      --algorithm            Print a summary of the algorithm used by
                             imgconceal, then exit.
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <https://github.com/tbpaolini/imgconceal/issues> or
<tpaolini@gmail.com>.
```

## Algorithm

The password is hashed using the [Argon2id](https://datatracker.ietf.org/doc/html/rfc9106) algorithm, generating a pseudo-random sequence of 64 bytes. The first 32 bytes are used as the secret key for encrypting the hidden data ([XChaCha20-Poly1305](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha) algorithm), while the last 32 bytes are used to seed the pseudo-random number generator ([SHISHUA](https://espadrine.github.io/blog/posts/shishua-the-fastest-prng-in-the-world.html) algorithm) used for shuffling the positions on the image where the hidden data is written.

In the case of a JPEG cover image, the hidden data is written to the least significant bits of the quantized [AC coefficients](https://en.wikipedia.org/wiki/JPEG#Discrete_cosine_transform) that are not 0 or 1 (that happens after the lossy step of the JPEG algorithm, so the hidden data is not lost). For a PNG or WebP cover image, the hidden data is written to the least significant bits of the RGB color values of the pixels that are not fully transparent. Other image formats are not currently supported as cover image, however any file format can be hidden on the cover image (size permitting). Before encryption, the hidden data is compressed using the [Deflate](https://www.zlib.net/feldspar.html) algorithm.

All in all, the data hiding process goes as:

1. Hash the password (output: 64 bytes).
2. Use first half of the hash as the secret key for encryption.
3. Seed the PRNG with the second half of the hash.
4. Scan the cover image for suitable bits where hidden data can be stored.
5. Using the PRNG, shuffle the order in which those bits are going to be written.
6. Compress the file being hidden.
7. Encrypt the compressed file.
8. Break the bytes of the encrypted data into bits.
9. Write those bits to the cover image (on the shuffled order).

The file's name and timestamps are also stored (both of which are also encrypted), so when extracted the file has the same name and modified time. The hidden data is extracted by doing the file operations in reverse order, after hashing the password and unscrambling the read order.

For details on the specifics of each of those steps, please refer to [its source code](https://github.com/tbpaolini/imgconceal/tree/master/src). It has plenty of comments to detail each operation that **imgconceal** does. Nothing up my sleeves :)

## Compiling imgconceal

Pre-compiled binaries are already provided. But if you want to build the program yourself, this section should help you.

### General instructions

This program was built using the GCC compiler (version 11.3.0 on Linux, version 12.2.0 on Windows). We believe that later versions of GCC should be able to compile *imgconceal* without issues.

The GNU toolchain was used to develop the program on Linux (package `build-essential` of Ubuntu 22.04.2, running on WSL). On Windows, we used the GNU toolchain on the MSYS2/MinGW framework (package `mingw-w64-ucrt-x86_64-toolchain`), linking with the Microsoft's Universal C Runtime (UCRT).

The following third party libraries where used, and need to have their development versions installed on your system before compiling *imgconceal*:

* `libsodium` (hashing and encryption) - version 1.0.18
* `zlib` (data compression) - version 1.2.11
* `libjpeg-turbo` (low level manipulation of JPEG images) - version 2.1.2
* `libpng` (low level manipulation of PNG images) - version 1.6.37
* `libwebp` (low level manipulation of WebP images) - version 1.2.2

In order to compile the program on Linux, on the terminal navigate to the root directory of the project and then run `make`. On Windows, you can do the same but on the MSYS2 UCRT64 terminal.

### Detailed instructions

#### Linux

*Note: The commands and package names used here are from Ubuntu, and distros based on it. If compiling on a different distro, you should check the names for it.*

You should install the GNU toolchain, Git, and the aforementioned development packages. You can install everything at once with this command:

```shell
sudo apt install build-essential git libsodium-dev zlib1g-dev libjpeg-turbo8-dev libpng-dev libwebp-dev
```

Navigate to the directory where you want to save the project, and run this command to download *imgconceal*'s source code:

```shell
cd "directory of your choice"
git clone https://github.com/tbpaolini/imgconceal.git
```

Then navigate to the newly created `imgconceal` subdirectory, and run `make`:

```shell
cd imgconceal
make
```

Once the build is finished, the compiled executable is saved to the subdirectory `bin/linux/release`.

You can instead run `make debug` to compile the program with debug symbols (the output will be on `bin/linux/debug`), so it is possible debug it on GDB. Before compiling a different type of build, you should run `make clean` to delete the temporary files created by the compilation, this way release and debug binaries do not get mixed with each other.

#### Windows

Download MSYS2 from its [official website](https://www.msys2.org/), and install it. A few different terminals are installed on your system, they go to your start menu. The one that you should use for this project is **MSYS2 UCRT64**, all commands listed on this section should be run on it.

First, update the packages to the latest version by running:

```shell
pacman -Syu
```

It might ask you to restart the terminal in order to install the updates. If that happens, close the terminal, open it, then run the above command again. Do it until you are no longer asked to restart the terminal.

You should install the UCRT version of the GNU toolchain, Git, and the aforementioned development packages. You can install everything at once with this command:

```shell
pacman -S --needed base-devel git mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-libjpeg-turbo mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-libwebp mingw-w64-x86_64-zlib
```

Navigate to the directory where you want to save the project, and run this command to download *imgconceal*'s source code:

```shell
cd "directory of your choice"
git clone https://github.com/tbpaolini/imgconceal.git
```

Then navigate to the newly created `imgconceal` subdirectory, and run `mingw32-make`:

```shell
cd imgconceal
mingw32-make
```

Once the build is finished, the compiled executable is saved to the subdirectory `bin/windows/release`.

You can instead run `mingw32-make debug` to compile the program with debug symbols (the output will be on `bin/windows/debug`), so it is possible debug it on GDB. Before compiling a different type of build, you should run `mingw32-make clean` to delete the temporary files created by the compilation, this way release and debug binaries do not get mixed with each other.

## Disclaimer

The *imgconceal* program, besides scrambling the data through the whole image, makes no attempt of fooling statistical analysis methods that attempt to detect whether an image contains data hidden through steganographic means. But it is worthy noting that probably not too many people know about steganography and steganalysis, so it should suffice to conceal files from a casual observer :)

Anyways, I do not intend the program to be used for any critical or serious purposes. It is just a fun tool for entertainment, like perhaps some sort of crypto puzzle / ARG (alternate reality game), exchanging hidden files with a friend, etc. 

With all that considered, though this program comes with no guarantees, using a strong password should be enough to prevent the data from being extracted, even if someone knows that there is hidden data. The password hashing algorithm was intentionally slowed down, which should be hardly noticeable under normal usage but quickly adds up when brute-forcing a password.
