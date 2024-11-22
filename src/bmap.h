//  ____   __  __    _    ____
// | __ ) |  \/  |  / \  |  _  \  Bmap parser for Modern C++
// |  _ \ | |\/| | / _ \ | |_)  | version 1.0.0
// | |_) || |  | |/ ___ \|  __ /  https://github.com/theswiftfox/bmap-cpp
// |____/ |_|  |_/_/   \_\_|

// SPDX-FileCopyrightText: 2023 Elena Gantner <https://github.com/theswiftfox>
// SPDX-License-Identifier: MIT

#ifndef BMAP_H
#define BMAP_H

#define BMAP_COPY_DEBUG_PRINT

#if defined BMAP_DEBUG_PRINT || defined BMAP_COPY_DEBUG_PRINT
#include <iostream>
#endif

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <tinyxml2.h>
#include <unistd.h>

#ifdef USE_GCC_COMPAT
// compatibility for std::format if GCC < 13.
// IWYU pragma is there because it doesn't recognize the format function
#include "format_gcc12_compat.h" // IWYU pragma: keep
#else
#include <format>
#endif

constexpr const size_t MAX_BUF_SIZE = 4 * 1024 * 1024 * 2;

namespace xml {
template <typename T>
concept Parseable = requires(const tinyxml2::XMLElement *elem) {
    { T::parse(elem) };
};

template <typename T,
          typename = typename std::enable_if_t<std::is_arithmetic_v<T>, T>>
T value(const tinyxml2::XMLElement *elem) {
    if (elem == nullptr)
        throw std::runtime_error("Element is null");
    std::stringstream strm(elem->GetText());
    T res;
    strm >> res;
    if (strm.fail())
        throw std::runtime_error(std::format(
            "Failed to convert element to type {}", typeid(T).name()));
    return res;
}

template <typename T, typename = typename std::enable_if_t<
                          std::is_same_v<T, std::string>, T>>
std::string value(const tinyxml2::XMLElement *elem) {
    return std::string(elem->GetText());
}

template <Parseable T> T value(const tinyxml2::XMLElement *elem) {
    std::string text(elem->GetText());
    return T::parse(text);
}

template <typename T>
T valueFromSimpleXPath(const tinyxml2::XMLElement *root,
                       const std::string_view &xPath) {
    auto elem = root;
    for (const auto subpath :
         xPath | std::views::split('/') | std::views::transform([](auto word) {
             return std::string(word.data(), word.size());
         })) {
        elem = elem->FirstChildElement(subpath.data());
        if (elem == nullptr)
            throw std::runtime_error(std::format(
                "Unable to evaluate XPath. No child found with name {}",
                subpath.data()));
    }

    return value<T>(elem);
}
} // namespace xml

namespace bmap {

struct Range {
    size_t offset;
    size_t blockCount;
    std::string checksum;

    static Range parse(const tinyxml2::XMLElement *elem) {
        if (!elem)
            throw std::runtime_error("Cannot initialize Range from NULL!");

        const auto stringVal = std::string(elem->GetText());

        const auto to_sizeT = [](std::string in) {
            std::stringstream strm(in);
            size_t out;
            strm >> out;
            return out;
        };

        size_t start = 0;
        size_t len = 1;
        if (stringVal.find('-') == std::string::npos) {
            start = to_sizeT(stringVal);
        } else {
            auto range = stringVal | std::views::split('-') |
                         std::views::transform([](auto rng) {
                             return std::string(rng.data(), rng.size());
                         });
            auto it = range.begin();
            start = to_sizeT(*it);
            len = to_sizeT(*(++it)) - start + 1;
        }

        const auto checksum = elem->Attribute("chksum");

        return Range{
            start,
            len,
            std::string(checksum),
        };
    }
};

struct BmapFile {
    size_t imageSize;
    size_t blockSize;
    size_t blocksCount;
    size_t mappedBlocksCount;

    std::string checksumType;
    std::string checksum;

    std::vector<Range> blockMap;

    static BmapFile from_xml(const std::string &xmlPath) {
        if (!std::filesystem::exists(xmlPath)) {
            throw std::runtime_error(std::format(
                "File not found! Path {} does not exist.", xmlPath));
        }
        std::ifstream file(xmlPath, std::ios::in);
        std::vector<char> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

        return from_xml_data(data);
    }

    static BmapFile from_xml_data(const std::vector<char> &bytes) {
        if (bytes.empty()) {
            throw std::runtime_error("bmap file is empty!");
        }

        tinyxml2::XMLDocument doc;
        doc.Parse(bytes.data(), bytes.size());

        const auto root = doc.RootElement();

        const auto imageSize =
            xml::valueFromSimpleXPath<size_t>(root, "ImageSize");
        const auto blockSize =
            xml::valueFromSimpleXPath<size_t>(root, "BlockSize");
        const auto blocksCount =
            xml::valueFromSimpleXPath<size_t>(root, "BlocksCount");
        const auto mappedBlocksCount =
            xml::valueFromSimpleXPath<size_t>(root, "MappedBlocksCount");

        const auto chcksmType =
            xml::valueFromSimpleXPath<std::string>(root, "ChecksumType");
        const auto chcksum =
            xml::valueFromSimpleXPath<std::string>(root, "BmapFileChecksum");

        std::vector<Range> blockMap;
        const auto blockMapElem = root->FirstChildElement("BlockMap");
        if (blockMapElem) {
            for (auto rangeElem = blockMapElem->FirstChildElement("Range");
                 rangeElem != nullptr;
                 rangeElem = rangeElem->NextSiblingElement()) {
                blockMap.push_back(Range::parse(rangeElem));
            }
        }

        return BmapFile{imageSize,  blockSize, blocksCount, mappedBlocksCount,
                        chcksmType, chcksum,   blockMap};
    }

#ifdef BMAP_DEBUG_PRINT
    void print() const {
        std::cout << "Bmap: \n"
                  << "  imageSize: " << imageSize << "\n"
                  << "  blockSize: " << blockSize << "\n"
                  << "  blocks: " << blocksCount << "\n"
                  << "  mappedBlocks: " << mappedBlocksCount << "\n"
                  << "  checksumType: " << checksumType << "\n"
                  << "  checksum: " << checksum << "\n"
                  << "  BlockMap: \n";

        for (const auto range : blockMap) {
            std::cout << "   Range:"
                         "    offset: "
                      << range.offset
                      << "\n"
                         "    blocks: "
                      << range.blockCount
                      << "\n"
                         "    checksum: "
                      << range.checksum << "\n";
        }
        std::cout << std::endl;
    }
#endif
};

struct Progress {
    const size_t mappedBlocks;
    size_t blocksWritten;

    uint8_t percent() const {
        return uint8_t(100.0f / (float)mappedBlocks * (float)blocksWritten);
    }
};

typedef std::function<void(const Progress &)> ProgressCallback;

inline void copy(const std::string &wicPath, const std::string &targetDisk,
                 const ProgressCallback &callback = nullptr) {
    if (!wicPath.ends_with(".wic") && !wicPath.ends_with("wic.gz")) {
        throw std::runtime_error(
            std::format("Expected '.wic' or '.wic.gz' got '{}'", wicPath));
    }

    // todo: support wic.gz
    if (wicPath.ends_with("wic.gz")) {
        throw std::runtime_error("Compressed wic files are currently not supported :(");
    }

    if (!std::filesystem::exists(wicPath)) {
        throw std::runtime_error("wic file not found");
    }
    if (!std::filesystem::exists(targetDisk)) {
        throw std::runtime_error("target disk not found");
    }

    const auto wicfilepath = std::filesystem::path(wicPath);
    // const auto wicfileWithoutStem =
    //     std::string(wicPath).replace(wicPath.size() - 4, 4, "");
    const auto bmapFilePath =
        wicfilepath.parent_path() / std::format("{}.bmap", wicPath);

#ifdef BMAP_COPY_DEBUG_PRINT
    std::cout << "Found .bmap file: " << bmapFilePath << std::endl;
#endif

    const auto bmapFile = BmapFile::from_xml(bmapFilePath);

    // todo: validate checksum

#ifdef BMAP_COPY_DEBUG_PRINT
    std::cout << "Image Size: " << bmapFile.imageSize << " Blocks" << std::endl;
#endif

    auto progress = Progress{bmapFile.mappedBlocksCount, 0};

    std::ifstream wicFile(wicfilepath, std::ios::in | std::ios::binary);

    // std::ofstream blockDevice(targetDisk, std::ios::out | std::ios::binary);
    auto blockDevice = std::fopen(targetDisk.c_str(), "w+b");
    if (blockDevice == nullptr) {
        throw std::runtime_error(
            std::format("Unable to open block device {} for writing. Maybe "
                        "missing permissions?",
                        targetDisk));
    }

    const auto blockFd = fileno(blockDevice);

    std::vector<uint8_t> buff(
        std::min(bmapFile.blockSize * 1024 * 2, MAX_BUF_SIZE));
    char *const buffPtrChar = reinterpret_cast<char *>(buff.data());
    const auto bufferMaxBlocks = buff.size() / bmapFile.blockSize;
    for (const auto &range : bmapFile.blockMap) {
        if (range.offset > progress.blocksWritten) {
            const auto seekOffs = range.offset - progress.blocksWritten;
            // move outp if not aligned with range
            std::fseek(blockDevice, seekOffs, SEEK_CUR);
            wicFile.seekg(seekOffs, std::ios_base::cur); // same for inp
        }

        for (auto blockCount = range.blockCount; blockCount > 0;) {
            const auto readBlocks =
                bufferMaxBlocks < blockCount ? bufferMaxBlocks : blockCount;
            const auto byteCount = readBlocks * bmapFile.blockSize;
            wicFile.read(buffPtrChar, byteCount);
            // blockDevice.write(buffPtrChar, byteCount);
            std::fwrite(buff.data(), sizeof(uint8_t), byteCount, blockDevice);

            progress.blocksWritten += readBlocks;

            if (callback) {
                callback(progress);
            }

            blockCount -= readBlocks;
        }
        fsync(blockFd);
#ifdef BMAP_COPY_DEBUG_PRINT
        std::cout << "Blocks written: " << progress.blocksWritten
                  << " (" << progress.percent() << "%)"
                  << " Remaining: "
                  << bmapFile.mappedBlocksCount - progress.blocksWritten
                  << std::endl;
#endif
    }

    std::fclose(blockDevice);
    wicFile.close();

#ifdef BMAP_COPY_DEBUG_PRINT
    std::cout << "Copy done." << std::endl;
#endif
}

} // namespace bmap

#endif