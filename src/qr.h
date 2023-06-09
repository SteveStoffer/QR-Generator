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

  void printQR();         
  void printData();       

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
  void setFuncBlocks(int, int, bool); 
  void setFinderBlocks(int, int);     
  void setAlignmentBlocks(int, int);  
  void drawAlignmentBlocks();         
  void drawPatterns();                
  void drawCodewords();               
  void drawFormat(int);               
  void drawVersion();                 
  void mask(int);                     

  // Encoding functions
  std::vector<std::uint8_t> encodeText(std::string_view);                   
  std::vector<std::uint8_t> generateEDC(const std::vector<std::uint8_t>&, int);   
  std::vector<std::uint8_t> addEDCInterleave(const std::vector<std::uint8_t>&);   

  // Reed Solomon Math 
  void rsGenerateLogExp();                                             
  std::uint8_t reedSolomonMult(std::uint8_t, std::uint8_t);            
  std::uint8_t reedSolomonDiv(std::uint8_t, std::uint8_t);             
  std::vector<std::uint8_t> rsPolyMult(const std::vector<std::uint8_t>&, 
                                       const std::vector<std::uint8_t>&);  
  std::vector<std::uint8_t> rsPolyDiv(const std::vector<std::uint8_t>&, 
                                      const std::vector<std::uint8_t>&); 
  std::vector<std::uint8_t> rsGeneratePoly(int);                       

  int formatBits(ErrCor); 
  
  int version_;                               // Version number of QR code
  int size_;                                  // Height and Witdh of QR code
  int mask_;                                  // Mask pattern used
  std::string plain_text_;                    // Original text
  ErrCor correctionLevel_;                    // Correction level for QR Code
  std::vector<std::vector<bool> > blocks_;    // Blocks that make up the QR code 
  std::vector<std::vector<bool> > funcBlock_; // Blocks that will not be masked
  std::vector<std::uint8_t> data_;            // Text encoded into bytes + EDC
  std::vector<std::uint8_t> rsLog_;           // Log values for RS algorithm
  std::vector<std::uint8_t> rsExp_;           // Exp values for RS algorithm
  const Encoding* kEncoding_;                 // Encoding method used
  static const std::string kAlphanumericChar_;              
  static const std::int8_t kEC_codewords_per_block_[4][41]; 
  static const std::int8_t kErr_corr_blocks_[4][41];        
}; // QRCode

#endif // QR_H_
