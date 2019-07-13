#ifndef CARDDB_H
#define CARDDB_H

#include <string>
#include <vector>

#include "namedbitfield.h"
#include "alignment.h"
#include "replayqueryresult.h"
#include "stringtable.h"

class ReplayDb {
public:
    struct MatchResult {
        bool flipped;
        unsigned long long sort;
        unsigned int match0;
        unsigned int match1;
    };

private:

    std::string gameName;
    std::map<std::string, unsigned int> idMap;

    NamedBitField modeNames;
    NamedBitField sourceNames;
    NamedBitField resultNames;

    unsigned int cardCount;
    unsigned char * searchTable;
    unsigned char * replayTable;
    unsigned int replayCount;
    unsigned int replayCapacity;

    unsigned char * cachedSearchBitField;
    unsigned char * cachedFlipSearchBitField;

    unsigned int searchRowSz;
    unsigned int replayRowSz;

    unsigned int cardBitFieldByteSize;
    StringTable stringTable;

    void PrintIndexes(const unsigned int * cardIndexes, unsigned int count);
    void PrintBitString(const unsigned int * bitString, unsigned int count);
    void PrintCompareBitString(const unsigned int * bitStringA, const unsigned int * bitStringB, unsigned int count);
    
    bool IsBigEndian();
    void CacheSearchBitField(unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1);
    MatchResult Match(unsigned int replayIndex, bool flipped, unsigned long long minDate, bool ranked, bool unranked, unsigned int sourcesBitField, unsigned int modesBitField, unsigned int resultBitField);

    unsigned int GetReplayIndex(const char * id);
    bool Load();

    std::string GetId(unsigned int replayIndex);
    unsigned long long GetDate(unsigned int replayIndex);
    std::string GetResult(unsigned int replayIndex);
    std::string GetResultsDesc(unsigned int replayIndex);
    bool GetRanked(unsigned int replayIndex);
    std::string GetMode(unsigned int replayIndex);
    std::string GetTitle(unsigned int replayIndex);
    std::string GetLink(unsigned int replayIndex);
    std::string GetSource(unsigned int replayIndex);
    std::string GetDeck0(unsigned int replayIndex);
    std::string GetDeck1(unsigned int replayIndex);
    std::string GetRegion(unsigned int replayIndex);
    std::string GetAuthorLink(unsigned int replayIndex);
    std::string GetAuthorName(unsigned int replayIndex);

public:
    ReplayDb(const char * gameName, unsigned int numCards);
    ~ReplayDb();

    void Save();

    void RemoveReplay(const char * id);
    void SetReplay(const char * id, unsigned long long date, const char * result, const char * resultDesc, const char * mode, const char * title, const char * link, const char * source, const char * deck0, const char * deck1, const char * region, const char * authorLink, const char * authorName, bool ranked, unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1);

    unsigned int GetReplayCount();
    ReplayResult GetReplay(unsigned int replayIndex);
    ReplayResult GetReplay(const char * id);

    ReplayQueryResult * NewGames(unsigned int offset, unsigned int numResults, unsigned long long minDate, bool ranked, bool unranked, bool onlyWins, unsigned int numSources, std::string * sources, unsigned int numModes, std::string * modes);
    ReplayQueryResult * Search(unsigned int offset, unsigned int numResults, unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1, unsigned long long minDate, bool ranked, bool unranked, bool fromPlayer, bool fromOpponent, bool onlyWins, unsigned int numSources, std::string * sources, unsigned int numModes, std::string * modes);
};

#endif // CARDDB_H
