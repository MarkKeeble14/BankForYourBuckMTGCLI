#include <ostream>
#include <iostream>
#include <CardInformation.h>
#include <CardSetData.h>
#include <Rarity.h>

using namespace std;

void CardSetData::TrackCard(CardInformation* card)
{
    m_Cards.push_back(card);
}

double CardSetData::GetChanceToPull()
{
    double chance = 0;
    for (auto& c : this->m_Cards)
    {
        chance += c->GetChanceToPull();
    }
    return chance;
}

vector<CardInformation*> CardSetData::GetSpecificCards(string str)
{
    vector<CardInformation*> vec;
    for (auto& d : m_Cards)
    {
        if (d->GetName() == str)
        {
            vec.push_back(d);
        }
    }
    return vec;
}

size_t CardSetData::GetSpecificCardCount(string str)
{
    return GetSpecificCards(str).size();
}