//  ____   __  __    _    ____
// | __ ) |  \/  |  / \  |  _  \  Bmap parser for Modern C++
// |  _ \ | |\/| | / _ \ | |_)  | version 1.0.0
// | |_) || |  | |/ ___ \|  __ /  https://github.com/theswiftfox/bmap-cpp
// |____/ |_|  |_/_/   \_\_|

// SPDX-FileCopyrightText: 2023 Elena Gantner <https://github.com/theswiftfox>
// SPDX-License-Identifier: MIT

#ifndef BMAP_H
#define BMAP_H

#ifdef BMAP_DEBUG_PRINT
#include <iostream>
#endif

#include <cstddef>
#include <format>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <tinyxml2.h>

namespace xml {
    template<typename T>
    concept Parseable = requires(const tinyxml2::XMLElement* elem) {
        { T::parse(elem) };
    };

    template<
        typename T, 
        typename = typename std::enable_if_t<std::is_arithmetic_v<T>, T>>
    T value(const tinyxml2::XMLElement* elem) {
        if (elem == nullptr) throw std::runtime_error("Element is null");
        std::stringstream strm(elem->GetText());
        T res;
        strm >> res;
        if (strm.fail()) throw std::runtime_error(std::format("Failed to convert element to type {}", typeid(T).name()));
        return res;
    }

    template<
        typename T,
        typename = typename std::enable_if_t<std::is_same_v<T, std::string>, T>>
    std::string value(const tinyxml2::XMLElement* elem) {
        return std::string(elem->GetText());
    }

    template<Parseable T> T value(const tinyxml2::XMLElement* elem) {
        std::string text(elem->GetText());
        return T::parse(text);
    }

    template<typename T> T valueFromSimpleXPath(const tinyxml2::XMLElement* root, const std::string_view& xPath) {
        auto elem = root;
        for (const auto subpath: xPath | 
                    std::views::split('/') | 
                    std::views::transform([](auto word) { return std::string(word.data(), word.size());})) {
            elem = elem->FirstChildElement(subpath.data());
            if (elem == nullptr) throw std::runtime_error(std::format("Unable to evaluate XPath. No child found with name {}", subpath.data()));
        }

        return value<T>(elem);
    }
}

namespace bmap {

struct Range {
    size_t offset;
    size_t blockCount;
    std::string checksum;

    static Range parse(const tinyxml2::XMLElement* elem) {
        if (!elem) throw std::runtime_error("Cannot initialize Range from NULL!");

        const auto stringVal = std::string(elem->GetText());

        const auto to_sizeT = [](std::string in) { 
            std::stringstream strm(in);
            size_t out;
            strm >> out;
            return out;
        };

        auto range = stringVal | std::views::split('-') | std::views::transform([](auto rng) { return std::string(rng.data(), rng.size()); });
        auto it = range.begin();
        const auto start = to_sizeT(*it);
        const auto len = (it != range.end() ? to_sizeT(*(++it)) - start : 0) +1;

        const auto checksum = elem->Attribute("chksum");
        
        return Range {
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

    static BmapFile parse(const tinyxml2::XMLElement* root) {
        const auto imageSize = xml::valueFromSimpleXPath<size_t>(root, "ImageSize");
        const auto blockSize = xml::valueFromSimpleXPath<size_t>(root, "BlockSize");
        const auto blocksCount = xml::valueFromSimpleXPath<size_t>(root, "BlocksCount");
        const auto mappedBlocksCount = xml::valueFromSimpleXPath<size_t>(root, "MappedBlocksCount");
        
        const auto chcksmType = xml::valueFromSimpleXPath<std::string>(root, "ChecksumType");
        const auto chcksum = xml::valueFromSimpleXPath<std::string>(root, "BmapFileChecksum");
    
        std::vector<Range> blockMap;
        const auto blockMapElem = root->FirstChildElement("BlockMap");
        if (blockMapElem) {
            for (auto rangeElem = blockMapElem->FirstChildElement("Range"); rangeElem != nullptr; rangeElem = rangeElem->NextSiblingElement()) {
                blockMap.push_back(
                    Range::parse(rangeElem)
                );
            }
        }

        return BmapFile {
            imageSize,
            blockSize,
            blocksCount,
            mappedBlocksCount,
            chcksmType,
            chcksum,
            blockMap
        };
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
            std::cout <<
                "   Range:"
                "    offset: " << range.offset << "\n"
                "    blocks: " << range.blockCount << "\n"
                "    checksum: " << range.checksum << "\n";
        }
        std::cout << std::endl;
    }
#endif
};

}

#endif