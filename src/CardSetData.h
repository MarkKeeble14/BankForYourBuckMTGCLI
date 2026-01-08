#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <Rarity.h>
#include <CardInformation.h>

using namespace std;

class CardSetData {
private:
    // Vector of sets
    vector<CardInformation*> m_Cards;

    string m_SetName;

    // The total number of m_Cards in the m_Set
    size_t m_TotalCardsInSet = 1;
public:
    CardSetData() {};

    void SetTotalCardsInSet(size_t totalCardsInSet) { this->m_TotalCardsInSet = totalCardsInSet; }

    void SetSetName(string setName) { this->m_SetName = setName; }

    size_t GetTotalCardsInSet() { return m_TotalCardsInSet; }

    size_t GetTotalCardCount() { return m_Cards.size(); }

    size_t GetSpecificCardCount(string str);

    vector<CardInformation*> GetCards() { return m_Cards; }

    vector<CardInformation*> GetSpecificCards(string str);

    void TrackCard(CardInformation*);

    double GetChanceToPull();

    friend std::ostream& operator << (std::ostream& os, const CardSetData& setData);
};

inline std::ostream& operator << (std::ostream& os, const CardSetData& setData) {
    os << setData.m_SetName << " - (" + to_string(setData.m_Cards.size()) + "/" + to_string(setData.m_TotalCardsInSet) << "), Cards:\n[\n";
    
    for (auto& c : setData.m_Cards)
    {
        cout << *c << endl;
    }
    
    cout << "]";
    return os;
}