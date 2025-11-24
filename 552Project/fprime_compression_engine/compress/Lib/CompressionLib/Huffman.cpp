#include "compress/Lib/CompressionLib/Huffman.hpp"

#include <fstream>
#include <queue>
#include <vector>
#include <array>
#include <memory>

// A tiny Huffman just for demo/testing.
// Output file: <input>.huff
// Format (super simple!):
//   [256 * 4 bytes: frequencies as uint32_t]
//   [compressed bitstream]

namespace CompressionLib {

  namespace {
    struct Node {
      std::uint32_t freq;
      int16_t       left;
      int16_t       right;
      std::uint8_t  byte;
      bool          isLeaf;
    };

    struct QItem {
      std::uint32_t freq;
      int16_t       idx;
      bool operator>(const QItem& other) const {
        return freq > other.freq;
      }
    };

    void buildCodes(
        const std::vector<Node>& nodes,
        int16_t idx,
        std::vector<std::vector<bool>>& codes,
        std::vector<bool>& scratch
    ) {
      const Node& n = nodes[idx];
      if (n.isLeaf) {
        codes[n.byte] = scratch;
        return;
      }
      // left = 0
      scratch.push_back(false);
      buildCodes(nodes, n.left, codes, scratch);
      scratch.pop_back();
      // right = 1
      scratch.push_back(true);
      buildCodes(nodes, n.right, codes, scratch);
      scratch.pop_back();
    }

  } // anon

  Result huffmanCompressFile(const std::string& inPath) {
    Result r{};

    // 1) read file
    std::ifstream in(inPath, std::ios::binary);
    if (!in) {
      r.error = -1;
      return r;
    }
    std::vector<std::uint8_t> data(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    in.close();

    r.bytesIn = static_cast<std::uint32_t>(data.size());
    if (data.empty()) {
      // write empty output and return
      std::ofstream out(inPath + ".huff", std::ios::binary);
      out.close();
      r.bytesOut = 0;
      r.error = 0;
      return r;
    }

    // 2) build frequency table
    std::array<std::uint32_t, 256> freq{};
    for (auto b : data) {
      freq[b]++;
    }

    // 3) build tree
    std::vector<Node> nodes;
    nodes.reserve(512);
    std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;

    for (int i = 0; i < 256; ++i) {
      if (freq[i] > 0) {
        Node n{};
        n.freq   = freq[i];
        n.left   = -1;
        n.right  = -1;
        n.byte   = static_cast<std::uint8_t>(i);
        n.isLeaf = true;
        nodes.push_back(n);
        pq.push(QItem{freq[i], static_cast<int16_t>(nodes.size() - 1)});
      }
    }

    if (nodes.size() == 1) {
      // special case: only one symbol
      // write frequencies and then the symbol literally
      std::ofstream out(inPath + ".huff", std::ios::binary);
      if (!out) {
        r.error = -2;
        return r;
      }
      // write freq table
      for (auto f : freq) {
        out.write(reinterpret_cast<const char*>(&f), sizeof(std::uint32_t));
      }
      // write the literal data (no compression)
      out.write(reinterpret_cast<const char*>(data.data()), data.size());
      out.close();
      r.bytesOut = static_cast<std::uint32_t>(256 * 4 + data.size());
      r.error = 0;
      return r;
    }

    while (pq.size() > 1) {
      auto a = pq.top(); pq.pop();
      auto b = pq.top(); pq.pop();
      Node parent{};
      parent.freq   = a.freq + b.freq;
      parent.left   = a.idx;
      parent.right  = b.idx;
      parent.isLeaf = false;
      parent.byte   = 0;
      nodes.push_back(parent);
      pq.push(QItem{parent.freq, static_cast<int16_t>(nodes.size() - 1)});
    }

    const int16_t rootIdx = pq.top().idx;

    // 4) build code table
    std::vector<std::vector<bool>> codes(256);
    std::vector<bool> scratch;
    buildCodes(nodes, rootIdx, codes, scratch);

    // 5) write output
    std::ofstream out(inPath + ".huff", std::ios::binary);
    if (!out) {
      r.error = -3;
      return r;
    }

    // write freq table (so we can decode later)
    for (auto f : freq) {
      out.write(reinterpret_cast<const char*>(&f), sizeof(std::uint32_t));
    }

    // pack bits
    std::uint8_t currentByte = 0;
    int bitCount = 0;
    for (auto b : data) {
      const auto& code = codes[b];
      for (bool bit : code) {
        currentByte <<= 1;
        if (bit) {
          currentByte |= 1;
        }
        bitCount++;
        if (bitCount == 8) {
          out.put(static_cast<char>(currentByte));
          currentByte = 0;
          bitCount = 0;
        }
      }
    }
    // flush
    if (bitCount > 0) {
      currentByte <<= (8 - bitCount);
      out.put(static_cast<char>(currentByte));
    }

    out.close();

    // compute bytesOut
    std::ifstream outCheck(inPath + ".huff", std::ios::binary | std::ios::ate);
    r.bytesOut = static_cast<std::uint32_t>(outCheck.tellg());
    outCheck.close();

    r.error = 0;
    return r;
  }

} // namespace CompressionLib
