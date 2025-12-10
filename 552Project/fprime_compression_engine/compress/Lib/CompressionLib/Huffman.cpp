#include "compress/Lib/CompressionLib/Huffman.hpp"

#include <cstdint>
#include <fstream>
#include <queue>
#include <vector>
#include <array>
#include <memory>

namespace CompressionLib {

namespace {

// ---------- Helpers for endian-safe header I/O ----------

void writeUint32(std::ofstream& os, std::uint32_t v) {
  std::uint8_t b[4];
  b[0] = static_cast<std::uint8_t>(v & 0xFFu);
  b[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
  b[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
  b[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
  os.write(reinterpret_cast<const char*>(b), 4);
}

std::uint32_t readUint32(std::ifstream& is, bool& ok) {
  std::uint8_t b[4];
  if (!is.read(reinterpret_cast<char*>(b), 4)) {
    ok = false;
    return 0;
  }
  ok = true;
  return static_cast<std::uint32_t>(b[0]) |
         (static_cast<std::uint32_t>(b[1]) << 8) |
         (static_cast<std::uint32_t>(b[2]) << 16) |
         (static_cast<std::uint32_t>(b[3]) << 24);
}

void writeUint16(std::ofstream& os, std::uint16_t v) {
  std::uint8_t b[2];
  b[0] = static_cast<std::uint8_t>(v & 0xFFu);
  b[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
  os.write(reinterpret_cast<const char*>(b), 2);
}

std::uint16_t readUint16(std::ifstream& is, bool& ok) {
  std::uint8_t b[2];
  if (!is.read(reinterpret_cast<char*>(b), 2)) {
    ok = false;
    return 0;
  }
  ok = true;
  return static_cast<std::uint16_t>(b[0]) |
         (static_cast<std::uint16_t>(b[1]) << 8);
}

// ---------- Huffman structures ----------

struct HuffNode {
  std::uint8_t symbol;
  std::uint64_t freq;
  HuffNode* left;
  HuffNode* right;
  bool isLeaf() const { return left == nullptr && right == nullptr; }
};

struct NodePtrCompare {
  bool operator()(const HuffNode* a, const HuffNode* b) const {
    return a->freq > b->freq; // min-heap
  }
};

// Build Huffman tree from frequency table.
// freqs[sym] = frequency, sym 0..255.
HuffNode* buildTree(const std::array<std::uint64_t,256>& freqs) {
  std::priority_queue<HuffNode*, std::vector<HuffNode*>, NodePtrCompare> pq;

  for (std::size_t i = 0; i < 256; ++i) {
    if (freqs[i] > 0) {
      auto* node = new HuffNode{
        static_cast<std::uint8_t>(i),
        freqs[i],
        nullptr,
        nullptr
      };
      pq.push(node);
    }
  }

  // Edge case: file of length 0 → no tree
  if (pq.empty()) {
    return nullptr;
  }

  // Edge case: only one symbol → single-node tree
  if (pq.size() == 1) {
    auto* only = pq.top(); pq.pop();
    // We can just return it; encoding will be 0 bits repeatedly
    return only;
  }

  while (pq.size() > 1) {
    HuffNode* a = pq.top(); pq.pop();
    HuffNode* b = pq.top(); pq.pop();

    auto* parent = new HuffNode{
      0,               // symbol unused for internal nodes
      a->freq + b->freq,
      a,
      b
    };
    pq.push(parent);
  }

  return pq.top();
}

void freeTree(HuffNode* node) {
  if (!node) return;
  freeTree(node->left);
  freeTree(node->right);
  delete node;
}

// Recursively build codes: symbol -> bitstring (as vector<bool>)
void buildCodeTable(HuffNode* node,
                    std::vector<bool>& prefix,
                    std::array<std::vector<bool>,256>& table) {
  if (!node) return;

  if (node->isLeaf()) {
    // Edge case: single symbol, prefix can be empty; assign '0' at least
    if (prefix.empty()) {
      prefix.push_back(false);
    }
    table[node->symbol] = prefix;
    return;
  }

  // Left = 0
  prefix.push_back(false);
  buildCodeTable(node->left, prefix, table);
  prefix.pop_back();

  // Right = 1
  prefix.push_back(true);
  buildCodeTable(node->right, prefix, table);
  prefix.pop_back();
}

// ---------- Bit writer / reader ----------

struct BitWriter {
  std::ofstream& os;
  std::uint8_t current = 0;
  int bitCount = 0;

  explicit BitWriter(std::ofstream& out) : os(out) {}

  void writeBit(bool bit) {
    current <<= 1;
    if (bit) {
      current |= 0x01u;
    }
    ++bitCount;
    if (bitCount == 8) {
      os.put(static_cast<char>(current));
      bitCount = 0;
      current = 0;
    }
  }

  void writeBits(const std::vector<bool>& bits) {
    for (bool b : bits) {
      writeBit(b);
    }
  }

  void flush() {
    if (bitCount > 0) {
      current <<= (8 - bitCount); // pad with zeros
      os.put(static_cast<char>(current));
      bitCount = 0;
      current = 0;
    }
  }
};

struct BitReader {
  std::ifstream& is;
  std::uint8_t current = 0;
  int bitsLeft = 0;

  explicit BitReader(std::ifstream& in) : is(in) {}

  // Returns (ok, bit). ok=false if we hit EOF.
  std::pair<bool, bool> readBit() {
    if (bitsLeft == 0) {
      char c;
      if (!is.get(c)) {
        return {false, false};
      }
      current = static_cast<std::uint8_t>(c);
      bitsLeft = 8;
    }
    bool bit = (current & 0x80u) != 0;
    current <<= 1;
    --bitsLeft;
    return {true, bit};
  }
};

// Derive output path for decompression
std::string deriveOutputPath(const std::string& inPath) {
  const std::string algoExt = ".huff";

  // 1) Strip ".huff" if present
  std::string tmp = inPath;
  if (tmp.size() >= algoExt.size() &&
      tmp.compare(tmp.size() - algoExt.size(), algoExt.size(), algoExt) == 0) {
    tmp.erase(tmp.size() - algoExt.size());  // remove trailing ".huff"
  } else {
    // Fallback: no .huff suffix → just append "_DC"
    return inPath + "_DC";
  }

  // 2) Find original extension (e.g., ".txt")
  auto dotPos = tmp.find_last_of('.');
  if (dotPos == std::string::npos) {
    // No extension: just append "_DC"
    return tmp + "_DC";
  }

  std::string base    = tmp.substr(0, dotPos);  // ".../dickens"
  std::string origExt = tmp.substr(dotPos);     // ".txt"

  // 3) Insert "_DC" before original extension
  return base + "_DC" + origExt;               // ".../dickens_DC.txt"
}


} // namespace

// -------------------- Public API: COMPRESS --------------------

Result huffmanCompressFile(const std::string& inPath) {
  Result r{};

  // Open once for size
  std::ifstream sizeStream(inPath, std::ios::binary | std::ios::ate);
  if (!sizeStream) {
    r.error = -1;
    return r;
  }
  auto size = sizeStream.tellg();
  sizeStream.close();
  if (size < 0) {
    r.error = -1;
    return r;
  }
  r.bytesIn = static_cast<std::uint32_t>(size);

  // Read entire file
  std::ifstream in(inPath, std::ios::binary);
  if (!in) {
    r.error = -1;
    return r;
  }
  std::vector<std::uint8_t> data;
  data.assign(std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>());
  in.close();

  if (data.empty()) {
    // Empty file: just write header with zero symbols
    const std::string outPath = inPath + ".huff";
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) {
      r.error = -2;
      return r;
    }
    // Magic
    out.write("HUF1", 4);
    // original size = 0
    writeUint32(out, 0);
    // symbol count = 0
    writeUint16(out, 0);
    out.close();
    r.bytesOut = 4 + 4 + 2;
    r.error = 0;
    return r;
  }

  // Build frequency table
  std::array<std::uint64_t,256> freqs{};
  freqs.fill(0);
  for (std::uint8_t b : data) {
    freqs[b]++;
  }

  // Build tree
  HuffNode* root = buildTree(freqs);

  // Build code table
  std::array<std::vector<bool>,256> codes;
  std::vector<bool> prefix;
  buildCodeTable(root, prefix, codes);

  // Count number of distinct symbols
  std::uint16_t numSymbols = 0;
  for (std::size_t i = 0; i < 256; ++i) {
    if (freqs[i] > 0) {
      ++numSymbols;
    }
  }

  const std::string outPath = inPath + ".huff";
  std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    freeTree(root);
    r.error = -2;
    return r;
  }

  // ----- Write header -----
  // Magic
  out.write("HUF1", 4);

  // Original size
  writeUint32(out, static_cast<std::uint32_t>(data.size()));

  // Symbol count
  writeUint16(out, numSymbols);

  // For each symbol, write symbol + freq
  for (std::size_t i = 0; i < 256; ++i) {
    if (freqs[i] > 0) {
      std::uint8_t sym = static_cast<std::uint8_t>(i);
      out.put(static_cast<char>(sym));
      writeUint32(out, static_cast<std::uint32_t>(freqs[i]));
    }
  }

  // ----- Write encoded bitstream -----
  BitWriter bw(out);
  for (std::uint8_t b : data) {
    const auto& code = codes[b];
    bw.writeBits(code);
  }
  bw.flush();

  out.flush();
  r.bytesOut = static_cast<std::uint32_t>(out.tellp());
  out.close();

  freeTree(root);
  r.error = 0;
  return r;
}

// -------------------- Public API: DECOMPRESS --------------------

Result huffmanDecompressFile(const std::string& inPath) {
  Result r{};

  std::ifstream in(inPath, std::ios::binary | std::ios::ate);
  if (!in) {
    r.error = -1;
    return r;
  }
  auto fsize = in.tellg();
  if (fsize < 0) {
    r.error = -1;
    return r;
  }
  r.bytesIn = static_cast<std::uint32_t>(fsize);
  in.seekg(0, std::ios::beg);

  // ----- Read header -----
  char magic[4];
  if (!in.read(magic, 4)) {
    r.error = -3;
    return r;
  }
  if (!(magic[0] == 'H' && magic[1] == 'U' && magic[2] == 'F' && magic[3] == '1')) {
    r.error = -3; // not a recognized Huffman format
    return r;
  }

  bool ok = true;
  std::uint32_t origSize = readUint32(in, ok);
  if (!ok) {
    r.error = -3;
    return r;
  }

  std::uint16_t numSymbols = readUint16(in, ok);
  if (!ok) {
    r.error = -3;
    return r;
  }

  std::array<std::uint64_t,256> freqs{};
  freqs.fill(0);

  for (std::uint16_t i = 0; i < numSymbols; ++i) {
    char symChar;
    if (!in.get(symChar)) {
      r.error = -3;
      return r;
    }
    std::uint8_t sym = static_cast<std::uint8_t>(symChar);
    std::uint32_t f = readUint32(in, ok);
    if (!ok) {
      r.error = -3;
      return r;
    }
    freqs[sym] = f;
  }

  // ----- Rebuild tree -----
  HuffNode* root = buildTree(freqs);

  // Edge case: empty file
  if (origSize == 0) {
    const std::string outPath = deriveOutputPath(inPath);
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) {
      freeTree(root);
      r.error = -2;
      return r;
    }
    out.close();
    r.bytesOut = 0;
    r.error = 0;
    freeTree(root);
    return r;
  }

  // If root is nullptr here, something is wrong (non-empty file, zero freqs)
  if (!root) {
    r.error = -3;
    return r;
  }

  // ----- Decode bitstream -----
  BitReader br(in);
  std::vector<std::uint8_t> output;
  output.reserve(origSize);

  while (output.size() < origSize) {
    HuffNode* node = root;
    // Descend until leaf
    while (!node->isLeaf()) {
      auto [hasBit, bit] = br.readBit();
      if (!hasBit) {
        // Ran out of bits before reconstructing originalSize bytes
        freeTree(root);
        r.error = -3;
        return r;
      }
      node = bit ? node->right : node->left;
      if (!node) {
        freeTree(root);
        r.error = -3;
        return r;
      }
    }
    output.push_back(node->symbol);
  }

  freeTree(root);

  // ----- Write output file -----
  const std::string outPath = deriveOutputPath(inPath);
  std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    r.error = -2;
    return r;
  }
  if (!output.empty()) {
    out.write(reinterpret_cast<const char*>(output.data()),
              static_cast<std::streamsize>(output.size()));
  }
  out.close();

  r.bytesOut = static_cast<std::uint32_t>(output.size());
  r.error = 0;
  return r;
}

} // namespace CompressionLib
