#ifndef QR_H_
#define QR_H_

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

class QRCode {
 public:
  // Error Correction level in a QR Code
  enum class ErrCor {
    kLow = 0,   // 7%  Error Correction
    kMedium,    // 15% Error Correction
    kQuartile,  // 25% Error Correction
    kHigh,      // 30% Error Correction
  }; // ErrCor

  class Encoding {
   public:
    // Constants for length of bits depending on version number
    // and encoding mode.
    static const Encoding kNumeric_;
    static const Encoding kAlpha_;
    static const Encoding kByte_;
    static const Encoding kEci_;
    static const Encoding kKanji_;

    int getEncodingMode() const;
    int getBitsPerChar(int) const;
   private:
    // Constructor
    Encoding(int, int, int, int);

    int encoding_mode_;
    int bits_per_char_[3];
  }; // Encoding

  class BitBuffer : public std::vector<bool> {
   public:
    BitBuffer();
    void appendBits(std::uint32_t, int);
  }; // BitBuffer

  // QR Code constructor.
  QRCode(std::string, ErrCor err = ErrCor::kLow, int msk = 0);
  ~QRCode();

  int getEncoding() { return kEncoding_->getEncodingMode(); }
  int getBitsPerChar() { return kEncoding_->getBitsPerChar(version_); }
  int getVersion() { return version_; }
  int getSize() { return size_; }
  int getMask() { return mask_; }
  std::string getText() { return plain_text_; }

  void printQR();         // Prints the actual QR code to the terminal.
  void printData();       // Prints encoded QR data.

 private:
  void determineEncoding(std::string_view);
  std::vector<int> determineAlignmentPos() const;
  int getTotalModules(int);
  int getTotalCodewords(int, ErrCor);
  int getCapacity(int, ErrCor);
  void setVersionAndErrorLevel(int, int, ErrCor);

  // Functions to determine type of text given.
  bool isNumeric(std::string_view);
  bool isAlphanumeric(std::string_view);
  bool isByte(std::string_view);
  bool isKanji(std::string_view);

  // Functions that return capacities for each encoding mode and version.
  int numericCapacity(int);
  int alphanumbericCapacity(int);
  int byteCapacity(int);
  int kanjiCapacity(int);

  // Functions that set blocks and draws blocks.
  void setFuncBlocks(int, int, bool); // Helper function to set function blocks to make sure they do not get overwritten.
  void setFinderBlocks(int, int);     // Sets all finder blocks in the correct location
  void setAlignmentBlocks(int, int);  // Helper function to set aligment blocks.
  void drawAlignmentBlocks();         // Draws all alignment blocks
  void drawPatterns();                // Draws all timing blocks, finder blocks, aligment blocks, format blocks, and version blocks.
  void drawCodewords();               // Draws all codewords into the QR code, without overwriting function blocks.
  void drawFormat(int);               // Draws format information (Error correction level and mask).
  void drawVersion();                 // Draws version data for versions 7 - 40.
  void mask(int);                     // Applies a mask to the data bits.

  // Encoding functions
  std::vector<std::uint8_t> encodeText(std::string_view);                   // Encodes text based on encoding method.
  std::vector<std::uint8_t> generateEDC(std::vector<std::uint8_t>&, int);   // Generates the correct error data correction codewords.
  std::vector<std::uint8_t> addEDCInterleave(std::vector<std::uint8_t>&);   // Splits data into blocks, appends EDC, and interleaves bits.

  // Reed Solomon Math 
  void rsGenerateLogExp();                                             // Generates logarithmic and exponential tables.
  std::uint8_t reedSolomonMult(std::uint8_t, std::uint8_t);            // Returns the correct exponent for multiplication.
  std::uint8_t reedSolomonDiv(std::uint8_t, std::uint8_t);             // Returns the correct exponent for division.
  std::vector<std::uint8_t> reedSolomonPolyMult(const std::vector<std::uint8_t>&, const std::vector<std::uint8_t>&);  // Polynomial multiplication.
  std::vector<std::uint8_t> reedSolomonPolyDiv(const std::vector<std::uint8_t>&, const std::vector<std::uint8_t>&); // Divides two polynomials and returns the remainder.
  std::vector<std::uint8_t> rsGeneratePoly(int);                       // Generates a polynomial of degree 'n' to find the remainder in RS division.

  int formatBits(ErrCor); // Returns a value from 0 to 3 depending on error correction level
  
  int version_;                                // Version number of QR code
  int size_;                                   // Height and Witdh of QR code
  int mask_;                                   // Mask pattern used
  std::string plain_text_;                     // Original text
  ErrCor correctionLevel_;                     // Error correction level for the QR Code
  std::vector<std::vector<bool> > blocks_;     // Blocks that make up the QR code 
  std::vector<std::vector<bool> > funcBlock_;  // Blocks that will not be masked
  std::vector<std::uint8_t> data_;             // Text encoded into bytes with EDC
  std::vector<std::uint8_t> rsLog_;            // Log values for Reed Soloman algorithm
  std::vector<std::uint8_t> rsExp_;            // Exponent values for Reed Soloman algorithm
  const Encoding* kEncoding_;                               // Encoding method used
  static const std::string kAlphanumericChar_;              // Valid alphanumeric char set
  static const std::int8_t kEC_codewords_per_block_[4][41]; // Error correction codewords per block
  static const std::int8_t kErr_corr_blocks_[4][41];        // Number of error correction blocks
}; // QRCode

#endif // QR_H_
