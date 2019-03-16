#include "replaydb.h"
#include <cmath>
#include <stdio.h>

#include <chrono>
#include <iostream>
using namespace std::chrono;

#define REPLAY_ID_SIZE 18
#define REPLAY_DATE_SIZE sizeof(unsigned long long) // YYYYMMDDHHMM
#define REPLAY_RESULTS_DESC_SIZE 7
#define REPLAY_TITLE_SIZE 256
#define REPLAY_LINK_SIZE 96
#define REPLAY_DECK0_SIZE 18
#define REPLAY_DECK1_SIZE 18
#define REPLAY_REGION_SIZE 42
#define REPLAY_AUTHOR_LINK_SIZE 96
#define REPLAY_AUTHOR_NAME_SIZE 42

#define REPLAY_RANKED_BITS 1
#define REPLAY_MODE_BITS 7
#define REPLAY_SOURCE_BITS 6
#define REPLAY_RESULT_BITS 4

struct ReplayBits {
    unsigned int ranked : REPLAY_RANKED_BITS;
    unsigned int mode : REPLAY_MODE_BITS;
    unsigned int source : REPLAY_SOURCE_BITS;
    unsigned int result : REPLAY_RESULT_BITS;
};
#define REPLAY_BITS_SIZE sizeof(ReplayBits)


#define REPLAY_MIN_CAPACITY 1024
#define REPLAY_GROW_CAPACITY 512

#define ARCHIVE_VERSION_NUMBER 1
#define ARCHIVE_STAMP (('R' << 0) | ('R' << 8) | ('D' << 16) | ('B' << 24))

void ReplayDb::CacheSearchBitField(unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1) {
    memset(this->cachedSearchBitField, 0, this->searchRowSz);

    for (unsigned int a=0; a<numCards0; ++a) {
        unsigned int index = cardIndexes0[a];
        unsigned int byteIndex = index / 8;
        unsigned int bitOffset = index - byteIndex * 8;

        this->cachedSearchBitField[byteIndex] |= (1 << bitOffset);
    }

    for (unsigned int a=0; a<numCards1; ++a) {
        unsigned int index = cardIndexes1[a];
        unsigned int byteIndex = index / 8;
        unsigned int bitOffset = index - byteIndex * 8;

        this->cachedSearchBitField[this->cardBitFieldByteSize + byteIndex] |= (1 << bitOffset);
    }

    unsigned int cards0Pos = 0;
    unsigned int cards1Pos = cards0Pos + this->cardBitFieldByteSize;

    memcpy(this->cachedFlipSearchBitField, this->cachedSearchBitField, cards0Pos);
    memcpy(this->cachedFlipSearchBitField+cards0Pos, this->cachedSearchBitField+cards1Pos, this->cardBitFieldByteSize);
    memcpy(this->cachedFlipSearchBitField+cards1Pos, this->cachedSearchBitField+cards0Pos, this->cardBitFieldByteSize);
}

unsigned int popcount(unsigned int i) {
     i = i - ((i >> 1) & 0x55555555);
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

ReplayDb::MatchResult ReplayDb::Match(unsigned int replayIndex, bool flipped, unsigned long long minDate, bool ranked, bool unranked, unsigned int sourcesBitField, unsigned int modesBitField, unsigned int resultBitField) {
    const unsigned char * searchBitField = flipped ? this->cachedFlipSearchBitField : this->cachedSearchBitField;
    const unsigned char * dateData = this->searchTable + this->searchRowSz * replayIndex;
    const unsigned char * data = dateData + REPLAY_DATE_SIZE + REPLAY_BITS_SIZE;
    const unsigned int * search0Int = (const unsigned int *)searchBitField;
    const unsigned int * search1Int = (const unsigned int *)(searchBitField + this->cardBitFieldByteSize);
    const unsigned int * replay0Int = (const unsigned int *)data;
    const unsigned int * replay1Int = (const unsigned int *)(data + this->cardBitFieldByteSize);
    unsigned int intCount = (unsigned int)(this->cardBitFieldByteSize / 4);

    MatchResult ret;

    ret.flipped = flipped;
    ret.sort = 0;
    ret.match0 = 0;
    ret.match1 = 0;

    unsigned long long date = *((unsigned long long *)dateData);
    if (date < minDate) {
        return ret;
    }

    ReplayBits * bits = (ReplayBits *)(dateData + sizeof(unsigned long long));
    if (!ranked && bits->ranked) {
        return ret;
    }
    if (!unranked && !bits->ranked) {
        return ret;
    }

    if (!this->sourceNames.NameMatchesSearchBitField(sourcesBitField, bits->source)) {
        return ret;
    }

    if (!this->modeNames.NameMatchesSearchBitField(modesBitField, bits->mode)) {
        return ret;
    }

    if (!this->resultNames.NameMatchesSearchBitField(resultBitField, bits->result)) {
        return ret;
    }
    
    for (unsigned int a=0; a<intCount; ++a) {
        ret.match0 += popcount(search0Int[a] & replay0Int[a]);
    }
    for (unsigned int a=0; a<intCount; ++a) {
        ret.match1 += popcount(search1Int[a] & replay1Int[a]);
    }

    if (flipped) {
        ret.sort = ret.match1 * 2 + ret.match0;
    } else {
        ret.sort = ret.match0 * 2 + ret.match1;
    }
    ret.sort = ret.sort << 44;
    ret.sort += date;

    return ret;
}

unsigned int ReplayDb::GetReplayIndex(const char * id) {
    for (unsigned int a=0; a<this->replayCount; ++a) {
        if (0 == strncmp((const char *)this->replayTable + (a * this->replayRowSz), id, REPLAY_ID_SIZE)) {
            return a;
        }
    }

    return (unsigned int)-1;
}

struct ArchiveHeader {
    unsigned int stamp;
    unsigned int version;
    unsigned int replayCount;
    unsigned int searchRowSz;
    unsigned int replayRowSz;

    unsigned int modeNamesPos;
    unsigned int sourceNamesPos;
    unsigned int resultNamesPos;
    unsigned int searchTablePos;
    unsigned int replayTablePos;
};

void ReplayDb::Save() {
    ArchiveHeader header;
    header.stamp = ARCHIVE_STAMP;
    header.version = ARCHIVE_VERSION_NUMBER;
    
    header.replayCount = this->replayCount;
    header.searchRowSz = this->searchRowSz;
    header.replayRowSz = this->replayRowSz;

    unsigned int sz = sizeof(ArchiveHeader);
    sz = ROUND_TO_ALIGN(sz);

    header.modeNamesPos = sz;
    sz = ROUND_TO_ALIGN(sz + this->modeNames.GetSerializeByteSize());

    header.sourceNamesPos = sz;
    sz = ROUND_TO_ALIGN(sz + this->sourceNames.GetSerializeByteSize());

    header.resultNamesPos = sz;
    sz = ROUND_TO_ALIGN(sz + this->resultNames.GetSerializeByteSize());

    unsigned int searchTableSz = this->replayCount * this->searchRowSz;
    header.searchTablePos = sz;
    sz = ROUND_TO_ALIGN(sz + searchTableSz);

    unsigned int replayTableSz = this->replayCount * this->replayRowSz;
    header.replayTablePos = sz;
    sz = ROUND_TO_ALIGN(sz + replayTableSz);

    unsigned char * data = new unsigned char[sz];
    memset(data, 0, sz);

    memcpy(data, &header, sizeof(ArchiveHeader));

    this->modeNames.SerializeOut(data + header.modeNamesPos);
    this->sourceNames.SerializeOut(data + header.sourceNamesPos);
    this->resultNames.SerializeOut(data + header.resultNamesPos);

    memcpy(data + header.searchTablePos, this->searchTable, searchTableSz);
    memcpy(data + header.replayTablePos, this->replayTable, replayTableSz);

    std::string fileName = std::string(this->gameName) + ".rrdb";
    FILE * f = fopen(fileName.c_str(), "wb");
    fwrite(data, sz, 1, f);
    fclose(f);

    delete [] data;
}

bool ReplayDb::Load() {
    std::string fileName = std::string(this->gameName) + ".rrdb";
    FILE * f = fopen(fileName.c_str(), "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    unsigned int sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char * data = new unsigned char[sz];
    fread(data, sz, 1, f);
    fclose(f);

    ArchiveHeader * header = (ArchiveHeader *)data;

    if (header->searchRowSz != this->searchRowSz || header->replayRowSz != this->replayRowSz) {
        delete [] data;
        return false;
    }

    this->modeNames.SerializeIn(data + header->modeNamesPos);
    this->sourceNames.SerializeIn(data + header->sourceNamesPos);
    this->resultNames.SerializeIn(data + header->resultNamesPos);

    this->replayCount = header->replayCount;
    this->replayCapacity = header->replayCount;

    unsigned int searchTableSz = this->replayCount * this->searchRowSz;
    this->searchTable = new unsigned char[searchTableSz];
    memcpy(this->searchTable, data + header->searchTablePos, searchTableSz);

    unsigned int replayTableSz = this->replayCount * this->replayRowSz;
    this->replayTable = new unsigned char[replayTableSz];
    memcpy(this->replayTable, data + header->replayTablePos, replayTableSz);

    delete [] data;

    this->idMap.clear();
    for (unsigned int a=0; a<this->replayCount; ++a) {
        std::string sid = this->GetId(a);
        this->idMap[sid] = a;
    }

    return true;
}

const ReplayBits * GetBits(unsigned char * replayData) {
    return (const ReplayBits *)(replayData + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE + REPLAY_DECK1_SIZE + REPLAY_REGION_SIZE + REPLAY_AUTHOR_LINK_SIZE + REPLAY_AUTHOR_NAME_SIZE);
}

std::string ReplayDb::GetId(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex;
    char out[REPLAY_ID_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_ID_SIZE);
    out[REPLAY_ID_SIZE] = 0;
    return out;
}

unsigned long long ReplayDb::GetDate(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE;
    return *(unsigned long long *)data;
}

std::string ReplayDb::GetResult(unsigned int replayIndex) {
    const ReplayBits * bits = GetBits(this->replayTable + this->replayRowSz * replayIndex);
    return this->resultNames.GetName(bits->result);
}

std::string ReplayDb::GetResultsDesc(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE;
    char out[REPLAY_RESULTS_DESC_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_RESULTS_DESC_SIZE);
    out[REPLAY_RESULTS_DESC_SIZE] = 0;
    return out;
}


std::string ReplayDb::GetMode(unsigned int replayIndex) {
    const ReplayBits * bits = GetBits(this->replayTable + this->replayRowSz * replayIndex);
    return this->modeNames.GetName(bits->mode);
}

bool ReplayDb::GetRanked(unsigned int replayIndex) {
    const ReplayBits * bits = GetBits(this->replayTable + this->replayRowSz * replayIndex);
    return !!bits->ranked;
}

std::string ReplayDb::GetTitle(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE;
    char out[REPLAY_TITLE_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_TITLE_SIZE);
    out[REPLAY_TITLE_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetLink(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE;
    char out[REPLAY_LINK_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_LINK_SIZE);
    out[REPLAY_LINK_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetSource(unsigned int replayIndex) {
    const ReplayBits * bits = GetBits(this->replayTable + this->replayRowSz * replayIndex);
    return this->sourceNames.GetName(bits->source);
}

std::string ReplayDb::GetDeck0(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE;
    char out[REPLAY_DECK0_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_DECK0_SIZE);
    out[REPLAY_DECK0_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetDeck1(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE;
    char out[REPLAY_DECK1_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_DECK1_SIZE);
    out[REPLAY_DECK1_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetRegion(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE + REPLAY_DECK1_SIZE;
    char out[REPLAY_REGION_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_REGION_SIZE);
    out[REPLAY_REGION_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetAuthorLink(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE + REPLAY_DECK1_SIZE + REPLAY_REGION_SIZE;
    char out[REPLAY_AUTHOR_LINK_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_AUTHOR_LINK_SIZE);
    out[REPLAY_AUTHOR_LINK_SIZE] = 0;
    return out;
}

std::string ReplayDb::GetAuthorName(unsigned int replayIndex) {
    const unsigned char * data = this->replayTable + this->replayRowSz * replayIndex + REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE + REPLAY_DECK1_SIZE + REPLAY_REGION_SIZE + REPLAY_AUTHOR_LINK_SIZE;
    char out[REPLAY_AUTHOR_NAME_SIZE+1];
    strncpy(out, (const char *)data, REPLAY_AUTHOR_NAME_SIZE);
    out[REPLAY_AUTHOR_NAME_SIZE] = 0;
    return out;
}

ReplayDb::ReplayDb(const char * gameName, unsigned int numCards) {
    this->gameName = gameName;
    this->cardCount = numCards;
    this->cardBitFieldByteSize = ROUND_TO_ALIGN((this->cardCount + 8 - 1) / 8);

    this->searchTable = 0;
    this->replayTable = 0;
    this->replayCount = 0;
    this->replayCapacity = 0;

    this->searchRowSz = ROUND_TO_ALIGN(REPLAY_DATE_SIZE + REPLAY_BITS_SIZE + this->cardBitFieldByteSize * 2);
    this->replayRowSz = ROUND_TO_ALIGN(REPLAY_ID_SIZE + REPLAY_DATE_SIZE + REPLAY_RESULTS_DESC_SIZE + REPLAY_TITLE_SIZE + REPLAY_LINK_SIZE + REPLAY_DECK0_SIZE + REPLAY_DECK1_SIZE + REPLAY_REGION_SIZE + REPLAY_AUTHOR_LINK_SIZE + REPLAY_AUTHOR_NAME_SIZE + REPLAY_BITS_SIZE);

    this->cachedSearchBitField = new unsigned char[this->searchRowSz];
    this->cachedFlipSearchBitField = new unsigned char[this->searchRowSz];

    if (!this->Load()) {
        this->replayCapacity = REPLAY_MIN_CAPACITY;
        this->searchTable = new unsigned char[this->replayCapacity * this->searchRowSz];
        this->replayTable = new unsigned char[this->replayCapacity * this->replayRowSz];
    }
}

ReplayDb::~ReplayDb() {
    delete [] this->cachedFlipSearchBitField;
    delete [] this->cachedSearchBitField;
    delete [] this->searchTable;
    delete [] this->replayTable;
}

void ReplayDb::RemoveReplay(const char * id) {
    unsigned int index = this->GetReplayIndex(id);
    if (index == (unsigned int)-1) {
        return;
    }

    std::string sid = id;
    this->idMap.erase(sid);

    unsigned int copyCount = this->replayCount - index - 1;
    if (copyCount > 0) {
        unsigned char * dstData = this->replayTable + this->replayRowSz * index;
        unsigned char * srcData = dstData + this->replayRowSz;
        unsigned int sz = this->replayRowSz * copyCount;
        memmove(dstData, srcData, sz);

        dstData = this->searchTable + this->searchRowSz * index;
        srcData = dstData + this->searchRowSz;
        sz = this->searchRowSz * copyCount;
        memmove(dstData, srcData, sz);
    }
    this->replayCount -= 1;
}

void ReplayDb::SetReplay(const char * id, unsigned long long date, const char * result, const char * resultDesc, const char * mode, const char * title, const char * link, const char * source, const char * deck0, const char * deck1, const char * region, const char * authorLink, const char * authorName, bool ranked, unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1) {
    unsigned int index = this->GetReplayIndex(id);
    if (index == (unsigned int)-1) {
        index = this->replayCount;

        if (this->replayCount+1 > this->replayCapacity) {
            unsigned int newCapacity = this->replayCapacity + REPLAY_GROW_CAPACITY;
            
            unsigned char * newSearchTable = new unsigned char[newCapacity * this->searchRowSz];
            unsigned char * newReplayTable = new unsigned char[newCapacity * this->replayRowSz];

            memcpy(newSearchTable, this->searchTable, this->replayCount * this->searchRowSz);
            memcpy(newReplayTable, this->replayTable, this->replayCount * this->replayRowSz);

            delete [] this->searchTable;
            this->searchTable = newSearchTable;

            delete [] this->replayTable;
            this->replayTable = newReplayTable;

            this->replayCapacity = newCapacity;
        }

        this->replayCount += 1;
    }

    unsigned char * destReplayData = this->replayTable + index * this->replayRowSz;

#define WRITE_REPLAY_FIELD(dest, d, sz) \
    memset(dest, 0, sz); \
    strncpy((char *)dest, d, sz); \
    dest += sz;

    WRITE_REPLAY_FIELD(destReplayData, id, REPLAY_ID_SIZE);

    *((unsigned long long *)destReplayData) = date;
    destReplayData += sizeof(unsigned long long);

    WRITE_REPLAY_FIELD(destReplayData, resultDesc, REPLAY_RESULTS_DESC_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, title, REPLAY_TITLE_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, link, REPLAY_LINK_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, deck0, REPLAY_DECK0_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, deck1, REPLAY_DECK1_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, region, REPLAY_REGION_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, authorLink, REPLAY_AUTHOR_LINK_SIZE);
    WRITE_REPLAY_FIELD(destReplayData, authorName, REPLAY_AUTHOR_NAME_SIZE);

    ReplayBits bits;
    bits.ranked = ranked ? 1 : 0;
    bits.mode = this->modeNames.GetBits(mode);
    bits.source = this->sourceNames.GetBits(source);
    bits.result = this->resultNames.GetBits(result);
    memcpy(destReplayData, &bits, REPLAY_BITS_SIZE);
    destReplayData += REPLAY_BITS_SIZE;

    unsigned char * destSearchData = this->searchTable + this->searchRowSz * index;
    *((unsigned long long *)destSearchData) = date;
    destSearchData += sizeof(unsigned long long);

    memcpy(destSearchData, &bits, REPLAY_BITS_SIZE);
    destSearchData += REPLAY_BITS_SIZE;

    this->CacheSearchBitField(numCards0, cardIndexes0, numCards1, cardIndexes1);
    memcpy(destSearchData, this->cachedSearchBitField, this->searchRowSz - REPLAY_DATE_SIZE - REPLAY_BITS_SIZE);

    std::string sid = id;
    this->idMap[sid] = index;
}

struct ReplaySortData {
    ReplayDb::MatchResult match;
    unsigned int replayIndex;
};

int ReplaySortDataComparator(const void * a, const void * b) {
    const ReplaySortData * rsd0 = (ReplaySortData *)a;
    const ReplaySortData * rsd1 = (ReplaySortData *)b;

    const unsigned long long s0 = rsd0->match.sort;
    const unsigned long long s1 = rsd1->match.sort;

    return s0 == s1 ? 0 : (s0 > s1 ? -1 : 1);
}

unsigned int ReplayDb::GetReplayCount() {
    return this->replayCount;
}

ReplayResult ReplayDb::GetReplay(unsigned int replayIndex) {
    ReplayResult ret;
    ret.flipped = false;
    ret.match0 = 0;
    ret.match1 = 0;

    unsigned int r = replayIndex;
    ret.id = this->GetId(r);
    ret.date = this->GetDate(r);
    ret.result = this->GetResult(r);
    ret.resultDesc = this->GetResultsDesc(r);
    ret.mode = this->GetMode(r);
    ret.title = this->GetTitle(r);
    ret.link = this->GetLink(r);
    ret.source = this->GetSource(r);
    ret.deck0 = this->GetDeck0(r);
    ret.deck1 = this->GetDeck1(r);
    ret.region = this->GetRegion(r);
    ret.authorLink = this->GetAuthorLink(r);
    ret.authorName = this->GetAuthorName(r);
    ret.ranked = this->GetRanked(r);

    return ret;
}

ReplayResult ReplayDb::GetReplay(const char * id) {
    std::string sid = id;
    if (this->idMap.count(sid) == 0) {
        ReplayResult ret;
        ret.flipped = false;
        ret.match0 = 0;
        ret.match1 = 0;
        ret.id = "";
        return ret;
    }

    return this->GetReplay(this->idMap[sid]);
}

ReplayQueryResult * ReplayDb::NewGames(unsigned int offset, unsigned int numResults, unsigned long long minDate, bool ranked, bool unranked, bool onlyWins, unsigned int numSources, std::string * sources, unsigned int numModes, std::string * modes) {
    if (!ranked && !unranked) {
        return 0;
    }

    // TODO: insertion sort instead of qsort every time
    ReplaySortData * results = new ReplaySortData[offset + numResults];
    unsigned int resultCount = 0;

    unsigned int sourcesBitField = this->sourceNames.GetSearchBitField(numSources, sources);
    unsigned int modesBitField = this->modeNames.GetSearchBitField(numModes, modes);
    unsigned int resultBitField = ~((unsigned int)0);

    if (onlyWins) {
        std::string sw = "win";
        resultBitField = this->resultNames.GetSearchBitField(1, &sw);
    }

    unsigned int validCount = 0;
    for (unsigned int a=0; a<this->replayCount; ++a) {
        MatchResult match;

        const unsigned char * dateData = this->searchTable + this->searchRowSz * a;
        unsigned long long date = *((unsigned long long *)dateData);
        if (date < minDate) {
            continue;
        }

        ReplayBits * bits = (ReplayBits *)(dateData + sizeof(unsigned long long));
        if (!ranked && bits->ranked) {
            continue;
        }
        if (!unranked && !bits->ranked) {
            continue;
        }

        if (!this->sourceNames.NameMatchesSearchBitField(sourcesBitField, bits->source)) {
            continue;
        }

        if (!this->modeNames.NameMatchesSearchBitField(modesBitField, bits->mode)) {
            continue;
        }

        if (!this->resultNames.NameMatchesSearchBitField(resultBitField, bits->result)) {
            continue;
        }

        match.flipped = false;
        match.sort = date;
        match.match0 = 0;
        match.match1 = 0;

        validCount += 1;

        if (resultCount < offset+numResults) {
            results[resultCount].match = match;
            results[resultCount].replayIndex = a;
            resultCount += 1;
            qsort((void *)results, resultCount, sizeof(ReplaySortData), ReplaySortDataComparator);
        } else if (match.sort > results[offset+numResults-1].match.sort) {
            results[offset+numResults-1].match = match;
            results[offset+numResults-1].replayIndex = a;
            qsort((void *)results, resultCount, sizeof(ReplaySortData), ReplaySortDataComparator);
        }
    }

    ReplayQueryResult * ret = new ReplayQueryResult();
    ret->replayCount = resultCount - offset;
    ret->totalReplayCount = validCount;
    ret->replays = new ReplayResult[ret->replayCount];

    for (unsigned int a=offset; a<resultCount; ++a) {
        unsigned int r = results[a].replayIndex;
        ret->replays[a - offset] = this->GetReplay(r);
        ret->replays[a - offset].flipped = results[a].match.flipped;
        ret->replays[a - offset].match0 = results[a].match.match0;
        ret->replays[a - offset].match1 = results[a].match.match1;
    }

    delete [] results;

    return ret;
}

ReplayQueryResult * ReplayDb::Search(unsigned int offset, unsigned int numResults, unsigned int numCards0, unsigned int * cardIndexes0, unsigned int numCards1, unsigned int * cardIndexes1, unsigned long long minDate, bool ranked, bool unranked, bool fromPlayer, bool fromOpponent, bool onlyWins, unsigned int numSources, std::string * sources, unsigned int numModes, std::string * modes) {
    if (!fromPlayer && !fromOpponent) {
        return 0;
    }
    if (!ranked && !unranked) {
        return 0;
    }

    // TODO: insertion sort instead of qsort every time
    ReplaySortData * results = new ReplaySortData[offset + numResults];
    unsigned int resultCount = 0;

    unsigned int sourcesBitField = this->sourceNames.GetSearchBitField(numSources, sources);
    unsigned int modesBitField = this->modeNames.GetSearchBitField(numModes, modes);
    unsigned int resultBitField = ~((unsigned int)0);
    unsigned int flipResultBitField = ~((unsigned int)0);

    if (onlyWins) {
        std::string sw = "win";
        std::string sl = "loss";
        resultBitField = this->resultNames.GetSearchBitField(1, &sw);
        flipResultBitField = this->resultNames.GetSearchBitField(1, &sl);
    }

    this->CacheSearchBitField(numCards0, cardIndexes0, numCards1, cardIndexes1);
    unsigned int validCount = 0;
    for (unsigned int a=0; a<this->replayCount; ++a) {
        MatchResult match;

        if (fromPlayer && fromOpponent) {
            MatchResult match0 = this->Match(a, false, minDate, ranked, unranked, sourcesBitField, modesBitField, resultBitField);
            MatchResult match1 = this->Match(a, true, minDate, ranked, unranked, sourcesBitField, modesBitField, flipResultBitField);
            match = match1.sort > match0.sort ? match1 : match0;
        } else if (fromPlayer) {
            match = this->Match(a, false, minDate, ranked, unranked, sourcesBitField, modesBitField, resultBitField);
        } else if (fromOpponent) {
            match = this->Match(a, true, minDate, ranked, unranked, sourcesBitField, modesBitField, flipResultBitField);
        }
        if (match.sort == 0) {
            continue;
        }

        if (match.match0 == 0 && match.match1 == 0) {
            continue;
        }

        validCount += 1;

        if (resultCount < offset+numResults) {
            results[resultCount].match = match;
            results[resultCount].replayIndex = a;
            resultCount += 1;
            qsort((void *)results, resultCount, sizeof(ReplaySortData), ReplaySortDataComparator);
        } else if (match.sort > results[offset+numResults-1].match.sort) {
            results[offset+numResults-1].match = match;
            results[offset+numResults-1].replayIndex = a;
            qsort((void *)results, resultCount, sizeof(ReplaySortData), ReplaySortDataComparator);
        }
    }

    ReplayQueryResult * ret = new ReplayQueryResult();
    ret->replayCount = resultCount - offset;
    ret->totalReplayCount = validCount;
    ret->replays = new ReplayResult[ret->replayCount];

    for (unsigned int a=offset; a<resultCount; ++a) {
        unsigned int r = results[a].replayIndex;
        ret->replays[a - offset] = this->GetReplay(r);
        ret->replays[a - offset].flipped = results[a].match.flipped;
        ret->replays[a - offset].match0 = results[a].match.match0;
        ret->replays[a - offset].match1 = results[a].match.match1;
    }

    delete [] results;

    return ret;
}

