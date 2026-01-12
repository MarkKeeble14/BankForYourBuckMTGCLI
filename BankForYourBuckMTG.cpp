// BangForYourBuckMTG.cpp : Defines the entry point for the application.
//

#include <nlohmann/json.hpp>
#include <curl.h>

#include <CardSetData.h>
#include <CardInformation.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>

namespace fs = filesystem;
using json = nlohmann::json;

using namespace std;

// Settings
const int FORCED_MILLISECONDS_BTW_API_CALLS = 100;
const size_t MIN_CARDS_TO_CONSIDER_SET = 2;

// String consts
const string verboseOptionString = "Verbose";
const string filterBasicsOptionString = "Filter Basic Lands from File";
const string LINE_ACROSS = "__________________________________________________\n";

// Map used when percent encoding a query for use in a URL
map<char, string> PERCENT_ENCODING_MAP =
{
    { ' ', "+" },
    { '!', "%21" },
    { '#', "%23" },
    { '$', "%24" },
    { '&', "%26" },
    { '\'', "%27" },
    { '(', "%28" },
    { ')', "%29" },
    { '*', "%2A" },
    { '+', "%2B" },
    { ',', "%2C" },
    { '/', "%2F" },
    { ':', "%3A" },
    { ';', "%3B" },
    { '=', "%3D" },
    { '?', "%3F" },
    { '@', "%40" },
    { '[', "%5B" },
    { ']', "%5D" }
};

// Helper func for performing a GET request
size_t WriteFunction(void* ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

// Performs a GET request
pair<CURLcode, string> GetReq(string url, curl_slist* headers, CURL* curl, bool print)
{
    string response_string;
    CURLcode result;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (print)
        cout << "GET req to: " << url << endl;

    // Perform the request
    result = curl_easy_perform(curl);

    // Force a delay as per API rules
    this_thread::sleep_for(chrono::milliseconds(FORCED_MILLISECONDS_BTW_API_CALLS));

    if (result != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
    }
    else
    {
        return pair<CURLcode, string>(result, response_string);
    }

    return pair<CURLcode, string>(result, "");
}

// Parses Card data from scryfall JSON string
CardInformation ParseCardInfoFromJson(string json)
{
    auto data = json::parse(json);
    return CardInformation(data["name"], data["set"], data["set_name"], data["rarity"]);
}

// Percent encode a query for use in a RESTful API call
string PercentEncodeSearchQuery(string str)
{
    string res = "";
    for (size_t i = 0; i < str.length(); i++)
    {
        if (PERCENT_ENCODING_MAP.count(str[i]) == 1)
            res += PERCENT_ENCODING_MAP[str[i]];
        else
            res += str[i];
    }
    return res;
}

// Returns true if toFind is found within findIn
bool TextContains(string findIn, string toFind)
{
    return findIn.find(toFind) != string::npos;
}

// Converts a string to uppercase and returns it
string ToLower(string s)
{
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Checks if toFind is found within vector vec
// Will be case agnostic if case sensitive is false
bool StringContainedInVector(vector<string> vec, string toFind, bool caseSensitive)
{
    toFind = ToLower(toFind);
    if (caseSensitive)
    {
        return find(vec.begin(), vec.end(), toFind) != vec.end();
    }
    else
    {
        for (auto& d : vec)
        {
            if (ToLower(d) == toFind)
            {
                return true;
            }
        }
        return false;
    }
}

// Checks if str contains a basic land type and return the result
bool IsBasicLand(string str)
{
    return TextContains(str, "Forest")
        || TextContains(str, "Mountain")
        || TextContains(str, "Swamp")
        || TextContains(str, "Island")
        || TextContains(str, "Plains");
}

// After a prompt, reads a token from command line returns it
string PromptSingleTokenCLI(string prompt)
{
    string in;
    if (prompt.length() > 0)
        cout << prompt << endl;
    cin >> in;
    return in;
}

// After a prompt, reads lines until a specific 'terminal token' is encountered
// Returns a vector consisting of all lines read
vector<string> PromptEndlessTokensCLI(string firstPrompt, string repeatPrompt, string terminationToken)
{
    vector<string> tokens;
    string in;
    string terminator = ToLower(terminationToken);

    if (firstPrompt.length() > 0)
        cout << firstPrompt << endl;

    while (getline(cin, in))
    {
        // Break upon hitting termination string
        if (ToLower(in) == terminator)
            break;

        cout << repeatPrompt;

        if (in.length() > 0)
            tokens.push_back(in);
    }

    return tokens;
}

// Prompts the user providing them a m_Set of acceptable responses
string PromptCLI(string prompt, map<string, pair<int, string>> acceptedResponses)
{
    string in;
    if (prompt.length() > 0)
        cout << prompt << endl;

    for (auto& d : acceptedResponses)
    {
        cout << d.second.first << ": " << d.second.second << endl;
    }

    cout << ">";

    // Fetch input until we have an allowable mode
    while (true)
    {
        cin >> in;

        // If the provided input is not accepted, print options and continue the loop
        if (acceptedResponses.count(in) != 1)
        {
            cout << LINE_ACROSS << endl;
            cout << "Invalid Input (" << in << "), accepted responses are: " << endl;
            for (auto& d : acceptedResponses)
            {
                cout << d.second.first << ": " << d.second.second << endl;
            }
            cout << ">";
            continue;
        }
        // Input is acceptable
        return in;
    }
}

// Performs a query based upon precon data
// Determines which precon contains the most m_Cards from interested m_Cards
// Verbose determines how much is printed
void RoutinePreconDataQuery(vector<string> interestedCards, bool verbose)
{
    // Iterate through all precon source files
    string path = ".\\libs\\CommanderPrecons\\precon_json";
    map<string, vector<CardInformation*>> results;

    for (auto& entry : fs::directory_iterator(path))
    {
        // Read from file
        ifstream file(entry.path());

        // If failed to open file, fail further
        if (!file)
        {
            cerr << "Failed to open file at path: " << entry.path() << endl;
            continue;
        }

        // For each one, parse the json
        auto data = json::parse(file);

        if (verbose)
        {
            cout << "Analyzing: " << data["name"] << endl;
        }

        // Add list for tracking results
        results[data["name"]] = {};

        // Read card m_Name
        string cardName = data["main"]["name"];

        // If the commander is one of the interested m_Cards, track that
        // "main" = commander
        if (StringContainedInVector(interestedCards, cardName, false))
        {
            // Make the card
            CardInformation* card = new CardInformation(cardName,
                data["main"]["set"],
                data["main"]["set_name"],
                data["main"]["rarity"]);

            results[data["name"]].push_back(card);

            if (verbose)
            {
                cout << data["name"] << " has: " << *card << endl;
            }
        }

        // Determine how many of the interested m_Cards are included and store that data in a map
        // "mainboard" = 99
        for (auto& d : data["mainboard"])
        {
            // Read card m_Name
            cardName = d["card"]["name"];
            if (StringContainedInVector(interestedCards, cardName, false))
            {
                // Make the card
                CardInformation* card = new CardInformation(cardName,
                    d["card"]["set"],
                    d["card"]["set_name"],
                    d["card"]["rarity"]);

                results[data["name"]].push_back(card);

                if (verbose)
                {
                    cout << data["name"] << " has: " << card->GetName() << " (" << card->GetRarity() << ")" << endl;
                }
            }
        }
    }

    cout << LINE_ACROSS << endl;

    // Eventually print the map results
    // Copy card m_Set data into a vector for easy sorting
    vector<pair<string, vector<CardInformation*>>> resultsDataVec;
    for (auto& d : results)
    {
        resultsDataVec.push_back(d);
    }
    // Sort
    sort(resultsDataVec.begin(), resultsDataVec.end(), [=](pair<string, vector<CardInformation*>>& a, pair<string, vector<CardInformation*>>& b)
        {
            return a.second.size() > b.second.size();
        });

    cout << "Best Precons to Purchase: " << endl;

    if (resultsDataVec.size() == 0)
    {
        cout << "The specified cards could not be found within any precons" << endl;
        return;
    }

    // Print
    for (auto& d : resultsDataVec)
    {
        if (d.second.size() > 0)
        {
            cout << endl << d.first << ": " << d.second.size() << "/" << interestedCards.size() << endl << "[" << endl;
            for (auto& c : d.second)
            {
                cout << c->GetName() << endl;
            }
            cout << "]" << endl;
        }
    }
}

// Performs a query based upon general m_Set data
// Determines which sets have the highest chance of finding you m_Cards named in interestedCards
// Verbose determines how much is printed
void RoutineGeneralSetDataQuery(vector<string> interestedCards, bool verbose)
{
    // Contains a vector of all the m_Cards pulled
    vector<CardInformation*> cards;
    vector<string> sets;

    // Contains information regarding each m_Set
    map<string, CardSetData> cardSetDataMap;

    string url;
    string searchedCard;

    // Init Curl
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    auto curl = curl_easy_init();

    // Setting up request headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: MTGPackToPurchase/1.0");
    headers = curl_slist_append(headers, "Accept: */*");

    // Make sure curl was properly initialized
    if (curl) {
        // iterating over all the m_Cards we wish to know about
        for (auto& s : interestedCards)
        {
            // Reset containers
            sets.clear();
            cards.clear();

            // Set card to search
            searchedCard = s;

            // GET : Search for a card m_Name
            url = "https://api.scryfall.com/cards/search?q=" + PercentEncodeSearchQuery(searchedCard) + "&unique=prints";
            pair<CURLcode, string> reqResponse = GetReq(url.c_str(), headers, curl, verbose);

            // If request failed for some reason, print err and move to next card
            if (reqResponse.first != CURLE_OK)
            {
                cerr << reqResponse.second << endl;
                continue;
            }

            // If request was successful, we process the result
            auto data = json::parse(reqResponse.second);

            if (verbose)
            {
                cout << data["data"].size() << " results returned for: " << searchedCard << endl;

                for (auto& d : data["data"])
                {
                    cout << d["set"] << endl;
                }

                cout << LINE_ACROSS << endl;
            }

            // Iterate over all card results - store in m_Cards vector
            for (auto& c : data["data"])
            {
                // Make the card
                CardInformation* card = new CardInformation(c["name"], c["set"], c["set_name"], c["rarity"]);

                // If there has already been a print in the m_Set
                // We want to only track this card if it has a different m_Rarity than what already exists to filter duplicates
                if (cardSetDataMap.count(c["set"]) == 1)
                {
                    // Add the card
                    cards.push_back(card);

                    cardSetDataMap[c["set"]].TrackCard(card);

                    if (verbose)
                    {
                        cout << "Already tracked set <" << c["set"] << ">" << endl;
                    }
                }
                else if (cardSetDataMap.count(c["set"]) == 0) // If this m_Set has not been added to the map, add it
                {
                    cards.push_back(card);
                    sets.push_back(c["set"]);

                    // Make a new CardSetData and track it
                    CardSetData d = CardSetData();
                    d.TrackCard(card);
                    cardSetDataMap[c["set"]] = d;
                }
            }

            // Iterate over all sets card occurs in and do a GET request for information
            // Do a GET request for each m_Set and populate map with:
            // - key=m_Set
            // - value contains the number of m_Cards in the m_Set and the m_Cards m_Rarity within that m_Set
            for (auto& set : sets)
            {
                url = "https://api.scryfall.com/sets/" + set;
                pair<CURLcode, string> reqResponse = GetReq(url.c_str(), headers, curl, verbose);

                // If request failed for some reason, print err and move to next card
                if (reqResponse.first != CURLE_OK)
                {
                    cerr << reqResponse.second << endl;
                    continue;
                }

                // If request was successful, we process the result
                auto data = json::parse(reqResponse.second);
                cardSetDataMap[set].SetTotalCardsInSet(data["card_count"]);
                cardSetDataMap[set].SetSetName(data["name"]);
            }

            // For each card, we now have to calculate the chance to pull it based on the m_Set data we know
            CardSetData setData;
            for (auto& card : cards)
            {
                setData = cardSetDataMap[card->GetSet()];

                // Calculate the chance to pull
                card->CalcChanceToPull(setData.GetTotalCardCount(), setData.GetTotalCardsInSet());
            }

            if (verbose)
            {
                cout << LINE_ACROSS << endl;
            }

            // Sort the m_Cards
            sort(cards.begin(), cards.end(), [=](CardInformation* c1, CardInformation* c2)
                {
                    return c1->GetChanceToPull() > c2->GetChanceToPull();
                });

            // With that, we should have all the information needed
            size_t cardCount;
            size_t totalCardsInSet;
            for (auto& card : cards)
            {
                // Set some variables
                setData = cardSetDataMap[card->GetSet()];
                cardCount = setData.GetSpecificCardCount(card->GetName());
                totalCardsInSet = setData.GetTotalCardsInSet();

                // Print results
                if (verbose)
                {
                    cout << *card << " (" << cardCount << "/" << totalCardsInSet << ")" << endl;
                }
            }

            if (verbose)
            {
                cout << LINE_ACROSS << endl;
            }
        }

        // Copy card m_Set data into a vector for easy sorting
        vector<pair<string, CardSetData>> setDataVec;
        for (auto& d : cardSetDataMap)
        {
            setDataVec.push_back(d);
        }
        // Sort
        sort(setDataVec.begin(), setDataVec.end(), [=](pair<string, CardSetData>& a, pair<string, CardSetData>& b)
            {
                return a.second.GetChanceToPull() > b.second.GetChanceToPull();
            });

        cout << "Best Packs to Purchase: " << endl;

        // Print setDataVec
        // This should represent the sets that offer the highest chance of pulling one of the m_Cards we are interested in
        double chanceToPull;
        for (auto& d : setDataVec)
        {
            // If the total m_Cards in the m_Set is less than MIN_CARDS_TO_CONSIDER_SET, the assumption is that
            // it is not a purchasable m_Set/booster pack and we will exclude from results
            if (d.second.GetTotalCardsInSet() < MIN_CARDS_TO_CONSIDER_SET) continue;

            // Calc chance to pull
            chanceToPull = d.second.GetChanceToPull() * 100;

            // Print
            cout << endl << d.first << ": " << d.second << " - Total: " << chanceToPull
                << "%, equivalent to " << setprecision(3) << (100 / chanceToPull) << " packs" << endl;
        }

        // Cleanup
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        curl = NULL;
    }
}

// Reads m_Cards from the command line and returns a vector of the results
vector<string> RoutineReadCardsFromCommandLine()
{
    vector<string> interestedCards = PromptEndlessTokensCLI("Enter Cards Line by Line - Enter 'Go' when Finished", ">", "GO");

    cout << LINE_ACROSS << endl;
    cout << "Cards: " << endl;

    size_t id = 0;
    for (auto& d : interestedCards)
    {
        cout << ++id << ": " << d << endl;
    }

    cout << LINE_ACROSS << endl;

    return interestedCards;
}

// Reads m_Cards from a specified file and returns a vector of the results
pair<int, vector<string>> RoutineReadCardsFromFile(bool filterBasicLands, bool promptForFilePath)
{
    vector<string> interestedCards;

    string in = ".\\cards_list.txt";
    if (promptForFilePath)
    {
        in = PromptSingleTokenCLI("Enter File Path: ");
    }

    // Read from file
    ifstream file(in);

    // If failed to open file, fail further
    if (!file)
    {
        cerr << "Failed to open file specified to be at: " << in << endl;
        cout << LINE_ACROSS << endl;
        return { -1, {} };
    }

    cout << "Cards: " << endl;

    cout << LINE_ACROSS << endl;

    // Otherwise, read file and store m_Cards to search for later
    string content;
    string cleaned;
    size_t id = 0;
    while (getline(file, content))
    {
        cleaned = content.substr(2, content.length());
        if (IsBasicLand(cleaned))
        {
            cout << id << ": Skipping Basic Land: " << cleaned << endl;
            continue;
        }

        interestedCards.push_back(cleaned);
        cout << ++id << ": " << interestedCards[interestedCards.size() - 1] << endl;
    }

    return { 0, interestedCards };
}

// The whole routine to read m_Cards
vector<string> RoutineReadCards(bool filterBasicLands)
{
    pair<int, vector<string>> res;
    map<string, pair<int, string>> modeMap = { {"1", {1, "Command Line"}}, {"2", {2, "File"}}, {"3", {3, "Back"}} };
    string in = PromptCLI("Read Cards from: ", modeMap);
    cout << "Chose: " << modeMap[in].second << endl;
    cout << LINE_ACROSS << endl;

    // Read m_Cards from file
    if (modeMap[in].first == 1)
    {
        return RoutineReadCardsFromCommandLine();
    }
    // Read m_Cards from command line
    else if (modeMap[in].first == 2) {
        return RoutineReadCardsFromFile(filterBasicLands, false).second;
    }
    else if (modeMap[in].first == 3)
    {
        return {};
    }
    return {};
}

// Main method
int main(int argc, char** argv) {
    map<string, bool> options = { {verboseOptionString, true}, {filterBasicsOptionString, true} };
    map<string, pair<int, string>> modeMap;
    string in;

    // No scientific notation
    cout << fixed;

    cout << "Welcome to the 'Magic the Gathering - Bang for your Buck' CLI" << endl;
    cout << LINE_ACROSS << endl;
    cout << "Default Options:" << endl << "Verbose: true" << endl << "Filter Basic Lands : true" << endl;
    cout << LINE_ACROSS << endl;

    // CLI Loop
    while (true)
    {
        // Fetch Input
        modeMap = { {"1", {1, "Run Query"}}, {"2", {2, "Options"}}, {"3", {3, "Exit"}} };
        in = PromptCLI("", modeMap);
        cout << LINE_ACROSS << endl;

        // Options
        if (modeMap[in].first == 1)
        {
            // Fetch Input
            modeMap = { {"1", {1, "General Set Data"}}, {"2", {2, "Precon Data"}}, {"3", {3, "Back"}} };
            in = PromptCLI("Please specify Mode: ", modeMap);
            cout << "Chose Mode: " << modeMap[in].second << endl;
            cout << LINE_ACROSS << endl;

            // General Set Data
            if (modeMap[in].first == 1)
            {
                // Read m_Cards
                vector<string> interestedCards = RoutineReadCards(options[filterBasicsOptionString]);

                // If no m_Cards have been specified, don't do the rest of the work
                if (interestedCards.size() == 0)
                {
                    continue;
                }

                // With our card list all m_Set, we can run the query
                RoutineGeneralSetDataQuery(interestedCards, options[verboseOptionString]);
            }
            // Precon Data
            else if (modeMap[in].first == 2)
            {
                // Read m_Cards
                vector<string> interestedCards = RoutineReadCards(options[filterBasicsOptionString]);

                // If no m_Cards have been specified, don't do the rest of the work
                if (interestedCards.size() == 0)
                {
                    continue;
                }

                // With our card list all m_Set, we can run the query
                RoutinePreconDataQuery(interestedCards, options[verboseOptionString]);
            }
            else if (modeMap[in].first == 3)
            {
                continue;
            }

            cout << LINE_ACROSS << endl;
        }
        // Run Query
        else if (modeMap[in].first == 2)
        {
            // Run Options
            // Build mode map
            modeMap.clear();
            int i = 1;
            for (auto& d : options)
            {
                modeMap[to_string(i)] = { i, d.first };
                i++;
            }
            string backString = to_string(modeMap.size() + 1);
            modeMap[backString] = { modeMap.size() + 1, "Back" };

            // Fetch option
            in = PromptCLI("Which Option to edit?: ", modeMap);

            if (in == to_string(modeMap.size()))
            {
                cout << LINE_ACROSS << endl;
                continue;
            }

            string option = modeMap[in].second;
            cout << LINE_ACROSS << endl;

            // Get new setting
            modeMap = { {"1", {1, "true"}}, {"2", {2, "false"}} };
            in = PromptCLI("New Value: ", modeMap);

            // Set option and print result
            options[option] = (in == "1" ? true : false);
            cout << "<" << option << "> now set to: " << (options[option] ? "true" : "false") << endl;

            cout << LINE_ACROSS << endl;
        }
        else if (modeMap[in].first == 3)
        {
            cout << "Exiting CLI" << endl;
            return 0;
        }
    }

    cout << LINE_ACROSS << endl;

    return 0;
}