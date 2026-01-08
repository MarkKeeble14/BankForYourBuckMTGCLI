#pragma once

#include <vector>
#include <string>

/*
needs to know
 - what sets it is in
 - whatever else
*/
class CardData {
private:
    // Vector of sets
    std::vector<std::string> m_Sets;
public:
    std::string GetSetString();
    void TrackSet(std::string set);

    // Constructors
    CardData() {};

    CardData(std::string set)
    {
        m_Sets.push_back(set);
    }
};