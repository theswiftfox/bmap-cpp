# Bmap-CPP

Header only Bmap parser leveraging tinyxml2.

## Usage
Copy bmap.h into your project and get ready to read bmaps! :)

## Coming Soon
Implementation of bmaptools copy to actually make use of the parsed bmap.

## Example

```cpp
#include <iostream>

#include "tinyxml2.h"

#define BMAP_DEBUG_PRINT // enable the print function of the BmapFile struct
#include "bmap.h"

constexpr static const char* bmapfile = 
R"(
<?xml version="1.0" ?>
<bmap version="2.0">
...
)";

int main() {
    tinyxml2::XMLDocument doc;
    doc.Parse(bmapfile);

    const auto rootElem = doc.RootElement();

    const auto bmapFile = bmap::BmapFile::parse(rootElem);

    std::cout << "Parse ok!" << std::endl;
    bmapFile.print();

    return 0;
}

```