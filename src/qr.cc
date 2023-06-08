#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "qr.h"

// ---------------------- Internal Encoding Class ----------------------
int QRCode::Encoding::getEncodingMode() const {
  return encoding_mode_;
}

int QRCode::Encoding::getBitsPerChar(int ver) const {
  if (ver > 0 && ver < 10) {
    return bits_per_char_[0];
  } else if (ver > 9 && ver < 27) {
    return bits_per_char_[1];
  } else if (ver > 26 && ver < 41) {
    return bits_per_char_[2];
  } else {
    throw std::logic_error("Invalid version");
  }
}

QRCode::Encoding::Encoding(int mode, int v1_9, int v10_26, int v27_40): 
                          encoding_mode_(mode) {
  bits_per_char_[0] = v1_9;
  bits_per_char_[1] = v10_26;
  bits_per_char_[2] = v27_40;
}

// ---------------------- Internal BitBuffer Class ----------------------
QRCode::BitBuffer::BitBuffer() : std::vector<bool>() {}

void QRCode::BitBuffer::appendBits(std::uint32_t val, int len) {
  if (len < 0 || len > 31 || val >> len != 0) {
    throw std::logic_error("Out of range");
  }
  for (int i = len - 1; i >= 0; --i) {
    this->push_back(((val >> i) & 1) != 0);
  }
}

// ---------------------- QRCode Class ----------------------
// QRCode constructor.
QRCode::QRCode(std::string text, ErrCor err, int msk):
                plain_text_(text), correctionLevel_(err), rsLog_(256), rsExp_(256) {
  if (msk < 0 || msk > 7) {
    msk = 0;
  }
  mask_ = msk;
  determineEncoding(text);
  setVersionAndErrorLevel(kEncoding_->getEncodingMode(), text.length(), err);
  size_ = (4 * version_) + 17;
  blocks_ = std::vector<std::vector<bool> >(size_, std::vector<bool>(size_));
  funcBlock_ = std::vector<std::vector<bool> >(size_, std::vector<bool>(size_));
  drawPatterns();
  data_ = encodeText(text);
  data_ = addEDCInterleave(data_);
  drawCodewords();
  mask(msk);
}
QRCode::~QRCode() {
  delete kEncoding_;
}

// Determines the method of encoding to be used.
void QRCode::determineEncoding(std::string_view text) {
  if (isNumeric(text)) {
    kEncoding_ = new Encoding(Encoding::kNumeric_);
  } else if (isAlphanumeric(text)) {
    kEncoding_ = new Encoding(Encoding::kAlpha_);
  } else if (isByte(text)) {
    kEncoding_ = new Encoding(Encoding::kByte_);
  } else if (isKanji(text)) {
    kEncoding_ = new Encoding(Encoding::kKanji_);
  }
}

// Determines the positions of the aligment blocks.
std::vector<int> QRCode::determineAlignmentPos() const {
  if (version_ == 1) {
    return std::vector<int>();
  } else {
    // Calculate distance between alignment patterns
    int intervals = std::floor(version_ / 7 + 1);
    int distance = 4 * version_ + 4;
    int step = std::ceil(distance / intervals / 2) * 2;

    std::vector<int> alignment_tracks;
    alignment_tracks.push_back(6);

    for (int i = 0; i < intervals; ++i) {
      alignment_tracks.push_back(distance + 6 - (intervals - 1 - i) * step);
    }

    return alignment_tracks;
  }
}

// Returns total modules for the QRCode based on version.
int QRCode::getTotalModules(int version) {
  if (version == 1) {
    return 21 * 21 - 3 * 8 * 8 - 2 * 15 - 1 - 2 * 5;
  }
  
  int alignBlocks = (version / 7) + 2;

  return pow((version * 4 + 17), 2) - 3 * 8 * 8 - (pow(alignBlocks, 2) - 3) * 5 * 5 - 2 * (version * 4 + 1) 
  + (alignBlocks - 2) * 5 * 2 - 2 * 15 - 1 - (version > 6 ? 2 * 3 * 6 : 0);
}

// Returns total codewords per block depending on version and error correction level.
int QRCode::getTotalCodewords(int version, ErrCor error_level) {
  return (getTotalModules(version) >> 3) - (kErr_corr_blocks_[static_cast<int>(error_level)][version] * kEC_codewords_per_block_[static_cast<int>(error_level)][version]);
}

// Returns total capacity depending on version, error correction level, and encoding method.
int QRCode::getCapacity(int version, ErrCor error_level) {
  int data_codewords = getTotalCodewords(version, error_level);
  int bits_per_char = kEncoding_->getBitsPerChar(version);
  int available_bits = (data_codewords << 3) - bits_per_char - 4;
  
  int mode = kEncoding_->getEncodingMode();
  switch (mode) {
    case 1: return numericCapacity(available_bits);
    case 2: return alphanumbericCapacity(available_bits);
    case 4: return byteCapacity(available_bits);
    case 7: return byteCapacity(available_bits);
    case 8: return kanjiCapacity(available_bits);
    default: throw std::logic_error("Invalid encoding mode.");
  }
}

// Sets version and error level. Chooses the smallest version possible with the highest
// error correction without increasing version.
void QRCode::setVersionAndErrorLevel(int mode, int length, ErrCor min_err_cor) {
  for (int i = 1; i <= 40; ++i) {
    for (int j = static_cast<int>(ErrCor::kHigh); j >= static_cast<int>(min_err_cor); --j) {
      int capacity = getCapacity(i, static_cast<ErrCor>(j));
      if (capacity >= length) {
        version_ = i;
        correctionLevel_ = static_cast<ErrCor>(j);
        return;
      }
    }
  }
  throw std::logic_error("String too long!");
}

bool QRCode::isNumeric(std::string_view text) {
  for (const auto& ch : text) {
    if (ch < '0' || ch > '9') {
      return false;
    }
  }
  return true;
}

bool QRCode::isAlphanumeric(std::string_view text) {
  for (const auto& ch : text) {
    if (kAlphanumericChar_.find(ch) == std::string::npos) {
      return false;
    }
  }
  return true;
}

// Only supporting ASCII characters currently.
bool QRCode::isByte(std::string_view text) {
  for (const auto& ch : text) {
    if (ch < ' ' || ch > '~') {
      return false;
    }
  }
  return true;
}

// Not supported currently.
bool QRCode::isKanji(std::string_view text) {
  return false;
}

int QRCode::numericCapacity(int bits) {
  return (bits / 10) * 3 + ((bits % 10) > 6 ? 2 : (bits % 10) > 3 ? 1 : 0);
}

int QRCode::alphanumbericCapacity(int bits) {
  return (bits / 11) * 2 + (bits % 11 > 5 ? 1 : 0);
}

int QRCode::byteCapacity(int bits) {
  return bits >> 3;
}

int QRCode::kanjiCapacity(int bits) {
  return bits / 13;
}

// Helper function to set each function block to true or false.
void QRCode::setFuncBlocks(int x, int y, bool isBlock) {
  blocks_.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(x)) = isBlock;
  funcBlock_.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(x)) = true;
}

void QRCode::setFinderBlocks(int x, int y) {
  for (int distance_y = -4; distance_y <= 4; ++distance_y) {
    for (int distance_x = -4; distance_x <= 4; ++distance_x) {
      int distance = std::max(std::abs(distance_x), std::abs(distance_y));
      int block_x = x + distance_x;
      int block_y = y + distance_y;
      if (0 <= block_x && block_x < size_ && 0 <= block_y && block_y < size_) {
        setFuncBlocks(block_x, block_y, distance != 2 && distance != 4);
      }
    }
  }
}

void QRCode::setAlignmentBlocks(int x, int y) {
  for (int distance_y = -2; distance_y <= 2; ++distance_y) {
    for (int distance_x = -2; distance_x <= 2; ++distance_x) {
      setFuncBlocks(x + distance_x, y + distance_y, std::max(std::abs(distance_x), std::abs(distance_y)) != 1);
    }
  }
}

void QRCode::drawAlignmentBlocks() {
  const std::vector<int> aligment_pattern = determineAlignmentPos();
  std::size_t intervals = aligment_pattern.size();
  for (std::size_t i = 0; i < intervals; ++i) {
    for (std::size_t j = 0; j < intervals; ++j) {
      if (!((i == 0 && j == 0) || (i == 0 && j == intervals - 1) || (i == intervals - 1 && j == 0 ))) {
        setAlignmentBlocks(aligment_pattern.at(i), aligment_pattern.at(j));
      }
    }
  }
}

void QRCode::drawPatterns() {
  // Set each timing block, timing blocks are in row 6 and and column 6
  // alternating true / false.
  for (int i = 0; i < size_; ++i) {
    setFuncBlocks(i, 6, i % 2 == 0);
    setFuncBlocks(6, i, i % 2 == 0);
  }

  // Set each finder block
  setFinderBlocks(3, 3);
  setFinderBlocks(size_ - 4, 3);
  setFinderBlocks(3, size_ - 4);

  drawAlignmentBlocks();
  drawFormat(mask_);
  drawVersion();
}

void QRCode::drawCodewords() {
  std::size_t i = 0;

  // Draw the codewords in the zig-zag pattern, two columns at a time.
  for (int right = size_ - 1; right >= 1; right -= 2) {

    // Skip the 7th column since it is always reserved.
    if (right == 6) { 
      right = 5;
    }
    
    // Starting from the bottom right corner, place blocks in a zig-zag pattern.
    for (int vert = 0; vert < size_; ++vert) {
      for (int j = 0; j < 2; ++j) { 
        std::size_t x = static_cast<std::size_t>(right - j);
        bool up = ((right + 1) & 2) == 0;
        std::size_t y = static_cast<std::size_t>(up ? size_ - 1 - vert : vert);

        // Don't overwrite function blocks.
        if (!funcBlock_.at(y).at(x) && i < data_.size() * 8) {
          blocks_.at(y).at(x) = (((data_.at(i >> 3) >> (7 - static_cast<int>(i & 7))) & 1) != 0) ? true : false;
          ++i;
        }
      }
    }
  }
}

void QRCode::drawFormat(int mask) {
  // The format blocks are always made up of 15 bits.
  // Find the first 5 format bits by shifting the format bits left 3, 
  // and bitwise OR the mask.
  int data = formatBits(correctionLevel_) << 3 | mask;

  // Divide the first 5 bits by the polynomial x^10 
  // (x^10 + x^9 ... x^2 + x + 1) and calculate the remainder.
  int remainder = data;
  for (int i = 0; i < 10; ++i) {
    remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
  }
  
  // Shift the bits left 10, bitwise OR the remainder and XOR 21522.
  int bits = (data << 10 | remainder) ^ 0x5412;
  
  // Set the first set of format bits in the 8th column.
  for (int i = 0; i <= 8; ++i) {
    if ( i <= 5) {
      setFuncBlocks(8, i, ((bits >> i) & 1) != 0);
    } else if (i == 6) { // Skip row 6 since it is a timing block
      continue;
    } else { // Subtract 1 from 'i' since 6 was skipped.
      setFuncBlocks(8, i, ((bits >> (i - 1)) & 1) != 0);
    }
  }

  setFuncBlocks(7, 8, ((bits >> 8) & 1) != 0);

  // Set the format blocks in the 8th row on the left side.
  for (int i = 9; i < 15; ++i) {
    setFuncBlocks(14 - i, 8, ((bits >> i) & 1) != 0);
  }

  // Set the function blocks in the 8th row on the right side.
  for (int i = 0; i < 8; ++i) {
     setFuncBlocks(size_ - 1 - i, 8, ((bits >> i) & 1) != 0);
  }

  // Set the function blocks in the 8th coloumn on the bottom.
  for (int i = 8; i < 15; ++i) {
    setFuncBlocks(8, size_ - 15 + i, ((bits >> i) & 1) != 0);
  }
  // Set the single dark block
  setFuncBlocks(8, size_ - 8, true);
}

void QRCode::drawVersion() {
  // No version blocks for v 1-6.
  if (version_ < 7) {
    return;
  }

  // The version blocks are made up of 18 bits. The first 6 bits are the
  // version in binary, the other 12 are the remainder of polynomial division
  // between the version and x^12 (x^12 + x^11 + x^10 + x^9... + x^2 + x + 1).
  int remainder = version_;
  for (int i = 0; i < 12; ++i) {
    // Basically multiplies remainder by 2, divides remainder by 2^11 * 7973
    // and XOR the calculated values.
    remainder = (remainder << 1) ^ ((remainder >> 11) * 0x1F25);
  }
  
  // Add the version bits to the other 12 bits by shifting the version by 12,
  // and bitwise OR the calculated remainder.
  long version_bits = static_cast<long>(version_) << 12 | remainder;
  
  // Place the version bits in a 6x3 rectangle above the bottom left finder blocks, 
  // and a 3x6 rectangle to the left of the top right finder blocks.
  for (int i = 0; i < 18; ++i) {
    setFuncBlocks(size_ - 11 + i % 3, i / 3, ((version_bits >> i) & 1) != 0);
    setFuncBlocks(i / 3, size_ - 11 + i % 3, ((version_bits >> i) & 1) != 0);
  }
}

void QRCode::mask(int mask) {
  if (mask < 0 || mask > 7) {
    throw std::logic_error("Invalid mask.");
  }
  
  std::size_t size = static_cast<std::size_t>(size_);

  for (std::size_t y = 0; y < size; ++y) {
    for (std::size_t x = 0; x < size; ++x) {
      bool swap;
      switch (mask) {
        // The mask pattern algorithms can be found here:
        // https://www.thonky.com/qr-code-tutorial/mask-patterns
        case 0: swap = (x + y) % 2 == 0;                   break;
        case 1: swap = y % 2 == 0;                         break;
        case 2: swap = x % 3 == 0;                         break;
        case 3: swap = (x + y) % 3 == 0;                   break;
        case 4: swap = (x / 3 + y / 2) % 2 == 0;           break;
        case 5: swap = x * y % 2 + x * y % 3 == 0;         break;
        case 6: swap = (x * y % 2 + x * y % 3) % 2 == 0;   break;
        case 7: swap = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
        default: throw std::logic_error("Invalid mask value.");
      }

      // Apply the mask to all blocks that aren't function blocks.
      blocks_.at(y).at(x) = blocks_.at(y).at(x) ^ (swap & !funcBlock_.at(y).at(x));
    }
  }
}

// Encodes text without applying masks
std::vector<std::uint8_t> QRCode::encodeText(std::string_view text) {
  int mode = kEncoding_->getEncodingMode();

  BitBuffer buffer;

  // Convert encoding mode used to binary
  buffer.appendBits(static_cast<std::uint32_t>(mode), 4);

  // Convert number of codewords to binary
  buffer.appendBits(static_cast<std::uint32_t>(text.length()), kEncoding_->getBitsPerChar(version_));

  if (mode == 1) { // Numeric
    // Split each number into 'groups' of three, then encode each group
    // with 10 bits
    int group = 0;
    int max = 0;

    for (const auto& ch : text) {
      if (ch < '0' || ch > '9') {
        throw std::logic_error("Numeric: Contains non numeric characters!");
      }
      group = group * 10 + (ch - '0');
      max++;
      if (max == 3) {
        buffer.appendBits(static_cast<std::uint32_t>(group), 10);
        max = 0;
        group = 0;
      }
    }

    // Check for extra digits
    if (max > 0) {
      buffer.appendBits(static_cast<std::uint32_t>(group), max * 3 + 1);
    }

  } else if (mode == 2) { // Alphanumeric
    // Split each character into 'groups' of two, then encode each
    // group with 11 bits. Each character is mapped to its aplha-
    // numeric code.
    int group = 0;
    int max = 0;

    for (const auto& ch : text) {
      std::size_t index = kAlphanumericChar_.find(ch);
      group = group * 45 + index;
      max++;
      if (max == 2) {
        buffer.appendBits(static_cast<std::uint32_t>(group), 11);
        group = 0;
        max = 0;
      }
    }

    // Check for one remaining character
    if (max > 0) {
      buffer.appendBits(static_cast<std::uint32_t>(group), 6); // Only 6 bits needed for one char.
    }

  } else if (mode == 4) { // Byte
    // Convert each char to binary using 8 bits per character.
    for (const auto& ch : text) {
      buffer.appendBits(static_cast<std::uint32_t>(ch), 8);
    }

  } else if (mode == 7) { // ECI

  } else if (mode == 8) { // Kanji

  }

  // Add terminator if possible
  std::size_t capacity = static_cast<std::size_t>(getTotalCodewords(version_, correctionLevel_)) * 8;
  buffer.appendBits(0, std::min(4, static_cast<int>(capacity - buffer.size())));
  buffer.appendBits(0, (8 - static_cast<int>(buffer.size() % 8)) % 8);

  // Add padding bytes until capacity is reached
  for (std::uint8_t byte = 0xEC; buffer.size() < capacity; byte ^= 0xEC ^ 0x11) {
    buffer.appendBits(byte, 8);
  }

  // Make a vector of bytes from the bit buffer
  std::vector<std::uint8_t> codewords(buffer.size() / 8);
  for (std::size_t i = 0; i < buffer.size(); ++i) {
    codewords.at(i >> 3) |= (buffer.at(i) ? 1 : 0) << (7 - (i & 7));
  }

  return codewords;
}

std::vector<std::uint8_t> QRCode::generateEDC(const std::vector<std::uint8_t>& data, int codewords) {

  // The degree will always be the total amount of codewords - the amount of data codewords.
  int degree = codewords - data.size();

  // Create a message polynomial with a size of total codewords. Copy the data codewords into
  // it and fill the remaining space with zeros.
  std::vector<std::uint8_t> messagePoly(codewords, 0);
  std::copy(data.cbegin(), data.cend(), messagePoly.begin());

  // Divide the message polynomial by the generated polynomial and return the remainder.
  return reedSolomonPolyDiv(messagePoly, rsGeneratePoly(degree));
}

std::vector<std::uint8_t> QRCode::addEDCInterleave(const std::vector<std::uint8_t>& data) {

  // Generate log and exponent tables
  rsGenerateLogExp();

  int num_blocks = kErr_corr_blocks_[static_cast<int>(correctionLevel_)][version_];
  int ECC_per_block = kEC_codewords_per_block_[static_cast<int>(correctionLevel_)][version_];
  int total_codewords = getTotalModules(version_) >> 3; // Divides result by 8.
  int num_short_blocks = num_blocks - total_codewords % num_blocks;
  int short_block_len = total_codewords / num_blocks;
  
  // Split data into blocks, generate EDC for each block.
  std::vector<std::vector<std::uint8_t> > split_blocks;
  for (int i = 0, j = 0; i < num_blocks; ++i) {

    // Calculate the amount of data codewords to be split into blocks by subtracting the number of
    // error code correction codewords per block from the length of a short block. Add 1 if 
    // splitting data into a long block.
    std::vector<uint8_t> block(data.cbegin() + j, data.cbegin() + (j + short_block_len - ECC_per_block + (i < num_short_blocks ? 0 : 1)));

    // Increment 'j' by the size of the block to keep track of the index for the data codewords.
    j += static_cast<int>(block.size());
    
    // Generate EDC for short and long blocks. Adding 1 to the length of the 'short_block_len'
    // if generating EDC for long blocks. Append the EDC to the block and insert into the
    // 'split_blocks' vector.
    const std::vector<uint8_t> edc = generateEDC(block, short_block_len + (i < num_short_blocks ? 0 : 1));
    if (i < num_short_blocks) block.push_back(0); // Pad short blocks with a '0' for now.
    block.insert(block.end(), edc.cbegin(), edc.cend());
    split_blocks.push_back(std::move(block));
  }

  // Interleave each byte from every block, ignoring the padding for short blocks.
  std::vector<uint8_t> EDC_interleave;
  for (int i = 0; i < split_blocks.at(0).size(); ++i) {
    for (int j = 0; j < split_blocks.size(); ++j) {
      if (i != short_block_len - ECC_per_block || j >= num_short_blocks)
        EDC_interleave.push_back(split_blocks.at(j).at(i));
    }
  }

  return EDC_interleave;
}

// ------------------------- Reed Solomon Math -------------------------

void QRCode::rsGenerateLogExp() {
  // Create logarithmic and expontent tables for GF(256).
  // More info can be found here: 
  // https://en.wikiversity.org/wiki/Reed%E2%80%93Solomon_codes_for_coders#Multiplication
  for (int exp = 1, val = 1; exp < 256; exp++) {
    val = val > 127 ? ((val << 1) ^ 285) : (val << 1);
    rsLog_.at(val) = static_cast<uint8_t>(exp % 255);
    rsExp_.at(exp % 255) = static_cast<uint8_t>(val);
  }
  rsExp_.at(255) = 1; // Exponent values cannot be zero.
}

std::uint8_t QRCode::reedSolomonMult(std::uint8_t x, std::uint8_t y) {
  return x && y ? rsExp_.at((rsLog_.at(static_cast<std::size_t>(x)) + rsLog_.at(static_cast<std::size_t>(y))) % 255) : 0;
}

std::uint8_t QRCode::reedSolomonDiv(std::uint8_t x, std::uint8_t y) {
  return rsExp_.at((rsLog_.at(static_cast<std::size_t>(x)) + rsLog_.at(static_cast<std::size_t>(y))) % 255);
}

std::vector<std::uint8_t> QRCode::reedSolomonPolyMult(const std::vector<std::uint8_t>& poly1, const std::vector<std::uint8_t>& poly2) {
  // 'coeffs' will be the resulting product polynomial, it will always be
  // poly1.size() + poly2.size() - 1 in length.
  std::vector<std::uint8_t> coeffs(poly1.size() + poly2.size() - 1, 0);

  // Multiply each term of poly1 by all terms of poly2.
  for (std::size_t index = 0; index < coeffs.size(); ++index) {
    std::uint8_t coeff = 0;
    
    for (std::size_t p1index = 0; p1index <= index; ++p1index) {
      std::size_t p2index = index - p1index;

      // Make sure the indexes are not out of range, multiply, then add.
      if (p1index < poly1.size() && p2index < poly2.size()) {
        coeff ^= reedSolomonMult(poly1.at(p1index), poly2.at(p2index));
      }
    }

    coeffs.at(index) = coeff;
  }
  return coeffs;
}

std::vector<std::uint8_t> QRCode::reedSolomonPolyDiv(const std::vector<std::uint8_t>& dividend, const std::vector<std::uint8_t>& divisor) {
  std::size_t quotientLenth = dividend.size() - divisor.size() + 1;

  // Assume all dividends are a remainder for now.
  std::vector<std::uint8_t> remainder(dividend);

  for (std::size_t count = 0; count < quotientLenth; ++count) {
    if (remainder[0]) { // If the first value is 0, just remove it.

      // Divide the first term of the dividend polynomial by the first term of the divisor polynomial.
      std::uint8_t factor = reedSolomonDiv(remainder.at(0), divisor.at(0));

      // Subtraction polynomial. The size will always be the same as the size of the remainder.
      std::vector<std::uint8_t> subtr(remainder.size(), 0);

      // Multiply the divisor polynomial by the above quotient and copy the values into the
      // subtraction polynomial vector.
      std::vector<std::uint8_t> product = reedSolomonPolyMult(divisor, { factor });
      std::copy(product.begin(), product.end(), subtr.begin());

      // Find the remainder by subtracting the result from the dividend.
      for (std::size_t index = 0; index < remainder.size(); ++index) {
        remainder.at(index) ^= subtr.at(index);
      }

      // Remove the first term from the remainder.
      remainder.erase(remainder.begin());
    } else {
      // Since the first term is 0, remove it.
      remainder.erase(remainder.begin());
    }
  }

  return remainder;
}

std::vector<std::uint8_t> QRCode::rsGeneratePoly(int degree) {
  std::vector<std::uint8_t> lastPoly = { 1 };

  // Generate a polynomial with 'degree' terms.
  for (std::size_t i = 0; i < degree; ++i) {
    lastPoly = reedSolomonPolyMult(lastPoly, { 1, rsExp_.at(i) });
  }

  return lastPoly;
}

int QRCode::formatBits(ErrCor errorCorrectionLevel) {
  switch(errorCorrectionLevel) {
    case ErrCor::kLow      : return 1;
    case ErrCor::kMedium   : return 0;
    case ErrCor::kQuartile : return 3;
    case ErrCor::kHigh     : return 2;
    default: throw std::logic_error("Invalid ECL");
  }
}

void QRCode::printQR() {
  for (int y = 0; y < size_; ++y) {
    for (int x = 0; x < size_; ++x) {
      std::cout << (blocks_[y][x] ? "██" : "  ");
    }
    std::cout << "\n";
  }
}

void QRCode::printData() {
  std::cout << "Data: ";
  for (int i = 0; i < data_.size(); ++i) {
    std::cout << static_cast<int>(data_[i]) << " ";
  }
  std::cout << "\n";
}

// ------Constants------                                Version Version Version
                                        // Encoding Mode,  1-9,  10-26,  27-40
const QRCode::Encoding QRCode::Encoding::kNumeric_ (   1,   10,     12,   14);
const QRCode::Encoding QRCode::Encoding::kAlpha_   (   2,   9,      11,   13);
const QRCode::Encoding QRCode::Encoding::kByte_    (   4,   8,      16,   16);
const QRCode::Encoding QRCode::Encoding::kEci_     (   7,   0,       0,    0);
const QRCode::Encoding QRCode::Encoding::kKanji_   (   8,   8,      10,   12);

// Supported alphanumeric char set.
const std::string QRCode::kAlphanumericChar_ = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

// The values for error correction code words per block and the number of error correction blocks
// can be found at: https://www.thonky.com/qr-code-tutorial/error-correction-table

const std::int8_t QRCode::kEC_codewords_per_block_[4][41] = {
  // Version: (index[0] is a placeholder)
  //    1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40  Error Correction
  {-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // Low
	{-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28}, // Medium
	{-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // Quartile
	{-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // High
};

const std::int8_t QRCode::kErr_corr_blocks_[4][41] = {
  // Version: (index[0] is a placeholder)
  //   1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40  Error Correction
	{-1, 1, 1, 1, 1, 1, 2, 2, 2, 2,  4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25}, // Low
	{-1, 1, 1, 1, 2, 2, 4, 4, 4, 5,  5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49}, // Medium
	{-1, 1, 1, 2, 2, 4, 4, 6, 6, 8,  8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68}, // Quartile
	{-1, 1, 1, 2, 4, 4, 4, 5, 6, 8,  8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81}, // High
};


