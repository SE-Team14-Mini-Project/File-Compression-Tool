
#include "interfaces.h"
#include <istream>

namespace team14 {

FrequencyTable buildFrequencyTable(std::istream& input) {
    FrequencyTable frequencies{};

    char byte;

    while (input.get(byte)) {
        unsigned char symbol = byte;
        frequencies[symbol]++;
    }

    return frequencies;
}

} // namespace team14
