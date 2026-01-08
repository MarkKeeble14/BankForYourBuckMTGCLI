#pragma once

#include <string>
#include <iostream>

#include "CardInformation.h"
#include "Rarity.h"

using namespace std;

// Example:
// 1/287 m_Cards in total
// The card is common (i.e, you have 10 chances to see in a pack)
// 1/287 * 10/14 = chance to find
// Example 2: 
// 1/234 m_Cards in total
// The card is uncommon
// 1/234 * 3/14 = chance to find
void CardInformation::CalcChanceToPull(size_t variantsInSet, size_t cardsInSet) 
{
    m_ChanceToPull = ((double)variantsInSet / cardsInSet) * GetRarityFactor(m_Rarity);
}

double CardInformation::GetRarityFactor(string rarityString)
{
    if (rarityString == "common")
        return COMMON_RARITY_MULT;
    if (rarityString == "uncommon")
        return UNCOMMON_RARITY_MULT;
    if (rarityString == "rare")
        return RARE_RARITY_MULT;
    if (rarityString == "mythic")
        return MYTHIC_RARITY_MULT;
    if (rarityString == "special")
        return SPECIAL_RARITY_MULT;
    else {
        cerr << "Unexpected Rarity encountered: <" + rarityString << ">" << endl;
        return UNEXPECTED_RARITY_MULT;
    }
}
