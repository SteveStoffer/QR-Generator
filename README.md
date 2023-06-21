# C++ QR Code Generator
![QRCode](https://github.com/SteveStoffer/QR-Generator/assets/59321074/58c71fc9-fb74-48a4-bb55-c8999772d1c5)

___
## Understanding a QR Code:

### Data Types:
- Numbers
- Alphanumeric Characters
- Bytes (8-bit Latin-1 characters)
- Kanji

### Sizes and Versions:
Each version of QR codes come in a variety of different sizes. For instance, version 1 is 21 x 21
pixels and version 40 is 177x177. In order to find the size needed for a QR code (based by
version), we simply multiply the version by 4, then add 17. `(version * 4) + 17`.

### Error Correction:
Each QR code contains error correction blocks. There are four different levels of error correction.
| Level   | Letter  | Data Recovery |
|---------|---------|---------------|
| Low     | L       | ~7%           |
| Medium  | M       | ~15%          |
| Quartile| Q       | ~25%          |
| High    | H       | ~30%          |
This means that if part of the QR code is covered, the code can still be read. With a higher
correction level, more of the QR code can be covered. Some like to put a company logo in the center
and use a higher correction level for advertising.

### Patterns of a QR Code:
Every QR code has the same pattern. QR codes are a sqaure made up of dark and light "boxes" and
each box is in a specific sequence.
- **Finder Pattern:** The finder pattern is made up of three 7x7 squares at the top left, top right,
 and bottom left corners.
- **Separators:** The separators are located around the finder pattern. This separates the finder 
pattern from the rest of the QR code and they are always made up of light colored blocks.
- **Timing Pattern:** The timing pattern is made up of alternating light and dark blocks, this 
allows the decoder to determine the width of a single block.
- **Alignment Pattern:** The alignment patterns are made up of a 5x5 square. There are n^2 - 3 
(n being height or width) alignment patterns in a QR code unless it is version 1. The alignment 
pattern will not be placed where a finder pattern is located.
- **Format Information:** These are blocks stored next to the finder patterns that store information 
about the error correction level and encoding mode.
- **Version Inforation:** The version information is made up of two 6x3 blocks located to the left 
of the right finder block, and above the bottom left finder block. This will be found on versions 7+.
- **One Dark Block:** One dark block is placed in the 9th column, and in the `((4 * version) + 10)` 
row.

![qr-parts](https://github.com/SteveStoffer/QR-Generator/assets/59321074/c62e385f-1699-4e0e-af26-94c8752b9ce2)
___
## Steps to Create a QR Code:
1. Determine the encoding method used based on given text.
2. Determine the version and error correction (if none is given).
3. Convert the encoding mode to bits.
4. Convert the number of code words to bits.
5. Convert each character to bits (For numeric and alphanumeric modes the characters need to be 
grouped before being encoded). The amount of bits needed per character also needs to be determined.
6. Add terminator block if possible.
7. Add padding bytes until the capacity is reached.
8. Draw all standard QR patterns.
9. Draw all encoded codewords in a zig zag pattern.
10. Apply mask and determine which mask has the lowest penalty score.
