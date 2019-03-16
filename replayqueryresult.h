#ifndef REPLAY_QUERY_RESULT_H
#define REPLAY_QUERY_RESULT_H

#include <string>

struct ReplayResult {
    bool flipped;
    unsigned int match0;
    unsigned int match1;

    std::string id;
    unsigned long long date;
    std::string result;
    std::string resultDesc;
    bool ranked;
    std::string mode;
    std::string title;
    std::string link;
    std::string source;
    std::string deck0;
    std::string deck1;
    std::string region;
    std::string authorLink;
    std::string authorName;
};

class ReplayQueryResult {
public:
    unsigned int replayCount;
    ReplayResult * replays;

    unsigned int totalReplayCount;

    ~ReplayQueryResult() {
        delete [] this->replays;
    }
};

#endif
