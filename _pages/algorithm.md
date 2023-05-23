---
layout: default
title: Algorithm
permalink: /algorithm/
last_modified_at: 2023-05-23 20:46:01 -0300
---

# Algorithm of *imgconceal*

The password is hashed using the [Argon2id](https://datatracker.ietf.org/doc/html/rfc9106) algorithm, generating a pseudo-random sequence of 64 bytes. The first 32 bytes are used as the secret key for encrypting the hidden data ([XChaCha20-Poly1305](https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-xchacha) algorithm), while the last 32 bytes are used to seed the pseudo-random number generator ([SHISHUA](https://espadrine.github.io/blog/posts/shishua-the-fastest-prng-in-the-world.html) algorithm) used for shuffling the positions on the image where the hidden data is written.

In the case of a JPEG cover image, the hidden data is written to the least significant bits of the quantized [AC coefficients](https://en.wikipedia.org/wiki/JPEG#Discrete_cosine_transform) that are not 0 or 1 (that happens after the lossy step of the JPEG algorithm, so the hidden data is not lost). For a PNG cover image, the hidden data is written to the least significant bits of the RGB color values of the pixels that are not fully transparent. Other image formats are not currently supported as cover image, however any file format can be hidden on the cover image (size permitting). Before encryption, the hidden data is compressed using the [Deflate](https://www.zlib.net/feldspar.html) algorithm.

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

For details on the specifics of each of those steps, please refer to [our source code](https://github.com/tbpaolini/imgconceal/tree/master/src). It has plenty of comments to detail each operation that **imgconceal** does. Nothing up the sleeves :)