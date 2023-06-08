#include <iostream>
#include <string>

#include "qr.h"

int main() {
  std::string text;
  int err;
  int mask;

  std::cout << "Enter text to be converted to QR Code: ";
  std::getline(std::cin, text);

  QRCode code(text, QRCode::ErrCor::kHigh, 9);

  std::cout << "Version: " << code.getVersion() << " Encoding Mode: " << code.getEncoding() << " Bits Per Char: "
            << code.getBitsPerChar() << " Mask: " << code.getMask() << " Size (H & W): " << code.getSize() << "\n";
  
  code.printQR();
  return 0;
}