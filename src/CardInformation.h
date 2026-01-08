#pragma once

#include <iomanip>
#include <ostream>
#include <string>

using namespace std;

class CardInformation {
private:
    string m_Name;
    string m_Set;
    string m_SetName;
    string m_Rarity;
    double m_ChanceToPull = 0;

    double GetRarityFactor(string rarityString);
public:
    CardInformation() {};
    CardInformation(string name, string set, string set_name,string rarity)
    {
        this->m_Name = name;
        this->m_Set = set;
        this->m_SetName = set_name;
        this->m_Rarity = rarity;
        this->m_ChanceToPull = 0;
    }

    string GetName() { return m_Name; }
    string GetRarity() { return m_Rarity; }
    string GetSet() { return m_Set; }
    double GetChanceToPull() { return m_ChanceToPull; }

    void CalcChanceToPull(size_t variantsInSet, size_t cardsInSet);

    friend ostream& operator << (ostream& os, const CardInformation& card);
};

inline ostream& operator << (ostream& os, const CardInformation& card) {
    os << card.m_Name + " - " + card.m_SetName + "[" + card.m_Set + "]: " 
        + card.m_Rarity << " (" << setprecision(5) << (card.m_ChanceToPull*100) << "%)";
    return os;
}